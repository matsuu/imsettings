/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * imsettings-object.c
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

#include <string.h>
#include "imsettings-utils.h"
#include "imsettings-object.h"
#include "imsettings-info.h"

#ifdef GNOME_ENABLE_DEBUG
#define d(e)	e
#else
#define d(e)
#endif

#define _n_pad4		imsettings_n_pad4
#define _pad4		imsettings_pad4
#define _skip_pad4	imsettings_skip_pad4
#define _swap16(_v_)							\
	(byte_order != _imsettings_host_byte_order ?			\
	 (_imsettings_host_byte_order == IMSETTINGS_OBJECT_MSB ?	\
	  GINT16_TO_BE (_v_) : GINT16_TO_LE (_v_)) :			\
	 (_v_))
#define _swapu16(_v_)							\
	(byte_order != _imsettings_host_byte_order ?			\
	 (_imsettings_host_byte_order == IMSETTINGS_OBJECT_MSB ?	\
	  GUINT16_TO_BE (_v_) : GUINT16_TO_LE (_v_)) :			\
	 (_v_))
#define _swap32(_v_)							\
	(byte_order != _imsettings_host_byte_order ?			\
	 (_imsettings_host_byte_order == IMSETTINGS_OBJECT_MSB ?	\
	  GINT32_TO_BE (_v_) : GINT32_TO_LE (_v_)) :			\
	 (_v_))
#define _swapu32(_v_)							\
	(byte_order != _imsettings_host_byte_order ?			\
	 (_imsettings_host_byte_order == IMSETTINGS_OBJECT_MSB ?	\
	  GUINT32_TO_BE (_v_) : GUINT32_TO_LE (_v_)) :			\
	 (_v_))
#define _swap64(_v_)							\
	(byte_order != _imsettings_host_byte_order ?			\
	 (_imsettings_host_byte_order == IMSETTINGS_OBJECT_MSB ?	\
	  GINT64_TO_BE (_v_) : GINT64_TO_LE (_v_)) :			\
	 (_v_))
#define _swapu64(_v_)							\
	(byte_order != _imsettings_host_byte_order ?			\
	 (_imsettings_host_byte_order == IMSETTINGS_OBJECT_MSB ?	\
	  GUINT64_TO_BE (_v_) : GUINT64_TO_LE (_v_)) :			\
	 (_v_))

static const guint8 _imsettings_dump_major_version = 2;
static const guint8 _imsettings_dump_minor_version = 0;
static guint8 _imsettings_host_byte_order = 0;
static gpointer imsettings_object_parent_class = NULL;

/*
 * Private functions
 */
static void
imsettings_object_finalize(GObject *object)
{
	if (G_OBJECT_CLASS (imsettings_object_parent_class)->finalize)
		G_OBJECT_CLASS (imsettings_object_parent_class)->finalize(object);
}

static void
imsettings_object_real_dump(IMSettingsObject  *object,
			    GDataOutputStream *stream)
{
	GHashTable *hash = IMSETTINGS_OBJECT_GET_CLASS (object)->imsettings_properties;
	GHashTableIter iter;
	gpointer key, value;

	g_data_output_stream_put_uint32(stream,
					g_hash_table_size(hash),
					NULL, NULL);
	g_hash_table_iter_init(&iter, hash);
	while (g_hash_table_iter_next(&iter, &key, &value)) {
		g_data_output_stream_put_uint32(stream, GPOINTER_TO_UINT (key), NULL, NULL);
		g_data_output_stream_put_uint32(stream, GPOINTER_TO_UINT (value), NULL, NULL);
	}
}

static void
imsettings_object_real_load(IMSettingsObject *object,
			    GDataInputStream *stream)
{
	guint n, i;
	guint32 k, v;

	n = imsettings_swapu32 (object, g_data_input_stream_read_uint32(stream, NULL, NULL));
	for (i = 0; i < n; i++) {
		k = imsettings_swapu32 (object, g_data_input_stream_read_uint32(stream, NULL, NULL));
		v = imsettings_swapu32 (object, g_data_input_stream_read_uint32(stream, NULL, NULL));
		g_hash_table_insert(IMSETTINGS_OBJECT_GET_CLASS (object)->imsettings_properties,
				    GUINT_TO_POINTER (k), GUINT_TO_POINTER (v));
	}
}

static void
imsettings_object_base_class_init(IMSettingsObjectClass *klass)
{
	klass->imsettings_properties = g_hash_table_new(g_direct_hash, g_direct_equal);
}

static void
imsettings_object_base_class_finalize(IMSettingsObjectClass *klass)
{
	g_hash_table_destroy(klass->imsettings_properties);
}

static void
imsettings_object_class_init(IMSettingsObjectClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	imsettings_object_parent_class = g_type_class_peek_parent(klass);

	object_class->finalize = imsettings_object_finalize;

	klass->dump = imsettings_object_real_dump;
	klass->load = imsettings_object_real_load;
}

static void
imsettings_object_init(IMSettingsObject *object)
{
	object->byte_order = imsettings_get_host_byte_order();
}

/*
 * Public functions
 */
GType
imsettings_object_get_type(void)
{
	static volatile gsize type_id_volatile = 0;

	if (g_once_init_enter(&type_id_volatile)) {
		GType type_id;
		GTypeInfo info = {
			.class_size     = sizeof (IMSettingsObjectClass),
			.base_init      = (GBaseInitFunc)imsettings_object_base_class_init,
			.base_finalize  = (GBaseFinalizeFunc)imsettings_object_base_class_finalize,
			.class_init     = (GClassInitFunc)imsettings_object_class_init,
			.class_finalize = NULL,
			.class_data     = NULL,
			.instance_size  = sizeof (IMSettingsObject),
			.n_preallocs    = 0,
			.instance_init  = (GInstanceInitFunc)imsettings_object_init,
			.value_table    = NULL
		};

		type_id = g_type_register_static(G_TYPE_OBJECT, "IMSettingsObject",
						 &info, G_TYPE_FLAG_ABSTRACT);

		g_once_init_leave(&type_id_volatile, type_id);
	}

	return type_id_volatile;
}

guint8
imsettings_get_host_byte_order(void)
{
	guint8 retval;
	gint i;
	gchar *p;

	if (_imsettings_host_byte_order != 0)
		return _imsettings_host_byte_order;

	i = 1;
	p = (gchar *)&i;
	/* determine the byte order */
	if (*p == 1) {
		/* I'm on LSB */
		retval = IMSETTINGS_OBJECT_LSB;
	} else {
		/* I'm on MSB */
		retval = IMSETTINGS_OBJECT_MSB;
	}
	_imsettings_host_byte_order = retval;

	return retval;
}

void
imsettings_object_class_install_property(IMSettingsObjectClass *oclass,
					 guint                  property_id,
					 GParamSpec            *spec,
					 gboolean               is_self_dump)
{
	g_return_if_fail (IMSETTINGS_IS_OBJECT_CLASS (oclass));

	g_object_class_install_property(G_OBJECT_CLASS (oclass), property_id, spec);
	if (g_object_class_find_property(G_OBJECT_CLASS (oclass), g_param_spec_get_name(spec))) {
		if (g_hash_table_lookup(oclass->imsettings_properties,
					GUINT_TO_POINTER (property_id)) == NULL) {
			g_hash_table_insert(oclass->imsettings_properties,
					    GUINT_TO_POINTER (property_id),
					    GUINT_TO_POINTER ((guint)is_self_dump));
		} else {
			g_printerr("Property `%s' is already registered",
				   g_param_spec_get_name(spec));
		}
	}
}

guint8
imsettings_object_get_byte_order(IMSettingsObject *object)
{
	return object->byte_order;
}

/**
 * dump format:
 * size		type	description
 * 1		BYTE	MSB(0xf0) LSB(0x0f)
 * 1		BYTE	dump major version
 * 1		BYTE	dump minor version
 * 1		PADDING	unused
 * 4		n	length of object type name
 * n		BYTE	object type name
 * PAD4(n)	PADDING	unused
 * 4		long	# of properties
 * 4		n	length of property name
 * n		BYTE	property name
 * PAD4(n)	PADDING	unused
 * 4		GType	property type
 * n		any	data
 * n		any	self dumped data
 */
/*
 * data format:
 *  G_TYPE_CHAR/G_TYPE_UCHAR
 *   1		BYTE	char
 *   PAD4(1)	PADDING	unused
 *
 *  G_TYPE_BOOLEAN
 *   4		BOOL	boolean
 *
 *  G_TYPE_INT/G_TYPE_UINT/G_TYPE_ENUM/G_TYPE_FLAGS
 *   4		INT	integer
 *
 *  G_TYPE_LONG/G_TYPE_ULONG
 *   4		LONG	long integer
 *
 *  G_TYPE_INT64/G_TYPE_UINT64
 *   8		INT64	64bit integer
 *
 *  G_TYPE_STRING
 *   n		BYTE	string
 *   1		BYTE	null
 *   PAD4(n+1)	PADDING	unused
 *
 *  G_TYPE_GTYPE
 *   4		n	length of type name
 *   n		BYTE	type name
 *   PAD4(n)	PADDING	unused
 *
 *  G_TYPE_OBJECT
 *   n		any	any of the above, including dump header
 *   1		BYTE	0
 *   PAD4(n+1)	PADDING	unused
 */
GString *
imsettings_object_dump(IMSettingsObject *object)
{
	GString *retval;
	GOutputStream *base_stream;
	GDataOutputStream *stream;
	GParamSpec **pspecs;
	guint n_properties, i, n;
	const gchar *type_name;
	gsize n_type_name;

	g_return_val_if_fail (IMSETTINGS_IS_OBJECT (object), NULL);

	base_stream = g_memory_output_stream_new(NULL, 0, g_realloc, g_free);
	stream = g_data_output_stream_new(base_stream);
	retval = g_string_new(NULL);

	g_data_output_stream_set_byte_order(stream,
					    object->byte_order == IMSETTINGS_OBJECT_MSB ?
					    G_DATA_STREAM_BYTE_ORDER_BIG_ENDIAN :
					    G_DATA_STREAM_BYTE_ORDER_LITTLE_ENDIAN);
	/* byte order */
	g_data_output_stream_put_byte(stream, object->byte_order, NULL, NULL);
	/* dump version */
	g_data_output_stream_put_byte(stream, _imsettings_dump_major_version, NULL, NULL);
	g_data_output_stream_put_byte(stream, _imsettings_dump_minor_version, NULL, NULL);
	/* padding */
	_pad4 (stream, 3);
	/* object type */
	type_name = g_type_name(G_OBJECT_TYPE (object));
	n_type_name = strlen(type_name);
	g_data_output_stream_put_uint32(stream, n_type_name, NULL, NULL);
	g_data_output_stream_put_string(stream, type_name, NULL, NULL);
	_pad4 (stream, n_type_name);
	/* # of properties */
	pspecs = g_object_class_list_properties(G_OBJECT_GET_CLASS (object),
						&n_properties);
	g_data_output_stream_put_uint32(stream, n_properties, NULL, NULL);
	n = n_properties;

	for (i = 0; i < n_properties; i++) {
		const gchar *prop_name;
		gsize pl, len;
		GType value_type;
		gchar c, *s;
		gboolean b;
		gint32 si;
		guint32 ui;
		gint64 si64;
		guint64 ui64;

		prop_name = g_param_spec_get_name(pspecs[i]);

		if (g_hash_table_lookup(IMSETTINGS_OBJECT_GET_CLASS (object)->imsettings_properties,
					GUINT_TO_POINTER (pspecs[i]->param_id)) == NULL) {
			if ((pspecs[i]->flags & G_PARAM_READABLE) == 0 ||
			    (pspecs[i]->flags & G_PARAM_WRITABLE) == 0 ||
			    (pspecs[i]->flags & (G_PARAM_CONSTRUCT|G_PARAM_CONSTRUCT_ONLY)) != 0) {
				/* no way of dumping this property */
				g_printerr("Unable to dump a property `%s' of %s",
					   prop_name,
					   g_type_name(G_OBJECT_TYPE (object)));
				g_free(pspecs);
				return NULL;
			}
		} else {
			/* this property doesn't want to be exported in the usual way */
			n--;
			continue;
		}

		pl = strlen(prop_name);
		/* length of property name */
		g_data_output_stream_put_uint32(stream, pl, NULL, NULL);
		/* property name */
		g_data_output_stream_put_string(stream, prop_name, NULL, NULL);
		_pad4 (stream, pl);

		value_type = G_PARAM_SPEC_VALUE_TYPE (pspecs[i]);
		g_data_output_stream_put_uint32(stream, value_type, NULL, NULL);
		switch (value_type) {
		    case G_TYPE_CHAR:
		    case G_TYPE_UCHAR:
			    g_object_get(object, prop_name, &c, NULL);
			    g_data_output_stream_put_byte(stream, c, NULL, NULL);
			    _pad4 (stream, 1);
			    break;
		    case G_TYPE_BOOLEAN:
			    g_object_get(object, prop_name, &b, NULL);
			    g_data_output_stream_put_uint32(stream, b, NULL, NULL);
			    break;
		    case G_TYPE_INT:
		    case G_TYPE_LONG:
			    g_object_get(object, prop_name, &si, NULL);
			    g_data_output_stream_put_int32(stream, si, NULL, NULL);
			    break;
		    case G_TYPE_UINT:
		    case G_TYPE_ULONG:
			    g_object_get(object, prop_name, &ui, NULL);
			    g_data_output_stream_put_uint32(stream, ui, NULL, NULL);
			    break;
		    case G_TYPE_INT64:
			    g_object_get(object, prop_name, &si64, NULL);
			    g_data_output_stream_put_int64(stream, si64, NULL, NULL);
			    break;
		    case G_TYPE_UINT64:
			    g_object_get(object, prop_name, &ui64, NULL);
			    g_data_output_stream_put_uint64(stream, ui64, NULL, NULL);
			    break;
		    case G_TYPE_STRING:
			    len = 0;
			    g_object_get(object, prop_name, &s, NULL);
			    if (s) {
				    len = strlen(s);
				    g_data_output_stream_put_string(stream, s, NULL, NULL);
				    g_free(s);
			    }
			    g_data_output_stream_put_byte(stream, 0, NULL, NULL);
			    _pad4 (stream, len + 1);
			    break;
		    default:
			    if (value_type == G_TYPE_GTYPE) {
				    GType t;
				    const gchar *tn;
				    gsize ntn;

				    g_object_get(object, prop_name, &t, NULL);
				    tn = g_type_name(t);
				    ntn = strlen(tn);
				    g_data_output_stream_put_uint32(stream, ntn, NULL, NULL);
				    g_data_output_stream_put_string(stream, tn, NULL, NULL);
				    _pad4 (stream, ntn);
			    } else if (g_type_is_a(value_type, G_TYPE_OBJECT)) {
				    GObject *o = NULL;
				    GString *s;

				    g_object_get(object, prop_name, &o, NULL);
				    s = imsettings_object_dump(IMSETTINGS_OBJECT (o));
				    /* workaround to let the loader know if this is
				     * a GObject.
				     */
				    g_data_output_stream_put_uint32(stream, G_TYPE_OBJECT, NULL, NULL);
				    if (s)
					    g_output_stream_write_all(G_OUTPUT_STREAM (stream),
								      s->str,
								      s->len,
								      NULL, NULL, NULL);
				    g_data_output_stream_put_byte(stream, 0, NULL, NULL);
				    _pad4 (stream, s->len + 1);
				    g_string_free(s, TRUE);
			    } else {
				    g_printerr("Unable to dump GParamSpec of type `%s'\n",
					       g_type_name(value_type));
			    }
			    break;
		}
	}

	g_free(pspecs);

	if (n != n_properties) {
		gpointer v;
		guint32 np = n;
		gsize pos = 8 + n_type_name + _n_pad4 (n_type_name);

		v = g_memory_output_stream_get_data(G_MEMORY_OUTPUT_STREAM (base_stream));
		if (object->byte_order == IMSETTINGS_OBJECT_MSB)
			np = GUINT32_TO_BE (np);
		else
			np = GUINT32_TO_LE (np);

		((gchar *)v)[pos + 0] = ((gchar *)&np)[0];
		((gchar *)v)[pos + 1] = ((gchar *)&np)[1];
		((gchar *)v)[pos + 2] = ((gchar *)&np)[2];
		((gchar *)v)[pos + 3] = ((gchar *)&np)[3];
	}

	if (IMSETTINGS_OBJECT_GET_CLASS (object)->dump) {
		IMSETTINGS_OBJECT_GET_CLASS (object)->dump(object, stream);
	} else {
		g_printerr("Failed to dump the details");
		g_string_free(retval, TRUE);
		retval = NULL;
		goto end;
	}

	g_string_append_len(retval,
			    g_memory_output_stream_get_data(G_MEMORY_OUTPUT_STREAM (base_stream)),
			    g_memory_output_stream_get_size(G_MEMORY_OUTPUT_STREAM (base_stream)));

  end:
	g_object_unref(stream);
	g_object_unref(base_stream);

	return retval;
}

IMSettingsObject *
imsettings_object_load_from_stream(GDataInputStream *stream)
{
	IMSettingsObject *object;
	guint8 byte_order, major_version, minor_version;
	GType type;
	guint n_properties, n, len, j;
	GParameter *params;
	GString *s;

	g_return_val_if_fail (G_IS_DATA_INPUT_STREAM (stream), NULL);

	/* make sure we are sure what the byte order does current host prefer */
	imsettings_get_host_byte_order();

	/* byte order on stream */
	byte_order = g_data_input_stream_read_byte(stream, NULL, NULL);
	major_version = g_data_input_stream_read_byte(stream, NULL, NULL);
	minor_version = g_data_input_stream_read_byte(stream, NULL, NULL);

	if (major_version > _imsettings_dump_major_version ||
	    (major_version == _imsettings_dump_major_version &&
	     minor_version > _imsettings_dump_minor_version)) {
		g_printerr("Unknown dumpped object version: %d.%d [expect: %d.%d or early]\n",
			   major_version, minor_version,
			   _imsettings_dump_major_version, _imsettings_dump_minor_version);
		return NULL;
	}
	if (byte_order != IMSETTINGS_OBJECT_MSB &&
	    byte_order != IMSETTINGS_OBJECT_LSB) {
		g_printerr("Unknown byte order `%02X'\n", byte_order);
		return NULL;
	}
	_skip_pad4 (stream, 3);
	g_data_input_stream_set_byte_order(stream,
					   byte_order == IMSETTINGS_OBJECT_MSB ?
					   G_DATA_STREAM_BYTE_ORDER_BIG_ENDIAN :
					   G_DATA_STREAM_BYTE_ORDER_LITTLE_ENDIAN);

	/* object type */
	len = _swapu32 (g_data_input_stream_read_uint32(stream, NULL, NULL));
	s = g_string_sized_new(len);
	for (j = 0; j < len; j++)
		g_string_append_c(s, g_data_input_stream_read_byte(stream, NULL, NULL));
	_skip_pad4 (stream, len);
	/* FIXME: We support only IMSettingsInfo to deliver so far.
	 * assuming it's.
	 */
	if (strcmp(s->str, "IMSettingsInfo") != 0) {
		g_warning("Unsupported GObject-based object is sent. no way of restore it.");
		return NULL;
	}
	G_STMT_START {
		volatile GType t = imsettings_info_get_type();
		t;
	} G_STMT_END;
	type = g_type_from_name(s->str);
	g_string_free(s, TRUE);
	/* # of properties */
	n_properties = _swapu32 (g_data_input_stream_read_uint32(stream, NULL, NULL));

	params = g_new(GParameter, n_properties + 1);

	for (n = 0; n < n_properties; n++) {
		gsize prop_len, i;
		GString *prop_name = g_string_new(NULL);
		GType value_type;

		/* length of property name */
		prop_len = _swapu32 (g_data_input_stream_read_uint32(stream, NULL, NULL));
		/* property name */
		for (i = 0; i < prop_len; i++) {
			g_string_append_c(prop_name,
					  g_data_input_stream_read_byte(stream, NULL, NULL));
		}
		_skip_pad4 (stream, prop_len);
		/* value type */
		value_type = g_data_input_stream_read_uint32(stream, NULL, NULL);

		params[n].name = g_string_free(prop_name, FALSE);
		params[n].value.g_type = 0;

		switch (value_type) {
		    case G_TYPE_CHAR:
			    g_value_init(&params[n].value, G_TYPE_CHAR);
			    g_value_set_char(&params[n].value,
					     g_data_input_stream_read_byte(stream, NULL, NULL));
			    _skip_pad4 (stream, 1);
			    break;
		    case G_TYPE_UCHAR:
			    g_value_init(&params[n].value, G_TYPE_UCHAR);
			    g_value_set_uchar(&params[n].value,
					      g_data_input_stream_read_byte(stream, NULL, NULL));
			    _skip_pad4 (stream, 1);
			    break;
		    case G_TYPE_BOOLEAN:
			    g_value_init(&params[n].value, G_TYPE_BOOLEAN);
			    g_value_set_boolean(&params[n].value,
						_swapu32 (g_data_input_stream_read_uint32(stream, NULL, NULL)));
			    break;
		    case G_TYPE_INT:
			    g_value_init(&params[n].value, G_TYPE_INT);
			    g_value_set_int(&params[n].value,
					    _swap32 (g_data_input_stream_read_int32(stream, NULL, NULL)));
			    break;
		    case G_TYPE_UINT:
			    g_value_init(&params[n].value, G_TYPE_UINT);
			    g_value_set_uint(&params[n].value,
					     _swapu32 (g_data_input_stream_read_uint32(stream, NULL, NULL)));
			    break;
		    case G_TYPE_LONG:
			    g_value_init(&params[n].value, G_TYPE_LONG);
			    g_value_set_long(&params[n].value,
					     _swap32 (g_data_input_stream_read_int32(stream, NULL, NULL)));
			    break;
		    case G_TYPE_ULONG:
			    g_value_init(&params[n].value, G_TYPE_ULONG);
			    g_value_set_ulong(&params[n].value,
					      _swapu32 (g_data_input_stream_read_uint32(stream, NULL, NULL)));
			    break;
		    case G_TYPE_INT64:
			    g_value_init(&params[n].value, G_TYPE_INT64);
			    g_value_set_int64(&params[n].value,
					      _swap64 (g_data_input_stream_read_int64(stream, NULL, NULL)));
			    break;
		    case G_TYPE_UINT64:
			    g_value_init(&params[n].value, G_TYPE_UINT64);
			    g_value_set_uint64(&params[n].value,
					       _swapu64 (g_data_input_stream_read_uint64(stream, NULL, NULL)));
			    break;
		    case G_TYPE_STRING:
			    G_STMT_START {
				    gchar c;

				    s = g_string_new(NULL);
				    while (1) {
					    c = g_data_input_stream_read_byte(stream, NULL, NULL);
					    if (c == 0)
						    break;
					    g_string_append_c(s, c);
				    }
				    _skip_pad4 (stream, s->len + 1);
				    g_value_init(&params[n].value, G_TYPE_STRING);
				    if (s->len > 0)
					    g_value_set_string(&params[n].value, s->str);
				    else
					    g_value_set_string(&params[n].value, NULL);
				    g_string_free(s, TRUE);
			    } G_STMT_END;
			    break;
		    case G_TYPE_OBJECT:
			    G_STMT_START {
				    gsize start, end;
				    IMSettingsObject *o;

				    start = g_buffered_input_stream_get_available(G_BUFFERED_INPUT_STREAM (stream));
				    o = imsettings_object_load_from_stream(stream);
				    /* dummy */
				    g_data_input_stream_read_byte(stream, NULL, NULL);

				    end = g_buffered_input_stream_get_available(G_BUFFERED_INPUT_STREAM (stream));
				    _skip_pad4 (stream, end - start);
				    g_value_init(&params[n].value, G_TYPE_OBJECT);
				    g_value_set_object(&params[n].value, o);
			    } G_STMT_END;
			    break;
		    default:
			    if (value_type == G_TYPE_GTYPE) {
				    guint ntn;
				    GType t;

				    ntn = g_data_input_stream_read_uint32(stream, NULL, NULL);
				    s = g_string_sized_new(ntn);
				    for (i = 0; i < ntn; i++)
					    g_string_append_c(s, g_data_input_stream_read_byte(stream, NULL, NULL));
				    _skip_pad4 (stream, ntn);

				    t = g_type_from_name(s->str);
				    g_string_free(s, TRUE);

				    if (t == 0) {
					    g_printerr("Unable to recover GType `%s'", s->str);
					    return NULL;
				    }
				    g_value_init(&params[n].value, G_TYPE_GTYPE);
				    g_value_set_gtype(&params[n].value, t);
			    } else {
				    g_printerr("Unable to load the dumpped object due to the unknown/unsupported object type `%s'",
					       g_type_name(value_type));

				    return NULL;
			    }
		}
	}

	params[n].name = NULL;
	object = g_object_newv(type, n_properties, params);
	object->byte_order = byte_order;

	IMSETTINGS_OBJECT_GET_CLASS (object)->load(object, stream);

	object->byte_order = imsettings_get_host_byte_order();

	for (n = 0; n < n_properties; n++) {
		g_free((gchar *)params[n].name);
		if (G_VALUE_HOLDS_STRING (&params[n].value))
			g_value_set_string(&params[n].value, NULL);
		else if (G_VALUE_HOLDS_OBJECT (&params[n].value))
			g_value_set_object(&params[n].value, NULL);
	}
	g_free(params);

	return object;
}

IMSettingsObject *
imsettings_object_load(const gchar *dump_string,
		       gsize        size)
{
	IMSettingsObject *object;
	GInputStream *base_stream;
	GDataInputStream *stream;

	g_return_val_if_fail (dump_string != NULL, NULL);

	base_stream = g_memory_input_stream_new_from_data(dump_string, size, NULL);
	stream = g_data_input_stream_new(base_stream);

	object = imsettings_object_load_from_stream(stream);

	g_object_unref(stream);
	g_object_unref(base_stream);

	return object;
}
