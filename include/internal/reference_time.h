/****************************************************************
 * @@@LICENSE
 *
 * Copyright (c) 2013 LG Electronics, Inc.
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
 * LICENSE@@@
 ****************************************************************/

/**
 *  @file reference_time.h
 *
 *  Interfece to some reference time source built to address issues caused by
 *  system time change.
 */

#ifndef __REFERENCE_TIME_H
#define __REFERENCE_TIME_H

#include <stdbool.h>
#include <time.h>

/**
 * @brief Convert to rtc time.
 *
 * @param  t
 *
 * @retval
 */
time_t to_rtc(time_t t);

/**
 * @brief Last wall time.
 *
 * @retval
 */
time_t rtc_wall_time(void);

/**
 * @brief Calculate the time difference between RTC time and wall time
 */
bool wall_rtc_diff(time_t *ret_delta);

/**
 * @brief Update the rtc and return the difference rtc changed by.
 *
 * @retval
 */
time_t update_rtc(time_t *ret_delta);

#endif
