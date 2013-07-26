# @@@LICENSE
#
#      Copyright (c) 2011-2013 LG Electronics, Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# LICENSE@@@

#
# sleepd/CMakeLists.txt
#


cmake_minimum_required(VERSION 2.8.7)

project(sleepd C)

include(webOS/webOS)
webos_modules_init(1 0 0 QUALIFIER RC2)
webos_component(1 1 1)

include(FindPkgConfig)

pkg_check_modules(GLIB2 REQUIRED glib-2.0)
include_directories(${GLIB2_INCLUDE_DIRS})
webos_add_compiler_flags(ALL ${GLIB2_CFLAGS_OTHER})

pkg_check_modules(LIBXML2 REQUIRED libxml-2.0)
include_directories(${LIBXML2_INCLUDE_DIRS})
webos_add_compiler_flags(ALL ${LIBXML2_CFLAGS_OTHER})

pkg_check_modules(SQLITE3 REQUIRED sqlite3)
include_directories(${SQLITE3_INCLUDE_DIRS})
webos_add_compiler_flags(ALL ${SQLITE3_CFLAGS_OTHER})

pkg_check_modules(LUNASERVICE2 REQUIRED luna-service2)
include_directories(${LUNASERVICE2_INCLUDE_DIRS})
webos_add_compiler_flags(ALL ${LUNASERVICE2_CFLAGS_OTHER})

pkg_check_modules(CJSON REQUIRED cjson)
include_directories(${CJSON_INCLUDE_DIRS})
webos_add_compiler_flags(ALL ${CJSON_CFLAGS_OTHER})

pkg_check_modules(NYXLIB REQUIRED nyx)
include_directories(${NYXLIB_INCLUDE_DIRS})
webos_add_compiler_flags(ALL ${NYXLIB_CFLAGS_OTHER})

# Nyxlib version 3.0.0 introduced an API break in nyx_system_reboot
string(REPLACE "." ";" NYXV ${NYXLIB_VERSION})
string(REPLACE "~" ";" NYXV "${NYXV}")
list(GET NYXV 0 NYXLIB_MAJOR)
if(${NYXLIB_MAJOR} GREATER 2)
	webos_add_compiler_flags(ALL -DREBOOT_TAKES_REASON)
endif()
unset(NYXV)
unset(NYXLIB_MAJOR)

pkg_check_modules(POWERD REQUIRED powerd)
include_directories(${POWERD_INCLUDE_DIRS})
webos_add_compiler_flags(ALL ${POWERD_CFLAGS_OTHER})

webos_add_compiler_flags(ALL -Wall)

webos_add_linker_options(ALL --no-undefined)

# Build the sleepd executable
include_directories(include/internal)

file(GLOB SOURCE_FILES src/alarms/*.c src/pwrevents/*.c src/utils/*.c src/*.c)

webos_configure_header_files(src)

add_executable(sleepd ${SOURCE_FILES})
target_link_libraries(sleepd 
                        ${GLIB2_LDFLAGS} 
                        ${LIBXML2_LDFLAGS}
                        ${SQLITE3_LDFLAGS}
                        ${LUNASERVICE2_LDFLAGS}
                        ${CJSON_LDFLAGS}
                        ${POWERD_LDFLAGS}
                        ${NYXLIB_LDFLAGS}
                        rt
                        pthread)

webos_build_daemon()
webos_build_system_bus_files()

install(PROGRAMS scripts/public/suspend_action DESTINATION ${WEBOS_INSTALL_SBINDIR})
install(FILES files/conf/sleepd.conf DESTINATION ${WEBOS_INSTALL_DEFAULTCONFDIR})
