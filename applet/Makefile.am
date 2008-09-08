##
# Global definitions
NULL =
INCLUDES =						\
	-DIMAPPLET_LOCALEDIR="\"$(datadir)/locale\""	\
	-DICONDIR="\"$(datadir)/pixmaps\""		\
	-l$(top_srcdir)/imsettings			\
	$(IMSETTINGS_CFLAGS)				\
	$(NULL)
LIBS =							\
	@LDFLAGS@					\
	$(IMSETTINGS_LIBS)				\
	$(top_builddir)/imsettings/libimsettings.la	\
	$(NULL)
DEPS =							\
	$(top_builddir)/imsettings/libimsettings.la	\
	$(NULL)
EXTRA_DIST =						\
	$(autostart_in_files)				\
	$(NULL)
DISTCLEANFILES =					\
	$(NULL)

if ENABLE_XIM
INCLUDES += 						\
	-I$(top_srcdir)/backends/xim			\
	-DENABLE_XIM					\
	$(XIM_CFLAGS)					\
	$(NULL)
LIBS += $(top_builddir)/backends/xim/libimsettings-xim.la
DEPS += $(top_builddir)/backends/xim/libimsettings-xim.la
endif
if ENABLE_XSETTINGS
INCLUDES += -DENABLE_XSETTINGS
LIBS += $(top_builddir)/imsettings/libimsettings-xsettings.la
DEPS += $(top_builddir)/imsettings/libimsettings-xsettings.la
endif

##
# Local definitions

##
# Local Rules
@INTLTOOL_DESKTOP_RULE@

##
# Target platform
bin_PROGRAMS = imsettings-applet
#
autostartdir = $(sysconfdir)/xdg/autostart
#
autostart_in_files = imsettings-applet.desktop.in
autostart_DATA = $(autostart_in_files:.desktop.in=.desktop)
#
imsettings_applet_SOURCES =				\
	main.c						\
	radiomenuitem.c					\
	radiomenuitem.h					\
	$(NULL)
imsettings_applet_CFLAGS =				\
	-DG_LOG_DOMAIN="\"IMSettings-Applet\""		\
	$(APPLET_CFLAGS)				\
	$(NULL)
imsettings_applet_LDADD =				\
	$(APPLET_LIBS)					\
	$(NULL)
