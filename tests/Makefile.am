##
# Global definitions
NULL =
SUBDIRS = testcases
INCLUDES =						\
	-I$(top_srcdir)/imsettings			\
	-DIMSETTINGS_SRCDIR="\"$(abs_srcdir)\""		\
	-DIMSETTINGS_BUILDDIR="\"$(abs_top_builddir)\""	\
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
	test-gcit-5					\
	test-gcit-7					\
	test-gcit-11					\
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
#
test_gcit_5_SOURCES =					\
	gcit_5.c					\
	$(common_sources)				\
	$(NULL)
#
test_gcit_7_SOURCES =					\
	gcit_7.c					\
	$(common_sources)				\
	$(NULL)
#
test_gcit_11_SOURCES =					\
	gcit_11.c					\
	$(common_sources)				\
	$(NULL)
test_gcit_11_CFLAGS =					\
	-DXINPUT_SUFFIX="\"$(XINPUT_SUFFIX)\""		\
	$(NULL)
