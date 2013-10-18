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
	const char *reason = "Unknown Failure";

	const char *tail;
	bool returnValue = false;

	rc = sqlite3_prepare_v2(db, cmd, -1, &stmt, &tail);

	if (stmt)
	{
		rc = sqlite3_step(stmt);

		if (rc == SQLITE_OK)
		{
			returnValue = true;
		}
		else if (rc == SQLITE_ROW)
		{
			int columns = sqlite3_data_count(stmt);

			if (columns == 1)
			{
				const char *column_text = (const char *) sqlite3_column_text(stmt, 0);
				returnValue = (strcmp(column_text, "ok") == 0);
			}
			else
			{
				reason = "Invalid column count";
			}
		}
		else
		{
			reason = "Failed to step";
		}
	}
	else
	{
		reason = "Failed to prepare statement";
	}

	sqlite3_finalize(stmt);

	if (!returnValue)
	{
		SLEEPDLOG_WARNING(MSGID_INTEGRITY_CHK_FAIL, 1, PMLOGKS(CAUSE, reason),
		                  "Integrity check failed");
	}

	return returnValue;
}

bool
smart_sql_exec(sqlite3 *db, const char *cmd)
{
	int rc;
	sqlite3_stmt *stmt;
	const char *tail;

	rc = sqlite3_prepare_v2(db, cmd, -1, &stmt, &tail);

	if (!stmt)
	{
		SLEEPDLOG_WARNING(MSGID_SQLITE_PREPARE_ERR, 2, PMLOGKFV(ERRCODE, "%d", rc),
		                  PMLOGKS(COMMAND, cmd), "");
		return false;
	}

	rc = sqlite3_step(stmt);

	if (rc != SQLITE_DONE)
	{
		SLEEPDLOG_WARNING(MSGID_SQLITE_STEP_ERR, 2, PMLOGKFV(ERRCODE, "%d", rc),
		                  PMLOGKS(COMMAND, cmd), "");
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

	if (rc != SQLITE_OK)
	{
		return NULL;
	}

	retVal = smart_sql_exec(db, "PRAGMA temp_store = MEMORY;");

	if (!retVal)
	{
		return NULL;
	}

	// TODO might want to enable sqlite3_palm_extension.so for
	// perf reasons.

	// SyncOff
	retVal = smart_sql_exec(db, "PRAGMA synchronous = 0");

	if (!retVal)
	{
		SLEEPDLOG_WARNING(MSGID_SET_SYNCOFF_ERR, 2, PMLOGKS(CAUSE,
		                  "Could not set syncoff on path"), PMLOGKS(PATH, path), "");
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

	sqlite3 *db  = _open(path);

	if (!db)
	{
		return false;
	}

	retVal = _check_integrity(db);

	if (!retVal)
	{
		SLEEPDLOG_ERROR(MSGID_DB_INTEGRITY_CHK_ERR, 1, PMLOGKS(PATH, path),
		                "Db corrupted");

		_close(db);

		char *journal = g_strdup_printf("%s-journal", path);

		if (journal != NULL)
		{
			remove(journal);
		}

		remove(path);

		if (journal != NULL)
		{
			g_free(journal);
		}

		db = _open(path);

		if (!db)
		{
			return false;
		}
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
