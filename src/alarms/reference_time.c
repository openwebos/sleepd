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
 *  @file reference_time.c
 *
 *  Implmenetation of reference time source
 */

#include "reference_time.h"

static time_t clock_to_reference = 0;
static const time_t invalid_time = ((time_t) - 1);

/**
 * Get current reference time value (seconds since epoch)
 *
 * @retval false if error met
 */
static bool reference_gettime(time_t *ret_time)
{
	/* Here we use CLOCK_BOOTTIME as a source of time which is affected only by
	 * adjtimex (slewing) for adjusting over half of second
	 */
	struct timespec ts;

	if (!clock_gettime(CLOCK_BOOTTIME, &ts))
	{
		return false;
	}

	ts.tv_sec += clock_to_reference;
	*ret_time = ts.tv_sec;
	return true;
}

time_t reference_time(void)
{
	time_t reftime;
	return reference_gettime(&reftime) ? reftime
	       : time(NULL);
}

time_t update_reference_time(bool (*callback)(time_t delta, void *user_data),
                             void *user_data)
{
	time_t systime, reftime, delta;

	if (time(&systime) == invalid_time)
	{
		return invalid_time;
	}

	if (!reference_gettime(&reftime))
	{
		return invalid_time;
	}

	delta = systime - reftime;

	if (!delta)
	{
		return 0;    /* no need to adjust */
	}

	if (callback == NULL || callback(delta, user_data))
	{
		clock_to_reference += delta;
		return delta;
	}
	else
	{
		// callback blocked adjustment
		return 0;
	}
}
