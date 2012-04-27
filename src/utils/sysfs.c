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
 * @file sysfs.c
 *
 * @brief Get or set sysfs entries.
 *
 */

#include <sys/types.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>

#include "logging.h"
#include "debug.h"

#define LOG_DOMAIN "sysfs:"

/**
 * Returns string in pre-allocated buffer.
 */
int
SysfsGetString(const char *path, char *ret_string, size_t maxlen)
{
    GError *gerror = NULL;
    char *contents = NULL;
    gsize len;

    if (!path || !g_file_get_contents(path, &contents, &len, &gerror)) {
        if (gerror) {
            SLEEPDLOG(LOG_CRIT, "%s: %s", __FUNCTION__, gerror->message);
            g_error_free(gerror);
        }
        return -1;
    }

    g_strstrip(contents);
    g_strlcpy(ret_string, contents, maxlen);

    g_free(contents);

    return 0;
}

int 
SysfsGetInt(const char *path, int *ret_data)
{
    GError *gerror = NULL;
    char *contents = NULL;
    char *endptr;
    gsize len;
    long int val;

    if (!path || !g_file_get_contents(path, &contents, &len, &gerror)) {
        if (gerror) {
            SLEEPDLOG(LOG_CRIT, "%s: %s", __FUNCTION__, gerror->message);
            g_error_free(gerror);
        }
        return -1;
    }

    val = strtol(contents, &endptr, 10);
    if (endptr == contents) {
        SLEEPDLOG(LOG_CRIT, "%s: Invalid input in %s.",
            __FUNCTION__, path);
        goto end;
    }

    if (ret_data)
        *ret_data = val;
end:
    g_free(contents);
    return 0;
}

int 
SysfsGetDouble(const char *path, double *ret_data)
{
    GError *gerror = NULL;
    char *contents = NULL;
    char *endptr;
    gsize len;
    float val;

    if (!path || !g_file_get_contents(path, &contents, &len, &gerror)) {
        if (gerror) {
            SLEEPDLOG(LOG_CRIT, "%s: %s", __FUNCTION__, gerror->message);
            g_error_free(gerror);
        }
        return -1;
    }

    val = strtod(contents, &endptr);
    if (endptr == contents) {
        SLEEPDLOG(LOG_CRIT, "%s: Invalid input in %s.",
            __FUNCTION__, path);
        goto end;
    }

    if (ret_data)
        *ret_data = val;
end:
    g_free(contents);
    return 0;
}

/**
 * @returns 0 on success or -1 on error
 */
int
SysfsWriteString(const char *path, const char *string)
{
    int fd;
    ssize_t n;
    
    fd = open(path, O_WRONLY);
    if (fd < 0)
        return -1;

    n = write(fd, string, strlen(string));
    close(fd);
    return n >= 0 ? 0 : -1;
}
