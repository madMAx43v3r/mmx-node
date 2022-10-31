#!/bin/sh

while true;
do
    PID=$(pgrep $1)
    if [ ! -e $PID ]; then
	    SIZE=$(ps -q $PID -o rss=)
	    SIZEMB=$((SIZE/1024))
	    if [ $SIZEMB -gt $2 ]; then
	        printf "Maximum memory of $2 MB exceeded: $SIZEMB MB\nTerminating process ..."
	        kill $PID
	        exit 0
	    fi
    fi
    sleep 1
done
