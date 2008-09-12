/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * compose.h
 * Copyright (C) 2008 Red Hat, Inc. All rights reserved.
 * 
 * Authors:
 *   Akira TAGOH  <tagoh@redhat.com>
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#ifndef __IMSETTINGS_COMPOSE_H__
#define __IMSETTINGS_COMPOSE_H__

#include <stdio.h>
#include <glib.h>

G_BEGIN_DECLS

typedef struct _Compose		Compose;
typedef struct _Sequence	Sequence;

struct _Compose {
	FILE     *fp;
	Sequence *seq_tree;
};

struct _Sequence {
	gulong    keysym;
	guint     modifiers;
	guint     mod_mask;
	GTree    *candidates;
	gchar    *string;
	gulong    composed;
};


Compose  *compose_new   (void);
void      compose_free  (Compose      *compose);
gboolean  compose_open  (Compose      *compose,
                         const gchar  *locale);
void      compose_close (Compose      *compose);
gboolean  compose_parse (Compose      *compose);
gboolean  compose_lookup(Compose      *composer,
			 Sequence    **sequence,
                         gulong        keysym,
                         guint         modifiers,
                         gchar       **result_string,
                         gulong       *result_keysym);


G_END_DECLS

#endif /* __IMSETTINGS_COMPOSE_H__ */
