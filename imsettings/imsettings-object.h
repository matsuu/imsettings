/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * imsettings-object.h
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
#ifndef __IMSETTINGS_IMSETTINGS_OBJECT_H__
#define __IMSETTINGS_IMSETTINGS_OBJECT_H__

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define IMSETTINGS_TYPE_OBJECT			(imsettings_object_get_type())
#define IMSETTINGS_OBJECT(_o_)			(G_TYPE_CHECK_INSTANCE_CAST ((_o_), IMSETTINGS_TYPE_OBJECT, IMSettingsObject))
#define IMSETTINGS_OBJECT_CLASS(_c_)		(G_TYPE_CHECK_CLASS_CAST ((_c_), IMSETTINGS_TYPE_OBJECT, IMSettingsObjectClass))
#define IMSETTINGS_IS_OBJECT(_o_)		(G_TYPE_CHECK_INSTANCE_TYPE ((_o_), IMSETTINGS_TYPE_OBJECT))
#define IMSETTINGS_IS_OBJECT_CLASS(_c_)		(G_TYPE_CHECK_CLASS_TYPE ((_c_), IMSETTINGS_TYPE_OBJECT))
#define IMSETTINGS_OBJECT_GET_CLASS(_o_)	(G_TYPE_INSTANCE_GET_CLASS ((_o_), IMSETTINGS_TYPE_OBJECT, IMSettingsObjectClass))

#define IMSETTINGS_OBJECT_LSB			(0x0f)
#define IMSETTINGS_OBJECT_MSB			(0xf0)


typedef struct _IMSettingsObjectClass		IMSettingsObjectClass;
typedef struct _IMSettingsObject		IMSettingsObject;


struct _IMSettingsObjectClass {
	GObjectClass parent_class;

	/* callback */
	void (* dump) (IMSettingsObject  *object,
		       GDataOutputStream *stream);
	void (* load) (IMSettingsObject  *object,
		       GDataInputStream  *stream);

	/*< private >*/
	GHashTable *imsettings_properties;
};

struct _IMSettingsObject {
	GObject parent_instance;

	/*< private >*/
	guint8  byte_order;
};


GType             imsettings_object_get_type              (void) G_GNUC_CONST;
void              imsettings_object_class_install_property(IMSettingsObjectClass *oclass,
                                                           guint                  property_id,
                                                           GParamSpec            *spec,
                                                           gboolean               is_self_dump);
guint8            imsettings_object_get_byte_order        (IMSettingsObject      *object);
GString          *imsettings_object_dump                  (IMSettingsObject      *object);
IMSettingsObject *imsettings_object_load                  (const gchar           *dump_string,
                                                           gsize                  size);
IMSettingsObject *imsettings_object_load_from_stream      (GDataInputStream      *stream);
guint8            imsettings_get_host_byte_order          (void);

G_END_DECLS

#endif /* __IMSETTINGS_IMSETTINGS_OBJECT_H__ */
