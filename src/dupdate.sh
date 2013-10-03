#!/bin/sh
DAEMON=/usr/sbin/dupdate
NAME=dupdate
DESC="Firmware update daemon"
DIR="/tmp/fwupdates"
EXEFILE="run"
ARGS="$DIR -x $EXEFILE -l"
PIDFILE=/var/run/dupdate.pid
test -f $DAEMON || exit 0

set -e

case "$1" in
    start)
        echo -n "starting $DESC: $NAME... "
	[ -d $DIR ] && rm -rf $DIR
	mkdir -p $DIR
	start-stop-daemon -S -m -p $PIDFILE -a $DAEMON -- $ARGS
	echo "done."
	;;
    stop)
        echo -n "stopping $DESC: $NAME... "
	start-stop-daemon -K -p $PIDFILE
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
