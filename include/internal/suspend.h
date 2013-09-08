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


#ifndef _SUSPEND_H_
#define _SUSPEND_H_

#include <luna-service2/lunaservice.h>
/**
 * @brief If from batterycheck, the reason why we woke up.
 */
enum
{
    BATTERYCHECK_NONE = 0,
    BATTERYCHECK_THRESHOLD_CHANGED,
    BATTERYCHECK_CRITICAL_LOW_BATTERY,
    BATTERYCHECK_CRITICAL_TEMPERATURE,
    BATTERYCHECK_END,
};

enum
{
    kPowerEventNone,
    kPowerEventForceSuspend,
    kPowerEventIdleEvent,
};
typedef int PowerEvent;

void ScheduleIdleCheck(int interval_ms, bool fromPoll);
void TriggerSuspend(const char *cause, PowerEvent power_event);
bool GetSuspendSettings(LSHandle *sh, LSMessage *message, void *ctx);
bool DisplayStatus(LSHandle *sh, LSMessage *message, void *user_data);
int com_palm_suspend_lunabus_init(void);
void switchoffDisplay(void);

#endif
