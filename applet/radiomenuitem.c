/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * radiomenuitem.c
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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib/gi18n-lib.h>
#include "radiomenuitem.h"


enum {
	PROP_0,
	PROP_IMAGE,
	LAST_PROP
};
enum {
	SIGNAL_0,
	LAST_SIGNAL
};

G_DEFINE_TYPE (IMSettingsRadioMenuItem, imsettings_radio_menu_item, GTK_TYPE_RADIO_MENU_ITEM);

/*
 * Private functions
 */
static void
imsettings_radio_menu_item_real_draw_indicator(GtkCheckMenuItem *check_menu_item,
					       GdkRectangle     *area)
{
	IMSettingsRadioMenuItem *item = IMSETTINGS_RADIO_MENU_ITEM (check_menu_item);

	if (item->image && GTK_WIDGET_DRAWABLE (item->image)) {
		if (gtk_check_menu_item_get_active(check_menu_item)) {
			gtk_widget_set_sensitive(item->image, TRUE);
		} else {
			gtk_widget_set_sensitive(item->image, FALSE);
		}
	}
}

static void
imsettings_radio_menu_item_real_set_property(GObject      *object,
					     guint         prop_id,
					     const GValue *value,
					     GParamSpec   *pspec)
{
	IMSettingsRadioMenuItem *item = IMSETTINGS_RADIO_MENU_ITEM (object);
	GtkWidget *image;

	switch (prop_id) {
	    case PROP_IMAGE:
		    image = GTK_WIDGET (g_value_get_object(value));
		    imsettings_radio_menu_item_set_image(item, image);
		    break;
	    default:
		    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		    break;
	}
}

static void
imsettings_radio_menu_item_real_get_property(GObject    *object,
					     guint       prop_id,
					     GValue     *value,
					     GParamSpec *pspec)
{
	IMSettingsRadioMenuItem *item = IMSETTINGS_RADIO_MENU_ITEM (object);

	switch (prop_id) {
	    case PROP_IMAGE:
		    g_value_set_object(value,
				       G_OBJECT (item->image));
		    break;
	    default:
		    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		    break;
	}
}

static void
imsettings_radio_menu_item_real_finalize(GObject *object)
{
	if (G_OBJECT_GET_CLASS (imsettings_radio_menu_item_parent_class)->finalize)
		(* G_OBJECT_GET_CLASS (imsettings_radio_menu_item_parent_class)->finalize) (object);
}

static void
imsettings_radio_menu_item_real_size_request(GtkWidget      *widget,
					     GtkRequisition *requisition)
{
	IMSettingsRadioMenuItem *item;
	gint child_width = 0;
	gint child_height = 0;
	GtkPackDirection pack_dir;

	if (GTK_IS_MENU_BAR (widget->parent))
		pack_dir = gtk_menu_bar_get_child_pack_direction(GTK_MENU_BAR (widget->parent));
	else
		pack_dir = GTK_PACK_DIRECTION_LTR;
	item = IMSETTINGS_RADIO_MENU_ITEM (widget);

	if (item->image &&
	    GTK_WIDGET_VISIBLE (item->image)) {
		GtkRequisition child_requisition;

		gtk_widget_size_request(item->image,
					&child_requisition);
		child_width = child_requisition.width;
		child_height = child_requisition.height;
	}

	(* GTK_WIDGET_CLASS (imsettings_radio_menu_item_parent_class)->size_request) (widget, requisition);

	 /* not done with height since that happens via the
	  * toggle_size_request
	  */
	 if (pack_dir == GTK_PACK_DIRECTION_LTR || pack_dir == GTK_PACK_DIRECTION_RTL)
		 requisition->height = MAX (requisition->height, child_height);
	 else
		 requisition->width = MAX (requisition->width, child_width);

	 /* Note that GtkMenuShell always size requests before
	  * toggle_size_request, so toggle_size_request will be able to use
	  * item->image->requisition
	  */
}

static void
imsettings_radio_menu_item_real_size_allocate(GtkWidget     *widget,
					      GtkAllocation *allocation)
{
	IMSettingsRadioMenuItem *item;
	GtkPackDirection pack_dir;

	if (GTK_IS_MENU_BAR (widget->parent))
		pack_dir = gtk_menu_bar_get_child_pack_direction(GTK_MENU_BAR (widget->parent));
	else
		pack_dir = GTK_PACK_DIRECTION_LTR;

	item = IMSETTINGS_RADIO_MENU_ITEM (widget);

	(* GTK_WIDGET_CLASS (imsettings_radio_menu_item_parent_class)->size_allocate) (widget, allocation);

	if (item->image) {
		gint x, y, offset;
		GtkRequisition child_requisition;
		GtkAllocation child_allocation;
		guint horizontal_padding, toggle_spacing;

		gtk_widget_style_get(widget,
				     "horizontal-padding", &horizontal_padding,
				     "toggle-spacing", &toggle_spacing,
				     NULL);

		/* Man this is lame hardcoding action, but I can't
		 * come up with a solution that's really better.
		 */

		gtk_widget_get_child_requisition(item->image,
						 &child_requisition);
		if (pack_dir == GTK_PACK_DIRECTION_LTR ||
		    pack_dir == GTK_PACK_DIRECTION_RTL) {
			offset = GTK_CONTAINER (item)->border_width + widget->style->xthickness;

			if ((gtk_widget_get_direction(widget) == GTK_TEXT_DIR_LTR) ==
			    (pack_dir == GTK_PACK_DIRECTION_LTR)) {
				x = offset + horizontal_padding + (GTK_MENU_ITEM (item)->toggle_size - toggle_spacing - child_requisition.width) / 2;
			} else {
				x = widget->allocation.width - offset - horizontal_padding - GTK_MENU_ITEM (item)->toggle_size + toggle_spacing + (GTK_MENU_ITEM (item)->toggle_size - toggle_spacing - child_requisition.width) / 2;
			}
			y = (widget->allocation.height - child_requisition.height) / 2;
		} else {
			offset = GTK_CONTAINER (item)->border_width + widget->style->ythickness;

			if ((gtk_widget_get_direction(widget) == GTK_TEXT_DIR_LTR) ==
			    (pack_dir == GTK_PACK_DIRECTION_TTB)) {
				y = offset + horizontal_padding + (GTK_MENU_ITEM (item)->toggle_size - toggle_spacing - child_requisition.height) / 2;
			} else {
				y = widget->allocation.height - offset - horizontal_padding - GTK_MENU_ITEM (item)->toggle_size + toggle_spacing + (GTK_MENU_ITEM (item)->toggle_size - toggle_spacing - child_requisition.height) / 2;
			}
			x = (widget->allocation.width - child_requisition.width) / 2;
		}

		child_allocation.width = child_requisition.width;
		child_allocation.height = child_requisition.height;
		child_allocation.x = widget->allocation.x + MAX (x, 0);
		child_allocation.y = widget->allocation.y + MAX (y, 0);

		gtk_widget_size_allocate(item->image, &child_allocation);
	}
}

static void
imsettings_radio_menu_item_real_forall(GtkContainer *container,
				       gboolean      include_internals,
				       GtkCallback   callback,
				       gpointer      callback_data)
{
	IMSettingsRadioMenuItem *item = IMSETTINGS_RADIO_MENU_ITEM (container);

	(* GTK_CONTAINER_CLASS (imsettings_radio_menu_item_parent_class)->forall) (container,
										   include_internals,
										   callback,
										   callback_data);

	if (item->image)
		(* callback) (item->image, callback_data);
}

static void
imsettings_radio_menu_item_real_remove(GtkContainer *container,
				       GtkWidget    *child)
{
	IMSettingsRadioMenuItem *item = IMSETTINGS_RADIO_MENU_ITEM (container);

	if (child == item->image) {
		gboolean widget_was_visible;

		widget_was_visible = GTK_WIDGET_VISIBLE (child);
		gtk_widget_unparent(child);
		item->image = NULL;

		if (GTK_WIDGET_VISIBLE (container) && widget_was_visible)
			gtk_widget_queue_resize(GTK_WIDGET (container));

		g_object_notify(G_OBJECT (item), "image");
	} else {
		(* GTK_CONTAINER_CLASS (imsettings_radio_menu_item_parent_class)->remove) (container, child);
	}
}

static void
imsettings_radio_menu_item_real_toggle_size_request(GtkMenuItem *menu_item,
						    gint        *requisition)
{
	IMSettingsRadioMenuItem *item = IMSETTINGS_RADIO_MENU_ITEM (menu_item);
	GtkPackDirection pack_dir;

	if (GTK_IS_MENU_BAR (GTK_WIDGET (menu_item)->parent))
		pack_dir = gtk_menu_bar_get_child_pack_direction (GTK_MENU_BAR (GTK_WIDGET (menu_item)->parent));
	else
		pack_dir = GTK_PACK_DIRECTION_LTR;

	*requisition = 0;

	if (item->image) {
		GtkRequisition image_requisition;
		guint toggle_spacing;

		gtk_widget_get_child_requisition(item->image,
						 &image_requisition);
		gtk_widget_style_get(GTK_WIDGET (item),
				     "toggle-spacing", &toggle_spacing,
				     NULL);

		if (pack_dir == GTK_PACK_DIRECTION_LTR ||
		    pack_dir == GTK_PACK_DIRECTION_RTL) {
			if (image_requisition.width > 0)
				*requisition = image_requisition.width + toggle_spacing;
		} else {
			if (image_requisition.height > 0)
				*requisition = image_requisition.height + toggle_spacing;
		}
	}
}

static void
imsettings_radio_menu_item_class_init(IMSettingsRadioMenuItemClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
	GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);
	GtkMenuItemClass *menuitem_class = GTK_MENU_ITEM_CLASS (klass);
	GtkCheckMenuItemClass *check_class = GTK_CHECK_MENU_ITEM_CLASS (klass);

	object_class->set_property = imsettings_radio_menu_item_real_set_property;
	object_class->get_property = imsettings_radio_menu_item_real_get_property;
	object_class->finalize     = imsettings_radio_menu_item_real_finalize;

	widget_class->size_request   = imsettings_radio_menu_item_real_size_request;
	widget_class->size_allocate  = imsettings_radio_menu_item_real_size_allocate;

	container_class->forall = imsettings_radio_menu_item_real_forall;
	container_class->remove = imsettings_radio_menu_item_real_remove;

	menuitem_class->toggle_size_request = imsettings_radio_menu_item_real_toggle_size_request;

	check_class->draw_indicator = imsettings_radio_menu_item_real_draw_indicator;

	/* properties */
	g_object_class_install_property(object_class, PROP_IMAGE,
					g_param_spec_object("image",
							    _("Image widget"),
							    _("Child widget to appear next to the menu text"),
							    GTK_TYPE_WIDGET,
							    G_PARAM_READWRITE));

	/* signals */
}

static void
imsettings_radio_menu_item_init(IMSettingsRadioMenuItem *item)
{
	item->image = NULL;
}

/*
 * Public functions
 */
GtkWidget *
imsettings_radio_menu_item_new_with_image(GSList      *group,
					  const gchar *label,
					  GtkWidget   *image)
{
	GtkRadioMenuItem *item;
	GtkWidget *accel_label;

	g_return_val_if_fail (GTK_IS_IMAGE (image), NULL);

	item = g_object_new(IMSETTINGS_TYPE_RADIO_MENU_ITEM, NULL);
	gtk_radio_menu_item_set_group(item, group);
	accel_label = g_object_new(GTK_TYPE_ACCEL_LABEL, NULL);
	gtk_label_set_text_with_mnemonic(GTK_LABEL (accel_label), label);
	gtk_misc_set_alignment(GTK_MISC (accel_label), 0.0, 0.5);

	gtk_container_add(GTK_CONTAINER (item), accel_label);
	gtk_accel_label_set_accel_widget(GTK_ACCEL_LABEL (accel_label),
					 GTK_WIDGET (item));
	imsettings_radio_menu_item_set_image(IMSETTINGS_RADIO_MENU_ITEM (item),
					     image);
	gtk_widget_show(accel_label);

	return GTK_WIDGET (item);
}

GtkWidget *
imsettings_radio_menu_item_new_from_stock(GSList        *group,
					  const gchar   *stock_id,
					  GtkAccelGroup *accel_group)
{
	GtkWidget *image, *item;
	GtkStockItem stock_item;

	g_return_val_if_fail (stock_id != NULL, NULL);

	image = gtk_image_new_from_stock(stock_id, GTK_ICON_SIZE_MENU);
	if (gtk_stock_lookup(stock_id, &stock_item)) {
		item = imsettings_radio_menu_item_new_with_image(group,
								 stock_item.label,
								 image);
		if (stock_item.keyval && accel_group)
			gtk_widget_add_accelerator(item,
						   "activate",
						   accel_group,
						   stock_item.keyval,
						   stock_item.modifier,
						   GTK_ACCEL_VISIBLE);
	} else {
		item = imsettings_radio_menu_item_new_with_image(group,
								 stock_id,
								 image);
	}

	return item;
}

GtkWidget *
imsettings_radio_menu_item_new_from_widget_with_image(IMSettingsRadioMenuItem *item,
						      const gchar             *label,
						      GtkWidget               *image)
{
	GSList *list = NULL;

	g_return_val_if_fail (IMSETTINGS_IS_RADIO_MENU_ITEM (item), NULL);

	if (item)
		list = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM (item));

	return imsettings_radio_menu_item_new_with_image(list, label, image);
}

void
imsettings_radio_menu_item_set_image(IMSettingsRadioMenuItem *item,
				     GtkWidget               *image)
{
	g_return_if_fail (IMSETTINGS_IS_RADIO_MENU_ITEM (item));

	if (image == item->image)
		return;
	if (item->image)
		gtk_container_remove(GTK_CONTAINER (item),
				     item->image);

	item->image = image;
	if (image == NULL)
		return;

	gtk_widget_set_parent(image, GTK_WIDGET (item));
	g_object_set(image,
		     "visible", TRUE,
		     "no-show-all", TRUE,
		     NULL);

	g_object_notify(G_OBJECT (item), "image");
}
