##
# Global definitions
NULL =
INCLUDES =						\
	-DIMSETTINGS_LOCALEDIR="\"$(datadir)/locale\""	\
	-DICONDIR="\"$(datadir)/pixmaps\""		\
	$(GCONF_CFLAGS)					\
	$(XFCE_XFCONF_CFLAGS)				\
	$(NULL)
LIBS =							\
	@LDFLAGS@					\
	$(GCONF_LIBS)					\
	$(XFCE_XFCONF_LIBS)				\
	$(NULL)
EXTRA_DIST =						\
	$(autostart_in_files)				\
	$(NULL)
DISTCLEANFILES =					\
	$(autostart_DATA)				\
	$(NULL)

##
# Local definitions
xfce_sources =						\
	main.c						\
	$(NULL)


##
# Local Rules
@INTLTOOL_DESKTOP_RULE@

##
# Target platform
bin_PROGRAMS =						\
	imsettings-xfce-helper				\
	$(NULL)
#
imsettings_xfce_helper_SOURCES =			\
	$(xfce_sources)					\
	$(NULL)
imsettings_xfce_helper_CFLAGS =				\
	-DG_LOG_DOMAIN="\"imsettings-xfce-helper\""	\
	$(NULL)
#
autostartdir = $(sysconfdir)/xdg/autostart
autostart_in_files =					\
	imsettings-xfce-helper.desktop.in		\
	$(NULL)
autostart_DATA = $(autostart_in_files:.desktop.in=.desktop)
