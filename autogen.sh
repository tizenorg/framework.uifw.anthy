#!/bin/sh

libtoolize=`which glibtoolize`
if test -z "$libtoolize"; then
    libtoolize=libtoolize
fi

$libtoolize --automake --copy --force
aclocal $ACLOCAL_AMFLAGS
autoheader
automake --add-missing --copy
autoconf
