##
# Global definitions
NULL =
INCLUDES =						\
	-DIMSETTINGS_LOCALEDIR="\"$(datadir)/locale\""	\
	-DXINIT_PATH="\"$(XINIT_PATH)\""		\
	-DXINPUT_PATH="\"$(XINPUT_PATH)\""		\
	-DXINPUT_SUFFIX="\"$(XINPUT_SUFFIX)\""		\
	-DXINPUTINFO_PATH="\"$(libexecdir)\""		\
	-I$(top_srcdir)/imsettings/			\
	$(IMSETTINGS_CFLAGS)				\
	$(NULL)
LIBS =							\
	@LDFLAGS@					\
	$(IMSETTINGS_LIBS)				\
	$(NULL)
EXTRA_DIST =						\
	imsettings.xml					\
	marshal.list					\
	$(NULL)
MAINTAINERCLEANFILES =					\
	$(imsettings_built_public_headers)		\
	$(imsettings_built_private_headers)		\
	$(imsettings_built_sources)			\
	$(stamp_files)					\
	$(NULL)
CLEANFILES =						\
	xgen-imh					\
	xgen-imc					\
	$(NULL)


##
# Local definitions
imsettings_public_headers =				\
	imsettings.h					\
	imsettings-info.h				\
	imsettings-observer.h				\
	imsettings-request.h				\
	imsettings-utils.h				\
	$(NULL)
imsettings_private_headers =				\
	imsettings-info-private.h			\
	$(NULL)
imsettings_built_public_headers =			\
	imsettings-glib-bindings.h			\
	$(NULL)
imsettings_built_private_headers =			\
	imsettings-glib-glue.h				\
	imsettings-marshal.h				\
	$(NULL)
#
imsettings_built_sources =				\
	imsettings-marshal.c				\
	$(NULL)
imsettings_sources =					\
	imsettings-info.c				\
	imsettings-observer.c				\
	imsettings-request.c				\
	imsettings-utils.c				\
	$(imsettings_built_sources)			\
	$(NULL)
#
stamp_files =						\
	stamp-bindings.h				\
	stamp-marshal.h					\
	$(NULL)


##
# Local Rules
imsettings-observer.c: imsettings-marshal.h imsettings-glib-glue.h
	@true
imsettings-glib-bindings.h imsettings-glib-glue.h: stamp-bindings.h
	@true
stamp-bindings.h: @REBUILD@ $(srcdir)/imsettings.xml Makefile
	dbus-binding-tool --prefix=imsettings \
			  --mode=glib-client \
			  --output=imsettings-glib-bindings.h $(srcdir)/imsettings.xml && \
	dbus-binding-tool --prefix=imsettings \
			  --mode=glib-server \
			  --output=imsettings-glib-glue.h $(srcdir)/imsettings.xml && \
	echo timestamp > $(@F)
imsettings-marshal.h: stamp-marshal.h
	@true
stamp-marshal.h: @REBUILD@ $(srcdir)/marshal.list Makefile
	@GLIB_GENMARSHAL@ --prefix=imsettings_marshal $(srcdir)/marshal.list \
			  --header > xgen-imh && \
	(cmp -s xgen-imh imsettings-marshal.h || cp xgen-imh imsettings-marshal.h) && \
	rm -f xgen-imh && \
	echo timestamp > $(@F)
imsettings-marshal.c: @REBUILD@ imsettings-marshal.h Makefile
	(echo "#include \"imsettings-marshal.h\""; \
	@GLIB_GENMARSHAL@ --prefix=imsettings_marshal $(srcdir)/marshal.list --body) > xgen-imc && \
	cp xgen-imc imsettings-marshal.c && \
	rm -f xgen-imc


##
# Target platform
lib_LTLIBRARIES = 					\
	libimsettings.la				\
	$(NULL)
#
imsettingsincdir = $(includedir)/imsettings
imsettingsinc_HEADERS =					\
	$(imsettings_public_headers)			\
	$(imsettings_built_public_headers)		\
	$(NULL)
#
noinst_HEADERS =					\
	$(imsettings_private_headers)			\
	$(imsettings_built_private_headers)		\
	$(NULL)
#
libimsettings_la_SOURCES =				\
	$(imsettings_sources)				\
	$(NULL)
libimsettings_la_CFLAGS =				\
	-DG_LOG_DOMAIN="\"IMSettings\""			\
	$(NULL)
libimsettings_la_LDFLAGS =				\
	$(LDFLAGS)					\
	$(NULL)
