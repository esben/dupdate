/sh

if [ "$1" != "--" ]; then
    exec $0 -- | logger -s -t $0
    exit
fi
exec 2>&1

set -e
trap onexit 1 2 3 15

# $1 integer (optional) Exit status. If not set, use `$?'
onexit() {
    local exit_status=${1:-$?}
    if [ "$exit_status" = 0 ]; then
	echo "success"
    else
	echo "error $exit_status"
    fi
    exit $exit_status
}

CHKSUM_FILE="sha256sum.txt"
verify_chksum() {
    echo "verifying checksums in $CHKSUM_FILE"
    if sha256sum -c $CHKSUM_FILE ; then
	echo
    else
	echo "error: checksum verification failed"
	onexit
    fi
}
if [ -f $CHKSUM_FILE ] ; then
    verify_chksum
fi

# do stuff here
ls

onexit
