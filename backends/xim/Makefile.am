##
# Global definitions
NULL =
INCLUDES =						\
	-DIMSETTINGS_LOCALEDIR="\"$(datadir)/locale\""	\
	-I$(top_srcdir)/imsettings			\
	$(IMSETTINGS_CFLAGS)				\
	$(XIM_CFLAGS)					\
	$(null)
LIBS =							\
	@LDFLAGS@					\
	$(IMSETTINGS_LIBS)				\
	$(XIM_LIBS)					\
	$(top_builddir)/imsettings/libimsettings.la	\
	$(NULL)
DEPS =							\
	$(top_builddir)/imsettings/libimsettings.la	\
	$(NULL)
EXTRA_DIST =						\
	$(NULL)
MAINTAINERCLEANFILES =					\
	$(xim_built_public_headers)			\
	$(xim_built_private_headers)			\
	$(xim_built_sources)				\
	$(stamp_files)					\
	$(NULL)
CLEANFILES =						\
	xgen-imh					\
	xgen-imc					\
	$(NULL)

##
# Local definitions
xim_public_headers =					\
	$(NULL)
xim_private_headers =					\
	client.h					\
	loopback.h					\
	proxy.h						\
	utils.h						\
	$(NULL)
xim_built_public_headers =				\
	$(NULL)
xim_built_private_headers =				\
	$(NULL)
#
xim_built_sources =					\
	$(NULL)
xim_sources =						\
	client.c					\
	loopback.c					\
	proxy.c						\
	utils.c						\
	$(xim_built_sources)				\
	$(NULL)
#
stamp_files =						\
	$(NULL)


##
# Local Rules

##
# Target platform

bin_PROGRAMS = imsettings-xim
#
lib_LTLIBRARIES =					\
	libimsettings-xim.la				\
	$(NULL)
#
noinst_HEADERS =					\
	$(xim_private_headers)				\
	$(xim_built_private_headers)			\
	$(NULL)
#
imsettings_xim_SOURCES =				\
	main.c						\
	$(NULL)
imsettings_xim_CFLAGS =					\
	-DG_LOG_DOMAIN="\"IMSettings-XIM\""		\
	$(NULL)
imsettings_xim_LDADD =					\
	libimsettings-xim.la				\
	$(NULL)
#
libimsettings_xim_la_SOURCES =				\
	$(xim_sources)					\
	$(NULL)
libimsettings_xim_la_CFLAGS =				\
	-DG_LOG_DOMAIN="\"libimsettings-xim\""		\
	$(NULL)
libimsettings_xim_la_LDFLAGS =							\
	$(LDFLAGS)								\
	-version-info $(LT_XIM_CURRENT):$(LT_XIM_REVISION):$(LT_XIM_AGE)	\
	$(NULL)
