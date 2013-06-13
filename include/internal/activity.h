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


#ifndef _ACTIVITY_H_
#define _ACTIVITY_H_

bool PwrEventActivityStart(const char *activity_id, int duration_ms);
void PwrEventActivityStop(const char *activity_id);

void PwrEventActivityPrint(void);

void PwrEventActivityPrintFrom(struct timespec *start);

bool PwrEventActivityCanSleep(struct timespec *now);

void PwrEventActivityRemoveExpired(struct timespec *now);

int PwrEventActivityCount(struct timespec *from);

bool PwrEventActivityCanSleep(struct timespec *now);
bool PwrEventFreezeActivities(struct timespec *now);
void PwrEventThawActivities(void);

long PwrEventActivityGetMaxDuration(struct timespec *now);

#endif
