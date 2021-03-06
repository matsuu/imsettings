/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * utils.h
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
#ifndef __XIM_UTILS_H__
#define __XIM_UTILS_H__

#include <glib.h>

G_BEGIN_DECLS

typedef struct _XimReply		XimReply;

struct _XimReply {
	guint16 major_opcode;
	guint16 minor_opcode;
	guint16 imid;
	guint16 icid;
};


gchar *xim_substitute_display_name(const gchar *display_name) G_GNUC_MALLOC;

G_END_DECLS

#endif /* __XIM_UTILS_H__ */
