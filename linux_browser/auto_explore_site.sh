#!/bin/bash
SITE=$1
echo $SITE > /tmp/url_file
LD_LIBRARY_PATH=$(pwd) ./QtTestBrowser -r /tmp/url_file \
       -robot-timeout 30 -robot-extra-time 5 -maximize -auto-explore
