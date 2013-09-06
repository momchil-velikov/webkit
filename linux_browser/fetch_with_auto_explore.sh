#!/bin/bash
SITE=$1
TARGET_DIR=$2
URL_FILE=$TARGET_DIR/url_file
TARGET_FILE=$TARGET_DIR/WTF_actionlog
echo $SITE > $URL_FILE
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd $TARGET_DIR

LD_LIBRARY_PATH=$DIR $DIR/QtTestBrowser -r $URL_FILE \
       -hidden-window \
       -robot-timeout 10 -robot-extra-time 1 -maximize -auto-explore

