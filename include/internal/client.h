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


#ifndef _PWREVENTS_CLIENT_H_
#define _PWREVENTS_CLIENT_H_

#include <stdbool.h>
#include <glib.h>

struct PwrEventClientInfo
{
	char *clientName;
	char *clientId;
	char *applicationName;

	bool requireSuspendRequest;
	bool requirePrepareSuspend;

	int ackSuspendRequest;
	int ackPrepareSuspend;

	int num_NACK_suspendRequest;
	int num_NACK_prepareSuspend;
};

#define PWREVENT_CLIENT_ACK   1
#define PWREVENT_CLIENT_NACK  0
#define PWREVENT_CLIENT_NORSP -1

// hash is client_id -> PwrEventClientInfo
GHashTable *PwrEventClientGetTable(void);

typedef const char *ClientUID;

struct PwrEventClientInfo *PwrEventClientLookup(ClientUID uid);

bool PwrEventClientRegister(ClientUID uid);

bool PwrEventClientUnregister(ClientUID uid);

void PwrEventClientTableCreate(void);
void PwrEventClientTableDestroy(void);

void PwrEventClientTablePrint(GLogLevelFlags lvl);
gchar *PwrEventGetClientTable();
gchar *PwrEventGetSuspendRequestNORSPList();
gchar *PwrEventGetPrepareSuspendNORSPList();

void PwrEventClientSuspendRequestNACKIncr(struct PwrEventClientInfo *info);
void PwrEventClientPrepareSuspendNACKIncr(struct PwrEventClientInfo *info);
void PwrEventClientPrintNACKRateLimited(void);

void PwrEventClientSuspendRequestRegister(ClientUID uid, bool reg);
void PwrEventClientPrepareSuspendRegister(ClientUID uid, bool reg);

void PwrEventVoteInit(void);

/* @returns true when all clients have acked, or if 1 nacks. */
bool PwrEventVoteSuspendRequest(ClientUID uid, bool ack);
bool PwrEventVotePrepareSuspend(ClientUID uid, bool ack);

bool PwrEventClientsApproveSuspendRequest(void);
bool PwrEventClientsApprovePrepareSuspend(void);

bool PwrEventClientUnregisterByName(char *clientName);

#endif // _PWREVENTS_CLIENT_H_
