##
# Global definitions
NULL =
CONFIGURE_DEPENDENCIES =	\
	xinput.sh.in		\
	$(NULL)
EXTRA_DIST =			\
	$(xinput_in_in_files)	\
	$(xinputsh_in_in)	\
	$(xinputinfosh_in)	\
	$(icon_DATA)		\
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


##
# Local Rules
$(xinput_DATA): $(xinput_in_files)
	cp `echo $@ | sed -e 's|$(XINPUT_SUFFIX)$$||'`.in $@
#
$(xinputsh_DATA): $(xinputsh_in)
	cp $(xinputsh_in) @XINPUTSH@

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
	imsettings-applet.png		\
	imsettings-unknown.png		\
	$(NULL)

.PHONY: clean-local
