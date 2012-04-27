# @@@LICENSE
#
#      Copyright (c) 2011-2012 Hewlett-Packard Development Company, L.P.
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


cmake_minimum_required(VERSION 2.6)

project(sleepd)

set(SLEEPD_VERSION_MAJOR 1)
set(SLEEPD_VERSION_MINOR 1)
set(SLEEPD_VERSION_PATCH 0)
set(SLEEPD_PROGRAM_VERSION "${SLEEPD_VERSION_MAJOR}.${SLEEPD_VERSION_MINOR}.${SLEEPD_VERSION_PATCH}")

# Make sure the build system's version of this component matches what we're building.
# The intent is for it to pass in SLEEPD_COMPONENT_VERSION via the command line.
if(DEFINED SLEEPD_COMPONENT_VERSION)
    if (NOT (SLEEPD_PROGRAM_VERSION STREQUAL SLEEPD_COMPONENT_VERSION))
        message(FATAL_ERROR "Component version from build system (${SLEEPD_COMPONENT_VERSION}) != program version (${SLEEPD_PROGRAM_VERSION})")
    endif()
endif()

# XXX Why is CMAKE_BUILD_TYPE empty under Ubuntu?
if(NOT CMAKE_BUILD_TYPE)
    set (CMAKE_BUILD_TYPE "Release")
endif()

if(NOT DEFINED ENV{PKG_CONFIG_PATH})
    set(ENV{PKG_CONFIG_PATH} "${CMAKE_INSTALL_PREFIX}/lib/pkgconfig")
endif()

if(CMAKE_INSTALL_PREFIX STREQUAL "/usr")
    set(SYSCONFDIR_FULL "/etc")
else()
    set(SYSCONFDIR_FULL "${CMAKE_INSTALL_PREFIX}/etc")
endif()

message("")

message(STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
message(STATUS "CMAKE_FIND_ROOT_PATH: ${CMAKE_FIND_ROOT_PATH}")
message(STATUS "ENV{PKG_CONFIG_PATH}: $ENV{PKG_CONFIG_PATH}")
message(STATUS "CMAKE_INSTALL_PREFIX: ${CMAKE_INSTALL_PREFIX}")

#  Note that TARGET_CORE_OS is no longer used, but keep it around in case it turns out to be needed later.
set(DEFAULT_TARGET_CORE_OS "ubuntu" CACHE INTERNAL "The default core OS for which to build")

# Should specify a core OS (ubuntu, rockhopper, etc.)
if(TARGET_CORE_OS)
    message(STATUS "TARGET_CORE_OS: ${TARGET_CORE_OS}")
else()
    set(TARGET_CORE_OS "${DEFAULT_TARGET_CORE_OS}")
    message(STATUS "No TARGET_CORE_OS value specified -- defaulting to ${TARGET_CORE_OS}")
endif()
set(TARGET_CORE_OS ${TARGET_CORE_OS} CACHE STRING "The core OS for which to build")

message("")


macro(add_flags flags)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${flags}")
endmacro()

macro(add_linker_flags flags)
    # Appending to CMAKE_EXE_LINKER_FLAGS fails to add them to the link command line -- why?
    # Note that using add_definitions() also fails to add to the link command line.
    add_flags("${flags}")
endmacro()


include(FindPkgConfig)

# Check that we have glib-2.0, libxml2, and sqlite3
pkg_check_modules(GLIB2 REQUIRED glib-2.0)
add_definitions(${GLIB2_CFLAGS})
pkg_check_modules(LIBXML2 REQUIRED libxml-2.0)
add_definitions(${LIBXML2_CFLAGS})
pkg_check_modules(SQLITE3 REQUIRED sqlite3)
add_definitions(${SQLITE3_CFLAGS})

# Check that we have luna-service2
pkg_check_modules(LUNASERVICE2 REQUIRED luna-service2)
add_definitions(${LUNASERVICE2_CFLAGS})

# luna-service2 requires cjson; make sure it's available
pkg_check_modules(CJSON REQUIRED cjson)
add_definitions(${CJSON_CFLAGS})

# Check that we have powerd
pkg_check_modules(POWERD REQUIRED powerd)
add_definitions(${POWERD_CFLAGS})

# Check that we have nyx-lib
pkg_check_modules(NYXLIB REQUIRED nyx)
add_definitions(${NYXLIB_CFLAGS})


# Specifying this causes "warning: 'struct timespec' declared inside parameter list" in include/clock.h.
#add_definitions("-std=c99")

# Turn on all useful warnings, but don't specify -Werror -- having warnings break the build makes
# switching to a new compiler a lengthy and painful ordeal.
add_definitions("-Wall")

# Require that all undefined symbols are satisfied by the libraries from target_link_libraries()
add_linker_flags("-Wl,--no-undefined")
# XXX_ The libraries that come with gcc fail when --no-allow-shlib-undefined is specified
#add_linker_flags("-Wl,--no-allow-shlib-undefined")


# Build the sleepd executable
include_directories(include src)

aux_source_directory(src/alarms alarms_files)
aux_source_directory(src/pwrevents pwrevents_files)
aux_source_directory(src/utils utils_files)

add_executable(sleepd ${alarms_files} ${pwrevents_files} ${utils_files} src/main.c src/config.c)
target_link_libraries(sleepd 
                        ${GLIB2_LDFLAGS} 
                        ${LIBXML2_LDFLAGS}
                        ${SQLITE3_LDFLAGS}
                        ${LUNASERVICE2_LDFLAGS}
                        ${CJSON_LDFLAGS}
                        ${POWERD_LDFLAGS}
                        ${NYXLIB_LDFLAGS})


install(TARGETS sleepd DESTINATION "sbin")
install(PROGRAMS src/scripts/suspend_action DESTINATION "sbin")
install(FILES preferences/sleepd.conf DESTINATION "${SYSCONFDIR_FULL}/default")
install(PROGRAMS etc/event.d/sleepd DESTINATION "${SYSCONFDIR_FULL}/event.d")


configure_file(${PROJECT_SOURCE_DIR}/doc/Doxyfile.in
                ${PROJECT_BINARY_DIR}/doc/Doxyfile)

# This adds a target called "docs" (i.e., make docs). doxygen and dot
# (from graphviz) are expected to be available.
# ${PROJECT_BINARY_DIR} is the build directory and ${PROJECT_SOURCE_DIR}
# is the source directory.
add_custom_target(docs 
                    doxygen ${PROJECT_BINARY_DIR}/doc/Doxyfile
                    SOURCES ${PROJECT_SOURCE_DIR}/doc/Doxyfile.in
                    WORKING_DIRECTORY ${PROJECT_BINARY_DIR}/doc)
