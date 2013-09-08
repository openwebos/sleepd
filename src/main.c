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
 * @mainpage sleepd
 *
 * @section summary Summary
 *
 * Open webOS component to manage device suspend-resume cycles.
 *
 * @section description Description
 *
 * Sleepd is one of the important daemons started when webOS boots. It is
 * responsible for scheduling platform sleeps as soon as it is idle, so that we
 * see optimum battery performance. To achieve this it keeps polling on the
 * system to see if any of the other services or processes need the platform
 * running, and if not it sends the suspend message to all these components (so
 * that they can finish whatever they are doing ASAP and suspend). Sleepd then
 * lets the kernel know that the platform is ready to sleep. Once an interrupt
 * (such as key press) has woken the platform up, sleepd lets the entire system
 * know that the platform is up and running so that all the activities can
 * resume.
 *
 * Sleepd also manages the RTC alarms on the system by maintaining a SQlite
 * database for all the requested alarms.
 *
 * @section code-organization Code Organization
 *
 * The code for sleepd is organized into two main categories:
 *
 * - A bunch of individual power watcher modules which tie into the service bus
 *   and react to IPC messages passed in and/or which start their own threads
 *   and run separately.
 *
 * - A central module initialization system which ties them all together and
 *   handles all of the bookkeeping to keep them all running and gracefully shut
 *   them down when the sleepd service is asked to stop.
 *
 * Documentation for each of the power management modules is available in the
 * section to the left entitled "Modules".
 *
 * The modules each register themselves with the main initialization code using
 * a macro called {@link INIT_FUNC}.  It uses GCC-specific functionality to
 * run hook registration code when the binary is being loaded into memory.  As a
 * result, all of the code to register these hooks is run before {@link main()}
 * is called.  This creates a very modular code organizational approach in which
 * new power saving modules can be added independently of the main
 * initialization system.
 */

#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <glib.h>
#include <pthread.h>
#include <stdbool.h>
#include <getopt.h>
#include <stdlib.h>
#include <cjson/json.h>
#include <luna-service2/lunaservice.h>

#include "init.h"
#include "logging.h"
#include "main.h"


static GMainLoop *mainloop = NULL;
static LSHandle *private_sh = NULL;
static LSPalmService *psh = NULL;

bool ChargerConnected(LSHandle *sh, LSMessage *message,
                      void *user_data); // defined in machine.c
bool ChargerStatus(LSHandle *sh, LSMessage *message,
                   void *user_data); // defined in machine.c

#define LOG_DOMAIN "SLEEPD-INIT: "

/**
 * Handle process signals asking us to terminate running of our service
 */
void
term_handler(int signal)
{
	g_main_loop_quit(mainloop);
}


GMainContext *
GetMainLoopContext(void)
{
	return g_main_loop_get_context(mainloop);
}

LSHandle *
GetLunaServiceHandle(void)
{
	return private_sh;
}

LSPalmService *
GetPalmService(void)
{
	return psh;
}

static nyx_device_handle_t nyxSystem = NULL;

nyx_device_handle_t
GetNyxSystemDevice(void)
{
	return nyxSystem;
}

/**
 * Main entry point for sleepd - runs the initialization hooks installed at program load time
 *
 * A bit counter-intuitively, this is not the first part of this program which
 * is run.
 *
 * First, everything which uses the {@link INIT_FUNC} macro in init.h are run,
 * which registers a bunch of hooks with the initialization system so that
 * individual modules can be registered without touching the main sleepd
 * initialization code.  Then, once all of those hooks are installed, execution
 * proceeds to this function which actually runs those hooks.
 *
 * - Initializes sleepd.
 * - Attaches as a Luna service under com.palm.sleep.
 * - Attaches to Nyx.
 * - Subscribes to events related to the charger being plugged and unplugged from the com.palm.power service.
 * - Calls {@link TheOneInit()} to finish initialization of the service.
 * - Issues a request to the com.palm.power service to check on the plugged/unplugged status of the charger.
 *
 * @param   argc        Number of command-line arguments.
 * @param   argv        List of command-line arguments.
 *
 * @todo Move the logging initialization functionality into {@link TheOneInit()}.
 */
int
main(int argc, char **argv)
{
	bool retVal;

	// FIXME integrate this into TheOneInit()
	LOGInit();
	LOGSetHandler(LOGSyslog);

	/*
	 * Register a function to be able to gracefully handle termination signals
	 * from the OS or other processes.
	 */
	signal(SIGTERM, term_handler);
	signal(SIGINT, term_handler);

#if !GLIB_CHECK_VERSION(2,32,0)

	if (!g_thread_supported())
	{
		g_thread_init(NULL);
	}

#endif

	mainloop = g_main_loop_new(NULL, FALSE);

	/*
	 *  initialize the lunaservice and we want it before all the init
	 *  stuff happening.
	 */
	LSError lserror;
	LSErrorInit(&lserror);

	/*
	 * Register ourselves as the com.palm.sleep service.
	 */
	retVal = LSRegisterPalmService("com.palm.sleep", &psh, &lserror);

	if (!retVal)
	{
		goto ls_error;
	}

	/*
	 * Attach our main loop to the service so we can process IPC messages addressed to us.
	 */
	retVal = LSGmainAttachPalmService(psh, mainloop, &lserror);

	if (!retVal)
	{
		goto ls_error;
	}

	/*
	 * Get our private bus for our service so we can pass a message to com.palm.power.
	 */
	private_sh = LSPalmServiceGetPrivateConnection(psh);

	/*
	 * Register with com.palm.power for events regarding changes in status
	 * to the plug/unplug state of any chargers which may be attached to our
	 * device.
	 */
	retVal = LSCall(private_sh, "luna://com.palm.lunabus/signal/addmatch",
	                "{\"category\":\"/com/palm/power\","
	                "\"method\":\"USBDockStatus\"}", ChargerStatus, NULL, NULL, &lserror);

	if (!retVal)
	{
		SLEEPDLOG(LOG_CRIT, "Error in registering for luna-signal \"chargerStatus\"");
		goto ls_error;
	}

	/*
	 * Connect to Nyx so we can use it later.
	 */
	int ret = nyx_device_open(NYX_DEVICE_SYSTEM, "Main", &nyxSystem);

	if (ret != NYX_ERROR_NONE)
	{
		SLEEPDLOG(LOG_CRIT, "Sleepd: Unable to open the nyx device system");
		abort();
	}


	/*
	 * Call our main initialization function - this is the function which
	 * is supposed to handle initializing pretty much everything for us.
	 */
	TheOneInit();

	/*
	 * Now that we've got something listening for charger status changes,
	 * request the current state of the charger from com.palm.power.
	 */
	LSCall(private_sh, "luna://com.palm.power/com/palm/power/chargerStatusQuery",
	       "{}", ChargerStatus, NULL, NULL, &lserror);

	SLEEPDLOG(LOG_INFO, "Sleepd daemon started\n");

	g_main_loop_run(mainloop);

end:
	g_main_loop_unref(mainloop);

	return 0;
ls_error:
	SLEEPDLOG(LOG_CRIT,
	          "Fatal - Could not initialize sleepd.  Is LunaService Down?. %s",
	          lserror.message);
	LSErrorFree(&lserror);
	goto end;
}
