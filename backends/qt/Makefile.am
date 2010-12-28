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
	$(top_builddir)/imsettings/libimsettings.la	\
	$(QT_LIBS)					\
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
	libimsettings-qt.la				\
	$(NULL)
#
libimsettings_qt_la_SOURCES =				\
	qt-module.cpp					\
	$(NULL)
libimsettings_qt_la_CXXFLAGS =				\
	-DG_LOG_DOMAIN="\"IMSettings-Qt backend\""	\
	$(CXXFLAGS)					\
	$(NULL)
libimsettings_qt_la_LDFLAGS =				\
	-avoid-version					\
	-module						\
	$(LDFLAGS)					\
	$(NULL)
libimsettings_qt_la_LIBADD =				\
	$(LDADDS)					\
	$(NULL)
