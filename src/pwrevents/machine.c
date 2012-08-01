/* @@@LICENSE
*
*      Copyright (c) 2011-2012 Hewlett-Packard Development Company, L.P.
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
* @file machine.c
*
* @brief This file contains functions used to manage machines registered with sleepd. 
*
*/


#include <unistd.h>
#include <stdlib.h>
#include <glib.h>
#include <string.h>
#include <stdbool.h>
#include <syslog.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <cjson/json.h>
#include <lunaservice.h>

#include "main.h"
#include "suspend.h"

#include "sysfs.h"

#include "machine.h"
#include "debug.h"
#include "logging.h"
#include "suspend.h"
#include "config.h"

static char *machineName = NULL;

bool suspend_with_charger = false;
bool visual_leds_suspend = false;
int fasthalt=0;

bool chargerIsConnected = false, usbconn = false, dockconn = false;

/**
 * Obtains the machine specific release name.
 * For e.g. If uname -r returns "2.6.22.1-11-palm-joplin-2430",
 * then GetMachineName should return "palm-joplin-2430"
 */

char *
MachineGetName(void)
{
    if (machineName)
    {
        return machineName;
    }

    struct utsname un;
    int ret;

    ret = uname(&un);
    if (ret < 0)
    {
        goto unknown;
    }

    // find first string after '-' that is not a digit
    char *machine_id = un.release;
    do {
        machine_id = strchr(machine_id, '-');
        if (!machine_id)
        {
            goto unknown;
        }
        machine_id++; // skip the '-'
    } while (g_ascii_isdigit(*machine_id) == TRUE);

    if (strlen(machine_id) == 0)
    {
        goto unknown;
    }

    machineName = g_strdup(machine_id);
    return machineName;
unknown:
    machineName = "unknown";
    return machineName;
}

bool
MachineCanSleep(void)
{
    int ret = access("/usr/sbin/suspend_action", R_OK | X_OK);
    bool suspend_action_present = (ret == 0);

    return suspend_action_present &&
          (!chargerIsConnected || suspend_with_charger);
}

const char *
MachineCantSleepReason(void)
{
    static char reason[512];

    int ret = access("/usr/sbin/suspend_action", R_OK | X_OK);
    bool suspend_action_present = (ret == 0);

    snprintf(reason, 512, "%s %s",
            !suspend_action_present ?
                    "suspend_action_not_present," : "",
            chargerIsConnected ? "charger_present" : "");

    return reason;
}


void MachineSleep(void)
{
	bool success;
	switchoffDisplay();

	if (gSleepConfig.visual_leds_suspend)
	{
		SysfsWriteString(
			"/sys/class/leds/core_navi_center/brightness", "0");
	}

	nyx_system_suspend(GetNyxSystemDevice(),&success);

	if (gSleepConfig.visual_leds_suspend)
	{
		SysfsWriteString(
			"/sys/class/leds/core_navi_center/brightness", "15");
	}
}

void
MachineForceShutdown(const char *reason)
{
    g_critical("Pwrevents shutting down system because of %s\n", reason);
    write_console("Pwrevents shutting down system because of %s\n", reason);

    if(gSleepConfig.fasthalt)
    	nyx_system_shutdown(GetNyxSystemDevice(),NYX_SYSTEM_EMERG_SHUTDOWN);
    else
    	nyx_system_shutdown(GetNyxSystemDevice(),NYX_SYSTEM_NORMAL_SHUTDOWN);
}

void
MachineForceReboot(const char *reason)
{
    g_critical("Pwrevents rebooting system because of %s\n", reason);
    write_console("Pwrevents rebooting system because of %s\n", reason);

    if(gSleepConfig.fasthalt)
    	nyx_system_reboot(GetNyxSystemDevice(),NYX_SYSTEM_EMERG_SHUTDOWN);
    else
    	nyx_system_reboot(GetNyxSystemDevice(),NYX_SYSTEM_NORMAL_SHUTDOWN);
}


void
TurnBypassOn(void)
{
    // 0 > level means on.
    SysfsWriteString("/sys/user_hw/pins/power/chg_bypass/level", "0");
}

void
TurnBypassOff(void)
{
    // 1 > level means off.
    SysfsWriteString("/sys/user_hw/pins/power/chg_bypass/level", "1");
}

int
MachineGetToken(const char *token_name, char *buf, int len)
{
    char *file_name = g_build_filename("/dev/tokens", token_name, NULL);
    int fd = open(file_name, O_RDONLY);
    if (fd < 0) return -1;

    int ret;
    do {
        ret = read(fd, buf, len);
    } while (ret < 0 && errno == -EINTR);

    buf[ret] = '\0';

    close(fd);
    g_free(file_name);
    
    if (ret < 0) return -1;
    return 0;
}

bool ChargerStatus(LSHandle *sh,
                   LSMessage *message, void *user_data)
{
	struct json_object *object;

	object = json_tokener_parse(LSMessageGetPayload(message));
    if (NULL == object) goto out;

    if(json_object_object_get(object, "Charging")) {
    	usbconn = json_object_get_boolean(json_object_object_get(object, "USBConnected"));
    	dockconn = json_object_get_boolean(json_object_object_get(object, "DockPower"));
    	g_debug("Charger connected/disconnected, usb : %s, dock : %s",usbconn?"true":"false",dockconn?"true":"false");
        chargerIsConnected = usbconn | dockconn;
    }
out:
    return true;
}
