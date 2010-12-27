##
# Global definitions
NULL =
INCLUDES =						\
	-DIMSETTINGS_LOCALEDIR="\"$(datadir)/locale\""	\
	-DLXDE_CONFIGDIR="\"$(sysconfdir)/lxde\""	\
	-I$(top_srcdir)/imsettings			\
	$(LXDE_CFLAGS)					\
	$(NULL)
LIBS =							\
	@LDFLAGS@					\
	$(top_builddir)/imsettings/libimsettings.la	\
	$(LXDE_LIBS)					\
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
	libimsettings-lxde.la				\
	$(NULL)
#
libimsettings_lxde_la_SOURCES =				\
	lxde-module.c					\
	$(NULL)
libimsettings_lxde_la_CFLAGS =				\
	-DG_LOG_DOMAIN="\"IMSettings-LXDE backend\""	\
	$(CFLAGS)					\
	$(NULL)
libimsettings_lxde_la_LDFLAGS =				\
	-avoid-version					\
	-module						\
	$(LDFLAGS)					\
	$(NULL)
libimsettings_lxde_la_LIBADD =				\
	$(LDADDS)					\
	$(NULL)
