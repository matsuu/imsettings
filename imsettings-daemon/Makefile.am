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
	marshal.list					\
	$(NULL)
CLEANFILES =						\
	$(gen_files)					\
	$(NULL)
DISTCLEANFILES =					\
	imsettings-daemon.service			\
	$(NULL)
MAINTAINERCLEANFILES =					\
	$(built_headers)				\
	$(built_sources)				\
	$(stamp_files)					\
	$(NULL)
BUILT_SOURCES =						\
	$(built_headers)				\
	$(built_sources)				\
	$(NULL)

##
# Local definitions
built_headers =						\
	imsettings-marshal.h				\
	$(NULL)
built_sources =						\
	imsettings-marshal.c				\
	$(NULL)
stamp_files =						\
	stamp-marshal.h					\
	$(NULL)
gen_files =						\
	xgen-imh					\
	xgen-imc					\
	$(NULL)
##
# Local Rules
%.service: %.service.in $(top_builddir)/config.log
	sed -e "s|\@libexecdir\@|$(libexecdir)|" $< > $@

imsettings-marshal.h: stamp-marshal.h
	@true
stamp-marshal.h: @REBUILD@ marshal.list Makefile
	$(AM_V_GEN) $(GLIB_GENMARSHAL) --prefix=imsettings_cclosure_marshal $(srcdir)/marshal.list --header > xgen-imh	\
	&& (cmp -s xgen-imh imsettings-marshal.h 2>/dev/null || cp xgen-imh imsettings-marshal.h)	\
	&& rm -f xgen-imh xgen-imh~					\
	&& echo timestamp > $@
imsettings-marshal.c: @REBUILD@ stamp-marshal.h Makefile
	$(AM_V_GEN) echo "#include \"imsettings-marshal.h\"" > xgen-imc	\
	&& $(GLIB_GENMARSHAL) --nostdinc --prefix=imsettings_cclosure_marshal $(srcdir)/marshal.list --body >> xgen-imc	\
	&& cp xgen-imc imsettings-marshal.c				\
	&& rm -f xgen-imc xgen-imc~

##
# Target platform
libexec_PROGRAMS =					\
	imsettings-daemon				\
	$(NULL)
#
imsettings_daemon_SOURCES =				\
	$(built_headers)				\
	$(built_sources)				\
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
