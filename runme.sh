#!/bin/bash

recording_started=0

function clean_up {
	#end recording
	if [ $recording_started == 1 ] ; then
		echo "dummy" > /devices/pseudo/thr_cnt\@0:thr_cnt
		echo "--- Recording completed at `date`"
	fi
	
	make clean 2>/dev/null
	
	exit
}
trap clean_up SIGINT SIGKILL SIGTERM

#make sure we are in the same folder with the source code
if ! [ -f "thr_cnt.c" ] ; then
	echo "ERR: You need to be in the same folder with the source code."
	exit
fi

if [ $# -lt 1 ] ; then
    echo "Usage: $0 [duration in sec]"
    exit
fi

#clean compile
make clean 2>/dev/null
err=`make 2>&1 | tee /dev/tty | grep [Ee]rror`
if ! [ -z "$err" ] ; then
    exit
fi

#begin recording
echo "dummy" > /devices/pseudo/thr_cnt\@0:thr_cnt &
recording_started=1
echo "--- Recording started at `date`"
./thr_cnt_user $1 > data.txt

clean_up
