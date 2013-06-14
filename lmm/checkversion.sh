#!/bin/bash

cd $1

HEADER_VERSION=$(grep VERSION_INFO version.h | cut -d " " -f 3)

CURR_VERSION=$(git describe --abbrev=0)

if [ -z "`git describe --exact-match 2>/dev/null`" ]; then
	# If we are past a tagged commit (like "v2.6.30-rc5-302-g72357d5"),
	# we pretty print it.
	if atag="`git describe 2>/dev/null`"; then
		CURR_VERSION=${CURR_VERSION}$(echo "$atag" | awk -F- '{printf("-%05d-%s", $(NF-1),$(NF))}')
	# If we don't have a tag at all we print -g{commitish}.
	else
		CURR_VERSION=${CURR_VERSION}$(printf '%s%s' -g $head)
	fi
fi

if git diff-index --name-only HEAD | grep -v "^scripts/package" | read dummy; then
	CURR_VERSION=${CURR_VERSION}$(printf '%s' -dirty)
fi

if [[ \"$CURR_VERSION\" != $HEADER_VERSION ]]
then
	echo "Header version needs update"
	echo "#ifndef VERSION_H" > version.h
	echo "#define VERSION_H" >> version.h
	echo "#define VERSION_INFO \"$CURR_VERSION"\" >> version.h
	echo "#endif" >> version.h
else
	echo "Header version is up to date."
fi
