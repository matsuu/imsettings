##
# Global definitions
NULL =
INCLUDES =						\
	-DIMSETTINGS_LOCALEDIR="\"$(datadir)/locale\""	\
	-I$(top_srcdir)/imsettings			\
	$(NULL)
LIBS =							\
	@LDFLAGS@					\
	$(top_builddir)/imsettings/libimsettings.la	\
	$(NULL)
EXTRA_DIST =						\
	$(NULL)

##
# Local definitions
xim_headers =						\
	client.h					\
	compose.h					\
	loopback.h					\
	proxy.h						\
	utils.h						\
	$(NULL)
xim_sources =						\
	client.c					\
	compose.c					\
	loopback.c					\
	proxy.c						\
	utils.c						\
	$(NULL)

##
# Local Rules

##
# Target platform
imsettingsdir = $(IMSETTINGS_MODULEDIR)
imsettings_LTLIBRARIES =				\
	libimsettings-xim.la				\
	$(NULL)
bin_PROGRAMS =						\
	imsettings-xim					\
	$(NULL)
#
libimsettings_xim_la_SOURCES =				\
	xim-module.c					\
	$(NULL)
libimsettings_xim_la_CFLAGS =				\
	-DG_LOG_DOMAIN="\"IMSettings-XIM backend\""	\
	$(CFLAGS)					\
	$(IMSETTINGS_CFLAGS)				\
	$(NULL)
libimsettings_xim_la_LDFLAGS =				\
	-avoid-version					\
	-module						\
	$(LDFLAGS)					\
	$(NULL)
libimsettings_xim_la_LIBADD =				\
	$(LDADDS)					\
	$(NULL)
#
imsettings_xim_SOURCES =				\
	$(xim_headers)					\
	$(xim_sources)					\
	main.c						\
	$(NULL)
imsettings_xim_CFLAGS =					\
	-DG_LOG_DOMAIN="\"IMSettings-XIM server\""	\
	-DXLOCALEDIR="\"$(datadir)/X11/locale\""		\
	$(CFLAGS)					\
	$(XIM_CFLAGS)					\
	$(X11_CFLAGS)					\
	$(NULL)
imsettings_xim_LDADD =					\
	$(LDADDS)					\
	$(XIM_LIBS)					\
	$(X11_LIBS)					\
	$(NULL)
