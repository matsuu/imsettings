/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * radiomenuitem.h
 * Copyright (C) 2008 Akira TAGOH
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
#ifndef __IMSETTINGS_RADIO_MENU_ITEM_H__
#define __IMSETTINGS_RADIO_MENU_ITEM_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define IMSETTINGS_TYPE_RADIO_MENU_ITEM			(imsettings_radio_menu_item_get_type())
#define IMSETTINGS_RADIO_MENU_ITEM(_o_)			(G_TYPE_CHECK_INSTANCE_CAST ((_o_), IMSETTINGS_TYPE_RADIO_MENU_ITEM, IMSettingsRadioMenuItem))
#define IMSETTINGS_RADIO_MENU_ITEM_CLASS(_c_)		(G_TYPE_CHECK_CLASS_CAST ((_c_), IMSETTINGS_TYPE_RADIO_MENU_ITEM, IMSettingsRadioMenuItemClass))
#define IMSETTINGS_IS_RADIO_MENU_ITEM(_o_)		(G_TYPE_CHECK_INSTANCE_TYPE ((_o_), IMSETTINGS_TYPE_RADIO_MENU_ITEM))
#define IMSETTINGS_IS_RADIO_MENU_ITEM_CLASS(_c_)	(G_TYPE_CHECK_CLASS_TYPE ((_c_), IMSETTINGS_TYPE_RADIO_MENU_ITEM))
#define IMSETTINGS_RADIO_MENU_ITEM_GET_CLASS(_o_)	(G_TYPE_INSTANCE_GET_CLASS ((_o_), IMSETTINGS_TYPE_RADIO_MENU_ITEM, IMSettingsRadioMenuItemClass))


typedef struct _IMSettingsRadioMenuItem			IMSettingsRadioMenuItem;
typedef struct _IMSettingsRadioMenuItemClass		IMSettingsRadioMenuItemClass;

struct _IMSettingsRadioMenuItem {
	GtkRadioMenuItem  parent_instance;

	GtkWidget        *image;
};

struct _IMSettingsRadioMenuItemClass {
	GtkRadioMenuItemClass  parent_class;
};


GType      imsettings_radio_menu_item_get_type                  (void) G_GNUC_CONST;
GtkWidget *imsettings_radio_menu_item_new_with_image            (GSList                  *group,
                                                                 const gchar             *label,
                                                                 GtkWidget               *image);
GtkWidget *imsettings_radio_menu_item_new_from_stock            (GSList                  *group,
                                                                 const gchar             *stock_id,
                                                                 GtkAccelGroup           *accel_group);
GtkWidget *imsettings_radio_menu_item_new_from_widget_with_image(IMSettingsRadioMenuItem *item,
                                                                 const gchar             *label,
                                                                 GtkWidget               *image);
void       imsettings_radio_menu_item_set_image                 (IMSettingsRadioMenuItem *item,
                                                                 GtkWidget               *image);


G_END_DECLS

#endif /* __IMSETTINGS_RADIO_MENU_ITEM_H__ */
