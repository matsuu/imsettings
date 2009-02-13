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
	$(NULL)


##
# Local definitions
xfce_sources =						\
	imsettings_plugin.c				\
	$(NULL)


##
# Local Rules

##
# Target platform
plugindir = $(libdir)/xfce4/mcs-plugins
plugin_LTLIBRARIES =					\
	imsettings_plugin.la				\
	$(NULL)
#
imsettings_plugin_la_SOURCES =				\
	$(xfce_sources)					\
	$(NULL)
imsettings_plugin_la_LDFLAGS =				\
	-avoid-version					\
	-module						\
	-no-undefined					\
	$(NULL)
