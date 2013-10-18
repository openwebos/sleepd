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
#include <stdlib.h>
#include <string.h>

#include "init.h"
#include "config.h"
#include "machine.h"
#include "debug.h"
#include "logging.h"

typedef struct _GNamedHookList
{
	GHookList   hookList;
	const char *name;
} GNamedHookList;

/* hash from string -> GNamedHookList */
static GHashTable *namedInitFuncs = NULL;

static void
HookInit(gpointer func)
{
	InitFunc f = func;
	int ret = f();

	if (ret < 0)
	{
		SLEEPDLOG_ERROR(MSGID_HOOKINIT_FAIL, 0, "Could not initialize %p", func);
	}
}

typedef struct
{
	GHook            base;
	const char      *func_name;
	InitFuncPriority priority;
} GPrioritizedHook;

/**
 * @returns 1 if new_hook should be inserted after sibling,
 * and <= 0 if new_hook should be inserted before sibling.
 */
static gint
GPrioritizedHookCompare(GHook *new_hook, GHook *sibling)
{
	GPrioritizedHook *new_hook_pr = (GPrioritizedHook *)new_hook;
	GPrioritizedHook *sibling_pr = (GPrioritizedHook *)sibling;

	return (new_hook_pr->priority > sibling_pr->priority);
}

/**
 * Add an InitFunc to the hooklist with the name 'name'
 */
void
NamedInitFuncAdd(const char *name, InitFuncPriority priority, InitFunc func,
                 const char *func_name)
{
	if (!namedInitFuncs)
	{
		namedInitFuncs = g_hash_table_new(g_str_hash, g_str_equal);

		if (!namedInitFuncs)
		{
			SLEEPDLOG_ERROR(MSGID_NAMED_INIT_FUNC_OOM, 0,
			                "Out of memory on initialization");
			abort();
		}
	}

	if (g_hash_table_lookup(namedInitFuncs, (gconstpointer)name) == NULL)
	{
		GNamedHookList *namedHookList = malloc(sizeof(GNamedHookList));

		if (!namedHookList)
		{
			SLEEPDLOG_ERROR(MSGID_NAMED_HOOK_LIST_OOM, 0,
			                "Out of memory on initialization");
			abort();
		}

		g_hook_list_init((GHookList *)namedHookList, sizeof(GPrioritizedHook));

		namedHookList->name = name;

		g_hash_table_insert(namedInitFuncs, (char *)name, namedHookList);
	}

	GHookList *hookList = (GHookList *)g_hash_table_lookup(namedInitFuncs,
	                      (gconstpointer)name);

	GPrioritizedHook *hook = (GPrioritizedHook *)g_hook_alloc(hookList);

	hook->base.data = func;
	hook->base.func = HookInit;
	hook->priority  = priority;
	hook->func_name = func_name;

	g_hook_insert_sorted(hookList, (GHook *)hook, GPrioritizedHookCompare);
}

void
GProritizedHookPrint(GHook *hook, gpointer data)
{
	GPrioritizedHook *gphook = (GPrioritizedHook *)hook;
	SLEEPDLOG_DEBUG("%d. %s", gphook->priority, gphook->func_name);
}

void
GHookListPrint(gpointer key, gpointer value, gpointer data)
{
	const char *hookName = (const char *)key;
	GHookList *hookList = (GHookList *)value;

	SLEEPDLOG_DEBUG("InitList: %s", hookName);
	g_hook_list_marshal(hookList, TRUE, GProritizedHookPrint, NULL);
}

/**
 * Print out the hook lists (for debug).
 */
void
PrintHookLists(void)
{
	g_hash_table_foreach(namedInitFuncs, GHookListPrint, NULL);
}

/**
 * Runs all of the initialization hooks
 *
 * This function runs all of the initialization functions which are preloaded
 * into our {@link namedInitFuncs} array via the use of the {@link INIT_FUNC}
 * macro using some GCC-specific functionality which runs code to install the
 * hooks as the sleepd binary is loaded from disk.
 */
void
TheOneInit(void)
{
	if (gSleepConfig.debug)
	{
		PrintHookLists();
	}

	/** Run common init funcs **/
	GHookList *commonInitFuncs = g_hash_table_lookup(namedInitFuncs,
	                             COMMON_INIT_NAME);

	if (commonInitFuncs)
	{
		g_hook_list_invoke(commonInitFuncs, FALSE);
	}
}
