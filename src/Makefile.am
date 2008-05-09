##
# Global definitions
NULL =
INCLUDES =						\
	-DIMSETTINGS_LOCALEDIR="\"$(datadir)/locale\""	\
	-DXINIT_PATH="\"$(XINIT_PATH)\""		\
	-DXINPUTRC_PATH="\"$(XINPUTRC_PATH)\""		\
	-DXINPUT_PATH="\"$(XINPUT_PATH)\""		\
	-DXINPUT_SUFFIX="\"$(XINPUT_SUFFIX)\""		\
	-D_GNU_SOURCE					\
	-I$(top_srcdir)/imsettings			\
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
	im-info-daemon.service.in			\
	im-settings-daemon.service.in			\
	$(NULL)
DISTCLEANFILES =					\
	im-info-daemon.service				\
	im-settings-daemon.service			\
	$(NULL)


##
# Local definitions

##
# Local Rules
%.service: %.service.in $(top_builddir)/config.log
	sed -e "s|\@libexecdir\@|$(libexecdir)|" $< > $@

##
# Target platform
libexec_PROGRAMS = im-settings-daemon im-info-daemon
#
im_settings_daemon_SOURCES =				\
	factory.c					\
	$(NULL)
im_settings_daemon_CFLAGS =				\
	-DG_LOG_DOMAIN="\"IM-Settings-Daemon\""		\
	$(X11_CFLAGS)					\
	$(NULL)
im_settings_daemon_LDADD =				\
	$(X11_LIBS)					\
	$(NULL)
#
im_info_daemon_SOURCES =				\
	factory_info.c					\
	$(NULL)
im_info_daemon_CFLAGS =					\
	-DG_LOG_DOMAIN="\"IM-Info-Daemon\""		\
	-fno-strict-aliasing				\
	$(IMINFO_CFLAGS)				\
	$(NULL)
im_info_daemon_LDADD =					\
	$(IMINFO_LIBS)					\
	$(NULL)
#
servicedir = $(DBUS_SERVICE_DIR)
service_DATA =						\
	im-info-daemon.service				\
	im-settings-daemon.service			\
	$(NULL)
