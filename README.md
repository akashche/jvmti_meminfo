JVM TI agent that logs RAM usage
--------------------------------

[JVM TI](https://docs.oracle.com/javase/8/docs/platform/jvmti/jvmti.html) agent that collects 
memory usage for current JVM process (using information from within the JVM and from OS) and
logs it into json file.

Download
--------

Windows `x86_64` binaries built with [openjdk_8u131](https://github.com/ojdkbuild/ojdkbuild/releases/tag/1.8.0.131-1): 

 - [memlog_agent-1.0.zip](https://github.com/akashche/memlog_agent/releases/download/1.0/memlog_agent-1.0.zip) (250 KB)

To build the agent on Linux see "How to build" section below.

Usage example
-------------

Checkout a sample test-suite to run with Nashorn:

    git clone https://github.com/akashche/javascript-tests-for-jvm.git
    cd javascript-tests-for-jvm

Adjust [agent config](https://github.com/akashche/memlog_agent/blob/master/resources/config.json) as needed:

 - `output_path_json`: path to the output JSON log
 - `stdout_messages`: whether to print agent init/shutdown messages to stdout
 - `cron_expr`: extended version of [Cron expression](https://en.wikipedia.org/wiki/Cron#CRON_expression) that allows to use "seconds" field
 - `timeout_divider`: timeout period, defined by Cron expression will be divided by this value (to allow sub-second logging)

Run test-suite specifying path to agent. JVM arguments can be specified with `-J-` prefix.

See [OpenJDK and Containers](https://developers.redhat.com/blog/2017/04/04/openjdk-and-containers/) article
for details about JVM memory usage tuning:

    jjs \
        -J-XX:MaxRAM=128M \
        -J-XX:+UseSerialGC \
        -J-XX:+TieredCompilation \
        -J-XX:TieredStopAtLevel=1 \
        -J-agentpath:/path/to/libmemlog_agent.so=/path/to/config.json \
        nashornLoader.js -- baseUrl=dojo load=doh test=nashornTests

`memlog.json` log file will be written, with each entry looks like this:

    {
        "currentTimeMillis": 1493820194707,
        "gcEventsCount": 0,
        "os": {
            "overall": 20905984,
            "VmPeak": 3686211584,
            <... more OS-specific values>
        },
        "jvm": {
            "overall": 15663104,
            "heap": {
                "committed": 8126464,
                "init": 8388608,
                "max": 1994850304,
                "used": 373832
            },
            "nonHeap": {
                "committed": 7536640,
                "init": 2555904,
                "max": -1,
                "used": 3467112
            }
        }
    }

Use sample [plotter.js](https://github.com/akashche/memlog_agent/blob/master/resources/plotter.js) script to
convert JSON log into table format:

    jjs /path/to/plotter.js memlog.json
    
`memlog.dat` file will be written:

    1 20 15
    2 28 16
    3 34 18
    ...

Use [gnuplot](http://www.gnuplot.info/) and [sample plot script](https://github.com/akashche/memlog_agent/blob/master/resources/memlog.plot)
to create a PNG chart:

    gnuplot /path/to/memlog.plot

![plot](https://raw.githubusercontent.com/akashche/memlog_agent/master/resources/memlog.png)

How to build
------------

[CMake](http://cmake.org/) is required for building.

[pkg-config](http://www.freedesktop.org/wiki/Software/pkg-config/) utility is used for dependency management.
For Windows users ready-to-use binary version of `pkg-config` can be obtained from [tools_windows_pkgconfig](https://github.com/staticlibs/tools_windows_pkgconfig) repository.

Build commands:

    git clone --recursive https://github.com/akashche/memlog_agent.git
    cd memlog_agent
    mkdir build
    cd build
    cmake ..
    cmake --build .
    ctest

`JAVA_HOME` environment variable must be set.

On Windows using Visual Studio 2013 Express these commands can be run using
Visual Studio development command prompt.

Cloning of [external_jansson](https://github.com/staticlibs/external_jansson.git) is not required on Linux - 
system Jansson library will be used instead (`jansson-devel` package needs to be installed).

License information
-------------------

This project is released under the [GNU General Public License, version 2](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html).

Changelog
---------

**2017-05-03**

 * version 1.0.0
 * initial public version
