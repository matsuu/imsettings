/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * imsettings-info.h
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
#ifndef __IMSETTINGS_IMSETTINGS_INFO_H__
#define __IMSETTINGS_IMSETTINGS_INFO_H__

#include <imsettings/imsettings-object.h>

G_BEGIN_DECLS

#define IMSETTINGS_TYPE_INFO		(imsettings_info_get_type())
#define IMSETTINGS_INFO(_o_)		(G_TYPE_CHECK_INSTANCE_CAST ((_o_), IMSETTINGS_TYPE_INFO, IMSettingsInfo))
#define IMSETTINGS_INFO_CLASS(_c_)	(G_TYPE_CHECK_CLASS_CAST ((_c_), IMSETTINGS_TYPE_INFO, IMSettingsInfoClass))
#define IMSETTINGS_IS_INFO(_o_)		(G_TYPE_CHECK_INSTANCE_TYPE ((_o_), IMSETTINGS_TYPE_INFO))
#define IMSETTINGS_IS_INFO_CLASS(_c_)	(G_TYPE_CHECK_CLASS_TYPE ((_c_), IMSETTINGS_TYPE_INFO))
#define IMSETTINGS_INFO_GET_CLASS(_o_)	(G_TYPE_INSTANCE_GET_CLASS ((_o_), IMSETTINGS_TYPE_INFO, IMSettingsInfoClass))


typedef struct _IMSettingsInfoClass	IMSettingsInfoClass;
typedef struct _IMSettingsInfo		IMSettingsInfo;

struct _IMSettingsInfoClass {
	IMSettingsObjectClass parent_class;
};
struct _IMSettingsInfo {
	IMSettingsObject parent_instance;
};


GType           imsettings_info_get_type              (void) G_GNUC_CONST;
IMSettingsInfo *imsettings_info_new                   (const gchar          *filename);
IMSettingsInfo *imsettings_info_new_with_lang         (const gchar          *filename,
						       const gchar          *language);
const gchar    *imsettings_info_get_language          (IMSettingsInfo       *info);
const gchar    *imsettings_info_get_filename          (IMSettingsInfo       *info);
const gchar    *imsettings_info_get_gtkimm            (IMSettingsInfo       *info);
const gchar    *imsettings_info_get_qtimm             (IMSettingsInfo       *info);
const gchar    *imsettings_info_get_xim               (IMSettingsInfo       *info);
const gchar    *imsettings_info_get_xim_program       (IMSettingsInfo       *info);
const gchar    *imsettings_info_get_xim_args          (IMSettingsInfo       *info);
const gchar    *imsettings_info_get_prefs_program     (IMSettingsInfo       *info);
const gchar    *imsettings_info_get_prefs_args        (IMSettingsInfo       *info);
const gchar    *imsettings_info_get_aux_program       (IMSettingsInfo       *info);
const gchar    *imsettings_info_get_aux_args          (IMSettingsInfo       *info);
const gchar    *imsettings_info_get_short_desc        (IMSettingsInfo       *info);
const gchar    *imsettings_info_get_long_desc         (IMSettingsInfo       *info);
const gchar    *imsettings_info_get_icon_file         (IMSettingsInfo       *info);
const gchar    *imsettings_info_get_supported_language(IMSettingsInfo       *info);
gboolean        imsettings_info_is_visible            (IMSettingsInfo       *info);
gboolean        imsettings_info_is_system_default     (IMSettingsInfo       *info);
gboolean        imsettings_info_is_user_default       (IMSettingsInfo       *info);
gboolean        imsettings_info_is_xim                (IMSettingsInfo       *info);
gboolean        imsettings_info_is_immodule_only      (IMSettingsInfo       *info);
gboolean        imsettings_info_compare               (const IMSettingsInfo *info1,
                                                       const IMSettingsInfo *info2);

G_END_DECLS

#endif /* __IMSETTINGS_IMSETTINGS_INFO_H__ */
