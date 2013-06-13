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
static LSHandle* private_sh = NULL;
static LSPalmService *psh = NULL;

bool ChargerConnected(LSHandle *sh,LSMessage *message, void *user_data); // defined in machine.c
bool ChargerStatus(LSHandle *sh,LSMessage *message, void *user_data); // defined in machine.c

#define LOG_DOMAIN "SLEEPD-INIT: "

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

int
main(int argc, char **argv)
{
    bool retVal;

    // FIXME integrate this into TheOneInit()
    LOGInit();
    LOGSetHandler(LOGSyslog);

    signal(SIGTERM, term_handler);
    signal(SIGINT, term_handler);

    if (!g_thread_supported ()) g_thread_init (NULL);

    mainloop = g_main_loop_new(NULL, FALSE);

    /**
     *  initialize the lunaservice and we want it before all the init
     *  stuff happening.
     */
    LSError lserror;
    LSErrorInit(&lserror);

    retVal = LSRegisterPalmService("com.palm.sleep", &psh, &lserror);
    if (!retVal)
    {
        goto ls_error;
    }

    retVal = LSGmainAttachPalmService(psh, mainloop, &lserror);
    if (!retVal)
    {
        goto ls_error;
    }

    private_sh = LSPalmServiceGetPrivateConnection(psh);

    retVal = LSCall(private_sh,"luna://com.palm.lunabus/signal/addmatch","{\"category\":\"/com/palm/power\","
              "\"method\":\"USBDockStatus\"}", ChargerStatus, NULL, NULL, &lserror);
    if (!retVal) {
		SLEEPDLOG(LOG_CRIT,"Error in registering for luna-signal \"chargerStatus\"");
		goto ls_error;
	}

 	int ret = nyx_device_open(NYX_DEVICE_SYSTEM, "Main", &nyxSystem);
 	if(ret != NYX_ERROR_NONE)
 	{
 		SLEEPDLOG(LOG_CRIT,"Sleepd: Unable to open the nyx device system");
 		abort();
 	}


    TheOneInit();

 	LSCall(private_sh,"luna://com.palm.power/com/palm/power/chargerStatusQuery","{}",ChargerStatus,NULL,NULL,&lserror);

    SLEEPDLOG(LOG_INFO,"Sleepd daemon started\n");

    g_main_loop_run(mainloop);

end:
    g_main_loop_unref(mainloop);

     return 0;
ls_error:
	SLEEPDLOG(LOG_CRIT,"Fatal - Could not initialize sleepd.  Is LunaService Down?. %s",
        lserror.message);
    LSErrorFree(&lserror);
    goto end;
}
