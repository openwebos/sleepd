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
 * @file config.c
 *
 * @brief Read configuration from powerd.conf file and intialize the global config structure "gPowerConfig"
 *
 */

/**
 * Sleepd configuration.
 */

#include <sys/stat.h>
#include <sys/types.h>
#include <glib.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#include "init.h"
#include "defines.h"

/**
 * default sleepd config
 */
SleepConfiguration gSleepConfig =
{
    .wait_idle_ms = 500,
    .wait_idle_granularity_ms = 100,

    .wait_suspend_response_ms = 30000,
    .wait_prepare_suspend_ms = 5000,
    .after_resume_idle_ms = 1000,
    .wait_alarms_s	= 5,

    .suspend_with_charger = 0,
    .disable_rtc_alarms = 0,
    /* Visual indicator: Turn on led when screen turns off, turn off led before we go to suspend. */
    .visual_leds_suspend = 0,

    .is_running = 1,
    .debug = 0,

    .preference_dir = WEBOS_INSTALL_LOCALSTATEDIR "/preferences/com.palm.sleep",

    .fasthalt = false
};

#define CONFIG_GET_INT(keyfile,cat,name,var)                    \
do {                                                            \
    int intVal;                                                 \
    GError *gerror = NULL;                                      \
    intVal = g_key_file_get_integer(keyfile,cat,name,&gerror);  \
    if (!gerror) {                                              \
        var = intVal;                                           \
        g_debug(#var " = %d", intVal);                          \
    }                                                           \
    else { g_error_free(gerror); }                              \
} while (0)

#define CONFIG_GET_BOOL(keyfile,cat,name,var)                   \
do {                                                            \
    bool boolVal;                                               \
    GError *gerror = NULL;                                      \
    boolVal = g_key_file_get_boolean(keyfile,cat,name,&gerror); \
    if (!gerror) {                                              \
        var = boolVal;                                          \
        g_debug(#var " = %s",                                   \
                  boolVal ? "true" : "false");                  \
    }                                                           \
    else { g_error_free(gerror); }                              \
} while (0)

static int
config_init(void)
{
    int ret;

    ret = mkdir(gSleepConfig.preference_dir, 0755);
    if (ret < 0 && errno != EEXIST)
    {
        perror("Sleepd: Could not mkdir the preferences dir.");
    }

    GKeyFile *config_file = NULL;
    bool retVal;

    config_file = g_key_file_new();
    if (!config_file)
    {
        return -1;
    }

// Load default values from configuration file
    char *config_path =
        g_build_filename(WEBOS_INSTALL_DEFAULTCONFDIR, "sleepd.conf", NULL);
    retVal = g_key_file_load_from_file(config_file, config_path,
        G_KEY_FILE_NONE, NULL);
    if (!retVal)
    {
        g_warning("%s cannot load config file from %s",
                __FUNCTION__, config_path);
        goto end;
    }

    /// [general]
    CONFIG_GET_INT(config_file, "general", "debug", gSleepConfig.debug);


    /// [suspend]
    CONFIG_GET_INT(config_file, "suspend", "wait_idle_ms",
                    gSleepConfig.wait_idle_ms);
    CONFIG_GET_INT(config_file, "suspend", "after_resume_idle_ms",
                    gSleepConfig.after_resume_idle_ms);
    CONFIG_GET_INT(config_file, "suspend", "wait_suspend_response_ms",
                    gSleepConfig.wait_suspend_response_ms);
    CONFIG_GET_INT(config_file, "suspend", "wait_prepare_suspend_ms",
                    gSleepConfig.wait_prepare_suspend_ms);
    CONFIG_GET_BOOL(config_file, "suspend", "wait_alarms_ms",
                    gSleepConfig.wait_alarms_s);

    CONFIG_GET_BOOL(config_file, "suspend", "suspend_with_charger",
                    gSleepConfig.suspend_with_charger);

    CONFIG_GET_BOOL(config_file, "suspend", "disable_rtc_alarms",
					gSleepConfig.disable_rtc_alarms);

    CONFIG_GET_BOOL(config_file, "suspend", "visual_leds_suspend",
    				gSleepConfig.visual_leds_suspend);

    CONFIG_GET_BOOL(config_file, "suspend", "fasthalt",
    				gSleepConfig.fasthalt);

end:
    g_free(config_path);
    if (config_file) g_key_file_free(config_file);
    return 0;
}


INIT_FUNC(INIT_FUNC_FIRST, config_init);
