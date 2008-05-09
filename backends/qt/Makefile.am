##
# Global definitions
NULL =
INCLUDES =						\
	-DIMSETTINGS_LOCALEDIR="\"$(datadir)/locale\""	\
	-I$(top_srcdir)/imsettings			\
	$(IMSETTINGS_CFLAGS)				\
	$(QT_CFLAGS)					\
	$(NULL)
LIBS =							\
	@LDFLAGS@					\
	$(QT_LIBS)					\
	$(NULL)
EXTRA_DIST =						\
	introspection.xml				\
	qt-im-settings-daemon.service.in		\
	$(NULL)
MAINTAINERCLEANFILES =					\
	$(qt_built_public_headers)			\
	$(qt_built_private_headers)			\
	$(qt_built_sources)				\
	$(stamp_files)					\
	$(NULL)


##
# Local definitions
qt_public_headers =					\
	$(NULL)
qt_private_headers =					\
	qt-imsettings.h					\
	$(NULL)
qt_built_public_headers =				\
	$(NULL)
qt_built_private_headers =				\
	qt-imsettings-glue.h				\
	$(NULL)
#
qt_built_sources =					\
	moc_qt-imsettings.cpp				\
	moc_qt-imsettings-glue.cpp			\
	qt-imsettings-glue.cpp				\
	$(NULL)
qt_sources =						\
	main.cpp					\
	qt-imsettings.cpp				\
	$(qt_built_sources)				\
	$(NULL)
#
stamp_files =						\
	stamp-moc_qt-imsettings.cpp			\
	stamp-glue.h					\
	$(NULL)


##
# Local Rules
main.cpp: qt-imsettings-glue.h
	@true
qt-imsettings.cpp: moc_qt-imsettings.cpp
	@true
moc_qt-imsettings.cpp: stamp-moc_qt-imsettings.cpp
	@true
stamp-moc_qt-imsettings.cpp: @REBUILD@ $(srcdir)/qt-imsettings.h Makefile
	$(MOC) $(srcdir)/qt-imsettings.h -o moc_qt-imsettings.cpp && \
	echo timestamp > $(@F)
qt-imsettings-glue.h qt-imsettings-glue.cpp: stamp-glue.h
	@true
stamp-glue.h: @REBUILD@ $(srcdir)/introspection.xml Makefile
	qdbusxml2cpp -c IMSettingsQtDBusAdaptor \
		     -a qt-imsettings-glue \
		     $(srcdir)/introspection.xml com.redhat.imsettings.Qt && \
	echo timestamp > $(@F)
moc_qt-imsettings-glue.cpp: @REBUILD@ qt-imsettings-glue.h Makefile
	$(MOC) qt-imsettings-glue.h -o $@

%.service: %.service.in $(top_builddir)/config.log
	sed -e "s|\@libexecdir\@|$(libexecdir)|" $< > $@


##
# Target platform
libexec_PROGRAMS = qt-im-settings-daemon
#
noinst_HEADERS =					\
	$(qt_private_headers)				\
	$(qt_built_private_headers)			\
	$(NULL)

qt_im_settings_daemon_SOURCES =				\
	$(qt_sources)					\
	$(NULL)
#
servicedir = $(DBUS_SERVICE_DIR)
service_DATA = qt-im-settings-daemon.service
