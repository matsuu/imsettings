##
# Global definitions
NULL =
INCLUDES =						\
	-DIMSETTINGS_LOCALEDIR="\"$(datadir)/locale\""	\
	-I$(top_srcdir)/imsettings			\
	$(XFCE_CFLAGS)					\
	$(NULL)
LIBS =							\
	@LDFLAGS@					\
	$(top_builddir)/imsettings/libimsettings.la	\
	$(XFCE_LIBS)					\
	$(NULL)
EXTRA_DIST =						\
	$(NULL)


##
# Local definitions

##
# Local Rules

##
# Target platform
imsettingsdir = $(IMSETTINGS_MODULEDIR)
imsettings_LTLIBRARIES =				\
	libimsettings-xfce.la				\
	$(NULL)
#
libimsettings_xfce_la_SOURCES =				\
	xfce-module.c					\
	$(NULL)
libimsettings_xfce_la_CFLAGS =				\
	-DG_LOG_DOMAIN="\"IMSettings-XFCE backend\""	\
	$(CFLAGS)					\
	$(NULL)
libimsettings_xfce_la_LDFLAGS =				\
	-avoid-version					\
	-module						\
	$(LDFLAGS)					\
	$(NULL)
libimsettings_xfce_la_LIBADD =				\
	$(LDADDS)					\
	$(NULL)
