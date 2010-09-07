#!/bin/sh
#
# This script prints a complete revision string based on information
# from git and/or the VERSIOn file.
#
# This is script is based on the setlocalversion file from the Linux kernel
#
# Author: Esben Haabendal <eha@doredevelopment.dk>
#

usage() {
	echo "Usage: $0 [srctree]" >&2
	exit 1
}

cd "${1:-.}" || usage

rc=0

file_version=`cat VERSION 2>/dev/null | tr -d '\n'`
version=${file_version:-UNKNOWN}

# Check for git and a git repo.
if head=`git rev-parse --verify --short HEAD 2>/dev/null`; then
    # git repository detected

    version=`git describe --exact-match 2>/dev/null`

    if [ -n "$version" ]; then
	# on a tagged commit

	echo -n $version

    else
	# not on a tagged commit

	if version="`git describe 2>/dev/null`"; then
	    # based on a tagged commit

	    tag_version=`echo "$version" | sed -e 's/\(.*\)-[^-]*-[^-]*/\1/'`

	    if [ "$tag_version" != "$file_version" ]; then
		echo "Warning: VERSION file does not match newest tag ($file_version != $tag_version)" >&2
		rc=1
	    fi

	    echo "$version" \
		| awk -F- '{printf("%s-%05d-%s", tag_version, $(NF-1),$(NF))}' \
		  tag_version=$tag_version

	else
	    # Not based on any tagged commits

	    printf '%s-g%s' $file_version $head

	fi

    fi

    # Update index only on r/w media
    [ -w . ] && git update-index --refresh --unmerged > /dev/null

    # Check for uncommitted changes
    if git diff-index --name-only HEAD \
	| grep -v "^version.sh" \
	| read dummy; then
	echo -n "-dirty"
#    else
#	echo
    fi

    # All done with git
    exit $rc

fi

# If not a git repo, we don't have a way to know if the sources are
# pristine so we cannot do much better than just printing the version
# number from the VERSION file
echo -n $file_version
exit $rc
