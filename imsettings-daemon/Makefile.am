##
# Global definitions
NULL =
INCLUDES =							\
	-DIMSETTINGS_LOCALEDIR="\"$(datadir)/locale\""		\
	-DXINIT_PATH="\"$(XINIT_PATH)\""			\
	-DXINPUTRC_PATH="\"$(XINPUTRC_PATH)\""			\
	-DXINPUT_PATH="\"$(XINPUT_PATH)\""			\
	-DXINPUT_SUFFIX="\"$(XINPUT_SUFFIX)\""			\
	-DIMSETTINGS_MODULE_PATH="\"$(IMSETTINGS_MODULEDIR)\""	\
	-D_GNU_SOURCE						\
	-I$(top_srcdir)/imsettings				\
	$(IMSETTINGS_CFLAGS)					\
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
	imsettings-daemon.service.in			\
	$(NULL)
DISTCLEANFILES =					\
	imsettings-daemon.service			\
	$(NULL)

##
# Local definitions

##
# Local Rules
%.service: %.service.in $(top_builddir)/config.log
	sed -e "s|\@libexecdir\@|$(libexecdir)|" $< > $@

##
# Target platform
libexec_PROGRAMS =					\
	imsettings-daemon				\
	$(NULL)
#
imsettings_daemon_SOURCES =				\
	imsettings-proc.c				\
	imsettings-proc.h				\
	imsettings-server.c				\
	imsettings-server.h				\
	main.c						\
	$(NULL)
imsettings_daemon_CFLAGS =				\
	-DG_LOG_DOMAIN="\"IMSettings-Daemon\""		\
	$(IMDAEMON_CFLAGS)				\
	$(NULL)
imsettings_daemon_LDADD =				\
	$(IMDAEMON_LIBS)				\
	$(NULL)
#
servicedir = $(DBUS_SERVICE_DIR)
service_DATA =						\
	imsettings-daemon.service			\
	$(NULL)
