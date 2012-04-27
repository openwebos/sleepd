/* @@@LICENSE
*
*      Copyright (c) 2011-2012 Hewlett-Packard Development Company, L.P.
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


#ifndef _MACHINE_H_
#define _MACHINE_H_

#include <stdbool.h>


bool MachineCanSleep(void);

const char * MachineCantSleepReason(void);

void MachineSleep(void);

void MachineForceReboot(const char *reason);

void MachineForceShutdown(const char *reason);

void TurnBypassOn(void);

void TurnBypassOff(void);

int MachineGetToken(const char *token_name, char *buf, int len);

#endif // _MACHINE_H_
