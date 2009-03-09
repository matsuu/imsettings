##
# Global definitions
NULL =
INCLUDES =						\
	-DIMSETTINGS_LOCALEDIR="\"$(datadir)/locale\""	\
	$(IMSETTINGS_CFLAGS)				\
	-I$(top_srcdir)/imsettings			\
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
	$(autostart_DATA)				\
	$(NULL)


##
# Local Rules
@INTLTOOL_DESKTOP_RULE@

##
# Target platform
bin_PROGRAMS =						\
	imsettings-info					\
	imsettings-list					\
	imsettings-reload				\
	imsettings-restart				\
	imsettings-start				\
	imsettings-stop					\
	$(NULL)
#
autostartdir = $(sysconfdir)/xdg/autostart
#
autostart_in_files =					\
	imsettings-start.desktop.in			\
	$(NULL)
autostart_DATA = $(autostart_in_files:.desktop.in=.desktop)
#
imsettings_info_SOURCES =				\
	imsettings-info.c				\
	$(NULL)
imsettings_list_SOURCES =				\
	imsettings-list.c				\
	$(NULL)
imsettings_reload_SOURCES =				\
	imsettings-reload.c				\
	$(NULL)
imsettings_restart_SOURCES =				\
	imsettings-restart.c				\
	$(NULL)
imsettings_start_SOURCES =				\
	imsettings-start.c				\
	$(NULL)
imsettings_stop_SOURCES =				\
	imsettings-stop.c				\
	$(NULL)
