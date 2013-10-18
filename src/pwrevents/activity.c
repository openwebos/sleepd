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
* @file activity.c
*
* @brief This file contains functions to manage activities, which are entities that can be
* registered with sleepd to prevent the system from suspending for a certain time duration.
*
*/


#include <glib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <luna-service2/lunaservice.h>

#include "suspend.h"
#include "main.h"
#include "clock.h"
#include "logging.h"
#include "activity.h"
#include "init.h"

//#include "metrics.h"

//#define CONFIG_ACTIVITY_TIMEOUT_RDX_REPORT

// Max duration at 15 minutes.
#define ACTIVITY_MAX_DURATION_MS (15*60*1000)

#define ACTIVITY_HIGH_DURATION_MS (10*60*1000)

#define LOG_DOMAIN "PWREVENT-ACTIVITY: "

/**
 * @defgroup PowerEvents    Power Events
 * @ingroup Sleepd
 * @brief Power events are all the events that affect system power.
 */

/**
 * @defgroup PowerActivities    Power Activities
 * @ingroup PowerEvents
 * @brief Duration of time to prevent the system from sleeping
 */

/**
 * @addtogroup PowerActivities
 * @{
 */

/**
* @brief Structure for maintaining all registered activities.
*/

typedef struct
{
	struct timespec start_time;
	struct timespec end_time;
	int duration_ms;

	char *activity_id;
} Activity;

GQueue *activity_roster = NULL;
pthread_mutex_t activity_mutex = PTHREAD_MUTEX_INITIALIZER;

bool gFrozen = false;




/**
 * @brief Initialize the activity queue
 */
static int
_activity_init(void)
{
	if (!activity_roster)
	{
		activity_roster = g_queue_new();
	}

	return 0;
}

/**
 * @brief Add a new activity to the global activity queue (activity_roster)
 *
 * @param activity_id Passed by the caller
 * @param duration_ms Duration for which the system cannot suspend with this activity
 *
 * @retval The new activity added
 */

static Activity *
_activity_new(const char *activity_id, int duration_ms)
{
	if (duration_ms >= ACTIVITY_MAX_DURATION_MS)
	{
		duration_ms = ACTIVITY_MAX_DURATION_MS;
	}

	Activity *activity = g_new0(Activity, 1);

	activity->activity_id = g_strdup(activity_id);
	activity->duration_ms = duration_ms;

	// end += duration
	ClockGetTime(&activity->start_time);

	activity->end_time.tv_sec = activity->start_time.tv_sec;
	activity->end_time.tv_nsec = activity->start_time.tv_nsec;

	ClockAccumMs(&activity->end_time, activity->duration_ms);

	return activity;
}

/**
 * @brief Free the activity specified.
 *
 * @param activity The activity to be freed
 *
 * @retval
 */

static void
_activity_free(Activity *activity)
{
	if (activity)
	{
		g_free(activity->activity_id);
		g_free(activity);
	}
}

/**
 * @brief Compare the expiry time of two activities
 *
 * @param a
 * @param b
 *
 * @retval 1 if expiry time of a is greater than b
 *         0 otherwise
 */

static int
_activity_compare(Activity *a, Activity *b)
{
	if (ClockTimeIsGreater(&a->end_time, &b->end_time))
	{
		return 1;
	}
	else
	{
		return -1;
	}
}


/**
 * @brief Count the number of activities beyond the timespec passed as argument
 *
 * @param from Start counting activities from this time
 *
 * @retval The number of activities beyond "from"
 */
static int
_activity_count(struct timespec *from)
{
	int count = 0;

	pthread_mutex_lock(&activity_mutex);

	GList *iter;

	for (iter = activity_roster->head; iter != NULL; iter = iter->next)
	{
		Activity *a = (Activity *)iter->data;

		// now > activity.end_time
		if (ClockTimeIsGreater(from, &a->end_time))
		{
			continue;
		}

		count++;
	}

	pthread_mutex_unlock(&activity_mutex);

	return count;
}

/**
* @brief Insert an activity into sorted list.
*
* @param  activity
* @return false if the activity cannot be created (if activities are frozen).
*/
static bool
_activity_insert(const char *activity_id, int duration_ms)
{
	bool ret = true;

	pthread_mutex_lock(&activity_mutex);

	if (gFrozen)
	{
		ret = false;
	}
	else
	{
		Activity *activity = _activity_new(activity_id, duration_ms);

		g_queue_insert_sorted(activity_roster, activity,
		                      (GCompareDataFunc)_activity_compare, NULL);
	}

	pthread_mutex_unlock(&activity_mutex);
	return ret;
}


/**
 * @brief Delete the activity from the global activity queue.
 *
 * @param activity_id   The activity which needs to be deleted.
 *
 * @retval The activity that was deleted.
 */

static Activity *
_activity_remove_id(const char *activity_id)
{
	Activity *ret_activity = NULL;
	pthread_mutex_lock(&activity_mutex);

	GList *iter;

	for (iter = activity_roster->head; iter != NULL; iter = iter->next)
	{
		Activity *a = (Activity *)iter->data;

		if (strcmp(a->activity_id, activity_id) == 0)
		{
			ret_activity = a;
			g_queue_delete_link(activity_roster, iter);
			break;
		}
	}

	pthread_mutex_unlock(&activity_mutex);

	return ret_activity;
}

/**
 * @brief Check whether the activity has expired
 *
 * @param a Activity whose expiry is to be checked
 * @param now Current time
 *
 * @retval True if the activity expired
 */

static bool
_activity_expired(Activity *a, struct timespec *now)
{
	// end > now
	return ClockTimeIsGreater(now, &a->end_time);
}


/**
 * @brief Retrieve the first activiy (from beginning or from end) that hasn't expired (without
 * locking the activity mutex)
 *
 * @param now Current time
 * @param getmax If True start from the end of acitivty queue, else start from the head
 *
 * @retval Activity.
 */

static Activity *
_activity_obtain_unlocked(struct timespec *now, bool getmax)
{
	Activity *ret_activity = NULL;
	GList *iter;

	if (getmax)
	{
		iter = activity_roster->tail;
	}
	else
	{
		iter = activity_roster->head;
	}

	while (iter != NULL)
	{
		Activity *a = (Activity *)iter->data;

		// return first activity that is not expired.
		if (!_activity_expired(a, now))
		{
			ret_activity = a;
			goto end;
		}

		if (getmax)
		{
			iter = iter->prev;
		}
		else
		{
			iter = iter->next;
		}
	}

end:
	return ret_activity;
}

/**
 * @brief Get the activity which will expire first (without
 * locking the activity mutex)
 *
 * @param now
 *
 * @retval Activity
 */
static Activity *
_activity_obtain_min_unlocked(struct timespec *now)
{
	return _activity_obtain_unlocked(now, false);
}

/**
 * @brief Get the activity which will expire last (without
 * locking the activity mutex)
 *
 * @param now
 *
 * @retval Activity
 */

static Activity *
_activity_obtain_max_unlocked(struct timespec *now)
{
	return _activity_obtain_unlocked(now, true);
}

/**
 * @brief Get the first activity expiring by locking activity_mutex.
 *
 * @param now
 *
 * @retval Activity
 */

static Activity *
_activity_obtain_min(struct timespec *now)
{
	Activity *ret_activity = NULL;

	pthread_mutex_lock(&activity_mutex);

	ret_activity = _activity_obtain_min_unlocked(now);

	pthread_mutex_unlock(&activity_mutex);

	return ret_activity;
}

/**
 * @brief Get the last activity expiring by locking activity_mutex.
 *
 * @param now
 *
 * @retval Activity
 */


static Activity *
_activity_obtain_max(struct timespec *now)
{
	Activity *max_activity = NULL;

	pthread_mutex_lock(&activity_mutex);

	max_activity = _activity_obtain_max_unlocked(now);

	pthread_mutex_unlock(&activity_mutex);

	return max_activity;
}

/**
 * @brief Print the details of all the activities starting from a specified time
 *
 * @param from Activities starting from this time stamp
 * @param now Current time
 *
 * @retval
 */

static void
_activity_print(struct timespec *from, struct timespec *now)
{
	struct timespec diff;
	int diff_ms;

	pthread_mutex_lock(&activity_mutex);

	GList *iter;

	for (iter = activity_roster->head; iter != NULL; iter = iter->next)
	{
		Activity *a = (Activity *)iter->data;

		// now > activity.end_time
		if (ClockTimeIsGreater(from, &a->end_time))
		{
			continue;
		}

		// end_time - now
		ClockDiff(&diff, &a->end_time, now);

		diff_ms = diff.tv_sec * 1000 + diff.tv_nsec / 1000000;

		SLEEPDLOG_DEBUG("_activity_print() : (%s) for %d ms, expiry in %d ms",
		                a->activity_id, a->duration_ms,
		                diff_ms);
	}

	pthread_mutex_unlock(&activity_mutex);
}

/**
 * @brief Free the memory for an activity.
 *
 * @param Activity
 */

static void
_activity_stop_activity(Activity *a)
{
	if (!a)
	{
		return;
	}

	_activity_free(a);
}

/**
* @brief Stop and free an activity.
*
* @param  activity_id
*/
static void
_activity_stop(const char *activity_id)
{
	Activity *a = _activity_remove_id(activity_id);

	if (!a)
	{
		return;
	}

	_activity_stop_activity(a);
}

/**
* @brief Starts an activity.
*
* @param  activity_id
* @param  duration_ms
*/
static bool
_activity_start(const char *activity_id, int duration_ms)
{
	/* replace exising *activity_id' */
	_activity_stop(activity_id);

	return _activity_insert(activity_id, duration_ms);
}

/**
* @brief Start an activity by the name of 'activity_id'.
*
* @param  activity_id  Should be in format com.domain.reverse-serial.
* @param  duration_ms
*
* @return false if the activity could not be created... (activities may be frozen).
*/
bool
PwrEventActivityStart(const char *activity_id, int duration_ms)
{
	bool retVal;

	retVal = _activity_start(activity_id, duration_ms);

	SLEEPDLOG_DEBUG("PwrEventActivityStart() : (%s) for %dms => %s", activity_id,
	                duration_ms, retVal ? "true" : "false");

	if (retVal)
	{
		/*
		    Force IdleCheck to run in case this activity is the same as
		    the current "long pole" activity but with a shorter life.
		*/
		ScheduleIdleCheck(0, false);
	}

	return retVal;
}

/**
* @brief Stop an activity
*
* @param activity_id of the activity than needs to be stopped
*
* @param  activity_id
*/
void
PwrEventActivityStop(const char *activity_id)
{
	SLEEPDLOG_DEBUG("PwrEventActivityStop() : (%s)", activity_id);

	_activity_stop(activity_id);

	ScheduleIdleCheck(0, false);
}

/**
* @brief Remove all expired activities...
*        This assumes the list is sorted.
*
* @param  now
*/
void
PwrEventActivityRemoveExpired(struct timespec *now)
{
	pthread_mutex_lock(&activity_mutex);

	GList *iter;

	for (iter = activity_roster->head; iter != NULL;)
	{
		Activity *a = (Activity *)iter->data;

		// remove expired
		if (_activity_expired(a, now))
		{
			GList *current_iter = iter;
			iter = iter->next;

			if (a->duration_ms >= ACTIVITY_HIGH_DURATION_MS)
			{
				LSError lserror;
				LSErrorInit(&lserror);

				SLEEPDLOG_DEBUG("Long activity %s of duration %d ms expired... sending RDX report.",
				                a->activity_id, a->duration_ms);
			}

			_activity_stop_activity(a);

			g_queue_delete_link(activity_roster, current_iter);
		}
		else
		{
			break;
		}
	}

	pthread_mutex_unlock(&activity_mutex);
}

/*
 * @brief Count the number of activities from "from"
 *
 * @param from
 *
 * @retval Number of activities
 *
 */

int
PwrEventActivityCount(struct timespec *from)
{
	return _activity_count(from);
}

/**
* @brief Prints the activities active in range from
*        time 'start' to now.
*
* @param  from
*/
void
PwrEventActivityPrintFrom(struct timespec *start)
{
	struct timespec now;
	ClockGetTime(&now);

	_activity_print(start, &now);
}

/*
 * @brief Print all the pending activities in the system
 */
void
PwrEventActivityPrint(void)
{
	struct timespec now;
	ClockGetTime(&now);

	_activity_print(&now, &now);
}

/**
* @brief Tells us if there are any activities that prevent suspend.
*
* @param now
*
* @retval
*/
bool
PwrEventActivityCanSleep(struct timespec *now)
{
	Activity *a = _activity_obtain_min(now);
	return NULL == a;
}

/**
 * @brief Returns the max duration for which the system cannot suspend due to an activity
 *
 * @param now
 *
 * @retval Duration
 *
 */
long
PwrEventActivityGetMaxDuration(struct timespec *now)
{
	Activity *a = _activity_obtain_max(now);

	if (!a)
	{
		return 0;
	}

	struct timespec diff;

	ClockDiff(&diff, &a->end_time, now);

	return ClockGetMs(&diff);
}

/*
 * @brief Stop any new activity.
 * Called when the system is about to suspend.
 *
 * @param now
 */
bool
PwrEventFreezeActivities(struct timespec *now)
{
	pthread_mutex_lock(&activity_mutex);

	if (_activity_obtain_min_unlocked(now) != NULL)
	{
		pthread_mutex_unlock(&activity_mutex);
		return false;
	}

	gFrozen = true;
	return true;
}

/**
 * @brief Again allow creation og new activities
 * Called when the system resumes
 */
void
PwrEventThawActivities(void)
{
	gFrozen = false;
	pthread_mutex_unlock(&activity_mutex);
}

INIT_FUNC(INIT_FUNC_EARLY, _activity_init);

/* @} END OF PowerActivities */
