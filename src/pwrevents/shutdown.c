/* @@@LICENSE
*
*      Copyright (c) 2011-2013 LG Electronics, Inc.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
* LICENSE@@@ */


/**
* @file shutdown.c
*
* @brief Two-tiered shutdown system sequence.
*
*
*/

#include <glib.h>
#include <cjson/json.h>
#include <syslog.h>
#include <luna-service2/lunaservice.h>

#include "lunaservice_utils.h"
#include "debug.h"
#include "main.h"
#include "logging.h"
#include "machine.h"
#include "init.h"

#define LOG_DOMAIN "SHUTDOWN: "


/**
* @brief Contains list of applications and services
*        interested in shutdown.
*/
typedef struct {
    GHashTable *applications;
    GHashTable *services;

    int num_ack;
    int num_nack;
} ShutdownClientList;

typedef enum {
    kShutdownReplyNoRsp,
    kShutdownReplyAck,
    kShutdownReplyNack,
    kShutdownReplyLast
} ShutdownReply;

const char* ShutdownReplyString[kShutdownReplyLast] =
{   "No_Response",
    "Ack",
    "Nack",
};

static const char*
shutdown_reply_to_string(ShutdownReply reply)
{
    if (reply < 0 || reply >= kShutdownReplyLast)
        return "";

    return ShutdownReplyString[reply];
}

/**
* @brief Client information.
*/
typedef struct {
    char            *id;
    char            *name;
    ShutdownReply    ack_shutdown;

    double           elapsed;
} ShutdownClient;

/**
* @brief States.
*/
enum {
    kPowerShutdownNone,

    kPowerShutdownApps,
    kPowerShutdownAppsProcess,

    kPowerShutdownServices,
    kPowerShutdownServicesProcess,

    kPowerShutdownAction,

    kPowerShutdownLast
};
typedef int ShutdownState;

/**
* @brief Events types that drive the state machine.
*/
typedef enum {
    kShutdownEventNone,

    kShutdownEventShutdownInit,
    kShutdownEventAbort,

    kShutdownEventAck,

    kShutdownEventAllAck,

    kShutdownEventTimeout,
} ShutdownEventType;

/**
* @brief Event
*/
typedef struct {
    ShutdownEventType    id;
    ShutdownClient      *client;
} ShutdownEvent;

/**
 * PowerShutdownProc the current state and returns the next state
 */
typedef bool (*PowerShutdownProc)(ShutdownEvent *event, ShutdownState *next);

typedef struct {
    const char *name;
    ShutdownState     state;
    PowerShutdownProc function;
} ShutdownStateNode;

static bool state_idle(ShutdownEvent *event, ShutdownState *next);
static bool state_shutdown_apps(ShutdownEvent *event, ShutdownState *next);
static bool state_shutdown_apps_process(ShutdownEvent *event, ShutdownState *next);
static bool state_shutdown_services(ShutdownEvent *event, ShutdownState *next);
static bool state_shutdown_services_process(ShutdownEvent *event, ShutdownState *next);
static bool state_shutdown_action(ShutdownEvent *event, ShutdownState *next);

static void send_shutdown_apps();
static void send_shutdown_services();

static bool shutdown_timeout(void *data);

/**
 * Mapping from state to function handling state.
 */
static const ShutdownStateNode kStateMachine[kPowerShutdownLast] = {
    { "ShutdownIdle",            kPowerShutdownNone,          state_idle                    },
    { "ShutdownApps",            kPowerShutdownApps,          state_shutdown_apps           },
    { "ShutdownAppsProcess",     kPowerShutdownAppsProcess,   state_shutdown_apps_process    },
    { "ShutdownServices",        kPowerShutdownServices,      state_shutdown_services       },
    { "ShutdownServicesProcess", kPowerShutdownServicesProcess, state_shutdown_services_process },
    { "ShutdownAction",          kPowerShutdownAction,        state_shutdown_action },
};

/* Globals */

const ShutdownStateNode  *gCurrentState = NULL;
ShutdownClientList       *sClientList = NULL;
LSMessage                *shutdown_message = NULL;

guint shutdown_apps_timeout_id = 0;
GTimer  *shutdown_timer = NULL;

/**
 * @defgroup ShutdownProcess	Shutdown Process
 * @ingroup PowerEvents
 * @brief The Shutdown Process:
 *
 * When sleepd receives the /shutdown/initiate luna-call, it sends the shutdown signal
 * (shutdownApplications) to all the registered applications. It will proceed to the next
 * stage i.e sending shutdown signal (shutdownServices) to all the registered services, if
 * all the registered applications respond back by "Ack" or after a timeout of 15 sec.
 *
 * Again after sending the shutdownServices signal, it will allow a max timeout of 15 sec
 * for all the registered services to respond back. Finally it will respond back to the
 * caller of the "initiate" luna-call with success to indicate the completion of the
 * shutdown process.
 */

/**
 * @addtogroup ShutdownProcess
 * @{
 */



/* Voting */



/**
 * @brief Allocate memory for a new shutdown client
 */

static ShutdownClient *
client_new(const char *key, const char *clientName)
{
    ShutdownClient *client = g_new0(ShutdownClient, 1);
    client->id  = g_strdup(key);
    client->name = g_strdup(clientName);
    client->ack_shutdown = kShutdownReplyNoRsp;

    return client;
}

/**
 * @brief Destroy the client
 */
static void
client_free(ShutdownClient *client)
{
    if (client)
    {
        g_free(client->id);
        g_free(client->name);
        g_free(client);
    }
}

/**
 * @brief Create a client structure for an application and add it to the global application list
 *
 * @param key Unique key for this client
 * @param clientName Name of the application
 */
static void
client_new_application(const char *key, const char *clientName)
{
    ShutdownClient *client = client_new(key, clientName);
    g_hash_table_replace(sClientList->applications, client->id, client);
}

/**
 * @brief Create a client structure for a service and add it to the global service list
 *
 * @param key Unique key for this client
 * @param clientName Name of the application
 */
static void
client_new_service(const char *key, const char *clientName)
{
    ShutdownClient *client = client_new(key, clientName);
    g_hash_table_replace(sClientList->services, client->id, client);
}

/**
 * @brief Clear the vote count for the given client
 */
static void
client_vote_clear(const char *key, ShutdownClient *client, void *data)
{
    _assert(client != NULL);

    client->ack_shutdown = kShutdownReplyNoRsp;
    client->elapsed = 0.0;
}

/**
 * @brief Remove the application from the global shutdown application list
 */

static void
client_unregister_application(const char *uid)
{
    g_hash_table_remove(sClientList->applications, uid);
}

/**
 * @brief Remove the service from the global shutdown service list
 */
static void
client_unregister_service(const char *uid)
{
    g_hash_table_remove(sClientList->services, uid);
}


/**
 * @brief Lookup a service in the service hash list
 */
static ShutdownClient *
client_lookup_service(const char *uid)
{
    return (ShutdownClient*)g_hash_table_lookup(
                            sClientList->services, uid);
}

/**
 * @brief Lookup an application in the application hash list
 */
static ShutdownClient *
client_lookup_app(const char *uid)
{
    return (ShutdownClient*)g_hash_table_lookup(
                            sClientList->applications, uid);
}


/**
 * @brief Reset global ACK & NACK counts.
 */
static void
client_list_reset_ack_count()
{
    sClientList->num_ack = 0;
    sClientList->num_nack = 0;
}

/**
 * @brief Clear the ACK & NACK counts for all the registered applications and services.
 */
static void
client_list_vote_init()
{
    g_hash_table_foreach(sClientList->applications,
            (GHFunc)client_vote_clear, NULL);
    g_hash_table_foreach(sClientList->services,
            (GHFunc)client_vote_clear, NULL);

    client_list_reset_ack_count();
}

/**
 * @brief Add an ACK or NACK vote for a client
 */

static void
client_vote(ShutdownClient *client, bool ack)
{
    if (!client) return;

    client->ack_shutdown = ack ? kShutdownReplyAck : kShutdownReplyNack;

    client->elapsed = g_timer_elapsed(shutdown_timer, NULL);

    if (ack)
    {
        sClientList->num_ack++;
    }
    else
    {
        sClientList->num_nack++;
    }
}

/**
 * @brief Log the client's response for the current shutdown voting process
 */
static void
client_vote_print(const char *key, ShutdownClient *client, void *data)
{
    SLEEPDLOG(LOG_INFO, "    %s %s %s @ %fs", client->id, client->name,
            shutdown_reply_to_string(client->ack_shutdown),
            client->elapsed);
}

/**
 * @brief Go through all the clients in the given hash table and log their responses
 */

static void
client_list_print(GHashTable *client_table)
{
    int size = g_hash_table_size(client_table);

    SLEEPDLOG(LOG_INFO, "clients:");

    if (size > 0)
    {
        g_hash_table_foreach(client_table,
                (GHFunc)client_vote_print, NULL);
    }
    else
    {
        SLEEPDLOG(LOG_INFO, "    No clients registered.");
    }
}

/**
* @brief Return tristate on apps readiness for shutdown.
*
* @retval 1 if ready, 0 if not ready, -1 if someone nacked.
*/
static int
shutdown_apps_ready()
{
    if (0 == sClientList->num_nack)
    {
        int num_clients = g_hash_table_size(sClientList->applications);
        return sClientList->num_ack >= num_clients;
    }
    else
    {
        return -1;
    }
}

/**
* @brief Return tristate on services readiness for shutdown.
*
* @retval 1 if ready, 0 if not ready, -1 if someone nacked.
*/
static int
shutdown_services_ready()
{
    if (0 == sClientList->num_nack)
    {
        int num_clients = g_hash_table_size(sClientList->services);
        return sClientList->num_ack >= num_clients;
    }
    else
    {
        return -1;
    }
}

/**
 * @brief The dispatcher for the next state in the shutdown process
 */

static void
shutdown_state_dispatch(ShutdownEvent *event)
{
    ShutdownState next_state = gCurrentState->state;
    bool running = true;

    while (running)
    {
        running = gCurrentState->function(event, &next_state);

        _assert(next_state >= gCurrentState->state);

        if (next_state != gCurrentState->state)
        {
            SLEEPDLOG(LOG_DEBUG, "Shutdown: entering state: %s @ %fs",
                    kStateMachine[next_state].name,
                    g_timer_elapsed(shutdown_timer, NULL));
        }

        gCurrentState = &kStateMachine[next_state];
    }
}

/**
 * @brief Broadcast the "shutdownApplications" signal
 */
static void
send_shutdown_apps()
{
    bool retVal;
    LSError lserror;
    LSErrorInit(&lserror);

    retVal = LSSignalSend(GetLunaServiceHandle(),
        "luna://com.palm.sleep/shutdown/shutdownApplications",
       "{}", &lserror);
    if (!retVal)
    {
        g_critical("%s Could not send shutdown applications", __FUNCTION__);
        LSErrorPrint(&lserror, stderr);
        LSErrorFree(&lserror);
    }
}

/**
 * @brief Broadcast the "shutdownServices" signal
 */

static void
send_shutdown_services()
{
    bool retVal;
    LSError lserror;
    LSErrorInit(&lserror);

    retVal = LSSignalSend(GetLunaServiceHandle(),
        "luna://com.palm.sleep/shutdown/shutdownServices",
        "{}", &lserror);
    if (!retVal)
    {
        g_critical("%s Could not send shutdown applications", __FUNCTION__);
        LSErrorPrint(&lserror, stderr);
        LSErrorFree(&lserror);
    }
}

/**
* @brief Unregister te application/service with the given ID.
* 		 Called by the cancel function set by LSSubscriptionSetCancelFunction.
*
* @param  clientId
*/
void
shutdown_client_cancel_registration(const char *clientId)
{
    client_unregister_application(clientId);
    client_unregister_service(clientId);
}


/**
* @brief Unregister te application/service with the given name.
*
* @param  clientName
*/
void
shutdown_client_cancel_registration_by_name(char * clientName)
{
    if (NULL == clientName)
        return;

    ShutdownClient *clientInfo=NULL;
    GHashTableIter iter;
    gpointer key, value;

    g_hash_table_iter_init (&iter, sClientList->applications);
    while (g_hash_table_iter_next (&iter, &key, &value))
	{
    	clientInfo=value;
    	if(!strcmp(clientInfo->name,clientName))
    	{
    		client_unregister_service(clientInfo->id);
    		break;
    	}
	}

    g_hash_table_iter_init (&iter, sClientList->services);
       while (g_hash_table_iter_next (&iter, &key, &value))
   	{
       	clientInfo=value;
       	if(!strcmp(clientInfo->name,clientName))
       	{
       		client_unregister_service(clientInfo->id);
       		return;
       	}
   	}
    return;
}

/**
 * @brief The Idle state : Sleepd will always be in this state until the "initiate" shutdown
 * message is received.
 */

static bool
state_idle(ShutdownEvent *event, ShutdownState *next)
{
    switch (event->id)
    {
    case kShutdownEventShutdownInit:
        client_list_vote_init();
        *next = kPowerShutdownApps;
        return true;
    default:
        return false;
    }
}

/**
 * @brief This is the first state that sleepd will go into once the shutdown process has begun.
 * In this state the "shutdownApplications" signal is sent to all the registered clients.
 */
static bool
state_shutdown_apps(ShutdownEvent *event, ShutdownState *next)
{
    client_list_reset_ack_count();

    event->id = kShutdownEventNone;
    *next = kPowerShutdownAppsProcess;

    shutdown_apps_timeout_id =
        g_timeout_add_seconds(15, (GSourceFunc)shutdown_timeout, NULL);

    send_shutdown_apps();

    return true;
}

/**
 * @brief This function is called when any of the clients haven't responded back even after 15 sec.
 */
static bool
shutdown_timeout(void *data)
{
    ShutdownEvent event;

    event.id = kShutdownEventTimeout;
    event.client = NULL;

    shutdown_state_dispatch(&event);

    return FALSE;
}

/**
 * @brief Check the client response for the "shutdownApplications" signal. We get to this state either when
 * a client ACKs or we have timed out.
 */
static bool
state_shutdown_apps_process(ShutdownEvent *event, ShutdownState *next)
{
    bool timeout = false;

    switch (event->id)
    {
    case kShutdownEventAck:
        client_vote(event->client, true);
        break;
    case kShutdownEventTimeout:
        timeout = true;
        break;
    default:
        break;
    }

    int readiness = shutdown_apps_ready();
    if (readiness > 0 || timeout)
    {
        if (timeout)
        {
            SLEEPDLOG(LOG_CRIT, "Shutdown apps timed out: ");
        }
        client_list_print(sClientList->applications);

        g_source_remove(shutdown_apps_timeout_id);

        *next = kPowerShutdownServices;
        return true;
    }
    else if (readiness < 0)
    {
        *next = kPowerShutdownNone;
        return false;
    }
    else
    {
        *next = kPowerShutdownAppsProcess;
        return false;
    }
}

/**
 * @brief This is the first state that sleepd will go into once the shutdown process has begun.
 * In this state the "shutdownServices" signal is sent to all the registered clients.
 */

static bool
state_shutdown_services(ShutdownEvent *event, ShutdownState *next)
{
    client_list_reset_ack_count();

    event->id = kShutdownEventNone;
    *next = kPowerShutdownServicesProcess;

    shutdown_apps_timeout_id =
        g_timeout_add_seconds(15, (GSourceFunc)shutdown_timeout, NULL);

    send_shutdown_services();

    return true;
}

/**
 * @brief Check the client response for the "shutdownServices" signal. We get to this state either when
 * a client ACKs or we have timed out.
 */
static bool
state_shutdown_services_process(ShutdownEvent *event, ShutdownState *next)
{
    bool timeout = false;

    switch (event->id)
    {
    case kShutdownEventAck:
        client_vote(event->client, true);
        break;
    case kShutdownEventTimeout:
        timeout = true;
        break;
    default:
        break;
    }

    int readiness = shutdown_services_ready();
    if (readiness > 0 || timeout)
    {
        if (timeout)
        {
            SLEEPDLOG(LOG_CRIT, "Shutdown services timed out: ");
        }
        client_list_print(sClientList->services);

        *next = kPowerShutdownAction;
        g_source_remove(shutdown_apps_timeout_id);
        return true;
    }
    else if (readiness < 0)
    {
        *next = kPowerShutdownNone;
        return false;
    }
    else
    {
        *next = kPowerShutdownServicesProcess;
        return false;
    }
}

/**
 * @brief This is the final state in the shutdown process when we notify the caller of the "initiate" method
 * that shutdown polling has been successfully completed.
 */

static bool
state_shutdown_action(ShutdownEvent *event, ShutdownState *next)
{
    bool retVal =
        LSMessageReply(GetLunaServiceHandle(), shutdown_message,
            "{\"success\":true}", NULL);
    if (!retVal)
    {
        g_critical("%s: Could not send shutdown success message",
                __FUNCTION__);
    }

    if (shutdown_message)
    {
        LSMessageUnref(shutdown_message);
        shutdown_message = NULL;
    }

    nyx_system_set_alarm(GetNyxSystemDevice(),0,NULL,NULL);

    return false;
}

/**
 * @brief Send response to the caller of the luna call.
 */

static void
send_reply(LSHandle *sh, LSMessage *message,
           const char *format, ...)
{
    bool retVal;
    char *payload;
    va_list vargs;

    va_start(vargs, format);
    payload = g_strdup_vprintf(format, vargs);
    va_end(vargs);

    retVal = LSMessageReply(sh, message, payload, NULL);
    if (!retVal)
    {
        g_critical("Could not send reply with payload %s",
            payload);
    }

    g_free(payload);
}


/**
 * @brief The callback function for "initiate" method. This will initiate the shutdown process.
 *
 * @param  sh
 * @param  message This method doesn't need any arguments.
 * @param  user_data
 */
static bool
initiateShutdown(LSHandle *sh, LSMessage *message, void *user_data)
{
    ShutdownEvent event;

    event.id = kShutdownEventShutdownInit;
    event.client = NULL;

    LSMessageRef(message);
    shutdown_message = message;

    g_timer_start(shutdown_timer);

    shutdown_state_dispatch(&event);
    return true;
}

/**
* @brief Called by test code to reset state machine to square 1.
*
* @param  sh
* @param  message
* @param  user_data
*
* @retval
*/
static bool
TESTresetShutdownState(LSHandle *sh, LSMessage *message, void *user_data)
{
    g_debug("Resetting shutdown state.");

    gCurrentState = &kStateMachine[kPowerShutdownNone];
    return true;
}

/**
 * @brief Callback function for "shutdownApplicationsAck" method. This will set the client's response
 * as ACK for the "shutdownApplications" signal and trigger the dispatcher for the shutdown state
 * machine, so that if the total client ACK count exceeds total number of clients, it can proceed
 * to the next state.
 *
 * @param sh
 * @param message with "clientId" string to retrieve client information
 * @param user_data
 */

static bool
shutdownApplicationsAck(LSHandle *sh, LSMessage *message,
                             void *user_data)
{
    struct json_object *object = json_tokener_parse(
                                    LSMessageGetPayload(message));
    if (is_error(object))
    {
        LSMessageReplyErrorBadJSON(sh, message);
        goto cleanup;
    }
    const char *clientId = json_object_get_string(
            json_object_object_get(object, "clientId"));
    if (!clientId)
    {
        LSMessageReplyErrorInvalidParams(sh, message);
        goto cleanup;
    }

    ShutdownEvent event;
    event.id = kShutdownEventAck;
    event.client = client_lookup_app(clientId);

    shutdown_state_dispatch(&event);

cleanup:
    if (!is_error(object)) json_object_put(object);
    return true;
}

/**
 * @brief Callback function for "shutdownServicesAck" method. This will set the client's response
 * as ACK for the "shutdownServices" signal and trigger the dispatcher for the shutdown state
 * machine, so that if the total client ACK count exceeds total number of clients, it can proceed
 * to the next state.
 *
 * @param sh
 * @param message with "clientId" string to retrieve client information
 * @param user_data
 */

static bool
shutdownServicesAck(LSHandle *sh, LSMessage *message,
                             void *user_data)
{
    struct json_object *object = json_tokener_parse(
                                    LSMessageGetPayload(message));
    if (is_error(object))
    {
        LSMessageReplyErrorBadJSON(sh, message);
        goto cleanup;
    }
    const char *clientId = json_object_get_string(
            json_object_object_get(object, "clientId"));
    if (!clientId)
    {
        LSMessageReplyErrorInvalidParams(sh, message);
        goto cleanup;
    }

    ShutdownEvent event;
    event.id = kShutdownEventAck;
    event.client = client_lookup_service(clientId);

    shutdown_state_dispatch(&event);

cleanup:
    if (!is_error(object)) json_object_put(object);
    return true;
}


/**
 * @brief Register an application for the "shutdownApplications" signal. Send the client id of the client
 * added.
 *
 * @param sh
 * @param message contains "clientName" for application's name.
 * @param user_data
 */

static bool
shutdownApplicationsRegister(LSHandle *sh, LSMessage *message,
                             void *user_data)
{
    struct json_object *object =
        json_tokener_parse(LSMessageGetPayload(message));
    if (is_error(object)) goto end;

    const char *clientId = LSMessageGetUniqueToken(message);
    const char *clientName = json_object_get_string(json_object_object_get(
        object, "clientName"));

    client_new_application(clientId, clientName);

    bool retVal;
    LSError lserror;
    LSErrorInit(&lserror);

    retVal = LSSubscriptionAdd(sh, "shutdownClient",
                               message, &lserror);
    if (!retVal)
    {
        g_critical("LSSubscriptionAdd failed.");
        LSErrorPrint(&lserror, stderr);
        LSErrorFree(&lserror);
    }

    send_reply(sh, message, "{\"clientId\":\"%s\"}", clientId);

    json_object_put(object);
end:
    return true;
}

/**
 * @brief Register an application for the "shutdownServices" signal. Send the client id of the client
 * added.
 *
 * @param sh
 * @param message contains "clientName" for service name.
 * @param user_data
 */


static bool
shutdownServicesRegister(LSHandle *sh, LSMessage *message,
                             void *user_data)
{
    struct json_object *object =
        json_tokener_parse(LSMessageGetPayload(message));
    if (is_error(object)) goto end;

    const char *clientId = LSMessageGetUniqueToken(message);
    const char *clientName = json_object_get_string(json_object_object_get(
        object, "clientName"));

    client_new_service(clientId, clientName);

    bool retVal;
    LSError lserror;
    LSErrorInit(&lserror);

    retVal = LSSubscriptionAdd(sh, "shutdownClient",
                             message, &lserror);
    if (!retVal)
    {
        g_critical("LSSubscriptionAdd failed.");
        LSErrorPrint(&lserror, stderr);
        LSErrorFree(&lserror);
    }

    send_reply(sh, message, "{\"clientId\":\"%s\"}", clientId);

    json_object_put(object);
end:
    return true;
}

/**
 * @brief Shutdown the machine forcefully
 *
 * @param sh
 * @param message with "reason" field for shutdown reason.
 * @param user_data
 */
static bool
machineOff(LSHandle *sh, LSMessage *message,
                             void *user_data)
{
    struct json_object *object = json_tokener_parse(
                                    LSMessageGetPayload(message));
    if (is_error(object))
    {
        LSMessageReplyErrorBadJSON(sh, message);
        goto cleanup;
    }
    const char *reason = json_object_get_string(
            json_object_object_get(object, "reason"));
    if (!reason)
    {
        LSMessageReplyErrorInvalidParams(sh, message);
        goto cleanup;
    }

    MachineForceShutdown(reason);
    LSMessageReplySuccess(sh, message);

cleanup:
    if (!is_error(object)) json_object_put(object);
    return true;
}

/**
 * @brief Reboot the machine forcefully by calling "reboot"
 *
 * @param sh
 * @param message with "reason" field for reboot reason.
 * @param user_data
 */

static bool
machineReboot(LSHandle *sh, LSMessage *message,
                             void *user_data)
{
    struct json_object *object = json_tokener_parse(
                                    LSMessageGetPayload(message));
    if (is_error(object))
    {
        LSMessageReplyErrorBadJSON(sh, message);
        goto cleanup;
    }
    const char *reason = json_object_get_string(
            json_object_object_get(object, "reason"));
    if (!reason)
    {
        LSMessageReplyErrorInvalidParams(sh, message);
        goto cleanup;
    }

    MachineForceReboot(reason);
    LSMessageReplySuccess(sh, message);

cleanup:
    if (!is_error(object)) json_object_put(object);
    return true;
}

LSMethod shutdown_methods[] = {
    { "initiate", initiateShutdown, },

    { "shutdownApplicationsRegister", shutdownApplicationsRegister },
    { "shutdownApplicationsAck", shutdownApplicationsAck },

    { "shutdownServicesRegister", shutdownServicesRegister },
    { "shutdownServicesAck", shutdownServicesAck },

    { "TESTresetShutdownState", TESTresetShutdownState },

    { "machineOff", machineOff },
    { "machineReboot", machineReboot },

    { },
};

LSSignal shutdown_signals[] = {
    { "shutdownApplications" },
    { "shutdownServices" },
    { },
};

static int
shutdown_init(void)
{
    sClientList = g_new0(ShutdownClientList, 1);
    sClientList->applications = g_hash_table_new_full(g_str_hash, g_str_equal,
                        NULL, (GDestroyNotify)client_free);
    sClientList->services = g_hash_table_new_full(g_str_hash, g_str_equal,
                        NULL, (GDestroyNotify)client_free);
    sClientList->num_ack = 0;
    sClientList->num_nack = 0;

    shutdown_timer = g_timer_new();

    gCurrentState = &kStateMachine[kPowerShutdownNone];

    LSError lserror;
    LSErrorInit(&lserror);

    if (!LSRegisterCategory(GetLunaServiceHandle(),
            "/shutdown", shutdown_methods, shutdown_signals, NULL, &lserror))
    {
        goto error;
    }

    return 0;

error:
    LSErrorPrint(&lserror, stderr);
    LSErrorFree(&lserror);
    return -1;
}

INIT_FUNC(INIT_FUNC_MIDDLE, shutdown_init);

/* @} END OF ShutdownProcess */
