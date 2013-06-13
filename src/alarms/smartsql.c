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
* @file smartsql.c
*
* @brief Convenience functions to interact with the rtc driver.
*
*/

#include <sqlite3.h>
#include <stdbool.h>
#include <string.h>

#include <glib.h>

#include <luna-service2/lunaservice.h>

#include "logging.h"

#define LOG_DOMAIN "POWERD-SMARTSQL: "

/**
 * @addtogroup NewInterface
 * @{
 */

static bool
_check_integrity(sqlite3 *db)
{
    const char *cmd = "PRAGMA integrity_check;";
    int rc;

    sqlite3_stmt *stmt;
    const char* tail;
    rc=sqlite3_prepare_v2( db, cmd,-1,&stmt,&tail);
    if (!stmt) goto fail;

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_OK)
        goto success;

    if (rc != SQLITE_ROW) goto fail;

    int columns = sqlite3_data_count(stmt);
    if (columns != 1) goto fail;
        
    const char *column_text = (const char*)sqlite3_column_text(stmt, 0);
    // A successful, no-error integrity check will be "ok" - all other strings imply failure
    if (strcmp(column_text,"ok") == 0)
        goto success;

    goto fail;

success:
    sqlite3_finalize(stmt);
    return true;
fail:
    sqlite3_finalize(stmt);
    fprintf(stderr, "integrity check failed");
    return false;
}

bool
smart_sql_exec(sqlite3 *db, const char *cmd)
{
    int rc;
    sqlite3_stmt *stmt;
    const char *tail;

    rc = sqlite3_prepare_v2( db, cmd, -1, &stmt, &tail);
    if (!stmt) {
        g_warning("%s: sqlite3_prepare error %d for cmd \"%s\"", __FUNCTION__, rc, cmd);
        return false;
    }
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        g_warning("%s: sqlite3_step error %d for cmd \"%s\"", __FUNCTION__, rc, cmd);
        sqlite3_finalize(stmt);
        return false;
    }
    sqlite3_finalize(stmt);
    return true;
}


static sqlite3 *
_open(const char *path)
{
    sqlite3 *db;
    int rc;
    bool retVal;

    rc = sqlite3_open(path, &db);
    if (rc != SQLITE_OK) {
        return NULL;
    }

    retVal = smart_sql_exec(db, "PRAGMA temp_store = MEMORY;");
    if (!retVal) {
        return NULL;
    }

    // TODO might want to enable sqlite3_palm_extension.so for
    // perf reasons.

    // SyncOff
    retVal = smart_sql_exec(db, "PRAGMA synchronous = 0");
    if (!retVal) {
        fprintf(stderr, "Could not set syncoff on %s\n", path);
    }

    return db;
}

static void
_close(sqlite3 *db)
{
    sqlite3_close(db);
}

bool
smart_sql_open(const char *path, sqlite3 **ret_db)
{
    bool retVal;

    sqlite3 *db  =_open(path);
    if (!db) return false;

    retVal = _check_integrity(db);
    if (!retVal) {
        SLEEPDLOG(LOG_ERR, "%s: %s corrupted... clearing.", __FUNCTION__, path);

        _close(db);

        char *journal = g_strdup_printf("%s-journal", path);
        remove(journal);
        remove(path);
        g_free(journal);

        db = _open(path);
        if (!db) return false;
    }

    *ret_db = db;
    return true;
}

void
smart_sql_close(sqlite3 *db)
{
    _close(db);
}

/* @} END OF NewInterface */
