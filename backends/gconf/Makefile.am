##
# Global definitions
NULL =
INCLUDES =						\
	-DIMSETTINGS_LOCALEDIR="\"$(datadir)/locale\""	\
	-I$(top_srcdir)/imsettings			\
	$(IMSETTINGS_CFLAGS)				\
	$(GCONF_CFLAGS)					\
	$(NULL)
LIBS =							\
	@LDFLAGS@					\
	$(IMSETTINGS_LIBS)				\
	$(GCONF_LIBS)					\
	$(NULL)
EXTRA_DIST =						\
	introspection.xml				\
	gconf-im-settings-daemon.service.in		\
	$(NULL)
MAINTAINERCLEANFILES =					\
	$(gconf_built_public_headers)			\
	$(gconf_built_private_headers)			\
	$(gconf_built_sources)				\
	$(stamp_files)					\
	$(NULL)


##
# Local definitions
gconf_public_headers =					\
	$(NULL)
gconf_private_headers =					\
	$(NULL)
gconf_built_public_headers =				\
	$(NULL)
gconf_built_private_headers =				\
	gconf-imsettings-glue.h				\
	$(NULL)
#
gconf_built_sources =					\
	$(NULL)
gconf_sources =						\
	main.c						\
	$(gconf_built_sources)				\
	$(NULL)
#
stamp_files =						\
	stamp-glue.h					\
	$(NULL)


##
# Local Rules
main.c: gconf-imsettings-glue.h
	@true
gconf-imsettings-glue.h: stamp-glue.h
	@true
stamp-glue.h: @REBUILD@ $(srcdir)/introspection.xml Makefile
	dbus-binding-tool --prefix=gconf_imsettings \
			  --mode=glib-server \
			  --output=gconf-imsettings-glue.h $(srcdir)/introspection.xml && \
	echo timestamp > $(@F)

%.service: %.service.in $(top_builddir)/config.log
	sed -e "s|\@libexecdir\@|$(libexecdir)|" $< > $@


##
# Target platform
libexec_PROGRAMS = gconf-im-settings-daemon
#
noinst_HEADERS =					\
	$(gconf_private_headers)			\
	$(gconf_built_private_headers)			\
	$(NULL)
#
gconf_im_settings_daemon_SOURCES =			\
	$(gconf_sources)				\
	$(NULL)
gconf_im_settings_daemon_CFLAGS =			\
	-DG_LOG_DOMAIN="\"Gconf-IM-Settings-Daemon\""	\
	$(NULL)
#
servicedir = $(DBUS_SERVICE_DIR)
service_DATA = gconf-im-settings-daemon.service
