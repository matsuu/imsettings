/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * rhbz_471833.c
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

#include "imsettings.h"
#include "imsettings-info.h"
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
	GVariant *v;
	IMSettingsInfo *info1, *info2;
	GError *error = NULL;

	imsettings_test_restart_daemons("rhbz_471833" G_DIR_SEPARATOR_S "case1");

	v = imsettings_client_get_info_variant(client, "SCIM", NULL, &error);
	fail_unless(v != NULL, "Failed to obtain a variant for SCIM");
	info1 = imsettings_info_new(v);
	g_variant_unref(v);
	fail_unless(info1 != NULL, "Failed to obtain IMSettingsInfo for SCIM");
	v = imsettings_client_get_info_variant(client, "scim", NULL, &error);
	fail_unless(v != NULL, "Failed to obtain a variant for scim");
	info2 = imsettings_info_new(v);
	g_variant_unref(v);
	fail_unless(info1 != NULL, "Failed to obtain IMSettingsInfo for scim");
	fail_unless(imsettings_info_compare(info1, info2), "Should be same object for SCIM and scim.");

	g_object_unref(info2);

	v = imsettings_client_get_info_variant(client, "test-scim.conf", NULL, &error);
	fail_unless(v != NULL, "Failed to obtain a variant for test-scim.conf");
	info2 = imsettings_info_new(v);
	g_variant_unref(v);
	fail_unless(info2 != NULL, "Failed to obtain IMSettingsInfo for test-scim.conf");
	fail_unless(imsettings_info_compare(info1, info2), "Should be same object for SCIM and test-scim.conf");

	g_object_unref(info1);
	g_object_unref(info2);

	v = imsettings_client_get_info_variant(client, "S C I M", NULL, &error);
	fail_unless(v != NULL, "Failed to obtain a variant for S C I M");
	info1 = imsettings_info_new(v);
	g_variant_unref(v);
	fail_unless(info1 != NULL, "Failed to obtain IMSettingsInfo for S C I M");
	v = imsettings_client_get_info_variant(client, "s c i m", NULL, &error);
	fail_unless(v != NULL, "Failed to obtain a variant for s c i m");
	info2 = imsettings_info_new(v);
	g_variant_unref(v);
	fail_unless(info2 != NULL, "Failed to obtain IMSettingsInfo for s c i m");
	fail_unless(imsettings_info_compare(info1, info2), "Should be same object for S C I M and s c i m.");

	g_object_unref(info2);

	v = imsettings_client_get_info_variant(client, "test-scim2.conf", NULL, &error);
	fail_unless(v != NULL, "Failed to obtain a variant for test-scim2.conf");
	info2 = imsettings_info_new(v);
	g_variant_unref(v);
	fail_unless(info2 != NULL, "Failed to obtain IMSettingsInfo for test-scim2.conf");
	fail_unless(imsettings_info_compare(info1, info2), "Should be same object for S C I M and test-scim2.conf");

	g_object_unref(info1);
	g_object_unref(info2);
} TEND

/************************************************************/
Suite *
imsettings_suite(void)
{
	Suite *s = suite_create("Red Hat Bugzilla");
	TCase *tc = tcase_create("Bug#471833: https://bugzilla.redhat.com/show_bug.cgi?id=471833");

	tcase_add_checked_fixture(tc, setup, teardown);
	tcase_set_timeout(tc, 10);

	T (issue);

	suite_add_tcase(s, tc);

	return s;
}
