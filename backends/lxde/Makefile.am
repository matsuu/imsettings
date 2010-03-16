##
# Global definitions
NULL =
INCLUDES =						\
	-DIMSETTINGS_LOCALEDIR="\"$(datadir)/locale\""	\
	-DLXDE_CONFIGDIR="\"$(sysconfdir)/lxde\""	\
	-I$(top_srcdir)/imsettings			\
	$(IMSETTINGS_CFLAGS)				\
	$(LXDE_CFLAGS)					\
	$(NULL)
LIBS =							\
	@LDFLAGS@					\
	$(IMSETTINGS_LIBS)				\
	$(LXDE_LIBS)					\
	$(top_builddir)/imsettings/libimsettings.la	\
	$(NULL)
DEPS =							\
	$(top_builddir)/imsettings/libimsettings.la	\
	$(NULL)
EXTRA_DIST =						\
	introspection.xml				\
	lxde-im-settings-daemon.service.in		\
	$(NULL)
MAINTAINERCLEANFILES =					\
	$(lxde_built_public_headers)			\
	$(lxde_built_private_headers)			\
	$(lxde_built_sources)				\
	$(stamp_files)					\
	$(NULL)


##
# Local definitions
lxde_public_headers =					\
	$(NULL)
lxde_private_headers =					\
	$(NULL)
lxde_built_public_headers =				\
	$(NULL)
lxde_built_private_headers =				\
	lxde-imsettings-glue.h				\
	$(NULL)
#
lxde_built_sources =					\
	$(NULL)
lxde_sources =						\
	main.c						\
	$(lxde_built_sources)				\
	$(NULL)
#
stamp_files =						\
	stamp-glue.h					\
	$(NULL)


##
# Local Rules
main.c: lxde-imsettings-glue.h
	@true
lxde-imsettings-glue.h: stamp-glue.h
	@true
stamp-glue.h: @REBUILD@ $(srcdir)/introspection.xml Makefile
	dbus-binding-tool --prefix=lxde_imsettings \
			  --mode=glib-server \
			  --output=lxde-imsettings-glue.h $(srcdir)/introspection.xml && \
	echo timestamp > $(@F)

%.service: %.service.in $(top_builddir)/config.log
	sed -e "s|\@libexecdir\@|$(libexecdir)|" $< > $@


##
# Target platform
libexec_PROGRAMS = lxde-im-settings-daemon
#
noinst_HEADERS =					\
	$(lxde_private_headers)				\
	$(lxde_built_private_headers)			\
	$(NULL)
#
lxde_im_settings_daemon_SOURCES =			\
	$(lxde_sources)					\
	$(NULL)
lxde_im_settings_daemon_CFLAGS =			\
	-DG_LOG_DOMAIN="\"LXDE-IM-Settings-Daemon\""	\
	$(NULL)
#
servicedir = $(DBUS_SERVICE_DIR)
service_DATA = lxde-im-settings-daemon.service
