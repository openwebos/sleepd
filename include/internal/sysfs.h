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



#ifndef _SYSFS_H_
#define _SYSFS_H_

int SysfsGetInt(const char *path, int *ret_data);
int SysfsGetDouble(const char *path, double *ret_data);
int SysfsGetString(const char *path, char *ret_string, size_t maxlen);

int SysfsWriteString(const char *path, const char *string);

#endif
