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


#ifndef __SAWMILL_LOGGER_H__
#define __SAWMILL_LOGGER_H__
void sawmill_logger_record_sleep(struct timespec time_awake);

void sawmill_logger_record_wake(struct timespec time_asleep);

void sawmill_logger_record_screen_toggle(bool on);

void get_time_now(struct timespec *);

#endif // __SAWMILL_LOGGER_H__
