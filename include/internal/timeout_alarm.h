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



#ifndef _TIMEOUT_ALARM_H
#define _TIMEOUT_ALARM_H

typedef struct _AlarmTimeout
{
	const char *table_id;

	const char *app_id;
	const char *key;
	const char *uri;
	const char *params;
	const char *activity_id;
	int activity_duration_ms;
	bool        public_bus;
	bool        wakeup;
	bool        calendar;
	time_t      expiry;
} _AlarmTimeout;

typedef struct _AlarmTimeoutNonConst
{
	char *table_id;

	char *app_id;
	char *key;
	char *uri;
	char *params;
	char *activity_id;
	int activity_duration_ms;
	bool        public_bus;
	bool        wakeup;
	bool        calendar;
	time_t      expiry;
} _AlarmTimeoutNonConst;

time_t rtc_wall_time(void);

void _timeout_create(_AlarmTimeout *timeout,
                     const char *app_id, const char *key,
                     const char *uri, const char *params,
                     bool public_bus, bool wakeup,
                     const char *activity_id,
                     int activity_duration_ms,
                     bool calendar, time_t expiry);

bool _timeout_set(_AlarmTimeout *timeout);

bool _timeout_read(_AlarmTimeoutNonConst *timeout, const char *app_id,
                   const char *key, bool public_bus);

bool _timeout_clear(const char *app_id, const char *key, bool public_bus);

bool _timeout_delete(const char *app_id, const char *key, bool public_bus);

void _queue_next_timeout(bool set_callback_fn);

bool timeout_get_next_wakeup(time_t *expiry, gchar **app_id, gchar **key);

bool update_timeouts_on_resume(void);

#endif
