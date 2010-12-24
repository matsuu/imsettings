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
	GVariant *v;

	o = imsettings_info_new(NULL);
	fail_unless(o == NULL, "Object without the valid variant wouldn't be created.");
	/* assuming the warning message should be there */
	g_free(imsettings_test_pop_error());

	v = imsettings_info_variant_new(t, NULL);
	o = imsettings_info_new(v);
	TNUL (o);
	g_variant_unref(v);
	g_free(t);
	g_object_unref(o);

	t = g_build_filename("filename", "like", "foo", "bar", "baz", "may", "be", "unlikely" "to", "appear", "in", "your", "directory", "structure", NULL);
	v = imsettings_info_variant_new(t, NULL);
	o = imsettings_info_new(v);
	fail_unless(o == NULL, "Object wouldn't be created with the non-existence file.");
	/* assuming the warning message should be there. */
	g_free(imsettings_test_pop_error());
} TEND

TDEF (imsettings_info_get_language) {
	gchar *t = g_build_filename(IMSETTINGS_SRCDIR, "testcases", "test-scim.conf", NULL);
	GVariant *v;

	v = imsettings_info_variant_new(t, "en_US");
	o = imsettings_info_new(v);
	TNUL (o);
	fail_unless(strcmp(imsettings_info_get_language(o), "en_US") == 0, "Object contains the different filename");
	g_free(t);
	g_variant_unref(v);
	g_object_unref(o);
} TEND

TDEF (imsettings_info_get_filename) {
	gchar *t = g_build_filename(IMSETTINGS_SRCDIR, "testcases", "test-scim.conf", NULL);
	GVariant *v;

	v = imsettings_info_variant_new(t, NULL);
	o = imsettings_info_new(v);
	TNUL (o);
	fail_unless(strcmp(imsettings_info_get_filename(o), t) == 0, "Object contains the different filename");
	g_free(t);
	g_variant_unref(v);
	g_object_unref(o);
} TEND

TDEF (imsettings_info_get_gtkimm) {
	gchar *t = g_build_filename(IMSETTINGS_SRCDIR, "testcases", "test-scim.conf", NULL);
	GVariant *v;

	v = imsettings_info_variant_new(t, NULL);
	o = imsettings_info_new(v);
	TNUL (o);
	fail_unless(strcmp(imsettings_info_get_gtkimm(o), "scim-bridge") == 0, "Object contains the different GTK+ immodule name");
	g_free(t);
	g_variant_unref(v);
	g_object_unref(o);
} TEND

TDEF (imsettings_info_get_qtimm) {
	gchar *t = g_build_filename(IMSETTINGS_SRCDIR, "testcases", "test-scim.conf", NULL);
	GVariant *v;

	v = imsettings_info_variant_new(t, NULL);
	o = imsettings_info_new(v);
	TNUL (o);
	fail_unless(strcmp(imsettings_info_get_qtimm(o), "xim") == 0, "Object contains the different Qt immodule name");
	g_free(t);
	g_variant_unref(v);
	g_object_unref(o);
} TEND

TDEF (imsettings_info_get_xim) {
	gchar *t = g_build_filename(IMSETTINGS_SRCDIR, "testcases", "test-scim.conf", NULL);
	GVariant *v;

	v = imsettings_info_variant_new(t, NULL);
	o = imsettings_info_new(v);
	TNUL (o);
	fail_unless(strcmp(imsettings_info_get_xim(o), "SCIM") == 0, "Object contains the different XIM value");
	g_free(t);
	g_variant_unref(v);
	g_object_unref(o);
} TEND

TDEF (imsettings_info_get_xim_program) {
	gchar *t = g_build_filename(IMSETTINGS_SRCDIR, "testcases", "test-scim.conf", NULL);
	GVariant *v;

	v = imsettings_info_variant_new(t, NULL);
	o = imsettings_info_new(v);
	TNUL (o);
	fail_unless(strcmp(imsettings_info_get_xim_program(o), "/usr/bin/scim") == 0, "Object contains the different XIM program name");
	g_free(t);
	g_variant_unref(v);
	g_object_unref(o);
} TEND

TDEF (imsettings_info_get_xim_args) {
	gchar *t = g_build_filename(IMSETTINGS_SRCDIR, "testcases", "test-scim.conf", NULL);
	GVariant *v;

	v = imsettings_info_variant_new(t, NULL);
	o = imsettings_info_new(v);
	TNUL (o);
	fail_unless(strcmp(imsettings_info_get_xim_args(o), "-d --no-socket") == 0, "Object contains the different XIM args");
	g_free(t);
	g_variant_unref(v);
	g_object_unref(o);
} TEND

TDEF (imsettings_info_get_prefs_program) {
	gchar *t = g_build_filename(IMSETTINGS_SRCDIR, "testcases", "test-scim.conf", NULL);
	GVariant *v;

	v = imsettings_info_variant_new(t, NULL);
	o = imsettings_info_new(v);
	TNUL (o);
	fail_unless(strcmp(imsettings_info_get_prefs_program(o), "/usr/bin/scim-setup") == 0, "Object contains the different preference program name");
	g_free(t);
	g_variant_unref(v);
	g_object_unref(o);
} TEND

TDEF (imsettings_info_get_prefs_args) {
	gchar *t = g_build_filename(IMSETTINGS_SRCDIR, "testcases", "test-scim.conf", NULL);
	GVariant *v;

	v = imsettings_info_variant_new(t, NULL);
	o = imsettings_info_new(v);
	TNUL (o);
	fail_unless(strcmp(imsettings_info_get_prefs_args(o), "--display :0") == 0, "Object contains the different preference args");
	g_free(t);
	g_variant_unref(v);
	g_object_unref(o);
} TEND

TDEF (imsettings_info_get_aux_program) {
	gchar *t = g_build_filename(IMSETTINGS_SRCDIR, "testcases", "test-scim.conf", NULL);
	GVariant *v;

	v = imsettings_info_variant_new(t, NULL);
	o = imsettings_info_new(v);
	TNUL (o);
	fail_unless(strcmp(imsettings_info_get_aux_program(o), "/bin/true") == 0, "Object contains the different auxiliary program name");
	g_free(t);
	g_variant_unref(v);
	g_object_unref(o);
} TEND

TDEF (imsettings_info_get_aux_args) {
	gchar *t = g_build_filename(IMSETTINGS_SRCDIR, "testcases", "test-scim.conf", NULL);
	GVariant *v;

	v = imsettings_info_variant_new(t, NULL);
	o = imsettings_info_new(v);
	TNUL (o);
	fail_unless(strcmp(imsettings_info_get_aux_args(o), "--version") == 0, "Object contains the different auxiliary args");
	g_free(t);
	g_variant_unref(v);
	g_object_unref(o);
} TEND

TDEF (imsettings_info_get_short_desc) {
	gchar *t = g_build_filename(IMSETTINGS_SRCDIR, "testcases", "test-scim.conf", NULL);
	GVariant *v;

	v = imsettings_info_variant_new(t, NULL);
	o = imsettings_info_new(v);
	TNUL (o);
	fail_unless(strcmp(imsettings_info_get_short_desc(o), "SCIM") == 0, "Object contains the different short description");
	g_free(t);
	g_variant_unref(v);
	g_object_unref(o);

	t = g_build_filename(IMSETTINGS_SRCDIR, "testcases", "test-scim2.conf", NULL);
	v = imsettings_info_variant_new(t, NULL);
	o = imsettings_info_new(v);
	TNUL (o);
	fail_unless(strcmp(imsettings_info_get_short_desc(o), "S C I M") == 0, "Object contains the different short description");
	g_free(t);
	g_variant_unref(v);
	g_object_unref(o);
} TEND

TDEF (imsettings_info_get_long_desc) {
	gchar *t = g_build_filename(IMSETTINGS_SRCDIR, "testcases", "test-scim.conf", NULL);
	GVariant *v;

	v = imsettings_info_variant_new(t, NULL);
	o = imsettings_info_new(v);
	TNUL (o);
	fail_unless(strcmp(imsettings_info_get_long_desc(o), "Smart Common Input Method platform") == 0, "Object contains the different long description");
	g_free(t);
	g_variant_unref(v);
	g_object_unref(o);
} TEND

TDEF (imsettings_info_get_icon_file) {
	gchar *t = g_build_filename(IMSETTINGS_SRCDIR, "testcases", "test-scim.conf", NULL);
	GVariant *v;

	v = imsettings_info_variant_new(t, NULL);
	o = imsettings_info_new(v);
	TNUL (o);
	fail_unless(strcmp(imsettings_info_get_icon_file(o), "foo.png") == 0, "Object contains the different icon file");
	g_free(t);
	g_variant_unref(v);
	g_object_unref(o);
} TEND

TDEF (imsettings_info_is_visible) {
	gchar *t = g_build_filename(IMSETTINGS_SRCDIR, "testcases", "test-scim.conf", NULL);
	GVariant *v;

	v = imsettings_info_variant_new(t, NULL);
	o = imsettings_info_new(v);
	TNUL (o);
	fail_unless(imsettings_info_is_visible(o), "Object has to be visible");
	g_free(t);
	g_variant_unref(v);
	g_object_unref(o);

	t = g_build_filename(IMSETTINGS_SRCDIR, "testcases", "test-scim2.conf", NULL);
	v = imsettings_info_variant_new(t, NULL);
	o = imsettings_info_new(v);
	TNUL (o);
	fail_unless(!imsettings_info_is_visible(o), "Object has to be invisible");
	g_free(t);
	g_variant_unref(v);
	g_object_unref(o);
} TEND

TDEF (imsettings_info_is_system_default) {
	gchar *t = g_build_filename(IMSETTINGS_SRCDIR, "testcases", "test-scim.conf", NULL);
	GVariant *v;

	v = imsettings_info_variant_new(t, NULL);
	o = imsettings_info_new(v);
	TNUL (o);
	/* this object is read-only. updating the value isn't supported anymore */
#if 0
	g_object_set(G_OBJECT (o), "is_system_default", TRUE, NULL);
	fail_unless(imsettings_info_is_system_default(o), "Object has to be the system default");
	g_object_set(G_OBJECT (o), "is_system_default", FALSE, NULL);
	fail_unless(!imsettings_info_is_system_default(o), "Object doesn't have to be the system default");
#endif
	g_free(t);
	g_variant_unref(v);
	g_object_unref(o);
} TEND

TDEF (imsettings_info_is_user_default) {
	gchar *t = g_build_filename(IMSETTINGS_SRCDIR, "testcases", "test-scim.conf", NULL);
	GVariant *v;

	v = imsettings_info_variant_new(t, NULL);
	o = imsettings_info_new(v);
	TNUL (o);
	/* this object is read-only. updating the value isn't supported anymore */
#if 0
	g_object_set(G_OBJECT (o), "is_user_default", TRUE, NULL);
	fail_unless(imsettings_info_is_user_default(o), "Object has to be the user default");
	g_object_set(G_OBJECT (o), "is_user_default", FALSE, NULL);
	fail_unless(!imsettings_info_is_user_default(o), "Object doesn't have to be the user default");
#endif
	g_free(t);
	g_variant_unref(v);
	g_object_unref(o);
} TEND

TDEF (imsettings_info_is_xim) {
	gchar *t = g_build_filename(IMSETTINGS_SRCDIR, "testcases", "xim.conf", NULL);
	GVariant *v;

	v = imsettings_info_variant_new(t, NULL);
	o = imsettings_info_new(v);
	TNUL (o);
	fail_unless(imsettings_info_is_xim(o), "Object has to be the XIM conf");
	g_free(t);
	g_variant_unref(v);
	g_object_unref(o);
	t = g_build_filename(IMSETTINGS_SRCDIR, "testcases", "test-scim.conf", NULL);
	v = imsettings_info_variant_new(t, NULL);
	o = imsettings_info_new(v);
	TNUL (o);
	fail_unless(!imsettings_info_is_xim(o), "Object doesn't have to be the XIM");
	g_free(t);
	g_variant_unref(v);
	g_object_unref(o);
} TEND

TDEF (imsettings_info_compare) {
	gchar *t = g_build_filename(IMSETTINGS_SRCDIR, "testcases", "test-scim.conf", NULL);
	IMSettingsInfo *b;
	GVariant *v;

	v = imsettings_info_variant_new(t, NULL);
	o = imsettings_info_new(v);
	b = imsettings_info_new(v);
	TNUL (o);
	TNUL (b);
	fail_unless(imsettings_info_compare(o, b), "Objects has to be the same");
	g_free(t);
	g_variant_unref(v);
	g_object_unref(b);
	t = g_build_filename(IMSETTINGS_SRCDIR, "testcases", "test-scim2.conf", NULL);
	v = imsettings_info_variant_new(t, NULL);
	b = imsettings_info_new(v);
	fail_unless(!imsettings_info_compare(o, b), "Objects doesn't have to be the same");
	g_free(t);
	g_variant_unref(v);
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

	suite_add_tcase(s, tc);

	return s;
}
