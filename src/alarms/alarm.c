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


/**
* @file alarm.c
*
* @brief Old interface (deprecated) to add/clear alarms using RTC.
*
*/

#include <glib.h>
#include <stdbool.h>
#include <time.h>
#include <string.h>
#include <luna-service2/lunaservice.h>

#include <cjson/json.h>

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

#include "lunaservice_utils.h"


#include "main.h"
#include "logging.h"
#include "config.h"
#include "timeout_alarm.h"
#include "reference_time.h"
#include "timesaver.h"

#define LOG_DOMAIN "ALARM: "

/**
 * @defgroup RTCAlarms  RTC alarms
 * @ingroup Sleepd
 * @brief Alarms for RTC wakeup.
 */

/**
 * @defgroup OldInterface   Old Interface
 * @ingroup RTCAlarms
 * @brief Old (deprecated) interface to add/clear RTC alarms
 */

/**
 * @addtogroup OldInterface
 * @{
 */

/**
* @brief A single alarm.
*/
typedef struct
{
	int         id;
	time_t      expiry;    /*< Number of seconds since 1/1/1970 epoch */

	bool        calendar;  /*< If true, Alarm represents a calendar time.
                            *  (i.e. Jan 5, 2009, 10:00am).
                            *
                            *  If false, Alarm is X seconds in future
                            *  (i.e. T+X).
                            */

	char       *key;               /*< alarm key */
	char       *serviceName;       /*< serviceName to notify */
	char       *applicationName;   /*< app source of alarm. */

	LSMessage  *message;   /*< Message to reply to. */
} _Alarm;

/**
* @brief Alarm queue.
*/
typedef struct
{

	GSequence *alarms;
	uint32_t seq_id;   // points to the next available id

	char *alarm_db;
} _AlarmQueue;

_AlarmQueue *gAlarmQueue = NULL;

static bool alarm_queue_add(uint32_t id, const char *key,
                            bool calendar, time_t expiry,
                            const char *serviceName, const char *applicationName,
                            bool subscribe, LSMessage *message);

bool alarm_queue_new(const char *key, bool calendar, time_t expiry,
                     const char *serviceName, const char *applicationName,
                     bool subscribe, LSMessage *message,
                     int *ret_id);

static bool alarm_write_db(void);
static void notify_alarms(void);
static void update_alarms(void);


/**
* @brief  Set alarm to fire in a fixed time in the future.
*
* luna://com.palm.sleep/time/alarmAdd
*
* Set alarm to expire in T+5hrs. Response will be sent
* as a call to "luna://com.palm.X/alarm".
*
* {"key":"calendarAlarm", "serviceName":"com.palm.X",
*  "relative_time":"05:00:00"}
*
* Subscribing indicates that you want the alarm message as a response to
* the current call.
*
* {"subscribe":true, "key":"calendarAlarm", "serviceName":"com.palm.X",
*  "relative_time":"05:00:00"}
*
* Alarm is sucessfully registered.
* {"alarmId":1}
*
* Alarm failed to be registered:
*
* {"returnValue":false, ...}
* {"returnValue":false,"serivceName":"com.palm.sleep",
*  "errorText":"com.palm.sleep is not running"}
*
* @param  sh
* @param  message
* @param  ctx
*
* @retval
*/
static bool
alarmAdd(LSHandle *sh, LSMessage *message, void *ctx)
{
	time_t alarm_time = 0;
	int rel_hour, rel_min, rel_sec;

	const char *key, *serviceName, *applicationName, *rel_time;
	bool subscribe;

	struct json_object *object;

	int alarm_id;
	bool retVal = false;

	LSError lserror;
	LSErrorInit(&lserror);
	time_t rtctime = 0;

	object = json_tokener_parse(LSMessageGetPayload(message));

	if (is_error(object))
	{
		goto malformed_json;
	}

	SLEEPDLOG_DEBUG("%s", LSMessageGetPayload(message));

	serviceName = json_object_get_string(
	                  json_object_object_get(object, "serviceName"));

	applicationName = LSMessageGetApplicationID(message);

	key = json_object_get_string(json_object_object_get(object, "key"));

	rel_time = json_object_get_string(
	               json_object_object_get(object, "relative_time"));

	if (!rel_time)
	{
		goto invalid_format;
	}

	if (!ConvertJsonTime(rel_time, &rel_hour, &rel_min, &rel_sec) ||
	        (rel_hour < 0 || rel_hour > 24 || rel_min < 0 || rel_min > 59 || rel_sec < 0 ||
	         rel_sec > 59))
	{
		goto invalid_format;
	}

	nyx_system_query_rtc_time(GetNyxSystemDevice(), &rtctime);

	SLEEPDLOG_DEBUG("alarmAdd(): (%s %s %s) in %s (rtc %ld)", serviceName,
	                applicationName, key, rel_time, rtctime);
	struct json_object *subscribe_json =
	    json_object_object_get(object, "subscribe");

	subscribe = json_object_get_boolean(subscribe_json);


	alarm_time = reference_time();
	alarm_time += rel_sec;
	alarm_time += rel_min * 60;
	alarm_time += rel_hour * 60 * 60;

	retVal = alarm_queue_new(key, false, alarm_time,
	                         serviceName, applicationName, subscribe, message, &alarm_id);

	if (!retVal)
	{
		goto error;
	}

	/*****************
	 * Use new timeout API
	 */
	{
		char *timeout_key = g_strdup_printf("%s-%d", key, alarm_id);
		_AlarmTimeout timeout;
		_timeout_create(&timeout, "com.palm.sleep", timeout_key,
		                "luna://com.palm.sleep/time/internalAlarmFired",
		                "{}",
		                false /*public bus*/,
		                true /*wakeup*/,
		                "" /*activity_id*/,
		                0 /*activity_duration_ms*/,
		                false /*calendar*/,
		                alarm_time);

		retVal = _timeout_set(&timeout);

		g_free(timeout_key);

		if (!retVal)
		{
			goto error;
		}
	}
	/*****************/

	/* Send alarm id of sucessful alarm add. */
	GString *reply = g_string_sized_new(512);
	g_string_append_printf(reply, "{\"alarmId\":%d", alarm_id);

	if (subscribe_json)
	{
		g_string_append_printf(reply, ",\"subscribed\":%s",
		                       subscribe ? "true" : "false");
	}

	g_string_append_printf(reply, "}");

	retVal = LSMessageReply(sh, message, reply->str, &lserror);

	g_string_free(reply, TRUE);
	goto cleanup;
error:
	retVal = LSMessageReply(sh, message, "{\"returnValue\":false,"
	                        "\"errorText\":\"Unknown error\"}", &lserror);
	goto cleanup;
invalid_format:
	retVal = LSMessageReply(sh, message, "{\"returnValue\":false,"
	                        "\"errorText\":\"Invalid format for alarm time.\"}", &lserror);
	goto cleanup;
malformed_json:
	LSMessageReplyErrorBadJSON(sh, message);
	goto cleanup;
cleanup:

	if (!is_error(object))
	{
		json_object_put(object);
	}

	if (!retVal && LSErrorIsSet(&lserror))
	{
		LSErrorPrint(&lserror, stderr);
		LSErrorFree(&lserror);
	}

	return true;
}

/**
* @brief Set a calendar event.
*
* luna://com.palm.sleep/time/alarmAddCalendar
*
* Message:
* Set alarm to expire at a fixed calendar time in UTC. Response will be sent
* as a call to "luna://com.palm.X/alarm".
*
* {"key":"calendarAlarm", "serviceName":"com.palm.X",
*  "date":"01-02-2009", "time":"13:40:03"}
*
* Subscribing indicates that you want the alarm message as a response to
* the current call.
*
* {"subscribe":true, "key":"calendarAlarm", "serviceName":"com.palm.X",
*  "date":"01-02-2009", "time":"13:40:03"}
*
* Response:
*
* Alarm is sucessfully registered for calendar date
* {"alarmId":1}
*
* Subscribe case:
* {"alarmId":1,"subscribed":true}
* {"alarmId":1,"fired":true}
*
* Alarm failed to be registered:
*
* {"returnValue":false, ...}
* {"returnValue":false,"serivceName":"com.palm.sleep",
*  "errorText":"com.palm.sleep is not running"}
*
* @param  sh
* @param  message
* @param  ctx
*
* @retval
*/
static bool
alarmAddCalendar(LSHandle *sh, LSMessage *message, void *ctx)
{
	int alarm_id;
	struct json_object *object;
	const char *key, *serviceName, *applicationName, *cal_date, *cal_time;
	struct tm gm_time;
	bool subscribe;
	bool retVal = false;
	gchar **cal_date_str;

	time_t alarm_time = 0;

	LSError lserror;
	LSErrorInit(&lserror);

	object = json_tokener_parse(LSMessageGetPayload(message));

	if (is_error(object))
	{
		goto malformed_json;
	}

	SLEEPDLOG_DEBUG("alarmAddCalendar() : %s", LSMessageGetPayload(message));

	serviceName = json_object_get_string(
	                  json_object_object_get(object, "serviceName"));

	applicationName = LSMessageGetApplicationID(message);

	key = json_object_get_string(json_object_object_get(object, "key"));

	cal_date = json_object_get_string(
	               json_object_object_get(object, "date"));
	cal_time = json_object_get_string(
	               json_object_object_get(object, "time"));


	if (!cal_date || !cal_time)
	{
		goto invalid_format;
	}

	int hour, min, sec;
	int month, day, year;

	if (!ConvertJsonTime(cal_time, &hour, &min, &sec))
	{
		goto invalid_format;
	}

	cal_date_str = g_strsplit(cal_date, "-", 3);

	if ((NULL == cal_date_str[0]) || (NULL == cal_date_str[1]) ||
	        (NULL == cal_date_str[2]))
	{
		goto invalid_format;
	}

	month = atoi(cal_date_str[0]);
	day = atoi(cal_date_str[1]);
	year = atoi(cal_date_str[2]);
	g_strfreev(cal_date_str);

	if (hour < 0 || hour > 24 || min < 0 || min > 59 ||
	        sec < 0 || sec > 59 ||
	        month < 1 || month > 12 || day < 1 || day > 31 || year < 0)
	{
		goto invalid_format;
	}

	SLEEPDLOG_DEBUG("alarmAddCalendar() : (%s %s %s) at %s %s", serviceName,
	                applicationName, key, cal_date, cal_time);

	struct json_object *subscribe_json =
	    json_object_object_get(object, "subscribe");

	subscribe = json_object_get_boolean(subscribe_json);

	memset(&gm_time, 0, sizeof(struct tm));

	gm_time.tm_hour = hour;
	gm_time.tm_min = min;
	gm_time.tm_sec = sec;
	gm_time.tm_mon = month - 1; // month-of-year [0-11]
	gm_time.tm_mday = day;      // day-of-month [1-31]
	gm_time.tm_year = year - 1900;

	/* timegm converts time(GMT) -> seconds since epoch */
	alarm_time = timegm(&gm_time);

	if (alarm_time < 0)
	{
		goto invalid_format;
	}

	retVal = alarm_queue_new(key, true, alarm_time,
	                         serviceName, applicationName, subscribe, message, &alarm_id);

	if (!retVal)
	{
		goto error;
	}

	/*****************
	 * Use new timeout API
	 */
	{
		char *timeout_key = g_strdup_printf("%s-%d", key, alarm_id);
		_AlarmTimeout timeout;
		_timeout_create(&timeout, "com.palm.sleep", timeout_key,
		                "luna://com.palm.sleep/time/internalAlarmFired",
		                "{}",
		                false /*public bus*/,
		                true /*wakeup*/,
		                "" /*activity_id*/,
		                0 /*activity_duration_ms*/,
		                true /*calendar*/,
		                alarm_time);

		retVal = _timeout_set(&timeout);

		g_free(timeout_key);

		if (!retVal)
		{
			goto error;
		}
	}
	/*****************/

	/* Send alarm id of sucessful alarm add. */
	GString *reply = g_string_sized_new(512);
	g_string_append_printf(reply, "{\"alarmId\":%d", alarm_id);

	if (subscribe_json)
	{
		g_string_append_printf(reply, ",\"subscribed\":%s",
		                       subscribe ? "true" : "false");
	}

	g_string_append_printf(reply, "}");

	retVal = LSMessageReply(sh, message, reply->str, &lserror);

	g_string_free(reply, TRUE);

	goto cleanup;
error:
	retVal = LSMessageReply(sh, message, "{\"returnValue\":false,"
	                        "\"errorText\":\"Unknown error\"}", &lserror);
	goto cleanup;
invalid_format:
	retVal = LSMessageReply(sh, message, "{\"returnValue\":false,"
	                        "\"errorText\":\"Invalid format for alarm time.\"}", &lserror);
	goto cleanup;
malformed_json:
	LSMessageReplyErrorBadJSON(sh, message);
	goto cleanup;
cleanup:

	if (!is_error(object))
	{
		json_object_put(object);
	}

	if (!retVal && LSErrorIsSet(&lserror))
	{
		LSErrorPrint(&lserror, stderr);
		LSErrorFree(&lserror);
	}

	return true;
}

/**
* @brief Query for set of alarms identified by 'serviceName' & 'key'.
*
* {"serviceName":"com.palm.X", "key":"calendarAlarm"}
*
* Response:
*
* {"alarms":
*   [{"alarmId":1,"key":"calendarAlarm"},
*    {"alarmId":2,"key":"calendarAlarm"},
*   ]}
*
* @param  sh
* @param  message
* @param  ctx
*
* @retval
*/
static bool
alarmQuery(LSHandle *sh, LSMessage *message, void *ctx)
{
	bool retVal;
	const char *serviceName, *key;
	struct json_object *object;
	GString *alarm_str = NULL;
	GString *buf = NULL;

	object = json_tokener_parse(LSMessageGetPayload(message));

	if (is_error(object))
	{
		goto malformed_json;
	}

	serviceName = json_object_get_string(
	                  json_object_object_get(object, "serviceName"));
	key = json_object_get_string(
	          json_object_object_get(object, "key"));

	if (!serviceName || !key)
	{
		goto invalid_format;
	}

	alarm_str = g_string_sized_new(512);

	if (!alarm_str)
	{
		goto cleanup;
	}

	bool first = true;
	GSequenceIter *iter = g_sequence_get_begin_iter(gAlarmQueue->alarms);

	while (!g_sequence_iter_is_end(iter))
	{
		_Alarm *alarm = (_Alarm *)g_sequence_get(iter);
		GSequenceIter *next = g_sequence_iter_next(iter);

		if (alarm && alarm->serviceName && alarm->key &&
		        (strcmp(alarm->serviceName, serviceName) == 0) &&
		        (strcmp(alarm->key, key) == 0))
		{
			g_string_append_printf(alarm_str,
			                       "%s{\"alarmId\":%d,\"key\":\"%s\"}",
			                       first ? "" : "\n,",
			                       alarm->id, alarm->key);
			first = false;
		}

		iter = next;
	}

	buf = g_string_sized_new(512);
	g_string_append_printf(buf, "{\"alarms\": [%s]}", alarm_str->str);

	LSError lserror;
	LSErrorInit(&lserror);
	retVal = LSMessageReply(sh, message, buf->str, &lserror);

	if (!retVal)
	{
		LSErrorPrint(&lserror, stderr);
		LSErrorFree(&lserror);
	}

	goto cleanup;

invalid_format:
	retVal = LSMessageReply(sh, message, "{\"returnValue\":false,"
	                        "\"errorText\":\"alarmQuery parameters are missing.\"}", &lserror);
	goto cleanup;
malformed_json:
	LSMessageReplyErrorBadJSON(sh, message);
	goto cleanup;
cleanup:

	if (alarm_str)
	{
		g_string_free(alarm_str, TRUE);
	}

	if (buf)
	{
		g_string_free(buf, TRUE);
	}

	if (!is_error(object))
	{
		json_object_put(object);
	}

	return true;
}

/**
* @brief Remove an alarm by id.
*
* {"alarmId":1}
*
* Response:
*
* {"returnValue":true}
*
* @param  sh
* @param  message
* @param  ctx
*
* @retval
*/
static bool
alarmRemove(LSHandle *sh, LSMessage *message, void *ctx)
{
	LSError lserror;
	LSErrorInit(&lserror);

	bool found = false;
	bool retVal;

	const char *payload = LSMessageGetPayload(message);
	struct json_object *object = json_tokener_parse(payload);

	if (is_error(object))
	{
		goto malformed_json;
	}

	SLEEPDLOG_DEBUG("alarmRemove() : %s", LSMessageGetPayload(message));

	int alarmId =
	    json_object_get_int(json_object_object_get(object, "alarmId"));

	GSequenceIter *iter = g_sequence_get_begin_iter(gAlarmQueue->alarms);

	while (!g_sequence_iter_is_end(iter))
	{
		_Alarm *alarm = (_Alarm *)g_sequence_get(iter);
		GSequenceIter *next = g_sequence_iter_next(iter);

		if (alarm && alarm->id == alarmId)
		{
			char *timeout_key = g_strdup_printf("%s-%d", alarm->key, alarm->id);
			_timeout_clear("com.palm.sleep", timeout_key,
			               false /*public_bus*/);
			g_free(timeout_key);

			g_sequence_remove(iter);
			found = true;
		}

		iter = next;
	}

	const char *response;

	if (found)
	{
		alarm_write_db();
		response = "{\"returnValue\":true}";
	}
	else
	{
		response = "{\"returnValue\":false}";
	}

	retVal = LSMessageReply(sh, message, response, &lserror);

	if (!retVal)
	{
		LSErrorPrint(&lserror, stderr);
		LSErrorFree(&lserror);
	}

	goto cleanup;
malformed_json:
	LSMessageReplyErrorBadJSON(sh, message);
	goto cleanup;
cleanup:

	if (!is_error(object))
	{
		json_object_put(object);
	}

	return true;
}

/**
* @brief Called when a RTC alarm is fired.
*
* @param  sh
* @param  message
* @param  ctx
*
* @retval
*/
static bool
internalAlarmFired(LSHandle *sh, LSMessage *message, void *ctx)
{
	update_alarms();
	return true;
}

LSMethod time_methods[] =
{

	{ "alarmAddCalendar", alarmAddCalendar },
	{ "alarmAdd", alarmAdd },
	{ "alarmQuery", alarmQuery },
	{ "alarmRemove", alarmRemove },
	{ "internalAlarmFired", internalAlarmFired },
	{ },
};

static void
alarm_free(_Alarm *a)
{
	SLEEPDLOG_DEBUG("Freeing alarm with id %d", a->id);

	g_free(a->serviceName);
	g_free(a->applicationName);

	if (a->message)
	{
		LSMessageUnref(a->message);
	}

	g_free(a);
}

static gint
alarm_cmp_func(_Alarm *a, _Alarm *b, gpointer data)
{
	return (a->expiry < b->expiry) ? -1 :
	       (a->expiry == b->expiry) ? 0 : 1;
}

static int
alarm_queue_create(void)
{
	gAlarmQueue = g_new0(_AlarmQueue, 1);
	gAlarmQueue->alarms = g_sequence_new((GDestroyNotify)alarm_free);
	gAlarmQueue->seq_id = 0;

	gAlarmQueue->alarm_db =
	    g_build_filename(gSleepConfig.preference_dir, "alarms.xml", NULL);

	return 0;
}

#define STD_ASCTIME_BUF_SIZE    26

static void
alarm_print(_Alarm *a)
{
	char buf[STD_ASCTIME_BUF_SIZE];
	struct tm tm;

	gmtime_r(&a->expiry, &tm);
	asctime_r(&tm, buf);

	SLEEPDLOG_DEBUG("(%s,%s) set alarm id %d @ %s",
	                a->serviceName ? : "null",
	                a->applicationName ? : "null",
	                a->id, buf);
}

static void
alarm_read_db(void)
{
	bool retVal;

	xmlDocPtr db = xmlReadFile(gAlarmQueue->alarm_db, NULL, 0);

	if (!db)
	{
		return;
	}

	xmlNodePtr cur = xmlDocGetRootElement(db);
	xmlNodePtr sub;

	if (!cur)
	{
		return;
	}

	sub = cur->children;

	while (sub != NULL)
	{
		if (!xmlStrcmp(sub->name, (const xmlChar *)"alarm"))
		{
			xmlChar *id = xmlGetProp(sub, (const xmlChar *)"id");
			xmlChar *key = xmlGetProp(sub, (const xmlChar *)"key");
			xmlChar *expiry = xmlGetProp(sub, (const xmlChar *)"expiry");
			xmlChar *calendar = xmlGetProp(sub, (const xmlChar *)"calendar");
			xmlChar *service = xmlGetProp(sub, (const xmlChar *)"serviceName");
			xmlChar *app = xmlGetProp(sub, (const xmlChar *)"applicationName");

			if (!id || !expiry)
			{
				goto clean_round;
			}

			unsigned long expiry_secs = 0;
			uint32_t alarmId = 0;
			bool isCalendar = false;

			if (expiry)
			{
				expiry_secs = atol((const char *)expiry);
			}

			if (id)
			{
				alarmId = atoi((const char *)id);
			}

			if (calendar)
			{
				isCalendar = atoi((const char *)calendar) > 0;
			}

			retVal = alarm_queue_add(alarmId, (const char *)key,
			                         isCalendar, expiry_secs,
			                         (const char *)service,
			                         (const char *)app, false, NULL);

			if (!retVal)
			{
				SLEEPDLOG_WARNING(MSGID_ALARM_NOT_SET, 3, PMLOGKFV(ALARM_ID, "%d", alarmId),
				                  PMLOGKS(SRVC_NAME, service), PMLOGKS(APP_NAME, app), "could not add alarm");
			}

clean_round:
			xmlFree(expiry);
			xmlFree(service);
			xmlFree(app);
		}

		sub = sub->next;
	}

	xmlFreeDoc(db);
}

static void
alarm_save(_Alarm *a, FILE *file)
{
	char buf[STD_ASCTIME_BUF_SIZE];
	struct tm tm;

	gmtime_r(&a->expiry, &tm);

	asctime_r(&tm, buf);
	g_strchomp(buf);

	fprintf(file, "<alarm id='%d' expiry='%ld' calendar='%d'"
	        " key='%s'"
	        " expiry_text='%s'"
	        " serviceName='%s'"
	        " applicationName='%s'/>\n",
	        a->id, a->expiry, a->calendar,
	        a->key, buf, a->serviceName ? : "", a->applicationName ? : "");
}

static bool
alarm_write_db(void)
{
	bool retVal = false;

	FILE *file = fopen(gAlarmQueue->alarm_db, "w");

	if (!file)
	{
		goto cleanup;
	}

	fprintf(file, "<alarms>\n");
	g_sequence_foreach(gAlarmQueue->alarms, (GFunc)alarm_save, file);
	fprintf(file, "</alarms>\n");
	fclose(file);

	retVal = true;
cleanup:
	return retVal;
}

/**
* @brief Create a new alarm and assign it an new id.
*
* @param  key
* @param  calendar_time
* @param  expiry
* @param  serviceName
* @param  applicationName
* @param  subscribe
* @param  message
* @param ret_id
*
*/
bool
alarm_queue_new(const char *key, bool calendar_time, time_t expiry,
                const char *serviceName,
                const char *applicationName,
                bool subscribe, LSMessage *message, int *ret_id)
{
	bool retVal;
	uint32_t id = gAlarmQueue->seq_id++;

	if (ret_id)
	{
		*ret_id = id;
	}

	retVal = alarm_queue_add(id, key, calendar_time, expiry,
	                         serviceName, applicationName,
	                         subscribe, message);

	if (retVal)
	{
		alarm_write_db();
	}

	return retVal;
}

/**
* @brief Obtain the next alarm that will fire.
*
*/
_Alarm *
alarm_queue_get_first(void)
{
	GSequenceIter *seq =
	    g_sequence_get_begin_iter(gAlarmQueue->alarms);

	if (g_sequence_iter_is_end(seq))
	{
		return NULL;
	}

	_Alarm *alarm = (_Alarm *)g_sequence_get(seq);
	return alarm;
}

/**
* @brief Adjusts the alarm when a time set occurs and the wall clock
*        and rtc clock diverge.
*
* This should also be called on init, in case of a crash before we
* were able to adjust the alarms successfully.
*
*/
void
recalculate_alarms(time_t delta)
{
	if (delta)
	{
		/* Adjust each fixed time alarm by the delta.
		 * i.e. 5 seconds in the future + delta = T + 5 + delta
		 */
		GSequenceIter *iter = g_sequence_get_begin_iter(gAlarmQueue->alarms);

		while (!g_sequence_iter_is_end(iter))
		{
			_Alarm *alarm = (_Alarm *)g_sequence_get(iter);
			GSequenceIter *next = g_sequence_iter_next(iter);

			if (alarm && !alarm->calendar)
			{
				alarm->expiry += delta;
			}

			iter = next;
		}

		/* resort */
		g_sequence_sort(gAlarmQueue->alarms,
		                (GCompareDataFunc)alarm_cmp_func, NULL);

		/* persist */
		alarm_write_db();
	}

	return;
}

void
update_alarms_delta(time_t delta)
{
	/* If the time changed, we need to readjust alarms,
	 * and persist the changes.
	 */
	if (delta)
	{
		recalculate_alarms(delta);
	}

	/* Trigger any pending alarms and remove them from the queue.
	 */
	notify_alarms();
}

/**
* @brief Set the next alarm.
*/
static void
update_alarms(void)
{
	update_alarms_delta(0);
}

/**
* @brief Add a new alarm to the queue.
*
* @param  id
* @param  calendar_time
* @param  expiry
* @param  serviceName
* @param  applicationName
*
* @retval
*/
static bool
alarm_queue_add(uint32_t id, const char *key, bool calendar_time,
                time_t expiry, const char *serviceName,
                const char *applicationName,
                bool subscribe, LSMessage *message)
{
	_Alarm *alarm = g_new0(_Alarm, 1);

	alarm->key = g_strdup(key);
	alarm->id = id;
	alarm->calendar = calendar_time;
	alarm->expiry = expiry;
	alarm->serviceName = g_strdup(serviceName);
	alarm->applicationName = g_strdup(applicationName);

	if (subscribe)
	{
		LSError lserror;
		LSErrorInit(&lserror);
		bool retVal = LSSubscriptionAdd(
		                  GetLunaServiceHandle(), "alarm", message, &lserror);

		if (!retVal)
		{
			LSErrorPrint(&lserror, stderr);
			LSErrorFree(&lserror);
			goto error;
		}

		LSMessageRef(message);
		alarm->message = message;
	}

	alarm_print(alarm);

	if (alarm->id >= gAlarmQueue->seq_id)
	{
		gAlarmQueue->seq_id = alarm->id + 1;
	}

	g_sequence_insert_sorted(gAlarmQueue->alarms,
	                         alarm, (GCompareDataFunc)alarm_cmp_func,
	                         NULL);

	update_alarms();
	return true;
error:
	return false;
}

/**
* @brief Sends a "/alarm" message to the service associated with this alarm.
*
* {"alarmId":1,"fired":true,"key":"appkey"}
*
* @param  alarm
*/
static void
fire_alarm(_Alarm *alarm)
{
	bool retVal;
	char buf_alarm[STD_ASCTIME_BUF_SIZE];

	struct tm tm_alarm;
	time_t rtctime = 0;

	gmtime_r(&alarm->expiry, &tm_alarm);
	asctime_r(&tm_alarm, buf_alarm);

	nyx_system_query_rtc_time(GetNyxSystemDevice(), &rtctime);

	SLEEPDLOG_DEBUG("fire_alarm() : Alarm (%s %s %s) fired at %s (rtc %ld)",
	                alarm->serviceName,
	                alarm->applicationName, alarm->key, buf_alarm, rtctime);

	GString *payload = g_string_sized_new(255);
	g_string_append_printf(payload, "{\"alarmId\":%d,\"fired\":true", alarm->id);

	if (alarm->key)
	{
		g_string_append_printf(payload, ",\"key\":\"%s\"", alarm->key);
	}

	if (alarm->applicationName && strcmp(alarm->applicationName, "") != 0)
	{
		g_string_append_printf(payload, ",\"applicationName\":\"%s\"",
		                       alarm->applicationName);
	}

	g_string_append_printf(payload, "}");

	LSError lserror;
	LSErrorInit(&lserror);

	if (alarm->serviceName && strcmp(alarm->serviceName, "") != 0)
	{
		char *uri = g_strdup_printf("luna://%s/alarm", alarm->serviceName);
		retVal = LSCall(GetLunaServiceHandle(), uri, payload->str,
		                NULL, NULL, NULL, &lserror);

		if (!retVal)
		{
			LSErrorPrint(&lserror, stderr);
			LSErrorFree(&lserror);
		}

		g_free(uri);
	}

	if (alarm->message)
	{
		retVal = LSMessageReply(GetLunaServiceHandle(), alarm->message,
		                        payload->str, &lserror);

		if (!retVal)
		{
			LSErrorPrint(&lserror, stderr);
			LSErrorFree(&lserror);
		}
	}

	g_string_free(payload, TRUE);
}

/**
* @brief Send message to each expired alarm.
*/
static void
notify_alarms(void)
{
	time_t now;
	bool fired = false;

	now = reference_time();

	GSequenceIter *iter = g_sequence_get_begin_iter(gAlarmQueue->alarms);

	while (!g_sequence_iter_is_end(iter))
	{
		_Alarm *alarm = (_Alarm *)g_sequence_get(iter);
		GSequenceIter *next = g_sequence_iter_next(iter);

		if (alarm && alarm->expiry <= now)
		{
			fire_alarm(alarm);
			g_sequence_remove(iter);

			fired = true;
		}

		iter = next;
	}

	if (fired)
	{
		alarm_write_db();
	}
}

/**
* @brief Init registers with bus and udev.
*
*/
int
alarm_init(void)
{
	LSError lserror;
	LSErrorInit(&lserror);

	if (!LSRegisterCategory(GetLunaServiceHandle(),
	                        "/time", time_methods, NULL, NULL, &lserror))
	{
		goto error;
	}

	alarm_queue_create();
	alarm_read_db();

	update_alarms();
	return 0;
error:
	return -1;
}

/* @} END OF OldInterface */

