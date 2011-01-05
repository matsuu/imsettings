##
# Global definitions
NULL =
EXTRA_DIST =			\
	$(xinput_in_in_files)	\
	$(xinputsh_in_in)	\
	$(xinputinfosh_in)	\
	$(icon_DATA)		\
	$(NULL)
CLEANFILES =			\
	$(stamp_files)		\
	$(NULL)
BUILT_FILES =			\
	$(xinput_DATA)		\
	$(xinputsh_DATA)	\
	$(xinputinfosh_DATA)	\
	$(NULL)


##
# Local definitions
xinput_in_in_files =		\
	none.in.in		\
	xim.in.in		\
	xcompose.in.in		\
	$(NULL)
xinput_in_files = $(xinput_in_in_files:.in.in=.in)
#
xinputsh_in_in = xinput.sh.in.in
xinputsh_in = $(xinputsh_in_in:.in=)
#
xinputinfosh_in = xinputinfo.sh.in
stamp_files =			\
	stamp-xinputinfosh	\
	$(NULL)


##
# Local Rules
all-local: stamp-xinputinfosh

$(xinput_DATA): $(xinput_in_files) $(builddir)/Makefile
	cp `echo $@ | sed -e 's|$(XINPUT_SUFFIX)$$||'`.in $@
#
$(xinputsh_DATA): $(xinputsh_in) $(builddir)/Makefile
	cp $(xinputsh_in) @XINPUTSH@
#
stamp-xinputinfosh: $(xinputinfosh_DATA) $(builddir)/Makefile
	chmod a+x $(xinputinfosh_DATA)
	echo timestamp > $(@F)

clean-local:
	-rm $(xinput_DATA)
	-rm $(xinputsh_DATA)
	-rm $(xinputinfosh_DATA)


##
# Target platform
xinputdir = $(XINPUT_PATH)
xinput_DATA = $(xinput_in_files:.in=$(XINPUT_SUFFIX))
#
xinputshdir = @XINIT_PATH@
xinputsh_DATA = @XINPUTSH@
#
xinputinfoshdir = $(libexecdir)
xinputinfosh_DATA = $(xinputinfosh_in:.in=)
#
icondir = $(datadir)/pixmaps
icon_DATA = 				\
	imsettings-unknown.png		\
	$(NULL)

.PHONY: clean-local
