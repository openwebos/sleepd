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
#include <stdlib.h>
#include <string.h>

#include "init.h"
#include "config.h"
#include "machine.h"
#include "debug.h"

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
        g_error("%s: Could not initialize %p\n", __FUNCTION__, func);
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
    GPrioritizedHook *new_hook_pr = (GPrioritizedHook*)new_hook;
    GPrioritizedHook *sibling_pr = (GPrioritizedHook*)sibling;

    return (new_hook_pr->priority > sibling_pr->priority);
}

/**
 * Add an InitFunc to the hooklist with the name 'name'
 */
void
NamedInitFuncAdd(const char *name, InitFuncPriority priority, InitFunc func, const char *func_name)
{
    if (!namedInitFuncs)
    {
        namedInitFuncs = g_hash_table_new(g_str_hash, g_str_equal);
        if (!namedInitFuncs)
        {
            g_error("%s: Out of memory on initialization.\n", __FUNCTION__);
            abort();
        }
    }

    if (g_hash_table_lookup(namedInitFuncs, (gconstpointer)name) == NULL)
    {
        GNamedHookList *namedHookList = malloc(sizeof (GNamedHookList));
        if (!namedHookList)
        {
            g_error("%s: Out of memory on initialization.\n", __FUNCTION__);
            abort();
        }
        g_hook_list_init((GHookList*)namedHookList, sizeof(GPrioritizedHook));

        namedHookList->name = name;
       
        g_hash_table_insert(namedInitFuncs, (char*)name, namedHookList);
    }

    GHookList *hookList = (GHookList*)g_hash_table_lookup(namedInitFuncs, (gconstpointer)name);

    GPrioritizedHook *hook = (GPrioritizedHook*)g_hook_alloc(hookList);

    hook->base.data = func;
    hook->base.func = HookInit;
    hook->priority  = priority;
    hook->func_name = func_name;

    g_hook_insert_sorted(hookList, (GHook*)hook, GPrioritizedHookCompare);
}

void
GProritizedHookPrint(GHook *hook, gpointer data)
{
    GPrioritizedHook *gphook = (GPrioritizedHook*)hook;
    g_info("%d. %s", gphook->priority, gphook->func_name);
}

void
GHookListPrint(gpointer key, gpointer value, gpointer data)
{
    const char *hookName = (const char*)key;
    GHookList *hookList = (GHookList*)value;

    g_info("InitList: %s", hookName);
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
 * Instantiates all the 
 */
void
TheOneInit(void)
{
    if (gSleepConfig.debug)
    {
        PrintHookLists();
    }

    /** Run common init funcs **/
    GHookList *commonInitFuncs = g_hash_table_lookup(namedInitFuncs, COMMON_INIT_NAME);
    if (commonInitFuncs)
    {
        g_info("\n%s Running common Inits", __FUNCTION__);
        g_hook_list_invoke(commonInitFuncs, FALSE);
    }
}
