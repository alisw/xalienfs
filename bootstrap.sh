#!/bin/sh
#
# Author: Derek Feichtinger, 19 Oct 2005

# create autotools build files from the CVS sources
LIBTOOLIZE=libtoolize

if test -x "`which glibtoolize 2>/dev/null`"; then
    LIBTOOLIZE=glibtoolize
fi

$LIBTOOLIZE --copy --force
autoheader 
aclocal
automake -acf
autoconf

