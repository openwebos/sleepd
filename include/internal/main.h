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


#ifndef _MAIN_H_
#define _MAIN_H_

/**
 * Main header for all orphan functions.
 */

#include <glib.h>

#include <luna-service2/lunaservice.h>
#include <nyx/nyx_client.h>

GMainContext * GetMainLoopContext(void);

LSHandle * GetLunaServiceHandle(void);

LSPalmService * GetPalmService(void);

nyx_device_handle_t GetNyxSystemDevice(void);

#endif
