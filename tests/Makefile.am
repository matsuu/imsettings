##
# Global definitions
NULL =
SUBDIRS = testcases
INCLUDES =						\
	-I$(top_srcdir)/imsettings			\
	-DIMSETTINGS_SRCDIR="\"$(abs_srcdir)\""		\
	-DIMSETTINGS_BUILDDIR="\"$(abs_top_builddir)\""	\
	-DXINPUT_SUFFIX="\"$(XINPUT_SUFFIX)\""		\
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
	test-imsettings-info				\
	test-gcit-11					\
	test-rhbz-431291				\
	test-rhbz-453358				\
	test-rhbz-455363				\
	test-rhbz-471833				\
	test-rhbz-490587				\
	test-rhbz-510666				\
	test-rhbz-514852				\
	test-rhbz-523349				\
	test-rhbz-526205				\
	test-rhbz-599924				\
	$(NULL)
endif
#
test_imsettings_info_SOURCES =				\
	imsettings-info.c				\
	$(common_sources)				\
	$(NULL)
#
test_imsettings_request_SOURCES =			\
	imsettings-request.c				\
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
#
test_gcit_13_SOURCES =					\
	gcit_13.c					\
	$(common_sources)				\
	$(NULL)
#
test_rhbz_431291_SOURCES =				\
	rhbz_431291.c					\
	$(common_sources)				\
	$(NULL)
#
test_rhbz_453358_SOURCES =				\
	rhbz_453358.c					\
	$(common_sources)				\
	$(NULL)
#
test_rhbz_455363_SOURCES =				\
	rhbz_455363.c					\
	$(common_sources)				\
	$(NULL)
#
test_rhbz_471833_SOURCES =				\
	rhbz_471833.c					\
	$(common_sources)				\
	$(NULL)
#
test_rhbz_490587_SOURCES =				\
	rhbz_490587.c					\
	$(common_sources)				\
	$(NULL)
#
test_rhbz_510666_SOURCES =				\
	rhbz_510666.c					\
	$(common_sources)				\
	$(NULL)
#
test_rhbz_514852_SOURCES =				\
	rhbz_514852.c					\
	$(common_sources)				\
	$(NULL)
#
test_rhbz_523349_SOURCES =				\
	rhbz_523349.c					\
	$(common_sources)				\
	$(NULL)
#
test_rhbz_526205_SOURCES =				\
	rhbz_526205.c					\
	$(common_sources)				\
	$(NULL)
#
test_rhbz_599924_SOURCES =				\
	rhbz_599924.c					\
	$(common_sources)				\
	$(NULL)
test_rhbz_599924_CFLAGS =				\
	$(GTHREAD_CFLAGS)				\
	$(NULL)
test_rhbz_599924_LDADD =				\
	$(GTHREAD_LIBS)					\
	$(NULL)
