/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * imsettings-object.c
 * Copyright (C) 2008 Red Hat Inc. All rights reserved.
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

#include <imsettings/imsettings-object.h>
#include "main.h"


IMSettingsObject *o;

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
TDEF (imsettings_object_new)
{
	o = g_object_new(IMSETTINGS_TYPE_OBJECT, NULL);
	fail_unless(o == NULL, "Object from Abstraction class doesn't have to be created.");
	/* assuming the warning message should be there */
	g_free(imsettings_test_pop_error());
} TEND

/************************************************************/
Suite *
imsettings_suite(void)
{
	Suite *s = suite_create("IMSettingsObject");
	TCase *tc = tcase_create("Generic Functionalities");

	tcase_add_checked_fixture(tc, setup, teardown);

	T (imsettings_object_new);

	suite_add_tcase(s, tc);

	return s;
}
