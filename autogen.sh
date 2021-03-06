#!/bin/sh
# Run this to generate all the initial makefiles, etc.

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

PKG_NAME="imsettings"

(test -f $srcdir/configure.ac \
  && test -f $srcdir/README \
  && test -d $srcdir/imsettings) || {
    echo -n "**Error**: Directory "\`$srcdir\'" does not look like the"
    echo " top-level $PKG_NAME directory"
    exit 1
}

which gnome-autogen.sh || {
    echo "You need to install gnome-common from the GNOME CVS"
    exit 1
}
ACLOCAL_FLAGS="$ACLOCAL_FLAGS -I m4macros"
REQUIRED_AUTOMAKE_VERSION=1.11 . gnome-autogen.sh
