/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * imsettings-info-private.h
 * Copyright (C) 2008 Red hat, Inc. All rights reserved.
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
#ifndef __IMSETTINGS_IMSETTINGS_INFO_PRIVATE_H__
#define __IMSETTINGS_IMSETTINGS_INFO_PRIVATE_H__

#include <glib.h>

G_BEGIN_DECLS

/* global config file */
#define IMSETTINGS_GLOBAL_XINPUT_CONF		"xinputrc"
/* user config file */
#define IMSETTINGS_USER_XINPUT_CONF		".xinputrc"
/* config file use to be determined that IM is never used no matter what */
#define IMSETTINGS_NONE_CONF			"none"
/* config file use to be determined that IM is always used no matter what */
#define IMSETTINGS_XIM_CONF			"xim"
#define IMSETTINGS_USER_SPECIFIC_SHORT_DESC	N_("User Specific")
#define IMSETTINGS_USER_SPECIFIC_LONG_DESC	N_("xinputrc was modified by the user")


void       _imsettings_info_init             (void);
void       _imsettings_info_finalize         (void);
GPtrArray *_imsettings_info_get_filename_list(void);

G_END_DECLS

#endif /* __IMSETTINGS_IMSETTINGS_INFO_PRIVATE_H__ */
