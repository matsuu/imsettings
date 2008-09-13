##
# Global definitions
NULL =
EXTRA_DIST =			\
	test-scim.conf		\
	test-scim2.conf		\
	test-issue7.conf	\
	xim.conf		\
	$(NULL)

##
# Local definitions
testcase_dist =							\
	imsettings-request/currentim/xinput.d			\
	imsettings-request/currentim/xinput.d/test-scim.conf	\
	imsettings-request/currentim/xinput.d/test-scim2.conf	\
	imsettings-request/currentim/.xinputrc			\
	imsettings-request/currentim/xinputrc			\
	imsettings-request/fake/xinput.d			\
	imsettings-request/fake/xinput.d/.xinputrc		\
	imsettings-request/fake/xinput.d/xinputrc		\
	imsettings-request/general/xinput.d			\
	imsettings-request/general/xinput.d/test-scim.conf	\
	imsettings-request/noent/xinput.d			\
	issue_11/xinput.d					\
	issue_11/xinput.d/scim.conf_				\
	issue_13/case1/xinput.d					\
	issue_13/case1/xinput.d/ja_JP				\
	issue_13/case1/xinput.d/kinput2-canna			\
	issue_13/case1/xinput.d/none.conf			\
	issue_13/case1/xinput.d/scim.conf			\
	issue_13/case1/xinput.d/xim.conf			\
	issue_13/case1/xinputrc					\
	issue_13/case2/xinput.d					\
	issue_13/case2/xinput.d/ja_JP				\
	issue_13/case2/xinput.d/kinput2-canna			\
	issue_13/case2/xinput.d/none.conf			\
	issue_13/case2/xinput.d/scim.conf			\
	issue_13/case2/xinput.d/xim.conf			\
	issue_13/case2/xinputrc					\
	issue_13/case3/xinput.d					\
	issue_13/case3/xinput.d/none.conf			\
	issue_13/case3/xinput.d/scim.conf			\
	issue_13/case3/xinput.d/xim.conf			\
	issue_13/case3/xinputrc					\
	issue_5							\
	issue_5/xinputrc					\
	issue_5/xinput.d					\
	rhbz_431291/case1/xinput.d				\
	rhbz_431291/case1/xinput.d/test-scim.conf		\
	rhbz_431291/case1/.xinputrc				\
	rhbz_431291/case1/xinputrc				\
	rhbz_453358/case1/xinput.d				\
	rhbz_453358/case1/xinput.d/test-scim.conf		\
	rhbz_453358/case1/xinput.d/test-scim2.conf		\
	rhbz_453358/case1/xinputrc				\
	rhbz_455363/case1/xinput.d				\
	rhbz_455363/case1/xinput.d/test-scim.conf		\
	rhbz_455363/case1/xinput.d/test-scim2.conf		\
	rhbz_455363/case1/xinputrc				\
	$(NULL)

##
# Local Rules
dist-hook:
	for file in $(testcase_dist); do						\
		if test -d $(srcdir)/$$file; then					\
			echo "Creating $(srcdir)/$$file on $(distdir)";			\
			if [ -z "$(MKDIR_P)" ]; then					\
				$(mkdir_p) $(distdir)/$(srcdir)/$$file || exit -1;	\
			else								\
				$(MKDIR_P) $(distdir)/$(srcdir)/$$file || exit -1;	\
			fi								\
		elif test -f $(srcdir)/$$file; then					\
			echo "Copying $(srcdir)/$$file to $(distdir)";			\
			cp -dp $(srcdir)/$$file $(distdir)/$(srcdir)/$$file || exit -1;	\
		elif test -L $(srcdir)/$$file; then					\
			echo "Copying $(srcdir)/$$file to $(distdir)";			\
			cp -d $(srcdir)/$$file $(distdir)/$(srcdir)/$$file || exit -1;	\
		else									\
			echo "Unable to archive $$file";				\
			exit -1;							\
		fi									\
	done
