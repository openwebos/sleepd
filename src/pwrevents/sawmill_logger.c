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
 * @file sawmill_logger.c
 *
 * @brief This module's purpose is to log periodic statistics
 * to /var/log/messages for processing by sawmill
 *
 */

#include <glib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <stdlib.h>
#include <luna-service2/lunaservice.h>

#include "init.h"
#include "sysfs.h"

#define PRINT_INTERVAL_MS 60000

static long unsigned int sTimeOnWake = 0;
static long unsigned int sTimeOnPrint = 0;
static long unsigned int sTimeScreenOn = 0;
static long unsigned int sTimeScreenOff = 0;
static int sMSUntilPrint = 0;
static bool sScreenIsOn = false;
//static timespec sTimeOnSuspended = 0;

static long unsigned int sTotalMSAwake = 0;
static long unsigned int sTotalMSAsleep = 0;
static long unsigned int sTotalMSScreenOn = 0;
static long unsigned int sTotalMSScreenOff = 0;
static bool sIsAwake = true;
static guint sTimerEventSource = 0;

#define NS_PER_MS 1000000
#define MS_PER_S 1000
long int
time_to_ms(struct timespec t)
{
	return (t.tv_sec * MS_PER_S) + (t.tv_nsec / NS_PER_MS);
}

/**
 *
 * @return  A relative timestamp that can be used to compute
 *          elapsed times.
 */
void
get_time_now(struct timespec *time_now)
{
	clock_gettime(CLOCK_REALTIME, time_now);
}

/**
 * @brief Return a monotonic time, in ms.
 *
 * @return  A relative timestamp that can be used to compute
 *          elapsed times.
 */
long int
time_now_ms()
{
	struct timespec time_now;
	get_time_now(&time_now);
	return time_to_ms(time_now);
}

void
sawmill_logger_record_sleep(struct timespec time_awake)
{
	sTotalMSAwake += time_to_ms(time_awake);
	sIsAwake = false;

	// calculate the amnt of time left to fire
	sMSUntilPrint = CLAMP(PRINT_INTERVAL_MS - (time_now_ms() - sTimeOnPrint), 0,
	                      PRINT_INTERVAL_MS);
}

void read_lvdisplay(char **buf)
{

	FILE *stream = popen("lvdisplay -c", "r");

	int i = 0;

	//char *buf[100];
	for (i = 0; i < 100; i++)
	{
		buf[i] = calloc(100, sizeof(char));
	}

	i = 0;

	while (fgets(buf[i], 100, stream))
	{
		//g_message("lvdisplay: %s", buf[i]);
		++i;
	}

	//  g_message("%s: i is: %d",__func__,i);



}

void read_proc_loadavg()
{
	char *path = "/proc/loadavg";

	GError *gerror = NULL;
	char *contents = NULL;
	gsize len;
	//int n;

	gchar **arr_file; //, **arr_line;

	if (!path || !g_file_get_contents(path, &contents, &len, &gerror))
	{
		if (gerror)
		{
			g_critical("%s: %s", __FUNCTION__, gerror->message);
			g_error_free(gerror);
		}
	}

	arr_file = g_strsplit_set(contents, " ", -1);

	g_message("loadavg:1m:%s:5m:%s:15m:%s kr/ke:%s pid:%s", arr_file[0],
	          arr_file[1], arr_file[2], arr_file[3], arr_file[4]);

	g_strfreev(arr_file);
	g_free(contents);

}


void read_proc_diskstats()
{

	GError *gerror = NULL;
	char *contents = NULL;
	gsize len;
	int n;

	gchar **arr_file, **arr_line;

	char *path = "/proc/diskstats";

	if (!path || !g_file_get_contents(path, &contents, &len, &gerror))
	{
		if (gerror)
		{
			g_critical("%s: %s", __FUNCTION__, gerror->message);
			g_error_free(gerror);
		}
	}

	/* mapping device to mount points
	    char *buf[100]={};

	    read_lvdisplay(buf);
	    int i=0;
	    while(buf[i]){
	        //g_message("while loop: i:%d buf:%s:", i, buf[i]);
	        if(!g_strcmp0(buf[i],"")) break;
	        //gchar **mapping_str = g_strsplit_set(buf[i],"/", -1);
	        //g_message("%s", mapping_str[3]);
	        i++;
	    }
	*/

	arr_file = g_strsplit_set(contents, "\n", -1);
	int newlines = g_strv_length(arr_file) - 1;

	char *all_devices = NULL, *one_device = NULL;

	for (n = 0; n < newlines ; n++)
	{

		arr_line = g_strsplit_set(arr_file[n], " ", -1); // cpu0
		int k = 0, skip = 0;
		int device_index = 0;

		while (arr_line[k])
		{
			if (!g_strcmp0(arr_line[k], ""))
			{
				++k;
				continue;
			}

			if (skip < 2)
			{
				++skip; // the initial 2 fields are major and minor numbers.
			}
			else
			{
				device_index = k;
				break;
			}

			//g_message("%d:%s",k,arr_line[k]);
			k = k + 1;

		}

		//g_message("device: %s", arr_line[device_index]);
		if (g_str_has_prefix(arr_line[device_index], "ram"))
		{
			g_strfreev(arr_line);
			continue;
		}

		if (g_str_has_prefix(arr_line[device_index], "loop"))
		{
			g_strfreev(arr_line);
			continue;
		}

		if (g_str_has_prefix(arr_line[device_index], "mmcblk0p"))
		{
			g_strfreev(arr_line);
			continue;
		}

		//if(g_str_has_prefix(arr_line[device_index],"mmcblk1p")) continue;

		one_device =    g_strdup_printf("%s:r:%s:w:%s:ip:%s",
		                                //path,
		                                arr_line[device_index],
		                                arr_line[device_index + 1], // reads_completed
		                                //arr_line[device_index+2], // reads_merged
		                                //arr_line[+3], // sectors_read
		                                //arr_line[+4], // ms_reading
		                                arr_line[device_index + 5], // writes_completed
		                                //arr_line[device_index+6], // writes_merged
		                                //arr_line[+7], // sectors_written
		                                //arr_line[+8], // ms_writing
		                                arr_line[device_index + 9] // io_inprogress
		                                //arr_line[22] // ms_io
		                                //arr_line[8] // ms_weighted_io
		                               );//g_strconcat(

		char *delete_this = all_devices;
		all_devices = g_strjoin(" ", one_device, all_devices, NULL);
		g_free(one_device);
		g_free(delete_this);
		g_strfreev(arr_line);
	}

	g_message("io:%s", all_devices);

	g_free(all_devices);
	g_strfreev(arr_file);
	g_free(contents);


}


void read_proc_stat()
{
	GError *gerror = NULL;
	char *contents = NULL;
	gsize len;
	int n;

	gchar **arr_file, **arr_line;

	char *path = "/proc/stat";

	if (!path || !g_file_get_contents(path, &contents, &len, &gerror))
	{
		if (gerror)
		{
			g_critical("%s: %s", __FUNCTION__, gerror->message);
			g_error_free(gerror);
		}
	}

	arr_file = g_strsplit_set(contents, "\n", -1);

	int newlines = g_strv_length(arr_file) - 1;
	char *ctxt = NULL;
	char *procs_running = NULL;

	for (n = 0; n < newlines ; n++)
	{

		arr_line = g_strsplit_set(arr_file[n], " ", -1);

		if (!g_strcmp0(arr_line[0], "ctxt"))
		{
			ctxt = g_strdup(arr_line[1]);
		}

		if (!g_strcmp0(arr_line[0], "procs_running"))
		{
			procs_running = g_strdup(arr_line[1]);
		}

		g_strfreev(arr_line);
	}

	arr_line = g_strsplit(arr_file[0], " ", -1); // cpu0
	g_message("%s_stat: u:%s ulp:%s sys:%s i:%s iow:%s int:%s sint:%s cs:%s pr:%s",
	          //path,
	          arr_line[0],
	          arr_line[2], // arr_line[1] is an empty string
	          arr_line[3],
	          arr_line[4],
	          arr_line[5],
	          arr_line[6],
	          arr_line[7],
	          arr_line[8],
	          ctxt,
	          procs_running
	         );
	g_strfreev(arr_line);

	g_free(ctxt);
	g_free(procs_running);

	g_strfreev(arr_file);
	g_free(contents);



}


void read_proc_meminfo()
{
	GError *gerror = NULL;
	char *contents = NULL;
	gsize len;
	int n;

	gchar **arr_file, **arr_line;

	char *path = "/proc/meminfo";

	if (!path || !g_file_get_contents(path, &contents, &len, &gerror))
	{
		if (gerror)
		{
			g_critical("%s: %s", __FUNCTION__, gerror->message);
			g_error_free(gerror);
		}
	}


	arr_file  = g_strsplit_set(contents, "\n", -1);

	int newlines = g_strv_length(arr_file) - 1;
	char *MemTotal = NULL;
	char *MemFree = NULL;
	char *SwapTotal = NULL;
	char *SwapFree = NULL;

	for (n = 0; n < newlines ; n++)
	{

		arr_line = g_strsplit_set(arr_file[n], ":", -1);

		if (!g_strcmp0(arr_line[0], "MemTotal"))
		{
			MemTotal = g_strdup(arr_line[1]);
		}

		if (!g_strcmp0(arr_line[0], "MemFree"))
		{
			MemFree = g_strdup(arr_line[1]);
		}

		if (!g_strcmp0(arr_line[0], "SwapTotal"))
		{
			SwapTotal = g_strdup(arr_line[1]);
		}

		if (!g_strcmp0(arr_line[0], "SwapFree"))
		{
			SwapFree = g_strdup(arr_line[1]);
		}

		g_strfreev(arr_line);
	}

	g_message("mem:mt:%s mf:%s st:%s sf:%s", g_strstrip(MemTotal),
	          g_strstrip(MemFree), g_strstrip(SwapTotal), g_strstrip(SwapFree));

	g_free(MemTotal);
	g_free(MemFree);
	g_free(SwapTotal);
	g_free(SwapFree);
	g_strfreev(arr_file);
	g_free(contents);


}


void read_proc_net_dev()
{
	GError *gerror = NULL;
	char *contents = NULL;
	gsize len;
	int n;

	gchar **arr_file, **arr_line;

	char *path = "/proc/net/dev";

	if (!path || !g_file_get_contents(path, &contents, &len, &gerror))
	{
		if (gerror)
		{
			g_critical("%s: %s", __FUNCTION__, gerror->message);
			g_error_free(gerror);
		}
	}

	arr_file = g_strsplit_set(contents, "\n", -1);
	int newlines = g_strv_length(arr_file) - 1;

	for (n = 2; n < newlines ; n++)
	{

		arr_line = g_strsplit(arr_file[n], ":", -1); //

		if ((!g_strcmp0(g_strstrip(arr_line[0]), "eth0")) ||
		        (!g_strcmp0(g_strstrip(arr_line[0]), "ppp0")))
		{

			char **arr_fields = g_strsplit(arr_line[1], " ", -1);
			int k = 0;
			int nth_field = 0;
			int received_packets = 0;
			int transmitted_packets = 0;

			while (arr_fields[k])
			{
				if (!g_strcmp0(arr_fields[k], ""))
				{
					++k;
					continue;
				}

				if (nth_field == 1)
				{
					received_packets = k;
				}

				if (nth_field == 9)
				{
					transmitted_packets = k;
				}

				++k;
				++nth_field;
			}

			g_message("net:%s:rp:%s tp:%s",
			          //path,
			          arr_line[0],
			          arr_fields[received_packets], // hack, values seem to start from arr_line[2]
			          arr_fields[transmitted_packets]
			         );
			g_strfreev(arr_fields);
		}

		g_strfreev(arr_line);
	}

	g_strfreev(arr_file);
	g_free(contents);
}

void get_battery_coulomb_reading(double *rc, double *c)
{
#define SYSFS_A6_DEVICE     "/sys/class/misc/a6_0/regs/"
#define DEF_BATTERY_PATH    "/sys/devices/w1 bus master/w1_master_slaves/"

	if (g_file_test(SYSFS_A6_DEVICE, G_FILE_TEST_IS_DIR))
	{
		SysfsGetDouble(SYSFS_A6_DEVICE "getrawcoulomb", rc);
		SysfsGetDouble(SYSFS_A6_DEVICE "getcoulomb", c);
	}
	else
	{
		SysfsGetDouble(DEF_BATTERY_PATH "getrawcoulomb", rc);
		SysfsGetDouble(DEF_BATTERY_PATH "getcoulomb", c);
	}
}

gboolean
sawmill_logger_update(gpointer data)
{
	if (sIsAwake)
	{
		double rc , c;
		get_battery_coulomb_reading(&rc, &c);
		sTimeOnPrint = time_now_ms();
		long unsigned int diff_awake = sTimeOnPrint - sTimeOnWake;
		g_message("%s: raw_coulomb: %f coulomb: %f time_awake_ms: %lu time_asleep_ms: %lu time_screen_on_ms: %lu time_screen_off_ms: %lu",
		          __func__,
		          rc, c,
		          sTotalMSAwake + diff_awake,
		          sTotalMSAsleep,
		          sTotalMSScreenOn + (sScreenIsOn ? (time_now_ms() - sTimeScreenOn) : 0),
		          sTotalMSScreenOff + (sScreenIsOn ? 0 : (time_now_ms() - sTimeScreenOff))
		         );
		read_proc_loadavg();
		read_proc_stat();
		read_proc_diskstats();
		read_proc_meminfo();
		read_proc_net_dev();

	}

	//TODO: use g_timer_source_set_interval(GTimerSource *tsource, guint interval_ms, gboolean from_poll)
	g_source_remove(sTimerEventSource);
	sTimerEventSource = g_timeout_add_full(G_PRIORITY_DEFAULT, PRINT_INTERVAL_MS,
	                                       sawmill_logger_update, GINT_TO_POINTER(TRUE), NULL);
	return FALSE;
}

// note we dont get ms resolution here
void
sawmill_logger_record_wake(struct timespec time_asleep)
{
	unsigned long int ms_asleep = time_to_ms(time_asleep);
	sTotalMSAsleep += ms_asleep;
	sTimeOnWake = time_now_ms();
	sIsAwake = true;

	//TODO: use g_timer_source_set_interval(GTimerSource *tsource, guint interval_ms, gboolean from_poll)
	g_source_remove(sTimerEventSource);
	sTimerEventSource = g_timeout_add_full(G_PRIORITY_DEFAULT,
	                                       CLAMP(sMSUntilPrint - ms_asleep, 0, PRINT_INTERVAL_MS), sawmill_logger_update,
	                                       GINT_TO_POINTER(FALSE), NULL);
}

void
sawmill_logger_record_screen_toggle(bool set_on)
{

	// ignoring duplicate calls
	if (set_on == sScreenIsOn)
	{
		return;
	}

	if (set_on)
	{
		sTimeScreenOn = time_now_ms();
		sTotalMSScreenOff += (sTimeScreenOn - sTimeScreenOff);
	}
	else
	{
		sTimeScreenOff = time_now_ms();
		sTotalMSScreenOn += (sTimeScreenOff - sTimeScreenOn);
	}

	sScreenIsOn = set_on;
}


static int
_sawlog_init(void)
{
	sTimeOnWake = time_now_ms();
	sTimeOnPrint = time_now_ms();
	sTimeScreenOn = time_now_ms();
	sTimeScreenOff = time_now_ms();
	sTimerEventSource = g_timeout_add_full(G_PRIORITY_DEFAULT, PRINT_INTERVAL_MS,
	                                       sawmill_logger_update, GINT_TO_POINTER(TRUE), NULL);

	return 0;
}

INIT_FUNC(INIT_FUNC_MIDDLE, _sawlog_init);
