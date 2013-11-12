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

#include <nyx/nyx_client.h>

#include "main.h"
#include "reference_time.h"

static time_t rtc_to_wall = 0;

time_t to_rtc(time_t t)
{
	return t - rtc_to_wall;
}

time_t rtc_wall_time(void)
{
	time_t rtctime = 0;
	nyx_system_query_rtc_time(GetNyxSystemDevice(), &rtctime);
	return rtctime + rtc_to_wall;
}

bool
wall_rtc_diff(time_t *ret_delta)
{
	time_t rtc_time_now = 0;
	time_t wall_time_now = 0;

	nyx_system_query_rtc_time(GetNyxSystemDevice(), &rtc_time_now);

	time(&wall_time_now);

	/* Calculate the time difference */
	time_t delta = wall_time_now - rtc_time_now;

	if (ret_delta)
	{
		*ret_delta = delta;
	}

	return true;
}

time_t update_rtc(time_t *ret_delta)
{
	bool retVal;
	time_t new_delta = 0;
	time_t delta = 0;

	retVal = wall_rtc_diff(&new_delta);

	if (!retVal)
	{
		return false;
	}

	if (new_delta != rtc_to_wall)
	{
		delta = new_delta - rtc_to_wall;
		rtc_to_wall = new_delta;
	}

	if (ret_delta)
	{
		*ret_delta = delta;
	}

	return true;
}
