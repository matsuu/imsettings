##
# Global definitions
NULL =
INCLUDES =						\
	-DIMAPPLET_LOCALEDIR="\"$(datadir)/locale\""	\
	-DICONDIR="\"$(datadir)/pixmaps\""		\
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
	$(autostart_in_files)				\
	$(schema_in_files)				\
	$(NULL)
DISTCLEANFILES =					\
	$(autostart_DATA)				\
	$(schema_DATA)					\
	$(NULL)

if ENABLE_XIM
INCLUDES += 						\
	-I$(top_srcdir)/backends/xim			\
	-DENABLE_XIM					\
	$(XIM_CFLAGS)					\
	$(NULL)
LIBS += $(top_builddir)/backends/xim/libimsettings-xim.la
DEPS += $(top_builddir)/backends/xim/libimsettings-xim.la
endif
if ENABLE_XSETTINGS
INCLUDES += -DENABLE_XSETTINGS
LIBS += $(top_builddir)/imsettings/libimsettings-xsettings.la
DEPS += $(top_builddir)/imsettings/libimsettings-xsettings.la
endif

##
# Local definitions

##
# Local Rules
@INTLTOOL_DESKTOP_RULE@
@INTLTOOL_SCHEMAS_RULE@

##
# Target platform
bin_PROGRAMS = imsettings-applet
#
autostartdir = $(sysconfdir)/xdg/autostart
#
autostart_in_files = imsettings-applet.desktop.in
autostart_DATA = $(autostart_in_files:.desktop.in=.desktop)
#
schemadir = @GCONF_SCHEMA_FILE_DIR@
schema_in_files = imsettings-applet.schemas.in
schema_DATA = $(schema_in_files:.schemas.in=.schemas)
#
imsettings_applet_SOURCES =				\
	main.c						\
	eggaccelerators.c				\
	eggaccelerators.h				\
	radiomenuitem.c					\
	radiomenuitem.h					\
	$(NULL)
imsettings_applet_CFLAGS =				\
	-DG_LOG_DOMAIN="\"IMSettings-Applet\""		\
	$(APPLET_CFLAGS)				\
	$(NULL)
imsettings_applet_LDADD =				\
	$(APPLET_LIBS)					\
	$(NULL)
#
if GCONF_SCHEMAS_INSTALL
install-data-local:
	GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(schema_DATA)
else
install-data-local:
endif
