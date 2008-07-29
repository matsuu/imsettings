NULL =
AUTOMAKE_OPTIONS = dist-bzip2
SUBDIRS = imsettings backends utils src data po tests

ACLOCAL_AMFLAGS =			\
	-Im4macros			\
	$(NULL)

CONFIGURE_DEPENDENCIES =		\
	requires			\
	$(NULL)

EXTRA_DIST =				\
	$(pkgconfig_DATA)		\
	$(noinst_DATA)			\
	intltool-extract.in		\
	intltool-merge.in		\
	intltool-update.in		\
	requires			\
	$(NULL)

DISTCLEANFILES =			\
	$(pkgconfig_DATA)		\
	$(noinst_DATA)			\
	$(NULL)

#
#
#
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = imsettings.pc
$(pkgconfig_DATA): $(top_builddir)/config.status
noinst_DATA = imsettings-uninstalled.pc
$(noinst_DATA): $(top_builddir)/config.status
