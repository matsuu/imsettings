##
# Global definitions
NULL =
INCLUDES =						\
	-DIMAPPLET_LOCALEDIR="\"$(datadir)/locale\""	\
	-l$(top_srcdir)/imsettings			\
	$(IMSETTINGS_CFLAGS)				\
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
	$(NULL)
DISTCLEANFILES =					\
	$(NULL)


##
# Local definitions

##
# Local Rules

##
# Target platform
bin_PROGRAMS = imsettings-applet
#
imsettings_applet_SOURCES =				\
	main.c						\
	radiomenuitem.c					\
	radiomenuitem.h					\
	$(NULL)
imsettings_applet_CFLAGS =				\
	-DG_LOG_DOMAIN="\"IMSettings-Applet\""		\
	-DPIXMAP_PATH="\"$(datadir)/pixmaps/\""		\
	$(APPLET_CFLAGS)				\
	$(NULL)
imsettings_applet_LDADD =				\
	$(APPLET_LIBS)					\
	$(NULL)
