##
# Global definitions
NULL =
SUBDIRS = testcases
INCLUDES =						\
	-I$(top_srcdir)/imsettings			\
	-DIMSETTINGS_SRCDIR="\"$(srcdir)\""		\
	$(IMSETTINGS_CFLAGS)				\
	$(CHECK_CFLAGS)					\
	$(NULL)
DEPS =							\
	$(top_builddir)/imsettings/libimsettings.la	\
	$(NULL)
LIBS =							\
	@LDFLAGS@					\
	$(top_builddir)/imsettings/libimsettings.la	\
	$(CHECK_LIBS)					\
	$(NULL)
EXTRA_DIST =						\
	$(NULL)
TESTS =							\
	$(noinst_PROGRAMS)				\
	$(NULL)


##
# Local definitions
common_private_headers =				\
	main.h						\
	$(NULL)
common_sources =					\
	main.c						\
	$(NULL)

##
# Local Rules


##
# Target platform
noinst_HEADERS =					\
	$(common_private_headers)			\
	$(NULL)
noinst_PROGRAMS =					\
	$(NULL)
if ENABLE_UNIT_TEST
noinst_PROGRAMS +=					\
	test-imsettings-object				\
	test-imsettings-info				\
	$(NULL)
endif
#
test_imsettings_object_SOURCES =			\
	imsettings-object.c				\
	$(common_sources)				\
	$(NULL)
#
test_imsettings_info_SOURCES =				\
	imsettings-info.c				\
	$(common_sources)				\
	$(NULL)
