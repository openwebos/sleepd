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


#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#include "logging.h"
#include "main.h"

#define LOG_DOMAIN "timesaver: "

#define POWERD_RESTORES_TIME

static char *time_db = NULL;
static char *time_db_tmp = NULL;

/**
 * @brief Save the current time in the file "time_saver" so that it can be used in future.
 *
 * @retval
 */

void
timesaver_save()
{
	if (!time_db)
    {
		time_db = g_build_filename(
				PREFERENCE_DIR, "time_saver", NULL);
		time_db_tmp = g_build_filename(
				PREFERENCE_DIR, "time_saver.tmp", NULL);
    }

    if (NULL == time_db)
    {
        // This can happen if we goto ls_error in main()
        g_warning("%s called with time database name (time_db) uninitialized", __FUNCTION__);
        goto cleanup;
    }

    //  First write the contents to tmp file and then rename to "time_saver" file
    //  to ensure file integrity with power cut or battery pull.

    int file = open(time_db_tmp, O_CREAT | O_WRONLY, S_IRWXU | S_IRGRP | S_IROTH);
    if (!file)
    {
        g_warning("%s: Could not save time to \"%s\"", __FUNCTION__, time_db_tmp);
        goto cleanup;
    }

    struct timespec tp;
    clock_gettime(CLOCK_REALTIME, &tp);

    SLEEPDLOG(LOG_DEBUG, "%s Saving to file %ld", __FUNCTION__, tp.tv_sec);

    char timestamp[16];

    snprintf(timestamp,sizeof(timestamp),"%ld", tp.tv_sec);

    write(file,timestamp,strlen(timestamp));
    fsync(file);
    close(file);

    int ret = rename(time_db_tmp,time_db);
    if (ret)
    {
    	g_warning("%s : Unable to rename %s to %s",__FUNCTION__,time_db_tmp,time_db);
    }
	unlink(time_db_tmp);

cleanup:
    return;
}
