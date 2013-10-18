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


#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#include "config.h"
#include "logging.h"
#include "main.h"
#include <ctype.h>

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
		              gSleepConfig.preference_dir, "time_saver", NULL);
		time_db_tmp = g_build_filename(
		                  gSleepConfig.preference_dir, "time_saver.tmp", NULL);
	}

	if (NULL == time_db)
	{
		// This can happen if we goto ls_error in main()
		SLEEPDLOG_DEBUG("called with time database name (time_db) uninitialized");
	}
	else
	{
		//  First write the contents to tmp file and then rename to "time_saver" file
		//  to ensure file integrity with power cut or battery pull.

		int file = open(time_db_tmp, O_CREAT | O_WRONLY, S_IRWXU | S_IRGRP | S_IROTH);

		if (file < 0)
		{
			SLEEPDLOG_WARNING(MSGID_TIME_NOT_SAVED_TO_DB, 0,
			                  "Could not save time to \"%s\"", time_db_tmp);
		}
		else
		{
			struct timespec tp;

			clock_gettime(CLOCK_REALTIME, &tp);

			SLEEPDLOG_DEBUG("Saving to file %ld", tp.tv_sec);

			char timestamp[16];

			snprintf(timestamp, sizeof(timestamp), "%ld", tp.tv_sec);

			write(file, timestamp, strlen(timestamp));

			fsync(file);

			close(file);

			int ret = rename(time_db_tmp, time_db);

			if (ret)
			{
				SLEEPDLOG_DEBUG("Unable to rename %s to %s", time_db_tmp, time_db);
			}

			unlink(time_db_tmp);
		}
	}

	return;
}

bool ConvertJsonTime(const char *time, int *hour, int *minute, int *second)
{
	gchar **time_str;
	int i = 0, j = 0, len;
	time_str = g_strsplit(time, ":", 3);

	if (!time_str)
	{
		return false;
	}

	if ((NULL == time_str[0]) || (NULL == time_str[1]) || (NULL == time_str[2]))
	{
		g_strfreev(time_str);
		return false;
	}

	for (i = 0; i < 3; i++)
	{
		char *timestr;
		timestr = time_str[i];
		len = strlen(time_str[i]);

		for (j = 0; j < len; j++)
		{
			if (!isdigit(timestr[j]))
			{
				SLEEPDLOG_DEBUG("%s contains non-numeric values", time);
				g_strfreev(time_str);
				return false;
			}
		}
	}

	*hour = atoi(time_str[0]);
	*minute = atoi(time_str[1]);
	*second = atoi(time_str[2]);
	g_strfreev(time_str);
	return true;
}
