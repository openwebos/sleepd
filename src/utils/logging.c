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
 * @file logging.c
 * 
 * @brief Logging interface.
 *
 * This is for redirecting GLib's logging commands (g_message, g_debug, g_error) 
 * to the appropriate handlers
 */

#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <syslog.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdarg.h>

#include "logging.h"


static int sLogLevel = G_LOG_LEVEL_MESSAGE;

static LOGHandler sHandler = LOGGlibLog;

void
_good_assert(const char * cond_str, bool cond)
{
    if (G_UNLIKELY(!(cond)))
    {
        g_critical("%s", cond_str);
        *(int*)0x00 = 0;
    }
}

int 
get_glib_from_syslog_level(int syslog_level) {
    switch(syslog_level) {
        case LOG_EMERG: /* system is unusable */
            return G_LOG_LEVEL_ERROR;
        case LOG_ALERT: /* action must be taken immediately */
            return G_LOG_LEVEL_CRITICAL;
        case LOG_CRIT: /* critical conditions */
            return G_LOG_LEVEL_CRITICAL;
        case LOG_ERR: /* error conditions */
            return G_LOG_LEVEL_CRITICAL;
        case LOG_WARNING: /* warning conditions */
            return G_LOG_LEVEL_WARNING;
        case LOG_NOTICE: /* normal but significant condition */
            return G_LOG_LEVEL_MESSAGE;
        case LOG_INFO: /* informational */
            return G_LOG_LEVEL_INFO;
        case LOG_DEBUG: /* debug-level messages */
            return G_LOG_LEVEL_DEBUG;
    }
    return G_LOG_LEVEL_INFO;
}

int 
get_syslog_from_glib_level(int glib_level) {
    switch (glib_level & G_LOG_LEVEL_MASK) {
        case G_LOG_LEVEL_ERROR:
            return LOG_CRIT;
        case G_LOG_LEVEL_CRITICAL:
            return LOG_ERR;
        case G_LOG_LEVEL_WARNING:
            return LOG_WARNING;
        case G_LOG_LEVEL_MESSAGE:
            return LOG_NOTICE;
        case G_LOG_LEVEL_INFO:
            return LOG_INFO;
        case G_LOG_LEVEL_DEBUG:
            return LOG_DEBUG;
    }
    return LOG_NOTICE;
}

/** 
 * @brief LOGSetLevel
 * 
 * @param level 
 */
void 
LOGSetLevel(int level)
{
    // asserting this is a log level
    g_assert( (level & G_LOG_LEVEL_MASK) != 0 );
    g_assert( (level | G_LOG_LEVEL_MASK) == G_LOG_LEVEL_MASK );
    sLogLevel = level;
}

/*
int LOGGetLevel()
{
    return sLogLevel;
}
*/

/** 
 * @brief logFilter
 * filter we use to redirect glib's messages
 * 
 * @param log_domain 
 * @param log_level 
 * @param message 
 * @param unused_data 
 */
static void
logFilter(const gchar *log_domain, GLogLevelFlags log_level, const gchar *message, gpointer unused_data)
{
    if (log_level > sLogLevel) return;

    g_assert( sHandler < LOG_NUM_HANDLERS );
    g_assert( sHandler >= 0 );

    switch (sHandler)
    {
        case LOGSyslog:
            syslog(get_syslog_from_glib_level(log_level), "%s", message);
            break;
        case LOGGlibLog:
            g_log_default_handler(log_domain, log_level, message, unused_data);
            break;
        default:
            fprintf(stderr, "%s: no handler %d for log message\n", __func__, sHandler);
            abort();
    }
}

/** 
 * @brief LOGSetHandler
 * 
 * @param h 
 */
void
LOGSetHandler(LOGHandler h)
{
    g_assert( h < LOG_NUM_HANDLERS );
    g_assert( h >= 0 );
    sHandler = h;
}

/** 
 * @brief LOGInit
 */
void 
LOGInit() 
{
   g_log_set_default_handler(logFilter, NULL);
}

void
write_console(char *format, ...)
{
    int fd;
    fd = open("/dev/console", O_RDWR | O_NOCTTY);
    if (fd < 0)
    {
        perror("open"); return;
    }

    va_list args;
    va_start(args, format);

    char buffer[1024];
    int len = vsnprintf(buffer, sizeof(buffer), format, args);

    if (len > 0)
    {
        write(fd, buffer, len);
    }

    va_end(args);

    close(fd);
}


