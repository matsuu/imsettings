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
testcase_dist =				\
	imsettings-request		\
	issue_11			\
	issue_11/xinput.d		\
	issue_11/xinput.d/scim.conf_	\
	issue_5				\
	issue_5/xinputrc		\
	issue_5/xinput.d		\
	$(NULL)
##
# Local Rules
dist-hook:
	for file in $(testcase_dist); do					\
		if test -d $(srcdir)/$$file; then				\
			echo "Creating $(srcdir)/$$file on $(distdir)";		\
			$(MKDIR_P) $(distdir)/$(srcdir)/$$file || exit -1;	\
		elif test -f $(srcdir)/$$file; then				\
			echo "Copying $(srcdir)/$$file to $(distdir)";		\
			cp -dp $(srcdir)/$$file $(distdir)/$(srcdir)/$$file || exit -1;	\
		elif test -L $(srcdir)/$$file; then				\
			echo "Copying $(srcdir)/$$file to $(distdir)";		\
			cp -d $(srcdir)/$$file $(distdir)/$(srcdir)/$$file || exit -1;	\
		else								\
			echo "Unable to archive $$file";			\
			exit -1;						\
		fi								\
	done
