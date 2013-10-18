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
* @file client.c
*
* @brief This file contains functions to manage clients registered with sleepd to
* veto suspend requests. All the added clients account for the decision by the system to
* suspend or not to suspend. If any client NACKS the suspend request, sleepd will not suspend
* the system.
*
*/

#include <glib.h>
#include <stdlib.h>

#include "logging.h"
#include "debug.h"
#include "client.h"

#define LOG_DOMAIN "PWREVENT-CLIENT: "

/**
 * @defgroup SuspendClient  Suspend Clients
 * @ingroup PowerEvents
 * @brief Clients registering for device suspend polling
 */

/**
 * @addtogroup SuspendClient
 * @{
 */


/**
 * @brief Global hash table for managing all clients registering to participate in polling
 * for device suspend decision.
 */
static GHashTable    *sClientList = NULL;

static int sNumSuspendRequest = 0;
static int sNumSuspendRequestAck = 0;
static int sNumPrepareSuspend  = 0;
static int sNumPrepareSuspendAck  = 0;

static int sNumNACK = 0;


/**
 * @brief Increment the client's total suspend request NACK response as well as total NACK responses for the
 * current polling for suspend request.
 *
 * @param info The client which responded with NACK for suspend request
 */

void
PwrEventClientSuspendRequestNACKIncr(struct PwrEventClientInfo *info)
{
	if (info)
	{
		info->num_NACK_suspendRequest++;
		sNumNACK++;
	}
}

/**
 * @brief Increment the client's total prepare suspend NACK response as well as total NACK responses for the
 * current polling for prepare suspend.
 *
 * @param info The client which responded with NACK for prepare suspend
 */
void
PwrEventClientPrepareSuspendNACKIncr(struct PwrEventClientInfo *info)
{
	if (info)
	{
		info->num_NACK_prepareSuspend++;
		sNumNACK++;
	}
}

/**
 * @brief Add a new client
 *
 * @retval The new added client
 */

static struct PwrEventClientInfo *
PwrEventClientInfoCreate(void)
{
	struct PwrEventClientInfo *ret_client;
	ret_client = malloc(sizeof(struct PwrEventClientInfo));

	if (!ret_client)
	{
		return NULL;
	}

	ret_client->clientName = NULL;
	ret_client->clientId = NULL;
	ret_client->requireSuspendRequest = false;
	ret_client->requirePrepareSuspend = false;

	ret_client->num_NACK_suspendRequest = 0;
	ret_client->num_NACK_prepareSuspend = 0;

	return ret_client;
}


/**
 * @brief Free a client
 *
 * @param client
 */

static void
PwrEventClientInfoDestroy(struct PwrEventClientInfo *client)
{
	if (!client)
	{
		return;
	}

	g_free(client->clientName);
	g_free(client->clientId);
	g_free(client->applicationName);

	free(client);
}

/**
 * @brief Register a new client with sleepd
 *
 * @param uid (char *) ID of the client thats registering
 */

bool
PwrEventClientRegister(ClientUID uid)
{
	if (PwrEventClientLookup(uid))
	{
		PwrEventClientUnregister(uid);
	}

	struct PwrEventClientInfo *clientInfo = PwrEventClientInfoCreate();

	if (!clientInfo)
	{
		return false;
	}

	PMLOG_TRACE("Registering client %s", uid);
	g_hash_table_replace(sClientList, g_strdup(uid), clientInfo);
	return true;
}

/**
 * @brief Unregister a client
 *
 * @param uid (char *) ID of the client to be unregistered
 */

bool
PwrEventClientUnregister(ClientUID uid)
{
	g_hash_table_remove(sClientList, uid);

	return true;
}

/**
 * @brief Function to free the memory allocated for the value used when removing the entry from the
 * GHashTable "sClientList".
 */
static void
ClientTableValueDestroy(gpointer value)
{
	struct PwrEventClientInfo *info = (struct PwrEventClientInfo *)value;

	PwrEventClientInfoDestroy(info);
}

/**
 * @brief Get the pointer to "sClientList" hash table.
 */
GHashTable *
PwrEventClientGetTable(void)
{
	return sClientList;
}

/**
 * @brief Create the new hash table "sClientList".
 */
void
PwrEventClientTableCreate(void)
{
	sClientList = g_hash_table_new_full(g_str_hash, g_str_equal,
	                                    g_free, ClientTableValueDestroy);
}

/**
 * @brief Destroy the "sClientList" hash table
 */

void
PwrEventClientTableDestroy(void)
{
	g_hash_table_remove_all(sClientList);
	g_hash_table_destroy(sClientList);
}

/**
 * @brief Retrieve the client information from its id
 *
 * @param uid
 *
 * @retval PwrEventClientInfo
 */

struct PwrEventClientInfo *
PwrEventClientLookup(ClientUID uid)
{
	struct PwrEventClientInfo *clientInfo = NULL;

	if (uid)
		clientInfo = (struct PwrEventClientInfo *)
		             g_hash_table_lookup(sClientList, uid);

	return clientInfo;
}

/**
 * Unregister a client by its name
 *
 * @param Client Name
 *
 * @retval TRUE if client with the given name found and unregistered
 */

bool PwrEventClientUnregisterByName(char *clientName)
{
	if (NULL == clientName)
	{
		return NULL;
	}

	struct PwrEventClientInfo *clientInfo = NULL;

	GHashTableIter iter;

	gpointer key, value;

	g_hash_table_iter_init(&iter, sClientList);

	while (g_hash_table_iter_next(&iter, &key, &value))
	{
		clientInfo = value;

		if (!strcmp(clientInfo->clientName, clientName))
		{
			PwrEventClientUnregister(clientInfo->clientId);
			return true;
		}
	}

	return false;
}

/**
 * @brief Map client response in int to string
 */
static const char *
AckToString(int ack)
{
	const char *ret;

	switch (ack)
	{
		case PWREVENT_CLIENT_ACK:
			ret = "ACK";
			break;

		case PWREVENT_CLIENT_NACK:
			ret = "NACK";
			break;

		case PWREVENT_CLIENT_NORSP:
			ret = "NORSP";
			break;

		default:
			ret = "Unknown";
	}

	return ret;
}

/**
 * @brief Helper function for printing each of the client information
 */
static void
get_client_table_str_helper(gpointer key, gpointer value, gpointer data)
{
	GString *str = (GString *) data;
	g_return_if_fail(str != NULL);

	struct PwrEventClientInfo *info =
	    (struct PwrEventClientInfo *)value;
	g_return_if_fail(info != NULL);

	g_string_append_printf(str, "    %s/%s - %s (%s) - NACKS: %d/%d\n",
	                       info->requireSuspendRequest ?
	                       AckToString(info->ackSuspendRequest) : "###",
	                       info->requirePrepareSuspend ?
	                       AckToString(info->ackPrepareSuspend) : "###",
	                       info->clientName,
	                       info->clientId,
	                       info->num_NACK_suspendRequest,
	                       info->num_NACK_prepareSuspend);
}


/**
 * @brief Go through each client in the hash table and get their details
 */

gchar *
PwrEventGetClientTable()
{
	GString *ret = g_string_sized_new(32);
	g_hash_table_foreach(sClientList, get_client_table_str_helper, ret);
	return g_string_free(ret, false);
}

/**
 * @brief Helper function for adding all clients who did not respond to suspend request message
 * to the string pointer passed.
 */
static void
get_SuspendRequest_NORSP_list_helper(gpointer key, gpointer value,
                                     gpointer data)
{
	GString *str = (GString *) data;
	g_return_if_fail(str != NULL);

	struct PwrEventClientInfo *info =
	    (struct PwrEventClientInfo *)value;
	g_return_if_fail(info != NULL);

	if ((info->requireSuspendRequest) &&
	        (info->ackSuspendRequest == PWREVENT_CLIENT_NORSP))
	{
		g_string_append_printf(str, "%s%s(%s)",
		                       str->len > 0 ? ", " : "",
		                       info->clientName,
		                       info->clientId);
	}
}

/**
 * @brief Go through each client in the hash table and print their details if they have not responded
 * back to suspend request message.
 */

gchar *
PwrEventGetSuspendRequestNORSPList()
{
	GString *ret = g_string_sized_new(32);
	g_hash_table_foreach(sClientList, get_SuspendRequest_NORSP_list_helper, ret);
	return g_string_free(ret, false);
}

/**
 * @brief Helper function for printing all clients who did not respond to prepare suspend message
 */

static void
get_PrepareSuspend_NORSP_list_helper(gpointer key, gpointer value,
                                     gpointer data)
{
	GString *str = (GString *) data;
	g_return_if_fail(str != NULL);

	struct PwrEventClientInfo *info =
	    (struct PwrEventClientInfo *)value;
	g_return_if_fail(info != NULL);

	if ((info->requirePrepareSuspend) &&
	        (info->ackPrepareSuspend == PWREVENT_CLIENT_NORSP))
	{
		g_string_append_printf(str, "%s%s(%s)",
		                       str->len > 0 ? ", " : "",
		                       info->clientName,
		                       info->clientId);
	}
}

/**
 * @brief Go through each client in the hash table and print their details if they have not responded
 * back to prepare suspend message.
 */
gchar *
PwrEventGetPrepareSuspendNORSPList()
{
	GString *ret = g_string_sized_new(32);
	g_hash_table_foreach(sClientList, get_PrepareSuspend_NORSP_list_helper, ret);
	return g_string_free(ret, false);
}


/**
 * @brief Helper function for printing information about all clients registered.
 */
void
PwrEventClientTablePrintHelper(gpointer key, gpointer value, gpointer data)
{
	GLogLevelFlags lvl = GPOINTER_TO_UINT(data);
	struct PwrEventClientInfo *info =
	    (struct PwrEventClientInfo *)value;

	if (!info)
	{
		return;
	}

	SLEEPDLOG_DEBUG(" %s/%s - %s (%s) - NACKS: %d/%d\n",
	                info->requireSuspendRequest ?
	                AckToString(info->ackSuspendRequest) : "###",
	                info->requirePrepareSuspend ?
	                AckToString(info->ackPrepareSuspend) : "###",
	                info->clientName,
	                info->clientId,
	                info->num_NACK_suspendRequest,
	                info->num_NACK_prepareSuspend);
}

/**
 * @brief Go through each client in the hash table and print their details
 */
void
PwrEventClientTablePrint(GLogLevelFlags lvl)
{
	SLEEPDLOG_DEBUG("PwrEvent clients:");
	g_hash_table_foreach(sClientList, PwrEventClientTablePrintHelper,
	                     GUINT_TO_POINTER(lvl));
}


/**
 * @brief If the client's total NACK count is greater than 0, then log it.
 */
void
_PwrEventClientPrintNACKHelper(gpointer key, gpointer value, gpointer ctx)
{
	struct PwrEventClientInfo *info =
	    (struct PwrEventClientInfo *)value;

	if (!info)
	{
		return;
	}

	int num_nacks = info->num_NACK_suspendRequest + info->num_NACK_prepareSuspend;

	if (num_nacks > 0)
	{
		SLEEPDLOG_DEBUG(" %s (%s) NACKs: %d", info->clientName, info->clientId,
		                num_nacks);
	}
}

/**
 * Log the details of each new client who NACK'ed either the suspend request of prepare suspend message
 */

void
PwrEventClientPrintNACKRateLimited(void)
{
	static int num_NACK = 0;

	if (sNumNACK > num_NACK)
	{
		num_NACK = sNumNACK;
		g_hash_table_foreach(sClientList, _PwrEventClientPrintNACKHelper, NULL);
	}
}

/**
 * @brief Register/Unregister the client with the given uid for suspend request message
 *
 * @param uid of the client
 * @param reg TRUE for registering and FALSE for unregistering
 *
 */
void
PwrEventClientSuspendRequestRegister(ClientUID uid, bool reg)
{
	struct PwrEventClientInfo *info = PwrEventClientLookup(uid);

	if (!info)
	{
		PMLOG_TRACE("SuspendRequestRegister : could not find uid %s", uid);
		return;
	}

	if (info->requireSuspendRequest != reg)
	{
		info->requireSuspendRequest = reg;
		sNumSuspendRequest += reg ? 1 : -1;
	}

	SLEEPDLOG_DEBUG("%s %sregistering for suspend_request", info->clientName,
	                reg ? "" : "de-");
}

/**
 * @brief Register/Unregister the client with the given uid for prepare suspend message
 *
 * @param uid of the client
 * @param reg TRUE for registering and FALSE for unregistering
 *
 */

void
PwrEventClientPrepareSuspendRegister(ClientUID uid, bool reg)
{
	struct PwrEventClientInfo *info = PwrEventClientLookup(uid);

	if (!info)
	{
		PMLOG_TRACE("PrepareSuspendRegister: could not find uid %s", uid);
		return;
	}

	if (info->requirePrepareSuspend != reg)
	{
		info->requirePrepareSuspend = reg;
		sNumPrepareSuspend += reg ? 1 : -1;
	}

	SLEEPDLOG_DEBUG("%s %sregistering for prepare_suspend", info->clientName,
	                reg ? "" : "de-");
}


/**
 * Helper function for initializing all counts before device suspend polling.
 * The counts sNumSuspendRequest and sNumPrepareSuspend keep a track of the
 * total expected responses for the suspend request and prepare suspend rounds
 * respectively.
 */
void
PwrEventVoteInitHelper(gpointer key, gpointer value, gpointer ctx)
{
	struct PwrEventClientInfo *info = (struct PwrEventClientInfo *)value;

	if (!info)
	{
		return;
	}

	info->ackSuspendRequest = PWREVENT_CLIENT_NORSP;
	info->ackPrepareSuspend = PWREVENT_CLIENT_NORSP;

	if (info->requireSuspendRequest)
	{
		sNumSuspendRequest++;
	}

	if (info->requirePrepareSuspend)
	{
		sNumPrepareSuspend++;
	}
}

/**
 * @brief Intialize all counts before the first polling round i.e the suspend request polling.
 */
void
PwrEventVoteInit(void)
{
	sNumSuspendRequestAck = 0;
	sNumSuspendRequest    = 0;

	sNumPrepareSuspendAck = 0;
	sNumPrepareSuspend    = 0;

	g_hash_table_foreach(sClientList, PwrEventVoteInitHelper, NULL);
}

/**
 * @brief Updates the response for the client with given id for the suspend request polling.
 *
 * @param uid
 * @param ack TRUE if an ack, FALSE if NACK.
 *
 * @retval true when all clients have acked.
 */
bool
PwrEventVoteSuspendRequest(ClientUID uid, bool ack)
{
	struct PwrEventClientInfo *info = PwrEventClientLookup(uid);

	if (!info)
	{
		PMLOG_TRACE("VoteSuspendRequest : could not find uid %s", uid);
		return false;
	}

	if (!ack)
	{
		SLEEPDLOG_DEBUG("%s(%s) SuspendRequestNACK.", info->clientName, info->clientId);
	}

	PMLOG_TRACE("%s %sACK suspend response", info->clientName, ack ? "" : "N");

	if (info->ackSuspendRequest != ack)
	{
		info->ackSuspendRequest = ack;
		sNumSuspendRequestAck += ack ? 1 : 0;
	}

	return (!ack || PwrEventClientsApproveSuspendRequest());
}


/**
 * @brief Updates the response for the client with given id for the prepare suspend polling.
 *
 * @param uid
 * @param ack TRUE if an ack, FALSE if NACK.
 *
 * @retval true when all clients have acked.
 */
bool
PwrEventVotePrepareSuspend(ClientUID uid, bool ack)
{
	struct PwrEventClientInfo *info = PwrEventClientLookup(uid);

	if (!info)
	{
		PMLOG_TRACE("VotePrepareSuspend : could not find uid %s", uid);
		return false;
	}

	if (!ack)
	{
		SLEEPDLOG_DEBUG("%s(%s) PrepareSuspendNACK", info->clientName, info->clientId);
	}

	PMLOG_TRACE("%s %sACK prepare suspend", info->clientName, ack ? "" : "N");

	if (info->ackPrepareSuspend != ack)
	{
		info->ackPrepareSuspend = ack;
		sNumPrepareSuspendAck += ack ? 1 : 0;
	}

	return (!ack || PwrEventClientsApprovePrepareSuspend());
}

/**
 * @brief Returns TRUE if the total number of received ACKs for suspend request round is greater
 * than the expected count.
 */
bool
PwrEventClientsApproveSuspendRequest(void)
{
	return sNumSuspendRequestAck >= sNumSuspendRequest;
}

/**
 * @brief Returns TRUE if the total number of received ACKs for prepare suspend round is greater
 * than the expected count.
 */
bool
PwrEventClientsApprovePrepareSuspend(void)
{
	return sNumPrepareSuspendAck >= sNumPrepareSuspend;
}

/* @} END OF SuspendClient */
