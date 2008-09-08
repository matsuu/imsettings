/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * factory_xsettings.c
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

#include <glib.h>
#include <gdk/gdk.h>
#include "imsettings/imsettings-xsettings.h"


static void
_terminate_cb(gpointer data)
{
	GMainLoop *loop = data;

	g_main_loop_quit(loop);
}

int
main(int    argc,
     char **argv)
{
	GdkDisplay *dpy;
	IMSettingsXSettings *settings;
	GMainLoop *loop;

	gdk_init(&argc, &argv);
	dpy = gdk_display_get_default();
	if (dpy == NULL) {
		g_print("Unable to open a X display\n");
		return 1;
	}
	if (imsettings_xsettings_is_available(dpy)) {
		g_print("XSETTINGS manager is already running.\n");
		return 1;
	}
	loop = g_main_loop_new(NULL, FALSE);
	settings = imsettings_xsettings_new(dpy, _terminate_cb, loop);

	g_main_loop_run(loop);

	imsettings_xsettings_free(settings);
	gdk_display_close(dpy);

	return 0;
}
