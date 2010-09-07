#!/bin/sh
DAEMON=/usr/sbin/dupdate
NAME=dupdate
DESC="Update daemon"
DIR="/tmp/fwupdates"
FILE="run_update.sh"
ARGS="--dir $DIR --file $FILE --daemon"

test -f $DAEMON || exit 0

set -e

case "$1" in
    start)
        echo -n "starting $DESC: $NAME... "
	[ -d $DIR ] && rm -rf $DIR
	mkdir -p $DIR
	start-stop-daemon -S -n $NAME -a $DAEMON -- $ARGS
	echo "done."
	;;
    stop)
        echo -n "stopping $DESC: $NAME... "
	start-stop-daemon -K -n $NAME
	echo "done."
	;;
    restart)
        echo -n "restarting $DESC: $NAME... "
 	$0 stop
	$0 start
	echo "done."
	;;
    *)
	echo "Usage: $0 {start|stop|restart}"
	exit 1
	;;
esac
