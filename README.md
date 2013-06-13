sleepd
======

Summary
-------
Open webOS component to manage device suspend-resume cycles.

Description
-----------
_Sleepd_ is one of the important daemons started when webOS boots. It is
responsible for scheduling platform sleeps as soon as it is idle, so that
we see optimum battery performance. To achieve this it keeps polling on the
system to see if any of the other services or processes need the platform
running, and if not it sends the suspend message to all these components
(so that they can finish whatever they are doing ASAP and suspend).
_Sleepd_ then lets the kernel know that the platform is ready to sleep.
Once an interrupt (such as key press) has woken the platform up, sleepd 
lets the entire system know that the platform is up and running so that all
the activities can resume. 

_Sleepd_ also manages the RTC alarms on the system by maintaining a _SQlite_
database for all the requested alarms.

How to Build on Linux
=====================

## Dependencies

Below are the tools and libraries (and their minimum versions) required to build
_sleepd_:

* cmake (version required by openwebos/cmake-modules-webos)
* gcc 4.6.3
* glib-2.0 2.32.1
* libxml2 2.7.2
* make (any version)
* openwebos/cjson 1.8.0
* openwebos/cmake-modules-webos 1.0.0 RC4
* openwebos/luna-service2 3.0.0
* openwebos/nyx-lib 2.0.0
* openwebos/powerd 4.0.0
* pkg-config 0.26
* sqlite3 3.6.20

## Building

Once you have downloaded the source, enter the following to build it (after
changing into the directory under which it was downloaded):

    $ mkdir BUILD
    $ cd BUILD
    $ cmake ..
    $ make
    $ sudo make install

The directory under which the files are installed defaults to `/usr/local/webos`.
You can install them elsewhere by supplying a value for `WEBOS_INSTALL_ROOT`
when invoking `cmake`. For example:

    $ cmake -D WEBOS_INSTALL_ROOT:PATH=$HOME/projects/openwebos ..
    $ make
    $ make install

will install the files in subdirectories of `$HOME/projects/openwebos`.

Specifying `WEBOS_INSTALL_ROOT` also causes `pkg-config` to look in that tree
first before searching the standard locations. You can specify additional
directories to be searched prior to this one by setting the `PKG_CONFIG_PATH`
environment variable.

If not specified, `WEBOS_INSTALL_ROOT` defaults to `/usr/local/webos`.

To configure for a debug build, enter:

    $ cmake -D CMAKE_BUILD_TYPE:STRING=Debug ..

To see a list of the make targets that `cmake` has generated, enter:

    $ make help

## Uninstalling

From the directory where you originally ran `make install`, enter:

 $ [sudo] make uninstall

You will need to use `sudo` if you did not specify `WEBOS_INSTALL_ROOT`.

# Copyright and License Information

Unless otherwise specified, all content, including all source code files and
documentation files in this repository are:

Copyright (c) 2011-2013 LG Electronics, Inc.

Unless otherwise specified or set forth in the NOTICE file, all content,
including all source code files and documentation files in this repository are:
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this content except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

