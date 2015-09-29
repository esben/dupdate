#!/bin/sh
DAEMON=/usr/bin/inotifyd
NAME=inotifyd
DESC="inotify event daemon"
PROG="/bin/echo"
ARGS="--syslog"
DIR="/tmp/somedir"
MASK=":wy"
PIDFILE=/var/run/inotifyd.pid
test -f $DAEMON || exit 0

set -e

case "$1" in
    start)
	echo -n "starting $DESC: $NAME... "
	mkdir -p $DIR
	start-stop-daemon -S -p $PIDFILE -a $DAEMON -- \
	    -p $PIDFILE -d $ARGS "$PROG" $DIR$MASK
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
