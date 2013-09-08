EventRacer WebKit-based browser
===============================

WebKit fork with the EventRacer instrumentation for race detection.


This tool has been tested with Ubuntu linux only using the Qt port of it. Most of the instrumentation must be
portable for other targers, but auto-exploration depends on Qt.


Compiling (tested only on Linux)
--------------------------------

Prerequisites:

   * Qt (we tested with 4.8.1, but it could probably work with 5.x as well)
   * gcc or g++ (tested with 4.6.3)

Compiling instructions (these instructions are standard for QtWebKit):

   * Download Qt 4
      * 4.8.1 is available only as source (for some reason a binary package is no longer available at Qt website).
         * Download site: http://download.qt-project.org/archive/qt/4.8/4.8.1/)
         * The file needed is qt-everywhere-opensource-src-4.8.1.zip
      * In the Qt archive there are compiling instructions (in doc/html/installation.html)
      * Extract 
      * To compile Qt, do:
         * export MAKE=/usr/bin/make
         * ./configure  -prefix /home/$USER/Qt4.8.1
         * make
         * make install
   * Download other prerequisites
      * sudo apt-get install bison flex libqt4-dev libqt4-opengl-dev libphonon-dev libicu-dev libsqlite3-dev libxext-dev libxrender-dev gperf libfontconfig1-dev libphonon-dev libpng12-dev libjpeg62-dev g++

   * Set an environment variable with the location of Qt
      * export QTDIR=/home/$USER/QtSDK/Desktop/Qt/4.8.1/gcc
   * Add Qt to the PATH
      * export PATH=$QTDIR/bin:$PATH
   * Compile
      * Tools/Scripts/build-webkit --no-netscape-plugin --qt --makeargs="-j8"
      
Alternatively, one can use the standard instructions for compiling QtWebKit at
http://trac.webkit.org/wiki/BuildingQtOnLinux and compile with different setting.

Running the browser
-------------------

To run the browser, call:
   * Tools/Scripts/run-launcher

The browser produces a file ER_actionlog. This file is then passed to EventRacer race analyzer.

The race analyzer is available here: https://github.com/eth-srl/EventRacer

Using auto exploration
----------------------

We added code to QtTestBrowser to automatically explore a website once it is loaded. To use it,
use a command like this:

   * ./auto_explore_site.sh http://maps.google.com/

After auto-exploration, a ER_actionlog file is produced. This file is then passed to EventRacer race analyzer.

The race analyzer is available here: https://github.com/eth-srl/EventRacer

Compiling other targets
-----------------------

With the exception of the auto-exploration, the instrumentation is entirely in WebCore, Web Template Foundation, and
the JavaScriptCore interpreter (the JIT or V8 is not supported). However, other targets are not
tested. We would be glad to hear your experience if you want to use some other port.

Credits
-------

This modified browser uses a fork on WebKit from 2012 (version 116000 from the WebKit svn)
(newer versions do not include the classic interpreter and are not compatible).
