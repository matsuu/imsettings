/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * rhbz_514852.c
 * Copyright (C) 2009-2010 Red Hat, Inc. All rights reserved.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "imsettings.h"
#include "imsettings-client.h"
#include "main.h"

IMSettingsClient *client;

/************************************************************/
/* common functions                                         */
/************************************************************/
void
setup(void)
{
	client = imsettings_client_new(NULL);
}

void
teardown(void)
{
	imsettings_test_reload_daemons();

	g_object_unref(client);
}

/************************************************************/
/* Test cases                                               */
/************************************************************/
TDEF (issue) {
	gchar *sim = NULL;
	gchar *p;
	GError *error = NULL;
	gchar *d, *tmpl = g_build_filename(g_get_tmp_dir(), "rhbz_514852.XXXXXX", NULL);
	gchar *dest = g_build_filename(IMSETTINGS_SRCDIR, "testcases", "rhbz_514852", "case1", NULL);
	gchar *xinputrc;
	GVariant *v, *vv;
	GVariantIter *iter;
	const gchar *key;
	IMSettingsInfo *info;
	gsize len, slen = strlen(XINPUT_SUFFIX);

	d = mkdtemp(tmpl);
	fail_unless(d != NULL, "Unable to create a temporary directory.");

	p = g_strdup_printf("cp -a %s %s", dest, d);
	if (!g_spawn_command_line_sync(p, NULL, NULL, NULL, &error))
		abort();
	g_free(p);

	xinputrc = g_build_filename(d, "case1", NULL);

	imsettings_test_restart_daemons_full(xinputrc, NULL, NULL);

	g_usleep(5 * G_USEC_PER_SEC);

	v = imsettings_client_get_info_variants(client, NULL, &error);
	fail_unless(v != NULL, "Unable to get the IM objects (take 1)");
	g_variant_get(v, "a{sv}", &iter);
	while (g_variant_iter_next(iter, "{&sv}", &key, &vv)) {
		len = strlen(key);
		if (len > slen &&
		    strcmp(&key[len - slen], XINPUT_SUFFIX) == 0)
			continue;
		info = imsettings_info_new(vv);
		if (imsettings_info_is_system_default(info)) {
			const gchar *n = imsettings_info_get_short_desc(info);

			fail_unless(sim == NULL, "Duplicate the status (take 1): %s %s", sim, n);
			sim = g_strdup(n);
		}
		g_object_unref(info);
	}
	g_variant_unref(v);
	fail_unless(sim != NULL, "No default system IM (take 1)");
	fail_unless(strcmp(sim, "SCIM") == 0, "Unexpected default IM (take 1): %s", sim);
	g_free(sim);
	sim = NULL;

	p = g_strdup_printf("ln -sf ../xinput.d/test-uim.conf %s/alternatives/xinputrc", xinputrc);
	if (!g_spawn_command_line_sync(p, NULL, NULL, NULL, &error))
		abort();
	g_free(p);

	g_usleep(5 * G_USEC_PER_SEC);

	v = imsettings_client_get_info_variants(client, NULL, &error);
	fail_unless(v != NULL, "Unable to get the IM objects (take 2)");
	g_variant_get(v, "a{sv}", &iter);
	while (g_variant_iter_next(iter, "{&sv}", &key, &vv)) {
		len = strlen(key);
		if (len > slen &&
		    strcmp(&key[len - slen], XINPUT_SUFFIX) == 0)
			continue;
		info = imsettings_info_new(vv);
		if (imsettings_info_is_system_default(info)) {
			const gchar *n = imsettings_info_get_short_desc(info);

			fail_unless(sim == NULL, "Duplicate status (take 2): %s", n);
			sim = g_strdup(n);
		}
		g_object_unref(info);
	}
	g_variant_unref(v);
	fail_unless(sim != NULL, "No default system IM (take 2)");
	fail_unless(strcmp(sim, "UIM") == 0, "Unexpected default IM for system (take 2)");
	g_free(sim);
	sim = NULL;

	p = g_strdup_printf("rm -rf %s", tmpl);
	if (!g_spawn_command_line_sync(p, NULL, NULL, NULL, &error))
		abort();

	g_free(xinputrc);
	g_free(sim);
	g_free(p);
	g_free(tmpl);
} TEND

/************************************************************/
Suite *
imsettings_suite(void)
{
	Suite *s = suite_create("Red Hat Bugzilla");
	TCase *tc = tcase_create("Bug#514852: https://bugzilla.redhat.com/show_bug.cgi?id=514852");

	tcase_add_checked_fixture(tc, setup, teardown);
	tcase_set_timeout(tc, 20);

	T (issue);

	suite_add_tcase(s, tc);

	return s;
}
