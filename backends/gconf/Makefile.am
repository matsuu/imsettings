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
	$(NULL)


##
# Local definitions

##
# Local Rules

##
# Target platform
imsettingsdir = $(IMSETTINGS_MODULEDIR)
imsettings_LTLIBRARIES =				\
	libimsettings-gconf.la				\
	$(NULL)
#
libimsettings_gconf_la_SOURCES =			\
	gconf-module.c					\
	$(NULL)
libimsettings_gconf_la_CFLAGS =				\
	-DG_LOG_DOMAIN="\"IMSettings-GConf backend\""	\
	$(CFLAGS)					\
	$(NULL)
libimsettings_gconf_la_LDFLAGS =			\
	-avoid-version					\
	-module						\
	$(LDFLAGS)					\
	$(NULL)
libimsettings_gconf_la_LIBADD =				\
	$(LDADDS)					\
	$(NULL)
