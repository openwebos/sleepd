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
 *
 *  Reference time by itself represents clock that ticks together with
 *  system-time and periodically adjusted to match with system-time. This
 *  approach allows to have full control over time adjustion process and fire
 *  time-change events.
 */

#ifndef __REFERENCE_TIME_H
#define __REFERENCE_TIME_H

#include <stdbool.h>
#include <time.h>

/**
 * System time unaffected by time change since last update_reference_time call
 */
time_t reference_time(void);

/**
 * Adjust reference time to system-time and fire callback.
 *
 * @param callback is called when time adjustment is going to be applied for
 *        reference time. If callback returns false - no adjustment is done and
 *        probably during next call we'll be in same position. If NULL passed
 *        as callback it will be ignored and adjustment will happen as if
 *        callback returned true.
 *
 * @param user_data passed to callback
 *
 * @retval reference adjustion value
 */
time_t update_reference_time(bool (*callback)(time_t delta, void *user_data), void *user_data);

#endif
