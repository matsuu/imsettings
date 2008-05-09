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

##
# Local definitions
xim_public_headers =					\
	$(NULL)
xim_private_headers =					\
	connection.h					\
	loopback.h					\
	protocol.h					\
	server.h					\
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
	connection.c					\
	loopback.c					\
	main.c						\
	protocol.c					\
	server.c					\
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
noinst_HEADERS =					\
	$(xim_private_headers)				\
	$(xim_built_private_headers)			\
	$(NULL)
#
imsettings_xim_SOURCES =				\
	$(xim_sources)					\
	$(NULL)
imsettings_xim_CFLAGS =					\
	-DG_LOG_DOMAIN="\"IMSettings-XIM\""		\
	$(NULL)
imsettings_xim_LDADD =					\
	$(NULL)
