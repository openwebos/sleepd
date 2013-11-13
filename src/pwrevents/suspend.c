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
 * @file suspend.c
 *
 * @brief Suspend/Resume logic to conserve battery when device is idle.
 *
 */

/***
 * PwrEvent State Machine.
 */

#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>

#include <syslog.h>

#include "suspend.h"
#include "clock.h"
#include "wait.h"
#include "machine.h"
#include "debug.h"
#include "main.h"
#include "timersource.h"
#include "activity.h"
#include "logging.h"
#include "client.h"
#include "timesaver.h"
#include "sysfs.h"
#include "init.h"
#include "timeout_alarm.h"
#include "reference_time.h"
#include "config.h"
#include "sawmill_logger.h"
#include "nyx/nyx_client.h"

#include <cjson/json.h>
#include <luna-service2/lunaservice.h>

#define LOG_DOMAIN "PWREVENT-SUSPEND: "

#define kPowerBatteryCheckReasonSysfs "/sys/power/batterycheck_wakeup"
#define kPowerWakeupSourcesSysfs      "/sys/power/wakeup_event_list"

#define MIN_IDLE_SEC 5

/*
 * @brief Power States
 */

enum
{
    kPowerStateOn,
    kPowerStateOnIdle,
    kPowerStateSuspendRequest,
    kPowerStatePrepareSuspend,
    kPowerStateSleep,
    kPowerStateKernelResume,
    kPowerStateActivityResume,
    kPowerStateAbortSuspend,
    kPowerStateLast
};
typedef int PowerState;

enum
{
    kResumeTypeKernel,
    kResumeTypeActivity,
    kResumeAbortSuspend
};

static char *resume_type_descriptions[] =
{
	"kernel",
	"pwrevent_activity",
	"pwrevent_non_idle",
	"abort_suspend",
};

// A PowerStateProc processes the current state and returns the next state
typedef PowerState(*PowerStateProc)(void);

typedef struct
{
	PowerState     state; // currently unused
	PowerStateProc function;
} PowerStateNode;

/*
 * @brief State Functions
 */

static PowerState StateOn(void);
static PowerState StateOnIdle(void);
static PowerState StateSuspendRequest(void);
static PowerState StatePrepareSuspend(void);
static PowerState StateSleep(void);
static PowerState StateKernelResume(void);
static PowerState StateActivityResume(void);
static PowerState StateAbortSuspend(void);

/**
 * @defgroup SuspendLogic   Suspend/Resume State Machine
 * @ingroup PowerEvents
 * @brief Suspend/Resume state machine:
 *
 * A separate thread "SuspendThread" handles the device suspend/resume logic, and maintains
 * a state machine with the following states:
 *
 * 1. On: This is the first state , in which the device stays as long as display is on, or
 * some activity is active or the device has been awake for less than after_resume_idle_ms.
 *
 * 2. OnIdle: The device goes into this state from "On" state, if the IdleCheck thread thinks that
 * the device can now suspend. However if the device is connected to charger and the "suspend_with_charger"
 * option is "false", the device will again go back to the "On" state, else the device will go into the next
 * state i.e the "SuspendRequest" state.
 *
 * 3. SuspendRequest: In this state the device will broadcast the "SuspendRequest" signal, to which all the
 * registered clients are supposed to respond back with an ACK / NACK. The device will stay in this state for
 * a max of 30 sec waiting for all responses. If all clients respond back with an ACK or it timesout, it will go
 * to the next state i.e "PrepareSuspend" state. However if any client responds back with a NACK it goes back
 * to the "On" state again.
 *
 * 4. PrepareSuspend: In this state, the device will broadcast the "PrepareSuspend" signal, with a max wait
 * of 5 sec for all responses. If all clients respond back with an ACK or it timesout, it will go
 * to the next state i.e "Sleep" state. However if any client responds back with NACK, it goes to the
 * "AbortSuspend" state.
 *
 * 5. Sleep: In this state it will first send the "Suspended" signal to everybody. If any activity is active
 * at this point it will go resume by going to the "ActivityResume" state, else it will set the next state to
 * "KernelResume" and let the machine sleep.
 *
 * 6. KernelResume: This is the default state in which the system will be after waking up from sleep. It will
 * broadcast the "Resume" signal , schedule the next IdleCheck sequence and go to the "On" state.
 *
 * 7. ActivityResume: It will broadcast the "Resume" signal schedule the next IdleCheck sequence and go back
 * to "On" state.
 *
 * 8. AbortSuspend: It will broadcast the "Resume" signal and go back to the "On" state.
 */

/**
 * @addtogroup SuspendLogic
 * @{
 */


// Mapping from state to function handling state.
static const PowerStateNode kStateMachine[kPowerStateLast] =
{
	[kPowerStateOn]             = { kPowerStateOn,               StateOn },
	[kPowerStateOnIdle]         = { kPowerStateOnIdle,           StateOnIdle },
	[kPowerStateSuspendRequest] = { kPowerStateSuspendRequest,   StateSuspendRequest },
	[kPowerStatePrepareSuspend] = { kPowerStatePrepareSuspend,   StatePrepareSuspend },
	[kPowerStateSleep]          = { kPowerStateSleep,            StateSleep },
	[kPowerStateKernelResume]   = { kPowerStateKernelResume,     StateKernelResume },
	[kPowerStateActivityResume] = { kPowerStateActivityResume,   StateActivityResume },
	[kPowerStateAbortSuspend]   = { kPowerStateAbortSuspend,     StateAbortSuspend }
};

// current state
static PowerStateNode gCurrentStateNode;
//static PowerState gCurrentState;

WaitObj gWaitResumeMessage;

PowerEvent gSuspendEvent = kPowerEventNone;

GTimerSource *idle_scheduler = NULL;

GMainLoop *suspend_loop = NULL;

WaitObj gWaitSuspendResponse;
WaitObj gWaitPrepareSuspend;

struct timespec sTimeOnStartSuspend;
struct timespec sTimeOnSuspended;
struct timespec sTimeOnWake;

struct timespec sSuspendRTC;
struct timespec sWakeRTC;

void SuspendIPCInit(void);
int SendSuspendRequest(const char *message);
int SendPrepareSuspend(const char *message);
int SendResume(int resumetype, char *message);
int SendSuspended(const char *message);

void
StateLoopShutdown(void)
{
	WaitObjectSignal(&gWaitSuspendResponse);
	WaitObjectSignal(&gWaitPrepareSuspend);
}

/**
 * @brief Schedule the IdleCheck thread after interval_ms from fromPoll
 */

void
ScheduleIdleCheck(int interval_ms, bool fromPoll)
{
	if (idle_scheduler)
	{
		g_timer_source_set_interval(idle_scheduler, interval_ms, fromPoll);
	}
	else
	{
		SLEEPDLOG_DEBUG("idle_scheduler not yet initialized");
	}
}

static nyx_device_handle_t nyxDev = NULL;

/**
 * @brief Get display status using NYX interface.
 */
static bool
IsDisplayOn(void)
{
	nyx_led_controller_state_t state = NYX_LED_CONTROLLER_STATE_UNKNOWN;

	if (nyxDev)
	{
		nyx_led_controller_get_state(nyxDev, NYX_LED_CONTROLLER_LCD, &state);
	}
	else
	{
		state = NYX_LED_CONTROLLER_STATE_ON;
	}

	return (state == NYX_LED_CONTROLLER_STATE_ON);
}

/**
 * @brief Turn off display using NYX interface.
 */
void
switchoffDisplay(void)
{
	if (nyxDev)
	{
		nyx_led_controller_effect_t effect;
		effect.required.effect = NYX_LED_CONTROLLER_EFFECT_LED_SET;
		effect.required.led = NYX_LED_CONTROLLER_LCD;
		effect.backlight.callback = NULL;
		effect.backlight.brightness_lcd = -1;
		nyx_led_controller_execute_effect(nyxDev, effect);
	}

	return;
}


/**
 * @brief Thread that's scheduled periodically to check if the system has been idle for
 * specified time, to trigger the next state in the state machine.
 */

gboolean
IdleCheck(gpointer ctx)
{
	bool suspend_active;
	bool activity_idle;

	struct timespec now;
	int next_idle_ms = 0;

	if (!IsDisplayOn())
	{

		ClockGetTime(&now);

		/*
		 * Enforce that the minimum time awake must be at least
		 * after_resume_idle_ms.
		 */
		struct timespec last_wake;
		last_wake.tv_sec = sTimeOnWake.tv_sec;
		last_wake.tv_nsec = sTimeOnWake.tv_nsec;

		ClockAccumMs(&last_wake, gSleepConfig.after_resume_idle_ms);

		if (!ClockTimeIsGreater(&last_wake, &now))
		{
			/*
			 * Do not sleep if any activity is still active
			 */

			activity_idle = PwrEventActivityCanSleep(&now);

			if (!activity_idle)
			{
				SLEEPDLOG_DEBUG("Can't sleep because an activity is active: ");
			}

			if (PwrEventActivityCount(&sTimeOnWake))
			{
				SLEEPDLOG_DEBUG("Activities since wake: ");
				PwrEventActivityPrintFrom(&sTimeOnWake);
			}

			PwrEventActivityRemoveExpired(&now);

			{
				time_t expiry = 0;
				gchar *app_id = NULL;
				gchar *key = NULL;

				if (timeout_get_next_wakeup(&expiry, &app_id, &key))
				{
					g_free(app_id);
					g_free(key);
					int next_wake = expiry - reference_time();

					if (next_wake >= 0 && next_wake <= gSleepConfig.wait_alarms_s)
					{
						SLEEPDLOG_DEBUG("Not going to sleep because an alarm is about to fire in %d sec\n",
						                next_wake);
						goto resched;
					}
				}

			}

			/*
			 * Wait for LunaSysMgr to deposit suspend_active token
			 * to signify that the system is completely booted and ready for
			 * suspend activity.
			 */

			suspend_active = (access("/tmp/suspend_active", R_OK) == 0);

			if (suspend_active && activity_idle)
			{
				TriggerSuspend("device is idle.", kPowerEventIdleEvent);
			}
		}
		else
		{
			struct timespec diff;
			ClockDiff(&diff, &last_wake, &now);
			next_idle_ms = ClockGetMs(&diff);
		}
	}

resched:
	{
		long wait_idle_ms = gSleepConfig.wait_idle_ms;
		long max_duration_ms = PwrEventActivityGetMaxDuration(&now);

		if (max_duration_ms > wait_idle_ms)
		{
			wait_idle_ms = max_duration_ms;
		}

		if (next_idle_ms > wait_idle_ms)
		{
			wait_idle_ms = next_idle_ms;
		}

		ScheduleIdleCheck(wait_idle_ms, true);
	}
	return TRUE;
}

static gboolean
SuspendStateUpdate(PowerEvent power_event)
{
	gSuspendEvent = power_event;
	PowerState next_state = kPowerStateLast;

	do
	{
		next_state = gCurrentStateNode.function();

		if (next_state != kPowerStateLast)
		{
			gCurrentStateNode = kStateMachine[next_state];
		}

	}
	while (next_state != kPowerStateLast);

	return FALSE;
}

/**
 * @brief Suspend state machine is run in this thread.
 *
 * @param  ctx
 *
 * @retval
 */
void *
SuspendThread(void *ctx)
{
	GMainContext *context;
	context = g_main_context_new();

	suspend_loop = g_main_loop_new(context, FALSE);
	g_main_context_unref(context);

	idle_scheduler = g_timer_source_new(
	                     gSleepConfig.wait_idle_ms, gSleepConfig.wait_idle_granularity_ms);

	g_source_set_callback((GSource *)idle_scheduler,
	                      IdleCheck, NULL, NULL);
	g_source_attach((GSource *)idle_scheduler,
	                g_main_loop_get_context(suspend_loop));
	g_source_unref((GSource *)idle_scheduler);

	g_main_loop_run(suspend_loop);
	g_main_loop_unref(suspend_loop);

	return NULL;
}

/**
 * @brief This is the first state , in which the device stays as long as display is on, or
 * some activity is active or the device has been awake for less than after_resume_idle_ms.
 *
 * @retval PowerState Next state
 */

static PowerState
StateOn(void)
{
	PowerState next_state;

	switch (gSuspendEvent)
	{
		case kPowerEventForceSuspend:
			next_state = kPowerStateSuspendRequest;
			break;

		case kPowerEventIdleEvent:
			next_state = kPowerStateOnIdle;
			break;

		case kPowerEventNone:
		default:
			next_state = kPowerStateLast;
			break;
	}

	gSuspendEvent = kPowerEventNone;

	return next_state;
}

/**
 * @brief The device goes into this state from "On" state, if the IdleCheck thread thinks that
 * the device can now suspend. However if the device is connected to charger and the "suspend_with_charger"
 * option is "false", the device will again go back to the "On" state, else the device will go into the next
 * state i.e the "SuspendRequest" state.
 *
 * @retval PowerState Next state
 */

static PowerState
StateOnIdle(void)
{
	if (!MachineCanSleep())
	{
		return kPowerStateOn;
	}

	return kPowerStateSuspendRequest;
}

#define START_LOG_COUNT 8
#define MAX_LOG_COUNT_INCREASE_RATE 512

/**
 * @brief In this state the device will broadcast the "SuspendRequest" signal, to which all the
 * registered clients are supposed to respond back with an ACK / NACK. The device will stay in this state for
 * a max of 30 sec waiting for all responses. If all clients respond back with an ACK or it timesout, it will go
 * to the next state i.e "PrepareSuspend" state. However if any client responds back with a NACK it goes back
 * to the "On" state again.
 *
 * @retval PowerState Next state.
 */

static PowerState
StateSuspendRequest(void)
{
	int timeout = 0;
	static int successive_ons = 0;
	static int log_count = START_LOG_COUNT;
	PowerState ret;

	ClockGetTime(&sTimeOnStartSuspend);

	WaitObjectLock(&gWaitSuspendResponse);

	PwrEventVoteInit();

	SendSuspendRequest("");

	// send msg to ask for permission to sleep
	SLEEPDLOG_DEBUG("Sent \"suspend request\", waiting up to %dms",
	                gSleepConfig.wait_suspend_response_ms);

	if (!PwrEventClientsApproveSuspendRequest())
	{
		// wait for the message to arrive
		timeout = WaitObjectWait(&gWaitSuspendResponse,
		                         gSleepConfig.wait_suspend_response_ms);
	}

	WaitObjectUnlock(&gWaitSuspendResponse);

	PwrEventClientTablePrint(G_LOG_LEVEL_DEBUG);

	if (timeout)
	{
		gchar *silent_clients = PwrEventGetSuspendRequestNORSPList();
		SLEEPDLOG_DEBUG("We timed-out waiting for daemons (%s) to acknowledge SuspendRequest.",
		                silent_clients);
		g_free(silent_clients);
		ret = kPowerStatePrepareSuspend;
	}
	else if (PwrEventClientsApproveSuspendRequest())
	{
		PMLOG_TRACE("Suspend response: go to prepare_suspend");
		ret = kPowerStatePrepareSuspend;
	}
	else
	{
		PMLOG_TRACE("Suspend response: stay awake");
		ret = kPowerStateOn;
	}

	if (ret == kPowerStateOn)
	{
		successive_ons++;

		if (successive_ons >= log_count)
		{
			SLEEPDLOG_DEBUG("%d successive votes to NACK SuspendRequest since previous suspend",
			                successive_ons);
			PwrEventClientTablePrint(G_LOG_LEVEL_WARNING);

			if (log_count >= MAX_LOG_COUNT_INCREASE_RATE)
			{
				log_count += MAX_LOG_COUNT_INCREASE_RATE;
			}
			else
			{
				log_count *= 2;
			}

			SLEEPDLOG_DEBUG("SuspendRequest - next count before logging is %d", log_count);
		}
	}
	else
	{
		// reset the exponential counter
		successive_ons = 0;
		log_count = START_LOG_COUNT;
	}

	return ret;
}

/**
 * @brief In this state, the device will broadcast the "PrepareSuspend" signal, with a max wait of 5 sec
 * for all responses. If all clients respond back with an ACK or it timesout, it will go to the next state
 * i.e "Sleep" state. However if any client responds back with NACK, it goes to the "AbortSuspend" state.
 *
 * @retval PowerState Next state.
 */

static PowerState
StatePrepareSuspend(void)
{
	int timeout = 0;
	static int successive_ons = 0;
	static int log_count = START_LOG_COUNT;

	WaitObjectLock(&gWaitPrepareSuspend);

	// send suspend request to all power-aware daemons.
	SendPrepareSuspend("");

	PMLOG_TRACE("Sent \"prepare suspend\", waiting up to %dms",
	            gSleepConfig.wait_prepare_suspend_ms);

	if (!PwrEventClientsApprovePrepareSuspend())
	{

		timeout = WaitObjectWait(&gWaitPrepareSuspend,
		                         gSleepConfig.wait_prepare_suspend_ms);
	}

	WaitObjectUnlock(&gWaitPrepareSuspend);

	PwrEventClientTablePrint(G_LOG_LEVEL_DEBUG);

	if (timeout)
	{
		gchar *silent_clients = PwrEventGetPrepareSuspendNORSPList();
		SLEEPDLOG_DEBUG("We timed-out waiting for daemons (%s) to acknowledge PrepareSuspend.",
		                silent_clients);
		gchar *clients = PwrEventGetClientTable();

		SLEEPDLOG_DEBUG("== NORSP clients ==\n %s\n == client table ==\n %s",
		                silent_clients, clients);
		g_free(clients);
		g_free(silent_clients);

		// reset the exponential counter
		successive_ons = 0;
		log_count = START_LOG_COUNT;
		return kPowerStateSleep;
	}
	else if (PwrEventClientsApprovePrepareSuspend())
	{
		PMLOG_TRACE("Clients all approved prepare_suspend");
		// reset the exponential counter
		successive_ons = 0;
		log_count = START_LOG_COUNT;
		return kPowerStateSleep;
	}
	else
	{
		// if any daemons nacked, quit suspend...
		PMLOG_TRACE("Some daemon nacked prepare_suspend: stay awake");
		successive_ons++;

		if (successive_ons >= log_count)
		{
			SLEEPDLOG_DEBUG("%d successive votes to NACK PrepareSuspend since previous suspend",
			                successive_ons);
			PwrEventClientTablePrint(G_LOG_LEVEL_WARNING);

			if (log_count >= MAX_LOG_COUNT_INCREASE_RATE)
			{
				log_count += MAX_LOG_COUNT_INCREASE_RATE;
			}
			else
			{
				log_count *= 2;
			}

			SLEEPDLOG_DEBUG("PrepareSuspend - next count before logging is %d", log_count);
		}

		return kPowerStateAbortSuspend;
	}
}

/**
 * @brief Instrument how much time it took to sleep.
 */
void
InstrumentOnSleep(void)
{
	struct timespec diff;
	struct timespec diffAwake;

	ClockGetTime(&sTimeOnSuspended);
	get_time_now(&sSuspendRTC);

	ClockDiff(&diff, &sTimeOnSuspended, &sTimeOnStartSuspend);

	ClockDiff(&diffAwake, &sTimeOnSuspended, &sTimeOnWake);

	GString *str = g_string_new("");

	g_string_append_printf(str, "PWREVENT-SLEEP after ");
	ClockStr(str, &diffAwake);
	g_string_append(str, "... decision took ");
	ClockStr(str, &diff);

	SLEEPDLOG_DEBUG(" Clock String : %s", str->str);

	g_string_free(str, TRUE);

	/* Rate-limited print NACK sources. */
	PwrEventClientPrintNACKRateLimited();

	sawmill_logger_record_sleep(diffAwake);
}

/**
 * @brief Instrument how much time it took to wake back up.
 */
void
InstrumentOnWake(int resumeType)
{
	ClockGetTime(&sTimeOnWake);
	get_time_now(&sWakeRTC);

	struct timespec diffAsleep;
	ClockDiff(&diffAsleep, &sWakeRTC, &sSuspendRTC);

	struct tm tm;
	gmtime_r(&(diffAsleep.tv_sec), &tm);

	if (tm.tm_year >= 70)
	{
		tm.tm_year -= 70;    // EPOCH returned by time() starts at 1970
	}

	GString *str = g_string_new("PWREVENT-WOKE after ");
	g_string_append_printf(str, "%lds : ", diffAsleep.tv_sec);

	if (tm.tm_year > 0)
	{
		g_string_append_printf(str, "%d years, ", tm.tm_year);
	}

	g_string_append_printf(str, "%d days, %dh-%dm-%ds\n", tm.tm_yday,
	                       tm.tm_hour, tm.tm_min, tm.tm_sec);

	SLEEPDLOG_DEBUG("%s (%s)", str->str, resume_type_descriptions[resumeType]);

	g_string_free(str, TRUE);

	sawmill_logger_record_wake(diffAsleep);
}


/**
 * @brief In this state it will first send the "Suspended" signal to everybody. If any activity is active
 * at this point it will go resume by going to the "ActivityResume" state, else it will set the next state
 * to "KernelResume" and let the machine sleep.
 *
 * @retval PowerState Next state.
 */

static PowerState
StateSleep(void)
{
	int nextState =
	    kPowerStateKernelResume; // assume a normal sleep ended by some kernel event

	PMLOG_TRACE("State Sleep, We will try to go to sleep now");

	SendSuspended("attempting to suspend (We are trying to sleep)");

	{
		time_t expiry = 0;
		gchar *app_id = NULL;
		gchar *key = NULL;

		if (timeout_get_next_wakeup(&expiry, &app_id, &key))
		{
			SLEEPDLOG_DEBUG("waking in %ld seconds for %s", expiry - reference_time(), key);
		}

		g_free(app_id);
		g_free(key);
	}

	InstrumentOnSleep();

	// save the current time to disk in case battery is pulled.
	timesaver_save();

	// if any activities were started, abort suspend.
	if (gSuspendEvent != kPowerEventForceSuspend &&
	        !PwrEventFreezeActivities(&sTimeOnSuspended))
	{
		SLEEPDLOG_DEBUG("aborting sleep because of current activity");
		PwrEventActivityPrintFrom(&sTimeOnSuspended);
		nextState = kPowerStateActivityResume;
	}

	else
	{
		if (MachineCanSleep())
		{
			if (queue_next_wakeup())
			{
				// let the system sleep now.
				MachineSleep();
			}
			else
			{
				SLEEPDLOG_DEBUG("We couldn't sleep because there can't setup wakup alarm");
				nextState = kPowerStateAbortSuspend;
			}
		}
		else
		{
			SLEEPDLOG_DEBUG("We couldn't sleep because a new gadget_event was received");
			nextState = kPowerStateAbortSuspend;
		}

		// We woke up from sleep.
		PwrEventThawActivities();
	}

	return nextState;
}

/**
 * @brief In this state the "Resume" signal will be broadcasted and the device will go back to the "On" state.
 *
 * @retval PowerState Next state.
 */

static PowerState
StateAbortSuspend(void)
{
	PMLOG_TRACE("State Abort suspend");
	SendResume(kResumeAbortSuspend, "resume (suspend aborted)");

	return kPowerStateOn;
}


/**
 * @brief Broadcast the resume signal when we wake up ( due to kernel sleep or activity)
 *
 * @retval PowerState Next state
 */

static PowerState
_stateResume(int resumeType)
{
	PMLOG_TRACE("We awoke");

	char *resumeDesc = g_strdup_printf("resume (%s)",
	                                   resume_type_descriptions[resumeType]);
	SendResume(resumeType, resumeDesc);
	g_free(resumeDesc);

#ifdef ASSERT_ON_BUG
	WaitObjectSignal(&gWaitSuspendResponse);
#endif

	InstrumentOnWake(resumeType);

	// if we are inactive in 1s, go back to sleep.
	ScheduleIdleCheck(gSleepConfig.after_resume_idle_ms, false);

	return kPowerStateOn;
}

/**
 * @brief This is the default state in which the system will be after waking up from sleep. It will
 * broadcast the "Resume" signal , schedule the next IdleCheck sequence and go to the "On" state.
 *
 * @retval PowerState Next state
 */

static PowerState
StateKernelResume(void)
{
	return _stateResume(kResumeTypeKernel);
}

/**
 * @brief We are in this state if the system did not really sleep but had to prevent the sleep because
 * an activity as active. It does so by broadcasting the "Resume" signal , schedule the next IdleCheck
 * sequence and go to the "On" state.
 *
 * @retval PowerState Next state
 */

static PowerState
StateActivityResume(void)
{
	return _stateResume(kResumeTypeActivity);
}

/**
 * @brief Initialize the Suspend/Resume state machine.
 */

static int
SuspendInit(void)
{
	pthread_t suspend_tid;

	// initialize wake time.
	ClockGetTime(&sTimeOnWake);

	WaitObjectInit(&gWaitSuspendResponse);
	WaitObjectInit(&gWaitPrepareSuspend);

	WaitObjectInit(&gWaitResumeMessage);

	com_palm_suspend_lunabus_init();
	PwrEventClientTableCreate();

	SuspendIPCInit();

	if (gSleepConfig.visual_leds_suspend)
	{
		if (SysfsWriteString("/sys/class/leds/core_navi_center/brightness", "15") < 0)
		{
			SysfsWriteString(
			    "/sys/class/leds/core_navi_left/brightness", "100");
			SysfsWriteString(
			    "/sys/class/leds/core_navi_right/brightness", "100");

		}
	}

	gCurrentStateNode = kStateMachine[kPowerStateOn];

	if (pthread_create(&suspend_tid, NULL, SuspendThread, NULL))
	{
		SLEEPDLOG_CRITICAL(MSGID_PTHREAD_CREATE_FAIL, 0,
		                   "Could not create SuspendThread\n");
		abort();
	}

	int ret = nyx_device_open(NYX_DEVICE_LED_CONTROLLER, "Default", &nyxDev);

	if (ret != NYX_ERROR_NONE)
	{
		SLEEPDLOG_ERROR(MSGID_NYX_DEV_OPEN_FAIL, 0,
		                "Unable to open the nyx device led controller");
	}

	return 0;
}

/**
 * @brief Iterate through the state machine
 */
void
TriggerSuspend(const char *reason, PowerEvent event)
{
	GSource *source = g_idle_source_new();
	g_source_set_callback(source,
	                      (GSourceFunc)SuspendStateUpdate, GINT_TO_POINTER(event), NULL);
	g_source_attach(source, g_main_loop_get_context(suspend_loop));

	g_source_unref(source);
}

INIT_FUNC(INIT_FUNC_END, SuspendInit);

/* @} END OF SuspendLogic */
