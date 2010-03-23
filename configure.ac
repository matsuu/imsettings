dnl Process this file with autoconf to produce a configure script.
AC_INIT([imsettings], 0.108.0, [http://code.google.com/p/imsettings/issues/entry])

. ./requires

AM_INIT_AUTOMAKE($PACKAGE_NAME, $PACKAGE_VERSION)
AM_MAINTAINER_MODE
AM_CONFIG_HEADER([config.h])

AC_PROG_LIBTOOL
AC_PROG_INTLTOOL

AC_PROG_CC
AC_PROG_CXX

GNOME_COMPILE_WARNINGS

GNOME_COMMON_INIT
GNOME_DEBUG_CHECK
GNOME_MAINTAINER_MODE_DEFINES

CFLAGS="$CFLAGS $WARN_CFLAGS"

dnl ======================================================================
dnl Libraries versioning
dnl ======================================================================
dnl Quote from Updating library version information at libtool.info
dnl and renumbering
dnl
dnl 1. Update the version information only immediately before a public
dnl    release of your software.  More frequent updates are unnecessary,
dnl    and only guarantee that the current interface number gets larger
dnl    faster.
dnl 2. If the library source code has changed at all since the last
dnl    update, then increment REVISION (`C:R:A' becomes `C:r+1:A')
dnl 3. If any interfaces have been added, removed, or changed since the
dnl    last update, increment CURRENT, and set REVISION to 0.
dnl 4. If any interfaces have been added since the last public release,
dnl    then increment AGE.
dnl 5. If any interfaces have been removed since the last public release,
dnl    then set AGE to 0.
dnl
LT_CURRENT=6
LT_REVISION=0
LT_AGE=2

LT_XIM_CURRENT=1
LT_XIM_REVISION=3
LT_XIM_AGE=0

AC_SUBST(LT_CURRENT)
AC_SUBST(LT_REVISION)
AC_SUBST(LT_AGE)
AC_SUBST(LT_XIM_CURRENT)
AC_SUBST(LT_XIM_REVISION)
AC_SUBST(LT_XIM_AGE)

dnl ======================================================================
dnl define variables
dnl ======================================================================
DBUS_SERVICE_DIR=`pkg-config --variable session_bus_services_dir dbus-1`
AC_SUBST(DBUS_SERVICE_DIR)
MOC=`pkg-config --variable moc_location QtCore`
[ -z $MOC ] && MOC=/bin/true
AC_SUBST(MOC)

dnl ======================================================================
dnl functions testing
dnl ======================================================================

dnl ======================================================================
dnl gettext stuff
dnl ======================================================================
GETTEXT_PACKAGE=$PACKAGE
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE", [Gettext package])

AM_GLIB_GNU_GETTEXT

dnl ======================================================================
dnl check pkg-config stuff
dnl ======================================================================
PKG_CHECK_MODULES(IMSETTINGS, dbus-1 >= $DBUS_REQUIRED
			      dbus-glib-1 >= $DBUS_GLIB_REQUIRED
			      gio-2.0 >= $GIO_REQUIRED)
PKG_CHECK_MODULES(IMDAEMON, gtk+-2.0
			    libnotify)
PKG_CHECK_MODULES(X11, x11)
PKG_CHECK_MODULES(CHECK, check >= $CHECK_REQUIRED,
	[use_check="yes"],
	[use_check="no"])
PKG_CHECK_MODULES(GCONF, gconf-2.0,
	[use_gconf="yes"],
	[use_gconf="no"])
PKG_CHECK_MODULES(QT, QtCore
		      QtDBus,
	[use_qt="yes"],
	[use_qt="no"])
PKG_CHECK_MODULES(XFCE_MCS, gconf-2.0
			libxfce4mcs-manager-1.0
			libxfce4util-1.0
			xfce-mcs-manager,
	[use_xfce_mcs="yes"],
	[use_xfce_mcs="no"])
PKG_CHECK_MODULES(XFCE_XFCONF, gconf-2.0
			libxfconf-0,
	[use_xfce_xfconf="yes"],
	[use_xfce_xfconf="no"])
PKG_CHECK_MODULES(LXDE, gtk+-2.0,
	[use_lxde="yes"],
	[use_lxde="no"])
PKG_CHECK_MODULES(XIM, libgxim >= $LIBGXIM_REQUIRED
		       gdk-2.0 >= $GDK_REQUIRED,
	[use_xim="yes"],
	[use_xim="no"])
PKG_CHECK_MODULES(APPLET, gtk+-2.0
			  libnotify
			  gconf-2.0
			  x11,
	[use_applet="yes"],
	[use_applet="no"])

AC_SUBST(IMSETTINGS_CFLAGS)
AC_SUBST(IMSETTINGS_LIBS)
AC_SUBST(IMINFO_CFLAGS)
AC_SUBST(IMINFO_LIBS)
AC_SUBST(X11_CFLAGS)
AC_SUBST(X11_LIBS)
AC_SUBST(GCONF_CFLAGS)
AC_SUBST(GCONF_LIBS)
AC_SUBST(QT_CFLAGS)
AC_SUBST(QT_LIBS)
AC_SUBST(XFCE_MCS_CFLAGS)
AC_SUBST(XFCE_MCS_LIBS)
AC_SUBST(XFCE_XFCONF_CFLAGS)
AC_SUBST(XFCE_XFCONF_LIBS)
AC_SUBST(LXDE_CFLAGS)
AC_SUBST(LXDE_LIBS)
AC_SUBST(XIM_CFLAGS)
AC_SUBST(XIM_LIBS)
AC_SUBST(XSETTINGS_CFLAGS)
AC_SUBST(XSETTINGS_LIBS)
AC_SUBST(APPLET_CFLAGS)
AC_SUBST(APPLET_LIBS)
AC_SUBST(CHECK_CFLAGS)
AC_SUBST(CHECK_LIBS)

AM_CONDITIONAL(ENABLE_UNIT_TEST, test x$use_check != xno)
AM_CONDITIONAL(ENABLE_GCONF, test x$use_gconf != xno)
AM_CONDITIONAL(ENABLE_QT, test x$use_qt != xno)
AM_CONDITIONAL(ENABLE_XFCE_MCS, test x$use_xfce_mcs != xno)
AM_CONDITIONAL(ENABLE_XFCE_XFCONF, test x$use_xfce_xfconf != xno)
AM_CONDITIONAL(ENABLE_LXDE, test x$use_lxde != xno)
AM_CONDITIONAL(ENABLE_XIM, test x$use_xim != xno)
AM_CONDITIONAL(ENABLE_APPLET, test x$use_applet != xno)

dnl ======================================================================
dnl check another libraries
dnl ======================================================================
AM_PATH_GLIB_2_0($GLIB_REQUIRED, :,
	AC_MSG_ERROR([
*** GLIB $GLIB_REQUIRED_VERSION or better is required. The latest version of
*** GLIB is always available from ftp://ftp.gtk.org/.]),
    glib)
if test "x$use_gconf" = "xyes"; then
   	AC_PATH_PROG(GCONFTOOL, gconftool-2, no)
	if test x"$GCONFTOOL" = "xno"; then
		AC_MSG_ERROR([gconftool-2 executable not found in your path - should be installed with GConf])
	fi
	AM_GCONF_SOURCE_2
else
	AM_CONDITIONAL(GCONF_SCHEMAS_INSTALL, 0)
fi

dnl ======================================================================
dnl options
dnl ======================================================================
AC_ARG_ENABLE(rebuilds,
	[AC_HELP_STRING([--disable-rebuilds],
		[disable all source autogeneration rules])],,
	[enable_rebuilds=yes])
AC_ARG_WITH(xinputdir,
	AC_HELP_STRING([--with-xinputdir], [build with the specific xinput script directory])
	,,
	with_xinputdir=no)
AC_ARG_WITH(xinputrcdir,
	AC_HELP_STRING([--with-xinputrcdir], [build with the specific xinputrc directory])
	,,
	with_xinputrcdir=no)
AC_ARG_WITH(xinitdir,
	AC_HELP_STRING([--with-xinitdir], [build with the specific xinit script directory])
	,,
	with_xinitdir=no)
AC_ARG_WITH(xinput-suffix,
	AC_HELP_STRING([--with-xinput-suffix], [specify the xinput file suffix])
	,,
	with_xinput_suffix=".conf")
AC_ARG_WITH(xinputsh,
	AC_HELP_STRING([--with-xinputsh], [specify the xinput shell script name])
	,,
	with_xinputsh=no)

dnl ======================================================================
dnl options - rebuilds
dnl ======================================================================
REBUILD=\#
if test "x$enable_rebuilds" = "xyes"; then
	REBUILD=
fi
AC_SUBST(REBUILD)

dnl ======================================================================
dnl options - xinputdir
dnl ======================================================================
AC_MSG_CHECKING(whether to build the specific xinput script directory)
if test "x$with_xinputdir" = xno; then
	with_xinputdir="${sysconfdir}/X11/xinit/xinput.d/"
	AC_MSG_RESULT(no)
else
	AC_MSG_RESULT(yes)
fi
XINPUT_PATH=$with_xinputdir
AC_SUBST(XINPUT_PATH)

dnl ======================================================================
dnl options - xinputrcdir
dnl ======================================================================
AC_MSG_CHECKING(whether to build the specific xinputrc directory)
if test "x$with_xinputrcdir" = xno; then
	with_xinputrcdir="${sysconfdir}/X11/xinit/"
	AC_MSG_RESULT(no)
else
	AC_MSG_RESULT(yes)
fi
XINPUTRC_PATH=$with_xinputrcdir
AC_SUBST(XINPUTRC_PATH)

dnl ======================================================================
dnl options - xinputsh
dnl ======================================================================
AC_MSG_CHECKING(whether to give the specific xinput.sh name)
if test "x$with_xinputsh" = xno; then
	with_xinputsh="xinput.sh"
	AC_MSG_RESULT(no)
else
	AC_MSG_RESULT(yes)
fi
XINPUTSH=$with_xinputsh
AC_SUBST(XINPUTSH)

dnl ======================================================================
dnl options - xinitdir
dnl ======================================================================
AC_MSG_CHECKING(whether to build the specific xinit script directory)
if test "x$with_xinitdir" = xno; then
	with_xinitdir="${sysconfdir}/X11/xinit/xinitrc.d/"
	AC_MSG_RESULT(no)
else
	AC_MSG_RESULT(yes)
fi
XINIT_PATH=$with_xinitdir
AC_SUBST(XINIT_PATH)

dnl ======================================================================
dnl options - xinit-suffix
dnl ======================================================================
AC_MSG_CHECKING(whether to build the specific xinput suffix)
if test "x$with_xinput_suffix" = xno; then
   	with_xinput_suffix=""
	AC_MSG_RESULT(no)
else
	AC_MSG_RESULT(yes)
fi
XINPUT_SUFFIX=$with_xinput_suffix
AC_SUBST(XINPUT_SUFFIX)

dnl ======================================================================
dnl output
dnl ======================================================================
AC_CONFIG_FILES([
	Makefile
	imsettings.pc
	imsettings-uninstalled.pc
	applet/Makefile
	applet/imsettings-applet.desktop.in
	backends/Makefile
	backends/gconf/Makefile
	backends/lxde/Makefile
	backends/qt/Makefile
	backends/xfce_mcs/Makefile
	backends/xfce_xfconf/Makefile
	backends/xim/Makefile
	data/Makefile
	data/none.in
	data/xcompose.in
	data/xim.in
	data/xinput.sh.in
	data/xinputinfo.sh
	imsettings/Makefile
	po/Makefile.in
	src/Makefile
	utils/Makefile
	tests/Makefile
	tests/testcases/Makefile
])
AC_OUTPUT

dnl ======================================================================
dnl result
dnl ======================================================================
echo ""
echo "========== Build Information =========="
echo " CFLAGS:                 $CFLAGS $IMSETTINGS_CFLAGS"
echo " LDFLAGS:                $LDFLAGS $IMSETTINGS_LIBS"
echo " GConf support:          $use_gconf"
if test "x$use_gconf" = "xyes"; then
   echo "   CFLAGS:               $GCONF_CFLAGS"
   echo "   LDFLAGS:              $GCONF_LIBS"
fi
echo " Qt support:             $use_qt"
if test "x$use_qt" = "xyes"; then
   echo "   CFLAGS:               $QT_CFLAGS"
   echo "   LDFLAGS:              $QT_LIBS"
fi
echo " Xfce mcs support:       $use_xfce_mcs"
if test "x$use_xfce_mcs" = "xyes"; then
   echo "   CFLAGS:               $XFCE_MCS_CFLAGS"
   echo "   LDFLAGS:              $XFCE_MCS_LIBS"
fi
echo " Xfce xfconf support:    $use_xfce_xfconf"
if test "x$use_xfce_xfconf" = "xyes"; then
   echo "   CFLAGS:               $XFCE_XFCONF_CFLAGS"
   echo "   LDFLAGS:              $XFCE_XFCONF_LIBS"
fi
echo " LXDE support:           $use_lxde"
if test "x$use_lxde" = "xyes"; then
   echo "   CFLAGS:               $LXDE_CFLAGS"
   echo "   LDFLAGS:              $LXDE_LIBS"
fi
echo " XIM support:            $use_xim"
if test "x$use_xim" = "xyes"; then
   echo "   CFLAGS:               $XIM_CFLAGS"
   echo "   LDFLAGS:              $XIM_LIBS"
fi
echo " Applet support:         $use_applet"
if test "x$use_applet" = "xyes"; then
   echo "   CFLAGS:               $APPLET_CFLAGS"
   echo "   LDFLAGS:              $APPLET_LIBS"
fi
echo " XINPUT_PATH:            $XINPUT_PATH"
echo " XINPUTRC_PATH:          $XINPUTRC_PATH"
echo " XINIT_PATH:             $XINIT_PATH"
echo " XINPUTSH:               $XINPUTSH"
echo " XINPUT_SUFFIX:          $XINPUT_SUFFIX"
echo " Rebuilds:               $enable_rebuilds"
echo " Unit testing:           $use_check"
echo ""
