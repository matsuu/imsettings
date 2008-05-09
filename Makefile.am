NULL =
AUTOMAKE_OPTIONS = dist-bzip2
SUBDIRS = imsettings backends utils src data po tests

CONFIGURE_DEPENDENCIES =		\
	requires			\
	$(NULL)

EXTRA_DIST =				\
	$(pkgconfig_DATA)		\
	imsettings-uninstalled.pc.in	\
	intltool-extract.in		\
	intltool-merge.in		\
	intltool-update.in		\
	requires			\
	$(NULL)

DISTCLEANFILES =			\
	$(pkgconfig_DATA)		\
	imsettings-uninstalled.pc	\
	$(NULL)

#
#
#
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = imsettings.pc
$(pkgconfig_DATA): $(top_builddir)/config.status
