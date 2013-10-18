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

#include <stdio.h>
#include <glib.h>
#include "logging.h"

void
_good_assert(const char *cond_str, bool cond)
{
	if (G_UNLIKELY(!(cond)))
	{
		SLEEPDLOG_CRITICAL(MSGID_ASSERTION_FAIL, 1, PMLOGKS(CAUSE, cond_str), "");
		*(int *)0x00 = 0;
	}
}

PmLogContext getsleepdcontext()
{
	static PmLogContext logContext = 0;

	if (0 == logContext)
	{
		PmLogGetContext("sleepd", &logContext);
	}

	return logContext;
}
