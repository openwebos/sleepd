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


#ifndef _CONFIG_H_
#define _CONFIG_H_

#include <stdbool.h>

/**
 * Sleep configuration.
 */

typedef struct {
    int wait_idle_ms;
    int wait_idle_granularity_ms;

    int wait_suspend_response_ms;
    int wait_prepare_suspend_ms;
    int after_resume_idle_ms;
    int wait_alarms_s;

    bool suspend_with_charger;
    bool visual_leds_suspend;

    int debug;
    bool use_syslog;

    bool disable_rtc_alarms;

    const char *preference_dir;

    /* These aren't really config, they are runtime parameters */
    int is_running;
    bool fasthalt;
} SleepConfiguration;

extern SleepConfiguration gSleepConfig;

#endif // _CONFIG_H_
