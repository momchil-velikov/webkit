#!/bin/bash
#./build_webkit.sh
export OUT=$PWD/linux_browser
mkdir -p $OUT

cp WebKitBuild/Release/bin/QtTestBrowser $OUT/QtTestBrowser
cp WebKitBuild/Release/lib/libQtWebKit.so.4 $OUT
cp -r $QTDIR/plugins/imageformats linux_browser
cp -r $QTDIR/plugins/codecs linux_browser
cp -r $QTDIR/plugins/iconengines linux_browser
cp $QTDIR/lib/libQtOpenGL.so.4 linux_browser
cp $QTDIR/lib/libQtGui.so.4 linux_browser
cp $QTDIR/lib/libQtNetwork.so.4 linux_browser
cp $QTDIR/lib/libQtCore.so.4 linux_browser
cp $QTDIR/lib/libQtSql.so.4 linux_browser
cp $QTDIR/lib/libQtXmlPatterns.so.4 linux_browser

