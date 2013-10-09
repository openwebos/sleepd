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
 * @file suspend_ipc.c
 *
 * @brief Power Events Luna calls.
 *
 */

#include <syslog.h>
#include <cjson/json.h>
#include <luna-service2/lunaservice.h>

#include "wait.h"
#include "init.h"
#include "main.h"
#include "debug.h"
#include "client.h"
#include "shutdown.h"
#include "suspend.h"
#include "activity.h"
#include "logging.h"
#include "lunaservice_utils.h"
#include "config.h"

#define LOG_DOMAIN "PWREVENT-SUSPEND: "

extern WaitObj gWaitSuspendResponse;
extern WaitObj gWaitPrepareSuspend;

extern bool visual_leds_suspend;

/**
 * @defgroup SuspendIPC Luna methods & signals
 * @ingroup SuspendLogic
 * @brief  Various luna methods & signals to support suspend/resume logic in sleepd, like registering clients
 * for suspend request or prepare suspend signals, start or end activity.
 *
 */

/**
 * @addtogroup SuspendIPC
 * @{
 */

/**
 * @brief Unregister the client by its name. This is required for requests redirected from powerd, since the
 * unique token generated from message will be different for such requests.
 *
 * @param  sh
 * @param  message
 * @param  ctx
 */

bool
clientCancelByName(LSHandle *sh, LSMessage *message, void *ctx)
{
	struct json_object *object = json_tokener_parse(LSMessageGetPayload(message));

	if (is_error(object))
	{
		goto out;
	}

	char *clientName = json_object_get_string(
	                       json_object_object_get(object, "clientName"));

	PwrEventClientUnregisterByName(clientName);
	shutdown_client_cancel_registration_by_name(clientName);

out:

	if (!is_error(object))
	{
		json_object_put(object);
	}

	return true;
}

/**
 * @brief Unregister a client by its id generated from the message. This will work for direct requests.
 *
 * @param  sh
 * @param  message
 * @param  ctx
 */

bool
clientCancel(LSHandle *sh, LSMessage *msg, void *ctx)
{
	const char *clientId = LSMessageGetUniqueToken(msg);
	PwrEventClientUnregister(clientId);
	shutdown_client_cancel_registration(clientId);
	return true;
}

/**
 * @brief Start an activity with its "id" and "duration" passed in "message"
 *
 * @param  sh
 * @param  message
 * @param  user_data
 */

bool
activityStartCallback(LSHandle *sh, LSMessage *message, void *user_data)
{
	LSError lserror;
	LSErrorInit(&lserror);

	const char *payload = LSMessageGetPayload(message);

	struct json_object *object = json_tokener_parse(payload);

	if (is_error(object))
	{
		goto malformed_json;
	}

	const char *activity_id = json_object_get_string(
	                              json_object_object_get(object, "id"));

	int duration_ms = json_object_get_int(
	                      json_object_object_get(object, "duration_ms"));

	if (!activity_id || !strlen(activity_id) || duration_ms <= 0)
	{
		goto malformed_json;
	}

	bool ret = PwrEventActivityStart(activity_id, duration_ms);

	if (!ret)
	{
		LSError lserror;
		LSErrorInit(&lserror);

		bool retVal = LSMessageReply(sh, message,
		                             "{\"returnValue\":false, \"errorText\":\"Activities Frozen\"}", &lserror);

		if (!retVal)
		{
			LSErrorPrint(&lserror, stderr);
			LSErrorFree(&lserror);
		}
	}
	else
	{
		LSMessageReplySuccess(sh, message);
	}

	goto end;

malformed_json:
	LSMessageReplyErrorBadJSON(sh, message);
	goto end;
end:

	if (!is_error(object))
	{
		json_object_put(object);
	}

	return true;
}

/**
 * @brief End the activity with the "id" specified in "message"
 *
 * @param  sh
 * @param  message
 * @param  user_data
 */
bool
activityEndCallback(LSHandle *sh, LSMessage *message, void *user_data)
{
	LSError lserror;
	LSErrorInit(&lserror);

	const char *payload = LSMessageGetPayload(message);

	struct json_object *object = json_tokener_parse(payload);

	if (is_error(object))
	{
		goto malformed_json;
	}

	const char *activity_id = json_object_get_string(
	                              json_object_object_get(object, "id"));

	if (!activity_id || !strlen(activity_id))
	{
		goto malformed_json;
	}

	PwrEventActivityStop(activity_id);

	LSMessageReplySuccess(sh, message);
	goto end;

malformed_json:
	LSMessageReplyErrorBadJSON(sh, message);
	goto end;
end:

	if (!is_error(object))
	{
		json_object_put(object);
	}

	return true;
}

/**
 * @brief Register a new client with the given name.
 *
 * @param  sh
 * @param  message
 * @param  data
 */

bool
identifyCallback(LSHandle *sh, LSMessage *message, void *data)
{
	bool retVal;
	LSError lserror;
	LSErrorInit(&lserror);

	const char *payload = LSMessageGetPayload(message);

	struct json_object *object = json_tokener_parse(payload);

	if (is_error(object))
	{
		goto malformed_json;
	}

	const char *applicationName = LSMessageGetApplicationID(message);
	const char *clientId = LSMessageGetUniqueToken(message);

	bool subscribe = json_object_get_boolean(
	                     json_object_object_get(object, "subscribe"));

	const char *clientName = json_object_get_string(
	                             json_object_object_get(object, "clientName"));

	if (!subscribe || !clientName)
	{
		goto invalid_syntax;
	}

	if (!LSSubscriptionAdd(sh, "PwrEventsClients", message, &lserror))
	{
		goto lserror;
	}

	if (!PwrEventClientRegister(clientId))
	{
		goto error;
	}

	struct PwrEventClientInfo *info = PwrEventClientLookup(clientId);

	if (!info)
	{
		goto error;
	}

	info->clientName = g_strdup(clientName);
	info->clientId = g_strdup(clientId);
	info->applicationName = g_strdup(applicationName);

	char *reply = g_strdup_printf(
	                  "{\"subscribed\":true,\"clientId\":\"%s\"}", clientId);

	SLEEPDLOG_DEBUG("Pwrevents received identify, reply with %s", reply);

	retVal = LSMessageReply(sh, message, reply, &lserror);

	g_free(reply);

	if (!retVal)
	{
		goto lserror;
	}

	goto end;

lserror:
	LSErrorPrint(&lserror, stderr);
	LSErrorFree(&lserror);
	goto end;
error:
	LSMessageReplyErrorUnknown(sh, message);
	goto end;
invalid_syntax:
	LSMessageReplyErrorInvalidParams(sh, message);
	goto end;
malformed_json:
	LSMessageReplyErrorBadJSON(sh, message);
	goto end;
end:

	if (!is_error(object))
	{
		json_object_put(object);
	}

	return true;
}

/**
 * @brief Force the device to go into suspend even with charger connected or any activity is still active.
 * (Used for testing purposes).
 *
 * @param  sh
 * @param  message
 * @param  user_data
 */

bool
forceSuspendCallback(LSHandle *sh, LSMessage *message, void *user_data)
{
	PMLOG_TRACE("Received force suspend");
	TriggerSuspend("forced suspend", kPowerEventForceSuspend);

	LSMessageReplySuccess(sh, message);

	return true;
}

/**
 * @brief Schedule the IdleCheck thread to check if the device can suspend
 * (Used for testing purposes).
 *
 * @param  sh
 * @param  message
 * @param  user_data
 */
bool
TESTSuspendCallback(LSHandle *sh, LSMessage *message, void *user_data)
{
	PMLOG_TRACE("Received TESTSuspend");
	ScheduleIdleCheck(100, false);
	LSMessageReplySuccess(sh, message);
	return true;
}

/**
 * @brief Broadcast the suspend request signal to all registered clients.
 */

int
SendSuspendRequest(const char *message)
{
	bool retVal;
	LSError lserror;
	LSErrorInit(&lserror);

	retVal = LSSignalSend(GetLunaServiceHandle(),
	                      "luna://com.palm.sleep/com/palm/power/suspendRequest",
	                      "{}", &lserror);

	if (!retVal)
	{
		LSErrorPrint(&lserror, stderr);
		LSErrorFree(&lserror);
	}

	return retVal;
}

/**
 * @brief Broadcast the prepare suspend signal to all registered clients.
 */

int
SendPrepareSuspend(const char *message)
{
	bool retVal;
	LSError lserror;
	LSErrorInit(&lserror);

	retVal = LSSignalSend(GetLunaServiceHandle(),
	                      "luna://com.palm.sleep/com/palm/power/prepareSuspend",
	                      "{}", &lserror);

	if (!retVal)
	{
		LSErrorPrint(&lserror, stderr);
		LSErrorFree(&lserror);
	}

	return retVal;
}

/**
 * @brief Broadcast the "resume" signal when the device wakes up from sleep, or the
 * suspend action is aborted on the system.
 */

int
SendResume(int resumetype, char *message)
{
	bool retVal;
	LSError lserror;
	LSErrorInit(&lserror);

	SLEEPDLOG_DEBUG("sending \"resume\" because %s", message);

	char *payload = g_strdup_printf(
	                    "{\"resumetype\":%d}", resumetype);

	retVal = LSSignalSend(GetLunaServiceHandle(),
	                      "luna://com.palm.sleep/com/palm/power/resume",
	                      payload, &lserror);

	if (!retVal)
	{
		LSErrorPrint(&lserror, stderr);
		LSErrorFree(&lserror);
	}

	g_free(payload);
	return retVal;
}


/**
 * @brief Broadcast the "suspended" signal when the system is just about to go to sleep.
 */
int
SendSuspended(const char *message)
{
	bool retVal;
	LSError lserror;
	LSErrorInit(&lserror);

	SLEEPDLOG_DEBUG("sending \"suspended\" because %s", message);

	retVal = LSSignalSend(GetLunaServiceHandle(),
	                      "luna://com.palm.sleep/com/palm/power/suspended",
	                      "{}", &lserror);

	if (!retVal)
	{
		LSErrorPrint(&lserror, stderr);
		LSErrorFree(&lserror);
	}

	return retVal;
}

/**
 * @brief Register a client (already registered with "identify" call) for "suspend request" signal.
 * This will add to the counter "sNumSuspendRequest" before every polling to make a decision to proceed
 * with the suspend action or postpone it later.
 *
 * @param  sh
 * @param  message
 * @param  data
 */

bool
suspendRequestRegister(LSHandle *sh, LSMessage *message, void *data)
{
	bool reg;


	struct json_object *object = json_tokener_parse(
	                                 LSMessageGetPayload(message));

	if (is_error(object))
	{
		goto malformed_json;
	}

	const char *clientId = json_object_get_string(
	                           json_object_object_get(object, "clientId"));

	if (!clientId)
	{
		goto invalid_syntax;
	}

	struct json_object *json_reg =
	    json_object_object_get(object, "register");

	if (!json_reg)
	{
		goto invalid_syntax;
	}

	reg = json_object_get_boolean(json_reg);

    SLEEPDLOG_DEBUG("RequestRegister - PwrEvent received from %s", clientId );

	PwrEventClientSuspendRequestRegister(clientId, reg);

	goto end;

invalid_syntax:
	LSMessageReplyErrorInvalidParams(sh, message);
	goto end;
malformed_json:
	LSMessageReplyErrorBadJSON(sh, message);
	goto end;
end:

	if (!is_error(object))
	{
		json_object_put(object);
	}

	return true;
}

/**
 * @brief Add the client's count in the total number of ACKs received for the "suspend request" signal.
 *
 * @param  sh
 * @param  message
 * @param  data
 */
bool
suspendRequestAck(LSHandle *sh, LSMessage *message, void *data)
{
	bool ack;


	struct json_object *object = json_tokener_parse(
	                                 LSMessageGetPayload(message));

	if (is_error(object))
	{
		goto malformed_json;
	}

	const char *clientId = json_object_get_string(
	                           json_object_object_get(object, "clientId"));

	struct json_object *json_ack =
	    json_object_object_get(object, "ack");

	if (!json_ack)
	{
		goto invalid_syntax;
	}

	ack = json_object_get_boolean(json_ack);

	struct PwrEventClientInfo *clientInfo = PwrEventClientLookup(clientId);
#if 0

	if (gPowerConfig.debug)
	{
		SLEEPDLOG(LOG_WARNING,
		          "PWREVENT-SUSPEND_REQUEST_%s from \"%s\" (%s)",
		          ack ? "ACK" : "NACK",
		          clientInfo && clientInfo->clientName ? clientInfo->clientName : "",
		          clientId);
	}

#endif

	if (!ack)
	{
		PwrEventClientSuspendRequestNACKIncr(clientInfo);
	}

	// returns true when all clients have acked.
	if (PwrEventVoteSuspendRequest(clientId, ack))
	{
		WaitObjectSignal(&gWaitSuspendResponse);
	}

	goto end;

malformed_json:
	LSMessageReplyErrorBadJSON(sh, message);
	goto end;
invalid_syntax:
	LSMessageReplyErrorInvalidParams(sh, message);
	goto end;
end:

	if (!is_error(object))
	{
		json_object_put(object);
	}

	return true;
}


/**
 * @brief Register a client (already registered with "identify" call) for "prepare suspend" signal.
 * This will add to the counter "sNumPrepareSuspend" before every polling to make a decision to proceed
 * with the suspend action or postpone it later.
 *
 * @param  sh
 * @param  message
 * @param  data
 */

bool
prepareSuspendRegister(LSHandle *sh, LSMessage *message, void *data)
{
	bool reg;

	struct json_object *object = json_tokener_parse(
	                                 LSMessageGetPayload(message));

	if (is_error(object))
	{
		goto malformed_json;
	}

	const char *clientId = json_object_get_string(
	                           json_object_object_get(object, "clientId"));

	if (!clientId)
	{
		goto invalid_syntax;
	}

	struct json_object *json_reg =
	    json_object_object_get(object, "register");

	if (!json_reg)
	{
		goto invalid_syntax;
	}

	reg = json_object_get_boolean(json_reg);

	SLEEPDLOG_DEBUG("SuspendRegister - PwrEvent : reg=%d from %s", reg, clientId);

	PwrEventClientPrepareSuspendRegister(clientId, reg);

	goto end;

invalid_syntax:
	LSMessageReplyErrorInvalidParams(sh, message);
	goto end;
malformed_json:
	LSMessageReplyErrorBadJSON(sh, message);
	goto end;
end:

	if (!is_error(object))
	{
		json_object_put(object);
	}

	return true;
}

/**
 * @brief Add the client's count in the total number of ACKs received for the "suspend request" signal.
 *
 * @param  sh
 * @param  message
 * @param  data
 */

bool
prepareSuspendAck(LSHandle *sh, LSMessage *message, void *data)
{
	bool ack;

	struct json_object *object = json_tokener_parse(
	                                 LSMessageGetPayload(message));

	if (is_error(object))
	{
		goto malformed_json;
	}

	const char *clientId = json_object_get_string(
	                           json_object_object_get(object, "clientId"));

	struct json_object *json_ack =
	    json_object_object_get(object, "ack");

	if (!json_ack)
	{
		goto invalid_syntax;
	}

	ack = json_object_get_boolean(json_ack);

	struct PwrEventClientInfo *clientInfo = PwrEventClientLookup(clientId);
#if 0

	if (gPowerConfig.debug)
	{
		SLEEPDLOG(LOG_WARNING,
		          "PWREVENT-PREPARE_SUSPEND_%s from \"%s\" (%s)",
		          ack ? "ACK" : "NACK",
		          clientInfo && clientInfo->clientName ? clientInfo->clientName : "",
		          clientId);
	}

#endif

	if (!ack)
	{
		PwrEventClientPrepareSuspendNACKIncr(clientInfo);
	}

	// returns true when all clients have acked.
	if (PwrEventVotePrepareSuspend(clientId, ack))
	{
		WaitObjectSignal(&gWaitPrepareSuspend);
	}

	goto end;
invalid_syntax:
	LSMessageReplyErrorInvalidParams(sh, message);
	goto end;
malformed_json:
	LSMessageReplyErrorBadJSON(sh, message);
	goto end;
end:

	if (!is_error(object))
	{
		json_object_put(object);
	}

	return true;
}

/**
* @brief Turn on/off visual leds suspend via luna-service.
*
* @param  sh
* @param  message
* @param  data
*
* @retval
*/
bool
visualLedSuspendCallback(LSHandle *sh, LSMessage *message, void *data)
{
	struct json_object *object = json_tokener_parse(
	                                 LSMessageGetPayload(message));

	if (is_error(object))
	{
		goto malformed_json;
	}

	struct json_object *json_on = json_object_object_get(object, "on");

	if (!json_on)
	{
		goto invalid_syntax;
	}

	bool on = json_object_get_boolean(json_on);
	gSleepConfig.visual_leds_suspend = on;

	goto end;
invalid_syntax:
	LSMessageReplyErrorInvalidParams(sh, message);
	goto end;
malformed_json:
	LSMessageReplyErrorBadJSON(sh, message);
	goto end;
end:

	if (!is_error(object))
	{
		json_object_put(object);
	}

	return true;
}

void
SuspendIPCInit(void)
{
	bool retVal;
	LSError lserror;
	LSErrorInit(&lserror);

	retVal = LSSubscriptionSetCancelFunction(GetLunaServiceHandle(),
	         clientCancel, NULL, &lserror);

	if (!retVal)
	{
		SLEEPDLOG_WARNING(MSGID_LS_SUBSCRIB_SETFUN_FAIL, 0, "Error in setting cancel function");
		goto ls_error;
	}

ls_error:
	LSErrorPrint(&lserror, stderr);
	LSErrorFree(&lserror);

}

LSMethod com_palm_suspend_methods[] =
{

	/* suspend methods*/

	{ "suspendRequestRegister", suspendRequestRegister },
	{ "prepareSuspendRegister", prepareSuspendRegister },
	{ "suspendRequestAck", suspendRequestAck },
	{ "prepareSuspendAck", prepareSuspendAck },
	{ "forceSuspend", forceSuspendCallback },
	{ "identify", identifyCallback },
	{ "clientCancelByName", clientCancelByName },

	{ "visualLedSuspend", visualLedSuspendCallback },
	{ "TESTSuspend", TESTSuspendCallback },

	{ },
};

LSMethod com_palm_suspend_public_methods[] =
{
	{ "activityStart", activityStartCallback },
	{ "activityEnd", activityEndCallback },
};

LSSignal com_palm_suspend_signals[] =
{

	/* Suspend signals */

	{ "suspendRequest" },
	{ "prepareSuspend" },
	{ "suspended" },
	{ "resume" },

	{ },
};

int com_palm_suspend_lunabus_init(void)
{
	LSError lserror;
	LSErrorInit(&lserror);

	if (!LSPalmServiceRegisterCategory(GetPalmService(), "/com/palm/power",
	                                   com_palm_suspend_public_methods, com_palm_suspend_methods,
	                                   com_palm_suspend_signals,
	                                   NULL, &lserror))
	{
		goto error;
	}

	return 0;

error:
	LSErrorPrint(&lserror, stderr);
	LSErrorFree(&lserror);
	return -1;
}

INIT_FUNC(INIT_FUNC_END, com_palm_suspend_lunabus_init);

/* @} END OF SuspendIPC */
