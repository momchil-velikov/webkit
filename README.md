EventRacer WebKit-based browser
===============================

WebKit fork with the EventRacer instrumentation for race detection.


This tool has been tested with Ubuntu linux only using the Qt port of it. Most of the instrumentation must be
portable for other targers, but auto-exploration depends on Qt.


Compiling (tested only on 64-bit Ubuntu 12.04)
----------------------------------------------

The compilation instructions are standard for QtWebKit (when qt4 is used):

Download prerequsities:

```
sudo apt-get install bison flex libqt4-dev libqt4-opengl-dev libphonon-dev libicu-dev libsqlite3-dev libxext-dev 
                     libxrender-dev gperf libfontconfig1-dev libphonon-dev libpng12-dev libjpeg62-dev g++
```

Set environment variables and compile:

```
export QTDIR=/usr/share/qt4
export PATH=$QTDIR/bin:$PATH
cd webkit
Tools/Scripts/build-webkit --no-netscape-plugin --qt --makeargs="-j8"
```
This should complete the building of WebKit. 


Troubleshooting Compilation
---------------------------

Use the steps below only if your linux distro does not have a libqt4-dev package. Then, you need to download and install Qt from source.
 * Download Qt4.8.1 source: http://download.qt-project.org/archive/qt/4.8/4.8.1/
 * Only source packages can be downloaded nowadays. The instructions are in doc/html/installation.html
    * Run ```export MAKE=/usr/bin/make && ./configure  -prefix /home/$USER/Qt4.8.1 && make && make install```
 * Then, before compiling WebKit, set ```export QTDIR=/home/$USER/Qt4.8.1```
 * Compile WebKit with ``` Tools/Scripts/build-webkit --no-netscape-plugin --qt --makeargs="-j8" ```
 * Alternatively, one can use the standard instructions for compiling QtWebKit at
http://trac.webkit.org/wiki/BuildingQtOnLinux and compile with different setting.

Running WebKit
--------------

To run the browser, call:
   * ./run_browser.sh

The browser produces a file called ER_actionlog. This file contains a trace of the releveant events. This file
passed to EventRacer for race analysis. 

The race analyzer is available here: https://github.com/eth-srl/EventRacer

Using WebKit in auto-exploration mode
-------------------------------------

We added code to QtTestBrowser to automatically explore a website once it is loaded. To use this feature, type:

   * ./auto_explore_site.sh http://maps.google.com/

After auto-exploration, the ER_actionlog file is produced. This file is then passed to EventRacer race analyzer.

The race analyzer is available here: https://github.com/eth-srl/EventRacer

Compiling other targets
-----------------------

With the exception of the auto-exploration, the instrumentation is entirely in WebCore, Web Template Foundation, and
the JavaScriptCore interpreter (the JIT or V8 is not supported). However, other targets are not
tested. We would be glad to hear your experience if you want to use some other port.

Credits
-------

This modified browser uses a fork on WebKit from 2012 (version 116000 from the WebKit svn)
(newer versions do not include the classic interpreter and are not compatible, newer versions may also require qt5).
