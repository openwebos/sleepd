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

#ifndef __LOGGING_H__
#define __LOGGING_H__
#include <sys/syslog.h>

typedef enum {
    LOGSyslog = 0,
    LOGGlibLog,
    LOG_NUM_HANDLERS
} LOGHandler;

void LOGSetHandler(LOGHandler h);
void LOGSetLevel(int level);
void LOGInit();

int get_glib_from_syslog_level(int syslog_level);
void write_console(char *format, ...);


#define SLEEPDLOG(syslog_level, ...) \
do {                         \
    g_log(G_LOG_DOMAIN, get_glib_from_syslog_level(syslog_level), LOG_DOMAIN __VA_ARGS__);      \
} while (0)

#endif
