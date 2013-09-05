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

The browser produces a file WTF_actionlog. This file is then passed to EventRacer race analyzer.

The race analyzer is available here: https://github.com/eth-srl/EventRacer

Using auto exploration
----------------------

We added code to QtTestBrowser to automatically explore a website once it is loaded. To use it,
use a command like this:

   * ./auto_explore_site.sh http://maps.google.com/

After auto-exploration, a WTF_actionlog file is produced. This file is then passed to EventRacer race analyzer.

The race analyzer is available here: https://github.com/eth-srl/EventRacer

Compiling other targets
-----------------------

With the exception of the auto-exploration, the instrumentation is entirely in WebCore, WTF, and
the JavaScriptCore interpreter (the JIT or V8 is not supported). However, other targets are not
tested. We would be glad to hear your experience if you want to use some other port.

Credits
-------

This modified browser uses a fork on WebKit from 2012 (version 116000 from the WebKit svn)
(newer versions do not include the classic interpreter and are not compatible).
