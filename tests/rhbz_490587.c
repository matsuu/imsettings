/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * rhbz_455363.c
 * Copyright (C) 2008-2010 Red Hat, Inc. All rights reserved.
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
#include "imsettings-utils.h"
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
	gchar *p;
	GError *error = NULL;
	gchar *d, *tmpl = g_build_filename(g_get_tmp_dir(), "rhbz_490587.XXXXXX", NULL);
	gchar *dest = g_build_filename(IMSETTINGS_SRCDIR, "testcases", "rhbz_490587", "case1", NULL);
	gchar *xinputrc, *dot_xinputrc;
	GFile *file;
	GFileInfo *finfo;
	const gchar *target;

	d = mkdtemp(tmpl);
	fail_unless(d != NULL, "Unable to create a temporary directory.");

	p = g_strdup_printf("cp -a %s %s", dest, d);
	if (!g_spawn_command_line_sync(p, NULL, NULL, NULL, &error))
		abort();
	g_free(p);

	xinputrc = g_build_filename(d, "case1", NULL);
	dot_xinputrc = g_build_filename(xinputrc, ".xinputrc", NULL);

	imsettings_test_restart_daemons_full(xinputrc, NULL, NULL);

	g_usleep(5 * G_USEC_PER_SEC);

	fail_unless(imsettings_client_switch_im(client, "none", TRUE, NULL, &error), "Unable to switch IM");
	fail_unless(error == NULL, "Unable to stop IM: %s", error ? error->message : "unknown");

	fail_unless(g_file_test(dot_xinputrc, G_FILE_TEST_EXISTS), "No .xinputrc created");

	file = g_file_new_for_path(dot_xinputrc);
	fail_unless(file != NULL, "Unable to create GFile instance.");

	finfo = g_file_query_info(file, G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK "," \
				  G_FILE_ATTRIBUTE_STANDARD_SYMLINK_TARGET,
				  G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
				  NULL, &error);
	fail_unless(finfo != NULL, "Unable to obtain GFileInfo.");

	target = g_file_info_get_symlink_target(finfo);
	fail_unless(target != NULL, "Unable to obtain the target of symlink: %s");

	p = g_path_get_basename(target);
	fail_unless(p && strcmp(p, IMSETTINGS_NONE_CONF XINPUT_SUFFIX) == 0, "Unexpected symlink was created for .xinputrc: actual: %s, expected: %s", p, IMSETTINGS_NONE_CONF XINPUT_SUFFIX);
	g_free(p);

	p = g_strdup_printf("rm -rf %s", tmpl);
	if (!g_spawn_command_line_sync(p, NULL, NULL, NULL, &error))
		abort();

	g_free(xinputrc);
	g_free(p);
	g_free(tmpl);
} TEND

/************************************************************/
Suite *
imsettings_suite(void)
{
	Suite *s = suite_create("Red Hat Bugzilla");
	TCase *tc = tcase_create("Bug#455363: https://bugzilla.redhat.com/show_bug.cgi?id=455363");

	tcase_add_checked_fixture(tc, setup, teardown);
	tcase_set_timeout(tc, 20);

	T (issue);

	suite_add_tcase(s, tc);

	return s;
}
