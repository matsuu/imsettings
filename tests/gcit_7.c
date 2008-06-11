/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * gcit_7.c
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

#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include "imsettings/imsettings-info.h"
#include "imsettings/imsettings-utils.h"
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
TDEF (issue) {
	gchar *t = g_build_filename(IMSETTINGS_SRCDIR, "testcases", "test-issue7.conf", NULL);
	GString *d;
	IMSettingsInfo *b = NULL;
	size_t len = strlen(t), length;
	gpointer p, begin;

	o = imsettings_info_new(t);
	TNUL (o);
	d = imsettings_object_dump(IMSETTINGS_OBJECT (o));
	fail_unless(d != NULL, "Failed to dump");

	/* try to validate */
	begin = p = &d->str[0];
	length = d->len;
	while (1) {
		gpointer r;

		if ((r = memchr(p, t[0], length)) == NULL) {
			fail("Unable to find a memory chunk");
			goto end;
		}
		if (memcmp(r, t, len) == 0) {
			gsize sdesc_size, n;
			gint i = 1;
			gchar *pp = (gchar *)&i;

			p = (gchar *)r + len + imsettings_n_pad4 (len);
			sdesc_size = strlen(imsettings_info_get_short_desc(o));
			if (*pp == 1) {
				n = (((gchar *)p)[3] << 24)
					+ (((gchar *)p)[2] << 16)
					+ (((gchar *)p)[1] << 8)
					+ ((gchar *)p)[0];
			} else {
				n = (((gchar *)p)[0] << 24)
					+ (((gchar *)p)[1] << 16)
					+ (((gchar *)p)[2] << 8)
					+ ((gchar *)p)[3];
			}
			fail_unless(n == sdesc_size, "Mismatch the short description size: expected %" G_GSIZE_FORMAT ", actual: %" G_GSIZE_FORMAT, sdesc_size, n);
			break;
		} else {
			p = (gchar *)r + 1;
		}
		if (((gchar *)p - (gchar *)begin) > d->len) {
			fail("Unable to find a filename section.");
			goto end;
		}
		length = d->len - ((gchar *)p - (gchar *)begin);
	}

	b = IMSETTINGS_INFO (imsettings_object_load(d->str, d->len));
	fail_unless(IMSETTINGS_IS_INFO (b), "Failed to load");
	fail_unless(imsettings_info_compare(o, b), "Objects has to be the same");
  end:
	g_free(t);
	g_string_free(d, TRUE);
	g_object_unref(o);
	if (b)
		g_object_unref(b);
} TEND

/************************************************************/
Suite *
imsettings_suite(void)
{
	Suite *s = suite_create("Google Code Issue Tracker");
	TCase *tc = tcase_create("Issue #7: http://code.google.com/p/imsettings/issues/detail?id=7");

	tcase_add_checked_fixture(tc, setup, teardown);

	T (issue);

	suite_add_tcase(s, tc);

	return s;
}
