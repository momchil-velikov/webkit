#!/bin/bash
SITE=$1
echo $SITE > /tmp/url_file
WebKitBuild/Release/bin/QtTestBrowser -r /tmp/url_file \
       -robot-timeout 15 -robot-extra-time 5 -maximize -auto-explore
ls -l ER_actionlog
