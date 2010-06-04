/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * rhbz_599924.c
 * Copyright (C) 2008-20010 Red Hat, Inc. All rights reserved.
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

#include <unistd.h>
#include <glib.h>
#include <glib/gthread.h>
#include "imsettings/imsettings-info.h"
#include "main.h"

#define N_(s)	s

/************************************************************/
/* common functions                                         */
/************************************************************/
void
setup(void)
{
	g_thread_init(NULL);
}

void
teardown(void)
{
}

static gpointer
_thread(gpointer data)
{
	IMSettingsInfo **info = (IMSettingsInfo **)data;
	gchar *xinputrc;
	static gboolean first = FALSE;
	static gint i = 0;

	if (!first) {
		first = TRUE;
		xinputrc = g_build_filename(IMSETTINGS_SRCDIR, "testcases", "rhbz_599924", "xinput.d", "xim2.conf", NULL);
	} else {
		xinputrc = g_build_filename(IMSETTINGS_SRCDIR, "testcases", "rhbz_599924", "xinput.d", "xim.conf", NULL);
	}
	g_print("creating...%d\n", ++i);
	*info = imsettings_info_new(xinputrc);
	g_free(xinputrc);

	return NULL;
}

/************************************************************/
/* Test cases                                               */
/************************************************************/
TDEF (issue) {
	GThread *th[256];
	IMSettingsInfo *i[256];
	int x;

	for (x = 0; x < 256; x++) {
		i[x] = NULL;
		th[x] = g_thread_create(&_thread, (gpointer)&i[x], TRUE, NULL);
	}
	for (x = 0; x < 256; x++) {
		g_thread_join(th[x]);
		fail_unless(i[x] != NULL, "Unable to create the instance.");
		fail_unless(imsettings_info_get_short_desc(i[x]) != NULL, "Unable to obtain the short description: %d", x);
		if (x > 0) {
			fail_unless(imsettings_info_compare(i[x - 1], i[x]), "Obtaining different information: %d vs %d", x - 1, x);
		}
	}
} TEND

/************************************************************/
Suite *
imsettings_suite(void)
{
	Suite *s = suite_create("Red Hat Bugzilla");
	TCase *tc = tcase_create("Bug#599924: https://bugzilla.redhat.com/show_bug.cgi?id=599924");

	tcase_add_checked_fixture(tc, setup, teardown);
	tcase_set_timeout(tc, 60);

	T (issue);

	suite_add_tcase(s, tc);

	return s;
}
