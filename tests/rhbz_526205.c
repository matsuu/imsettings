/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * rhbz_526205.c
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
#include "imsettings-info.h"
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
	GError *error = NULL;
	gchar *xinputrc = g_build_filename(IMSETTINGS_SRCDIR, "testcases", "rhbz_526205", "case1", NULL);
	const gchar *v;
	IMSettingsInfo *info;

	g_setenv("LANG", "en_US.UTF-8", 1);
	imsettings_test_restart_daemons_full(xinputrc, NULL, xinputrc);

	g_usleep(5 * G_USEC_PER_SEC);

	v = imsettings_client_get_user_im(client, NULL, &error);
	fail_unless(v != NULL, "Unexpected the user IM: (null)");
	fail_unless(strcmp(v, IMSETTINGS_XIM_CONF) != 0, "Unexpected the user IM: %s", v);
	info = imsettings_client_get_info_object(client, "X locale compose", NULL, &error);
	fail_unless(info == NULL, "Unexpected object.");
#if 0
	info = imsettings_request_get_info_object(req, IMSETTINGS_XIM_CONF, &error);
	fail_unless(info != NULL, "Unable to obtain IMSettingsInfo");
	v = imsettings_info_get_short_desc(info);
	fail_unless(strcmp(v, "none") == 0, "Unexpected the short description: %s", v);
#endif

	imsettings_client_set_locale(client, "pt_BR.UTF-8");
	v = imsettings_client_get_user_im(client, NULL, &error);
	fail_unless(v != NULL, "Unexpected the user IM (take 2): (null)");
	fail_unless(strcmp(v, IMSETTINGS_XIM_CONF) != 0, "Unexpected the user IM (take 2): %s", v);
	info = imsettings_client_get_info_object(client, "X locale compose", NULL, &error);
	fail_unless(info != NULL, "Unable to obtain IMSettingsInfo (take 2)");
	v = imsettings_info_get_short_desc(info);
	fail_unless(strcmp(v, "X locale compose") == 0, "Unexpected the short description (take 2): %s", v);

	g_free(xinputrc);
} TEND

/************************************************************/
Suite *
imsettings_suite(void)
{
	Suite *s = suite_create("Red Hat Bugzilla");
	TCase *tc = tcase_create("Bug#526205: https://bugzilla.redhat.com/show_bug.cgi?id=526205");

	tcase_add_checked_fixture(tc, setup, teardown);
	tcase_set_timeout(tc, 20);

	T (issue);

	suite_add_tcase(s, tc);

	return s;
}
