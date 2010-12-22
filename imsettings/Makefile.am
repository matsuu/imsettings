##
# Global definitions
NULL =
INCLUDES =							\
	-DIMSETTINGS_LOCALEDIR="\"$(datadir)/locale\""		\
	-DXINIT_PATH="\"$(XINIT_PATH)\""			\
	-DXINPUT_PATH="\"$(XINPUT_PATH)\""			\
	-DXINPUT_SUFFIX="\"$(XINPUT_SUFFIX)\""			\
	-DXINPUTINFO_PATH="\"$(libexecdir)\""			\
	-DICONDIR="\"$(datadir)/pixmaps\""			\
	-I$(top_srcdir)/imsettings/				\
	$(IMSETTINGS_CFLAGS)					\
	$(NULL)
LIBS =								\
	@LDFLAGS@						\
	$(IMSETTINGS_LIBS)					\
	$(NULL)
EXTRA_DIST =							\
	$(NULL)
MAINTAINERCLEANFILES =						\
	$(imsettings_built_public_headers)			\
	$(imsettings_built_private_headers)			\
	$(imsettings_built_sources)				\
	$(stamp_files)						\
	$(NULL)
CLEANFILES =							\
	xgen-imh						\
	xgen-imc						\
	$(NULL)
BUILT_FILES =							\
	$(imsettings_built_public_headers)			\
	$(imsettings_built_private_headers)			\
	$(imsettings_built_sources)				\
	$(NULL)


##
# Local definitions
imsettings_public_headers =					\
	imsettings.h						\
	imsettings-client.h					\
	imsettings-info.h					\
	imsettings-utils.h					\
	$(NULL)	
imsettings_private_headers =					\
	$(NULL)
imsettings_built_public_headers =				\
	$(NULL)
imsettings_built_private_headers =				\
	$(NULL)
#
imsettings_built_sources =					\
	$(NULL)
imsettings_sources =						\
	imsettings-client.c					\
	imsettings-info.c					\
	imsettings-utils.c					\
	$(imsettings_built_sources)				\
	$(NULL)
#
stamp_files =							\
	$(NULL)


##
# Local Rules


##
# Target platform
lib_LTLIBRARIES = 						\
	libimsettings.la					\
	$(NULL)
#
imsettingsincdir = $(includedir)/imsettings
imsettingsinc_HEADERS =						\
	$(imsettings_public_headers)				\
	$(imsettings_built_public_headers)			\
	$(NULL)
#
noinst_HEADERS =						\
	$(imsettings_private_headers)				\
	$(imsettings_built_private_headers)			\
	$(NULL)
#
libimsettings_la_SOURCES =					\
	$(imsettings_sources)					\
	$(NULL)
libimsettings_la_CFLAGS =					\
	-DG_LOG_DOMAIN="\"IMSettings\""				\
	$(NULL)
libimsettings_la_LDFLAGS =					\
	$(LDFLAGS)						\
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)	\
	$(NULL)
