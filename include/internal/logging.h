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
 * LICENSE@@@
 */

#ifndef __SLEEPD_LOGGING_H__
#define __SLEEPD_LOGGING_H__

#include <PmLogLib.h>

/* Logging for sleepd context ********
 * The parameters needed are
 * msgid - unique message id
 * kvcount - count for key-value pairs
 * ... - key-value pairs and free text. key-value pairs are formed using PMLOGKS or PMLOGKFV
 * e.g.)
 * LOG_CRITICAL(msgid, 2, PMLOGKS("key1", "value1"), PMLOGKFV("key2", "%s", value2), "free text message");
 **********************************************/
#define SLEEPDLOG_CRITICAL(msgid, kvcount, ...) \
        PmLogCritical(getsleepdcontext(), msgid, kvcount, ##__VA_ARGS__)

#define SLEEPDLOG_ERROR(msgid, kvcount, ...) \
        PmLogError(getsleepdcontext(), msgid, kvcount,##__VA_ARGS__)

#define SLEEPDLOG_WARNING(msgid, kvcount, ...) \
        PmLogWarning(getsleepdcontext(), msgid, kvcount, ##__VA_ARGS__)

#define SLEEPDLOG_INFO(msgid, kvcount, ...) \
        PmLogInfo(getsleepdcontext(), msgid, kvcount, ##__VA_ARGS__)

#define SLEEPDLOG_DEBUG(...) \
        PmLogDebug(getsleepdcontext(), ##__VA_ARGS__)


/** list of MSGID's pairs */

/** config.c */
#define MSGID_CONFIG_FILE_LOAD_ERR                "CONFIG_FILE_LOAD_ERR"     //Could not load config file from specified path

/** main.c */
#define MSGID_NYX_DEVICE_OPEN_FAIL                "NYX_DEVICE_OPEN_FAIL"     //Failed to open nyx device
#define MSGID_SRVC_REGISTER_FAIL                  "SRVC_REGISTER_FAIL"       //luna-bus registraton to check charger status failed

/** alarm.c */
#define MSGID_ALARM_NOT_SET                       "ALARM_NOT_SET"                  //Could not set alarm
#define MSGID_ADD_ALARM_INFO                      "ADD_ALARM_INFO"                 //Details of alarm to be added
#define MSGID_ALARM_ADD_CALENDER_INFO             "ALARM_ADD_CALENDER_INFO"        //Details of alarm to be added with calender date
#define MSGID_FIRE_ALARM_INFO                     "FIRE_ALARM_INFO"

/** smartsql.c */
#define MSGID_SQLITE_PREPARE_ERR                  "SQLITE_PREPARE_ERR"             //sqlite3 prepare error
#define MSGID_SQLITE_STEP_ERR                     "SQLITE_STEP_ERR"                //sqlite3 step error
#define MSGID_DB_INTEGRITY_CHK_ERR                "DB_INTEGRITY_CHK_ERR"           //db integrity check error
#define MSGID_INTEGRITY_CHK_FAIL                  "INTEGRITY_CHK_FAIL"             //db integrity check failed
#define MSGID_SET_SYNCOFF_ERR                     "SET_SYNCOFF_ERR"                //Failed to set syncoff on provided path

/** timeout_alarm.c */
#define MSGID_RTC_ERR                             "RTC_ERR"                        //RTC not working properly
#define MSGID_SELECT_EXPIRY_ERR                   "SELECT_EXPIRY_ERR"              //Failed to select expiry from timeout db
#define MSGID_RTC_ERR                             "RTC_ERR"                        //RTC not working properly
#define MSGID_TIMEOUT_MSG_ERR                     "TIMEOUT_MSG_ERR"                //could not send timeout message
#define MSGID_SQLITE_STEP_FAIL                    "SQLITE_STEP_FAIL"               //sqlite3 step error
#define MSGID_SQLITE_FINALIZE_FAIL                "SQLITE_FINALIZE_FAIL"           //sqlite3 finalize error
#define MSGID_EXPIRY_SELECT_FAIL                  "EXPIRY_SELECT_FAIL"             //Select operation from timeout db failed
#define MSGID_UPDATE_EXPIRY_FAIL                  "UPDATE_EXPIRY_FAIL"             //update expiry failed
#define MSGID_SELECT_EXPIRED_TIMEOUT              "SELECT_EXPIRED_TIMEOUT"         //select expired calendar timeouts error
#define MSGID_SQLITE_PREPARE_FAIL                 "SQLITE_PREPARE_FAIL"            //sqlite3 prepare error
#define MSGID_ALARM_TIMEOUT_SELECT                "ALARM_TIMEOUT_SELECT"           //Failed to select expiry from timeout db
#define MSGID_SELECT_EXPIRY_WITH_WAKEUP           "SELECT_EXPIRY_WITH_WAKEUP"      //Failed to select expiry from timeout db
#define MSGID_LSMESSAGE_REPLY_FAIL                "LSMESSAGE_REPLY_FAIL"           //could not send reply message
#define MSGID_SHORT_ACTIVITY_DURATION             "SHORT_ACTIVITY_DURATION"        //could not send reply <activity duration too short>
#define MSGID_UNKNOWN_ERR                         "UNKNOWN_ERR"                    //errorunknown
#define MSGID_INVALID_JSON_REPLY                  "INVALID_JSON_REPLY"             //invalid json reply message error
#define MSGID_DB_OPEN_ERR                         "DB_OPEN_ERR"                    //Failed to open database
#define MSGID_DB_CREATE_ERR                       "DB_CREATE_ERR"                  //could not create database
#define MSGID_INDEX_CREATE_FAIL                   "INDEX_CREATE_FAIL"              //could not create index
#define MSGID_CATEGORY_REG_FAIL                   "CATEGORY_REG_FAIL"              //could not register category
#define MSGID_METHOD_REG_ERR                      "METHOD_REG_ERR"                 //could not register for suspend resume signal
#define MSGID_UPDATE_RTC_FAIL                     "UPDATE_RTC_FAIL"                //could not get wall-rtc offset
#define MSGID_ALARM_TIMEOUT_INSERT                "ALARM_TIMEOUT_INSERT"           //Insert into AlarmTimeout failed
#define MSGID_SELECT_ALL_FROM_TIMEOUT             "SELECT_ALL_FROM_TIMEOUT"        //timeout read failed

/** init.c */
#define MSGID_HOOKINIT_FAIL                       "HOOKINIT_FAIL"                  //Failed to initialize
#define MSGID_NAMED_INIT_FUNC_OOM                 "NAMED_INIT_FUNC_OOM"            //Out of memory on initialization
#define MSGID_NAMED_HOOK_LIST_OOM                 "NAMED_HOOK_LIST_OOM"            //Out of memory on initialization

/** timesaver.c */
#define MSGID_TIME_NOT_SAVED_TO_DB                "TIME_NOT_SAVED_TO_DB"           //time not be saved to db before battery was pulledout

/** activity.c */

/** machine.c */
#define MSGID_FRC_SHUTDOWN                        "FRC_SHUTDOWN"             // Force Shutdown
#define MSGID_FRC_REBOOT                          "FRC_REBOOT"               // Force Reboot

/** shutdown.c */
#define MSGID_SHUTDOWN_APPS_SIG_FAIL              "SHUTDOWN_APPS_SIG_FAIL"   // Could not send shutdown applications
#define MSGID_SHUTDOWN_SRVC_SIG_FAIL              "SHUTDOWN_SRVC_SIG_FAIL"   // Could not send shutdown Services
#define MSGID_SHUTDOWN_REPLY_FAIL                 "SHUTDOWN_REPLY_FAIL"      // Could not send shutdown success message
#define MSGID_LSMSG_REPLY_FAIL                    "LSMSG_REPLY_FAIL"         // Could not send reply to caller
#define MSGID_LSSUBSCRI_ADD_FAIL                  "LSSUBSCRI_ADD_FAIL"       // LSSubscriptionAdd failed

/** suspend.c */
#define MSGID_PTHREAD_CREATE_FAIL                 "PTHREAD_CREATE_FAIL"      // Could not create SuspendThread
#define MSGID_NYX_DEV_OPEN_FAIL                   "NYX_DEV_OPEN_FAIL"        // Unable to open the nyx device led controller

/** suspend_ipc.c */
#define MSGID_LS_SUBSCRIB_SETFUN_FAIL             "LS_SUBSCRIB_SETFUN_FAIL"  // Error in setting cancel function

/** sawmill_logger.c */
#define MSGID_READ_PROC_MEMINFO_ERR               "READ_PROC_MEMINFO_ERR"    // Error while reading /proc/meminfo
#define MSGID_READ_PROC_STAT_ERR                  "READ_PROC_STAT_ERR"       // Error while reading /proc/stat
#define MSGID_READ_PROC_DISKSTAT_ERR              "READ_PROC_DISKSTAT_ERR"   // Error while reading /proc/diskstats
#define MSGID_READ_PROC_LOADAVG_ERR               "READ_PROC_LOADAVG_ERR"    // Error while reading /proc/loadavg
#define MSGID_READ_PROC_NETDEV_ERR                "READ_PROC_NETDEV_ERR"     // Error while reading /proc/net/dev
#define MSGID_ASSERTION_FAIL                      "ASSERTION_FAIL"           //Assertion failed

/** list of logkey ID's */

#define ERRTEXT                   "ERRTEXT"
#define ERRCODE                   "ERRCODE"
#define PATH                      "PATH"
#define CAUSE                     "CAUSE"
#define COMMAND                   "COMMAND"
#define NYX_QUERY_TIME            "NYX_QUERY_TIME"
#define ALARM_ID                  "ALARM_ID"
#define SRVC_NAME                 "SRVC_NAME"
#define APP_NAME                  "APP_NAME"

extern PmLogContext getsleepdcontext();

#endif // __LOGGING_H__
