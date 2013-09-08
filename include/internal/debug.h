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
#include <assert.h>
#include <glib.h>
#include <string.h>
#include <stdbool.h>

#ifndef _LOGDEBUG_H_
#define _LOGDEBUG_H_


/* define this to assert when BUG() macro is called */
#define ASSERT_ON_BUG

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "Sleepd"

// TODO use common error codes
#define FATAL_ERROR -1
#define ERROR       1
#define PWREVENTS_ERROR_TIMEOUT 355

/* Set the current log level */
void SetLogLevel(GLogLevelFlags newLogLevel);

/* Set the destination of the log */
void SetUseSyslog(int useSyslog);

/* Return the current log level */
GLogLevelFlags GetLogLevel();

int DebugInit(void);

void _good_assert(const char *cond_str, bool cond);

#ifdef ASSERT_ON_BUG
#define BUG() {                     \
    *( (int*) NULL) = 0;            \
}
#else
#define BUG() {}
#endif

#define _assert(cond)               \
do {                                \
    _good_assert(#cond, cond);      \
} while (0)

#define g_critical_once(...)        \
do {                                \
    static int seen = 0;            \
    if (!seen)                      \
    {                               \
        seen = 1;                   \
        g_critical(__VA_ARGS__);    \
    }                               \
} while(0)

#define g_info(...)                 \
    g_log (G_LOG_DOMAIN,            \
           G_LOG_LEVEL_INFO,        \
           __VA_ARGS__)

#define g_perror(...)           \
do {                                   \
    char buf[512];                     \
    buf[0] = 0;                        \
    g_critical(__VA_ARGS__);           \
    strerror_r(errno, buf, 512);       \
    g_error(buf); \
} while(0)

#define MESSAGE(...)                   \
do {                                \
    g_message(__VA_ARGS__);         \
} while (0)

#define TRACE(...)                  \
do {                                \
    g_debug(__VA_ARGS__);         \
} while(0)

#define TRACE_ERROR(...)  \
do {                      \
    g_error(__VA_ARGS__);        \
} while(0)

#define TRACE_PERROR g_perror

#define iferr(cond, text, goto_label) \
do {                                    \
    if (cond)                           \
    {                                   \
        TRACE_ERROR(text);              \
        goto goto_label;                \
    }                                   \
} while (0);

void print_trace(void);

#endif
