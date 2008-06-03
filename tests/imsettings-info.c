/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * imsettings-info.c
 * Copyright (C) 2008 Red Hat, Inc. All rights reserved.
 * 
 * Authors:
 *   Akira TAGOH  <tagoh@redhat.com>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <imsettings/imsettings-info.h>
#include "main.h"


IMSettingsInfo *o;

/************************************************************/
/* common functions                                         */
/************************************************************/
void
setup(void)
{
}

void
teardown(void)
{
}

/************************************************************/
/* Test cases                                               */
/************************************************************/
TDEF (imsettings_info_new)
{
	gchar *t = g_build_filename(IMSETTINGS_SRCDIR, "testcases", "test-scim.conf", NULL);

	o = imsettings_info_new(NULL);
	fail_unless(o == NULL, "Object without the filename wouldn't be created.");
	/* assuming the warning message should be there */
	g_free(imsettings_test_pop_error());

	o = imsettings_info_new(t);
	TNUL (o);
	g_free(t);
	g_object_unref(o);

	t = g_build_filename("filename", "like", "foo", "bar", "baz", "may", "be", "unlikely" "to", "appear", "in", "your", "directory", "structure", NULL);
	o = imsettings_info_new(t);
	fail_unless(o == NULL, "Object wouldn't be created with the non-existence file.");
	/* assuming the warning message should be there. */
	g_free(imsettings_test_pop_error());
} TEND

TDEF (imsettings_info_new_with_lang)
{
	gchar *t = g_build_filename(IMSETTINGS_SRCDIR, "testcases", "test-scim.conf", NULL);

	o = imsettings_info_new_with_lang(NULL, NULL);
	fail_unless(o == NULL, "Object without the filename and the language wouldn't be created.");
	/* assuming the warning message should be there */
	g_free(imsettings_test_pop_error());

	o = imsettings_info_new_with_lang(t, NULL);
	fail_unless(o == NULL, "Object without the language wouldn't be created.");
	/* assuming the warning message should be there */
	g_free(imsettings_test_pop_error());

	o = imsettings_info_new_with_lang(NULL, "C");
	fail_unless(o == NULL, "Object without the filename wouldn't be created.");
	/* assuming the warning message should be there */
	g_free(imsettings_test_pop_error());

	o = imsettings_info_new_with_lang(t, "en_US");
	TNUL (o);
	g_free(t);
	g_object_unref(o);

	t = g_build_filename("filename", "like", "foo", "bar", "baz", "may", "be", "unlikely" "to", "appear", "in", "your", "directory", "structure", NULL);
	o = imsettings_info_new_with_lang(t, "en_US");
	fail_unless(o == NULL, "Object wouldn't be created with the non-existence file.");
	/* assuming the warning message should be there. */
	g_free(imsettings_test_pop_error());
} TEND

TDEF (imsettings_info_get_language) {
	gchar *t = g_build_filename(IMSETTINGS_SRCDIR, "testcases", "test-scim.conf", NULL);

	o = imsettings_info_new_with_lang(t, "en_US");
	TNUL (o);
	fail_unless(strcmp(imsettings_info_get_language(o), "en_US") == 0, "Object contains the different filename");
	g_free(t);
	g_object_unref(o);
} TEND

TDEF (imsettings_info_get_filename) {
	gchar *t = g_build_filename(IMSETTINGS_SRCDIR, "testcases", "test-scim.conf", NULL);

	o = imsettings_info_new(t);
	TNUL (o);
	fail_unless(strcmp(imsettings_info_get_filename(o), t) == 0, "Object contains the different filename");
	g_free(t);
	g_object_unref(o);
} TEND

TDEF (imsettings_info_get_gtkimm) {
	gchar *t = g_build_filename(IMSETTINGS_SRCDIR, "testcases", "test-scim.conf", NULL);

	o = imsettings_info_new(t);
	TNUL (o);
	fail_unless(strcmp(imsettings_info_get_gtkimm(o), "scim-bridge") == 0, "Object contains the different GTK+ immodule name");
	g_free(t);
	g_object_unref(o);
} TEND

TDEF (imsettings_info_get_qtimm) {
	gchar *t = g_build_filename(IMSETTINGS_SRCDIR, "testcases", "test-scim.conf", NULL);

	o = imsettings_info_new(t);
	TNUL (o);
	fail_unless(strcmp(imsettings_info_get_qtimm(o), "xim") == 0, "Object contains the different Qt immodule name");
	g_free(t);
	g_object_unref(o);
} TEND

TDEF (imsettings_info_get_xim) {
	gchar *t = g_build_filename(IMSETTINGS_SRCDIR, "testcases", "test-scim.conf", NULL);

	o = imsettings_info_new(t);
	TNUL (o);
	fail_unless(strcmp(imsettings_info_get_xim(o), "SCIM") == 0, "Object contains the different XIM value");
	g_free(t);
	g_object_unref(o);
} TEND

TDEF (imsettings_info_get_xim_program) {
	gchar *t = g_build_filename(IMSETTINGS_SRCDIR, "testcases", "test-scim.conf", NULL);

	o = imsettings_info_new(t);
	TNUL (o);
	fail_unless(strcmp(imsettings_info_get_xim_program(o), "/usr/bin/scim") == 0, "Object contains the different XIM program name");
	g_free(t);
	g_object_unref(o);
} TEND

TDEF (imsettings_info_get_xim_args) {
	gchar *t = g_build_filename(IMSETTINGS_SRCDIR, "testcases", "test-scim.conf", NULL);

	o = imsettings_info_new(t);
	TNUL (o);
	fail_unless(strcmp(imsettings_info_get_xim_args(o), "-d --no-socket") == 0, "Object contains the different XIM args");
	g_free(t);
	g_object_unref(o);
} TEND

TDEF (imsettings_info_get_prefs_program) {
	gchar *t = g_build_filename(IMSETTINGS_SRCDIR, "testcases", "test-scim.conf", NULL);

	o = imsettings_info_new(t);
	TNUL (o);
	fail_unless(strcmp(imsettings_info_get_prefs_program(o), "/usr/bin/scim-setup") == 0, "Object contains the different preference program name");
	g_free(t);
	g_object_unref(o);
} TEND

TDEF (imsettings_info_get_prefs_args) {
	gchar *t = g_build_filename(IMSETTINGS_SRCDIR, "testcases", "test-scim.conf", NULL);

	o = imsettings_info_new(t);
	TNUL (o);
	fail_unless(strcmp(imsettings_info_get_prefs_args(o), "--display :0") == 0, "Object contains the different preference args");
	g_free(t);
	g_object_unref(o);
} TEND

TDEF (imsettings_info_get_aux_program) {
	gchar *t = g_build_filename(IMSETTINGS_SRCDIR, "testcases", "test-scim.conf", NULL);

	o = imsettings_info_new(t);
	TNUL (o);
	fail_unless(strcmp(imsettings_info_get_aux_program(o), "/bin/true") == 0, "Object contains the different auxiliary program name");
	g_free(t);
	g_object_unref(o);
} TEND

TDEF (imsettings_info_get_aux_args) {
	gchar *t = g_build_filename(IMSETTINGS_SRCDIR, "testcases", "test-scim.conf", NULL);

	o = imsettings_info_new(t);
	TNUL (o);
	fail_unless(strcmp(imsettings_info_get_aux_args(o), "--version") == 0, "Object contains the different auxiliary args");
	g_free(t);
	g_object_unref(o);
} TEND

TDEF (imsettings_info_get_short_desc) {
	gchar *t = g_build_filename(IMSETTINGS_SRCDIR, "testcases", "test-scim.conf", NULL);

	o = imsettings_info_new(t);
	TNUL (o);
	fail_unless(strcmp(imsettings_info_get_short_desc(o), "SCIM") == 0, "Object contains the different short description");
	g_free(t);
	g_object_unref(o);

	t = g_build_filename(IMSETTINGS_SRCDIR, "testcases", "test-scim2.conf", NULL);
	o = imsettings_info_new(t);
	TNUL (o);
	fail_unless(strcmp(imsettings_info_get_short_desc(o), "S C I M") == 0, "Object contains the different short description");
	g_free(t);
	g_object_unref(o);
} TEND

TDEF (imsettings_info_get_long_desc) {
	gchar *t = g_build_filename(IMSETTINGS_SRCDIR, "testcases", "test-scim.conf", NULL);

	o = imsettings_info_new(t);
	TNUL (o);
	fail_unless(strcmp(imsettings_info_get_long_desc(o), "Smart Common Input Method platform") == 0, "Object contains the different long description");
	g_free(t);
	g_object_unref(o);
} TEND

TDEF (imsettings_info_get_icon_file) {
	gchar *t = g_build_filename(IMSETTINGS_SRCDIR, "testcases", "test-scim.conf", NULL);

	o = imsettings_info_new(t);
	TNUL (o);
	fail_unless(strcmp(imsettings_info_get_icon_file(o), "foo.png") == 0, "Object contains the different icon file");
	g_free(t);
	g_object_unref(o);
} TEND

TDEF (imsettings_info_is_visible) {
	gchar *t = g_build_filename(IMSETTINGS_SRCDIR, "testcases", "test-scim.conf", NULL);

	o = imsettings_info_new(t);
	TNUL (o);
	fail_unless(imsettings_info_is_visible(o), "Object has to be visible");
	g_free(t);
	g_object_unref(o);

	t = g_build_filename(IMSETTINGS_SRCDIR, "testcases", "test-scim2.conf", NULL);
	o = imsettings_info_new(t);
	TNUL (o);
	fail_unless(!imsettings_info_is_visible(o), "Object has to be invisible");
	g_free(t);
	g_object_unref(o);
} TEND

TDEF (imsettings_info_is_system_default) {
	gchar *t = g_build_filename(IMSETTINGS_SRCDIR, "testcases", "test-scim.conf", NULL);

	o = imsettings_info_new(t);
	TNUL (o);
	g_object_set(G_OBJECT (o), "is_system_default", TRUE, NULL);
	fail_unless(imsettings_info_is_system_default(o), "Object has to be the system default");
	g_object_set(G_OBJECT (o), "is_system_default", FALSE, NULL);
	fail_unless(!imsettings_info_is_system_default(o), "Object doesn't have to be the system default");
	g_free(t);
	g_object_unref(o);
} TEND

TDEF (imsettings_info_is_user_default) {
	gchar *t = g_build_filename(IMSETTINGS_SRCDIR, "testcases", "test-scim.conf", NULL);

	o = imsettings_info_new(t);
	TNUL (o);
	g_object_set(G_OBJECT (o), "is_user_default", TRUE, NULL);
	fail_unless(imsettings_info_is_user_default(o), "Object has to be the user default");
	g_object_set(G_OBJECT (o), "is_user_default", FALSE, NULL);
	fail_unless(!imsettings_info_is_user_default(o), "Object doesn't have to be the user default");
	g_free(t);
	g_object_unref(o);
} TEND

TDEF (imsettings_info_is_xim) {
	gchar *t = g_build_filename(IMSETTINGS_SRCDIR, "testcases", "xim.conf", NULL);

	o = imsettings_info_new(t);
	TNUL (o);
	fail_unless(imsettings_info_is_xim(o), "Object has to be the XIM conf");
	g_free(t);
	g_object_unref(o);
	t = g_build_filename(IMSETTINGS_SRCDIR, "testcases", "test-scim.conf", NULL);
	o = imsettings_info_new(t);
	TNUL (o);
	fail_unless(!imsettings_info_is_xim(o), "Object doesn't have to be the XIM");
	g_free(t);
	g_object_unref(o);
} TEND

TDEF (imsettings_info_compare) {
	gchar *t = g_build_filename(IMSETTINGS_SRCDIR, "testcases", "test-scim.conf", NULL);
	IMSettingsInfo *b;

	o = imsettings_info_new(t);
	b = imsettings_info_new(t);
	TNUL (o);
	TNUL (b);
	fail_unless(imsettings_info_compare(o, b), "Objects has to be the same");
	g_free(t);
	g_object_unref(b);
	t = g_build_filename(IMSETTINGS_SRCDIR, "testcases", "test-scim2.conf", NULL);
	b = imsettings_info_new(t);
	fail_unless(!imsettings_info_compare(o, b), "Objects doesn't have to be the same");
	g_free(t);
	g_object_unref(o);
	g_object_unref(b);
} TEND

TDEF (imsettings_object_dump) {
	/* this testing may wants to share with imsettings_object_load */
} TEND

TDEF (imsettings_object_load) {
	gchar *t = g_build_filename(IMSETTINGS_SRCDIR, "testcases", "test-scim.conf", NULL);
	GString *d;
	IMSettingsInfo *b;

	o = imsettings_info_new(t);
	TNUL (o);
	d = imsettings_object_dump(IMSETTINGS_OBJECT (o));
	fail_unless(d != NULL, "Failed to dump");
	b = IMSETTINGS_INFO (imsettings_object_load(d->str, d->len));
	fail_unless(imsettings_info_compare(o, b), "Objects has to be the same");
	g_free(t);
	g_string_free(d, TRUE);
	g_object_unref(o);
	g_object_unref(b);
} TEND

/************************************************************/
Suite *
imsettings_suite(void)
{
	Suite *s = suite_create("IMSettingsInfo");
	TCase *tc = tcase_create("Generic Functionalities");

	tcase_add_checked_fixture(tc, setup, teardown);

	T (imsettings_info_new);
	T (imsettings_info_new_with_lang);
	T (imsettings_info_get_language);
	T (imsettings_info_get_filename);
	T (imsettings_info_get_gtkimm);
	T (imsettings_info_get_qtimm);
	T (imsettings_info_get_xim);
	T (imsettings_info_get_xim_program);
	T (imsettings_info_get_xim_args);
	T (imsettings_info_get_prefs_program);
	T (imsettings_info_get_prefs_args);
	T (imsettings_info_get_aux_program);
	T (imsettings_info_get_aux_args);
	T (imsettings_info_get_short_desc);
	T (imsettings_info_get_long_desc);
	T (imsettings_info_get_icon_file);
	T (imsettings_info_is_visible);
	T (imsettings_info_is_system_default);
	T (imsettings_info_is_user_default);
	T (imsettings_info_is_xim);
	T (imsettings_info_compare);
	T (imsettings_object_dump);
	T (imsettings_object_load);

	suite_add_tcase(s, tc);

	return s;
}
