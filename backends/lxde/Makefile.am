##
# Global definitions
NULL =
INCLUDES =						\
	-DIMSETTINGS_LOCALEDIR="\"$(datadir)/locale\""	\
	-DLXDE_CONFIGDIR="\"$(sysconfdir)/lxde\""	\
	$(GCONF_CFLAGS)					\
	$(LXDE_CFLAGS)					\
	$(NULL)
LIBS =							\
	@LDFLAGS@					\
	$(GCONF_LIBS)					\
	$(LXDE_LIBS)					\
	$(NULL)
EXTRA_DIST =						\
	$(autostart_in_files)				\
	$(NULL)
DISTCLEANFILES =					\
	$(autostart_DATA)				\
	$(NULL)

##
# Local definitions
lxde_sources =						\
	main.c						\
	$(NULL)


##
# Local Rules
@INTLTOOL_DESKTOP_RULE@

##
# Target platform
bin_PROGRAMS =						\
	imsettings-lxde-helper				\
	$(NULL)
#
imsettings_lxde_helper_SOURCES =			\
	$(lxde_sources)					\
	$(NULL)
imsettings_lxde_helper_CFLAGS =				\
	-DG_LOG_DOMAIN="\"imsettings-lxde-helper\""	\
	$(NULL)
#
autostartdir = $(sysconfdir)/xdg/autostart
autostart_in_files =					\
	imsettings-lxde-helper.desktop.in		\
	$(NULL)
autostart_DATA = $(autostart_in_files:.desktop.in=.desktop)
