sleepd
======

Sleepd is one of the important daemons started when webOS boots. It is responsible for scheduling platform sleeps as soon as it is idle, so that we see optimum battery performance. To achieve this it keeps polling on the system to see if any of the other services or processes need the platform running, and if not it sends the suspend message to all these components (so that they can finish whatever they are doing ASAP and suspend). Sleepd then lets the kernel know that the platform is ready to sleep. Once an interrupt (such as key press) has woken the platform up, sleepd lets the entire system know that the platform is up and running so that all the activities can resume. 

Sleepd also manages the RTC alarms on the system by maintaining a SQlite database for all the requested alarms.

How to Build on Linux
=====================

## Dependencies

Below are the tools and libraries (and their minimum versions) required to build sleepd:

* cmake 2.6
* gcc 4.3
* glib-2.0 2.16.6
* libxml2 2.7.2
* make (any version)
* openwebos/cjson 1.8.0
* openwebos/luna-service2 3.0.0
* openwebos/nyx-lib 2.0.0 RC 2
* openwebos/powerd 4.0.0
* pkg-config 0.22
* sqlite3 3.6.20


## Building

Once you have downloaded the source, execute the following to build it:

    $ mkdir BUILD
    $ cd BUILD
    $ cmake ..
    $ make
    $ sudo make install

The daemon and utility script will be installed under

    /usr/local/sbin

the default preferences file under

    /usr/local/etc/default

and the upstart script under

    /usr/local/etc/event.d

You can install it elsewhere by supplying a value for _CMAKE\_INSTALL\_PREFIX_ when invoking _cmake_. For example:

    $ cmake -D CMAKE_INSTALL_PREFIX:STRING=$HOME/projects/openwebos ..
    $ make
    $ make install
    
will install the files in subdirectories of $HOME/projects/openwebos instead of subdirectories of /usr/local. 

Specifying _CMAKE\_INSTALL\_PREFIX_ also causes the pkg-config files under it to be used to find headers and libraries. To have _pkg-config_ look in a different tree, set the environment variable PKG_CONFIG_PATH to the path to its _lib/pkgconfig_ subdirectory.

## Uninstalling

From the directory where you originally ran _make install_, invoke:

    $ sudo xargs rm < install_manifest.txt

## Generating documentation

The tools required to generate the documentation are:

* doxygen 1.6.3
* graphviz 2.20.2

Once you have run _cmake_, execute the following to generate the documentation:

    $ make docs

To view the generated HTML documentation, point your browser to

    doc/html/index.html

# Copyright and License Information

All content, including all source code files and documentation files in this repository are: 

 Copyright (c) 2011-2012 Hewlett-Packard Development Company, L.P.

All content, including all source code files and documentation files in this repository are:
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this content except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

