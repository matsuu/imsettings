/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * imsettings-info.c
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

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <glib/gi18n-lib.h>
#include "imsettings-utils.h"
#include "imsettings-info.h"
#include "imsettings-info-private.h"

#define IMSETTINGS_INFO_GET_PRIVATE(_o_)	(G_TYPE_INSTANCE_GET_PRIVATE ((_o_), IMSETTINGS_TYPE_INFO, IMSettingsInfoPrivate))
#define _skip_blanks(_str)				\
	while (1) {					\
		if (*(_str) == 0 || !isspace(*(_str)))	\
			break;				\
		(_str)++;				\
	}
#define _skip_tokens(_str)				\
	while (1) {					\
		if (*(_str) == 0 || isspace(*(_str)))	\
			break;				\
		(_str)++;				\
	}
#ifdef GNOME_ENABLE_DEBUG
#define d(e)	e
#else
#define d(e)
#endif


typedef struct _IMSettingsInfoPrivate {
	gchar    *language;
	gchar    *filename;
	gchar    *gtkimm;
	gchar    *qtimm;
	gchar    *xim;
	gchar    *xim_prog;
	gchar    *xim_args;
	gchar    *prefs_prog;
	gchar    *prefs_args;
	gchar    *aux_prog;
	gchar    *aux_args;
	gchar    *short_desc;
	gchar    *long_desc;
	gboolean  ignore;
	gboolean  is_system_default;
	gboolean  is_user_default;
	gboolean  is_xim;
} IMSettingsInfoPrivate;

enum {
	PROP_0 = 0,
	PROP_LANGUAGE,
	PROP_FILENAME,
	PROP_GTK_IMM,
	PROP_QT_IMM,
	PROP_XIM,
	PROP_IGNORE_FLAG,
	PROP_XIM_PROG,
	PROP_XIM_PROG_ARGS,
	PROP_PREFS_PROG,
	PROP_PREFS_PROG_ARGS,
	PROP_AUX_PROG,
	PROP_AUX_PROG_ARGS,
	PROP_SHORT_DESC,
	PROP_LONG_DESC,
	PROP_IS_SYSTEM_DEFAULT,
	PROP_IS_USER_DEFAULT,
	PROP_IS_XIM,
	LAST_PROP
};


static GPtrArray *_imsettings_filename_list = NULL;


G_DEFINE_TYPE (IMSettingsInfo, imsettings_info, IMSETTINGS_TYPE_OBJECT);

/*
 * Private functions
 */
static gchar *
_unquote_string(const gchar *str)
{
	gboolean dq = FALSE, sq = FALSE;
	const gchar *p;
	GString *retval = g_string_new(NULL);

	for (p = str; *p != 0 && (!isspace(*p) || dq || sq); p++) {
		if (*p == '"' && !sq)
			dq = !dq;
		else if (*p == '\'' && !dq)
			sq = !sq;
		else if (*p == '\\' && sq == 0) {
			switch (*(p + 1)) {
			    case '"':
			    case '\'':
			    case '\\':
				    g_string_append_c(retval, *(p + 1));
				    p++;
				    break;
			    case 'b':
				    g_string_append_c(retval, '\b');
				    p++;
				    break;
			    case 'f':
				    g_string_append_c(retval, '\f');
				    p++;
				    break;
			    case 'n':
				    g_string_append_c(retval, '\n');
				    p++;
				    break;
			    case 'r':
				    g_string_append_c(retval, '\r');
				    p++;
				    break;
			    case 't':
				    g_string_append_c(retval, '\t');
				    p++;
				    break;
			    default:
				    g_string_append_c(retval, *p);
				    break;
			}
		} else {
			g_string_append_c(retval, *p);
		}
	}

	return g_string_free(retval, FALSE);
}

static void
imsettings_info_notify_properties(GObject     *object,
				  const gchar *filename)
{
	GString *cmd, *str;
	gchar *xinputinfo, *p, buffer[256];
	static const gchar *_xinput_tokens[] = {
		"GTK_IM_MODULE=",
		"QT_IM_MODULE=",
		"XIM=",
		"IMSETTINGS_IGNORE_ME=",
		"XIM_PROGRAM=",
		"XIM_ARGS=",
		"PREFERENCE_PROGRAM=",
		"PREFERENCE_ARGS=",
		"AUXILIARY_PROGRAM=",
		"AUXILIARY_ARGS=",
		"SHORT_DESC=",
		"LONG_DESC=",
		NULL
	};
	static const gchar *properties[] = {
		"gtkimm",
		"qtimm",
		"xim",
		"ignore",
		"xim_prog",
		"xim_args",
		"prefs_prog",
		"prefs_args",
		"aux_prog",
		"aux_args",
		"short_desc",
		"long_desc",
		NULL
	};
	gint i;
	FILE *fp;
	guint prop;
	struct stat st;
	IMSettingsInfoPrivate *priv = IMSETTINGS_INFO_GET_PRIVATE (object);
	gchar *lang;

	cmd = g_string_new(NULL);
	str = g_string_new(NULL);
	if (priv->language)
		lang = g_strdup_printf("LANG=%s ", priv->language);
	else
		lang = g_strdup("");
	xinputinfo = g_build_filename(XINPUTINFO_PATH, "xinputinfo.sh", NULL);
	g_string_append_printf(cmd, "%s. %s %s", lang, xinputinfo, filename);

	g_free(lang);

	if (lstat(filename, &st) == -1 ||
	    (fp = popen(cmd->str, "r")) == NULL) {
		/* error happens. don't list. */
		g_object_set(object, "ignore", TRUE, NULL);
	} else {
		while (!feof(fp)) {
			if ((p = fgets(buffer, 255, fp)) != NULL) {
				_skip_blanks(p);
				while (*p != 0) {
					prop = 0;
					for (i = 0; _xinput_tokens[i] != NULL; i++) {
						size_t len = strlen(_xinput_tokens[i]);

						if (strncmp(p, _xinput_tokens[i], len) == 0) {
							gchar *ret;

							prop = i + (PROP_GTK_IMM - PROP_0);
							p += len;
							ret = _unquote_string(p);
							g_string_append(str, ret);
							g_free(ret);
							break;
						}
					}
					switch (prop) {
					    case PROP_0:
					    case PROP_FILENAME:
						    _skip_tokens(p);
						    break;
					    case PROP_GTK_IMM:
					    case PROP_QT_IMM:
					    case PROP_XIM:
					    case PROP_XIM_PROG:
					    case PROP_XIM_PROG_ARGS:
					    case PROP_PREFS_PROG:
					    case PROP_PREFS_PROG_ARGS:
					    case PROP_AUX_PROG:
					    case PROP_AUX_PROG_ARGS:
					    case PROP_SHORT_DESC:
					    case PROP_LONG_DESC:
						    d(g_print("  %s: %s\n",
							      _xinput_tokens[prop - (PROP_GTK_IMM - PROP_0)],
							      str->str));
						    g_object_set(object,
								 properties[prop - (PROP_GTK_IMM - PROP_0)], str->str,
								 NULL);
						    break;
					    case PROP_IGNORE_FLAG:
						    g_object_set(object,
								 "ignore",
								 (g_ascii_strcasecmp(str->str, "true") == 0 ||
								  g_ascii_strcasecmp(str->str, "yes") == 0 ||
								  g_ascii_strcasecmp(str->str, "1") == 0),
								 NULL);
						    break;
					    default:
						    break;
					}
					g_string_erase(str, 0, -1);
					_skip_blanks(p);
				}
			}
		}
		pclose(fp);
	}
	g_string_free(cmd, TRUE);
	g_string_free(str, TRUE);
}

static void
imsettings_info_set_property(GObject      *object,
			     guint         prop_id,
			     const GValue *value,
			     GParamSpec   *pspec)
{
	IMSettingsInfoPrivate *priv = IMSETTINGS_INFO_GET_PRIVATE (object);
	gchar *p, *n;
	struct stat st;

#define _set_str_prop(_m_)						\
	G_STMT_START {							\
		const gchar *v;						\
									\
		g_free(priv->_m_);					\
		v = g_value_get_string(value);				\
		if (v && v[0] != 0) {					\
			priv->_m_ = g_strdup(g_value_get_string(value)); \
		} else {						\
			priv->_m_ = NULL;				\
		}							\
	} G_STMT_END
#define _set_bool_prop(_m_)				\
	G_STMT_START {					\
		priv->_m_ = g_value_get_boolean(value);	\
	} G_STMT_END

	switch (prop_id) {
	    case PROP_LANGUAGE:
		    _set_str_prop(language);
		    if (priv->filename &&
			(imsettings_info_is_visible(IMSETTINGS_INFO (object)) ||
			 imsettings_info_is_xim(IMSETTINGS_INFO (object)))) {
			    /* XIM always has to be updated. */
			    imsettings_info_notify_properties(object, priv->filename);
		    }
		    break;
	    case PROP_FILENAME:
		    _set_str_prop(filename);
		    n = g_path_get_basename(priv->filename);
		    if (strcmp(n, IMSETTINGS_XIM_CONF XINPUT_SUFFIX) == 0) {
			    priv->is_xim = TRUE;
			    g_object_set(object, "ignore", FALSE, NULL);
		    } else if (strcmp(n, IMSETTINGS_NONE_CONF XINPUT_SUFFIX) == 0) {
			    g_object_set(object, "ignore", TRUE, NULL);
		    } else {
			    g_object_set(object, "ignore", FALSE, NULL);
		    }
		    p = g_path_get_dirname(priv->filename);
		    if ((strcmp(n, IMSETTINGS_USER_XINPUT_CONF) == 0 ||
			 strcmp(n, IMSETTINGS_USER_XINPUT_CONF ".bak") == 0) &&
			strcmp(p, g_get_home_dir()) == 0 &&
			lstat(priv->filename, &st) == 0 &&
			!S_ISLNK (st.st_mode)) {
			    imsettings_info_notify_properties(object, priv->filename);
			    /* special case to deal with the user-own conf file */
			    g_object_set(object,
					 "short_desc", IMSETTINGS_USER_SPECIFIC_SHORT_DESC,
					 "long_desc", IMSETTINGS_USER_SPECIFIC_LONG_DESC,
					 NULL);
		    } else if (stat(priv->filename, &st) == -1) {
			    /* maybe dead link */
			    g_object_set(object, "ignore", TRUE, NULL);
		    } else {
			    imsettings_info_notify_properties(object, priv->filename);
		    }
		    g_free(p);
		    g_free(n);
		    break;
	    case PROP_GTK_IMM:
		    _set_str_prop(gtkimm);
		    break;
	    case PROP_QT_IMM:
		    _set_str_prop(qtimm);
		    break;
	    case PROP_XIM:
		    _set_str_prop(xim);
		    break;
	    case PROP_IGNORE_FLAG:
		    _set_bool_prop(ignore);
		    break;
	    case PROP_XIM_PROG:
		    _set_str_prop(xim_prog);
		    break;
	    case PROP_XIM_PROG_ARGS:
		    _set_str_prop(xim_args);
		    break;
	    case PROP_PREFS_PROG:
		    _set_str_prop(prefs_prog);
		    break;
	    case PROP_PREFS_PROG_ARGS:
		    _set_str_prop(prefs_args);
		    break;
	    case PROP_AUX_PROG:
		    _set_str_prop(aux_prog);
		    break;
	    case PROP_AUX_PROG_ARGS:
		    _set_str_prop(aux_args);
		    break;
	    case PROP_SHORT_DESC:
		    _set_str_prop(short_desc);
		    break;
	    case PROP_LONG_DESC:
		    _set_str_prop(long_desc);
		    break;
	    case PROP_IS_SYSTEM_DEFAULT:
		    _set_bool_prop(is_system_default);
		    break;
	    case PROP_IS_USER_DEFAULT:
		    _set_bool_prop(is_user_default);
		    break;
	    default:
		    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		    break;
	}

#undef _set_bool_prop
#undef _set_str_prop
}

static void
imsettings_info_get_property(GObject    *object,
			     guint       prop_id,
			     GValue     *value,
			     GParamSpec *pspec)
{
	IMSettingsInfoPrivate *priv = IMSETTINGS_INFO_GET_PRIVATE (object);

#define _get_str_prop(_m_)						\
	g_value_set_string(value, imsettings_info_get_ ## _m_(IMSETTINGS_INFO (object)))
#define _get_bool_prop(_m_)			\
	g_value_set_boolean(value, priv->_m_)

	switch (prop_id) {
	    case PROP_LANGUAGE:
		    _get_str_prop(language);
		    break;
	    case PROP_FILENAME:
		    _get_str_prop(filename);
		    break;
	    case PROP_GTK_IMM:
		    _get_str_prop(gtkimm);
		    break;
	    case PROP_QT_IMM:
		    _get_str_prop(qtimm);
		    break;
	    case PROP_XIM:
		    _get_str_prop(xim);
		    break;
	    case PROP_IGNORE_FLAG:
		    _get_bool_prop(ignore);
		    break;
	    case PROP_XIM_PROG:
		    _get_str_prop(xim_program);
		    break;
	    case PROP_XIM_PROG_ARGS:
		    _get_str_prop(xim_args);
		    break;
	    case PROP_PREFS_PROG:
		    _get_str_prop(prefs_program);
		    break;
	    case PROP_PREFS_PROG_ARGS:
		    _get_str_prop(prefs_args);
		    break;
	    case PROP_AUX_PROG:
		    _get_str_prop(aux_program);
		    break;
	    case PROP_AUX_PROG_ARGS:
		    _get_str_prop(aux_args);
		    break;
	    case PROP_SHORT_DESC:
		    _get_str_prop(short_desc);
		    break;
	    case PROP_LONG_DESC:
		    _get_str_prop(long_desc);
		    break;
	    case PROP_IS_SYSTEM_DEFAULT:
		    _get_bool_prop(is_system_default);
		    break;
	    case PROP_IS_USER_DEFAULT:
		    _get_bool_prop(is_user_default);
		    break;
	    case PROP_IS_XIM:
		    _get_bool_prop(is_xim);
		    break;
	    default:
		    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		    break;
	}
}

static void
imsettings_info_finalize(GObject *object)
{
	IMSettingsInfoPrivate *priv = IMSETTINGS_INFO_GET_PRIVATE (object);

	g_free(priv->language);
	g_free(priv->filename);
	g_free(priv->gtkimm);
	g_free(priv->qtimm);
	g_free(priv->xim);
	g_free(priv->xim_prog);
	g_free(priv->xim_args);
	g_free(priv->prefs_prog);
	g_free(priv->prefs_args);
	g_free(priv->aux_prog);
	g_free(priv->aux_args);
	g_free(priv->short_desc);
	g_free(priv->long_desc);

	if (G_OBJECT_CLASS (imsettings_info_parent_class)->finalize)
		G_OBJECT_CLASS (imsettings_info_parent_class)->finalize(object);
}

static void
imsettings_info_dump(IMSettingsObject  *object,
		     GDataOutputStream *stream)
{
	IMSettingsInfoPrivate *priv = IMSETTINGS_INFO_GET_PRIVATE (object);
	gsize len = 0;

	if (IMSETTINGS_OBJECT_CLASS (imsettings_info_parent_class)->dump)
		IMSETTINGS_OBJECT_CLASS (imsettings_info_parent_class)->dump(object, stream);

	/* PROP_LANGUAGE */
	if (priv->language)
		len = strlen(priv->language);
	g_data_output_stream_put_uint32(stream, len, NULL, NULL);
	if (priv->language)
		g_data_output_stream_put_string(stream, priv->language, NULL, NULL);
	imsettings_pad4 (stream, len);
	/* PROP_FILENAME */
	len = strlen(priv->filename);
	g_data_output_stream_put_uint32(stream, len, NULL, NULL);
	g_data_output_stream_put_string(stream, priv->filename, NULL, NULL);
	imsettings_pad4 (stream, len);
	/* PROP_SHORT_DESC */
	if (priv->short_desc)
		len = strlen(priv->short_desc);
	else
		len = 0;
	g_data_output_stream_put_uint32(stream, len, NULL, NULL);
	if (priv->short_desc)
		g_data_output_stream_put_string(stream, priv->short_desc, NULL, NULL);
	imsettings_pad4 (stream, len);
	/* PROP_IS_XIM */
	g_data_output_stream_put_uint32(stream, priv->is_xim, NULL, NULL);
}

static void
imsettings_info_load(IMSettingsObject *object,
		     GDataInputStream *stream)
{
	IMSettingsInfoPrivate *priv = IMSETTINGS_INFO_GET_PRIVATE (object);
	gsize len;
	GString *s = g_string_new(NULL);

	if (IMSETTINGS_OBJECT_CLASS (imsettings_info_parent_class)->load)
		IMSETTINGS_OBJECT_CLASS (imsettings_info_parent_class)->load(object, stream);

	/* PROP_LANGUAGE */
	len = imsettings_swapu32 (object,
				  g_data_input_stream_read_uint32(stream, NULL, NULL));
	while (len > 0) {
		g_string_append_c(s, g_data_input_stream_read_byte(stream, NULL, NULL));
		len--;
	}
	if (s->len > 0)
		priv->language = g_string_free(s, FALSE);
	else
		g_string_free(s, TRUE);
	imsettings_skip_pad4 (stream, len);
	/* PROP_FILENAME */
	s = g_string_new(NULL);
	len = imsettings_swapu32 (object,
				  g_data_input_stream_read_uint32(stream, NULL, NULL));
	while (len > 0) {
		g_string_append_c(s, g_data_input_stream_read_byte(stream, NULL, NULL));
		len--;
	}
	priv->filename = g_string_free(s, FALSE);
	imsettings_skip_pad4 (stream, len);
	/* PROP_SHORT_DESC */
	s = g_string_new(NULL);
	len = imsettings_swapu32 (object,
				  g_data_input_stream_read_uint32(stream, NULL, NULL));
	while (len > 0) {
		g_string_append_c(s, g_data_input_stream_read_byte(stream, NULL, NULL));
		len--;
	}
	if (s->len > 0)
		priv->short_desc = g_string_free(s, FALSE);
	else
		g_string_free(s, TRUE);
	imsettings_skip_pad4 (stream, len);
	/* PROP_IS_XIM */
	priv->is_xim = imsettings_swapu32 (object,
					   g_data_input_stream_read_uint32(stream, NULL, NULL));
}

static void
imsettings_info_class_init(IMSettingsInfoClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	IMSettingsObjectClass *imsettings_class = IMSETTINGS_OBJECT_CLASS (klass);

	g_type_class_add_private(klass, sizeof (IMSettingsInfoPrivate));

	object_class->set_property = imsettings_info_set_property;
	object_class->get_property = imsettings_info_get_property;
	object_class->finalize     = imsettings_info_finalize;

	imsettings_class->dump = imsettings_info_dump;
	imsettings_class->load = imsettings_info_load;

	/* properties */
	/* those properties has to be dumped/loaded by self dumper/loader
	 * because we have to avoid overwriting by imsettings_info_notify_properties().
	 */
	imsettings_object_class_install_property(imsettings_class, PROP_LANGUAGE,
						 g_param_spec_string("language",
								     _("Language"),
								     _("A language to pull the information in."),
								     NULL,
								     G_PARAM_READWRITE | G_PARAM_CONSTRUCT),
						 TRUE);
	imsettings_object_class_install_property(imsettings_class, PROP_FILENAME,
						 g_param_spec_string("filename",
								     _("Filename"),
								     _("A filename referring to the IM information."),
								     NULL,
								     G_PARAM_READWRITE),
						 TRUE);
	imsettings_object_class_install_property(imsettings_class, PROP_SHORT_DESC,
						 g_param_spec_string("short_desc",
								     _("Short Description"),
								     _("Short Description"),
								     NULL,
								     G_PARAM_READWRITE),
						 TRUE);
	imsettings_object_class_install_property(imsettings_class, PROP_IS_XIM,
						 g_param_spec_boolean("is_xim",
								      _("XIM"),
								      _("Whether or not IM is a XIM server."),
								      FALSE,
								      G_PARAM_READABLE),
						 TRUE);
	g_object_class_install_property(object_class, PROP_GTK_IMM,
					g_param_spec_string("gtkimm",
							    _("GTK+ immodule"),
							    _("A module name used for GTK+"),
							    NULL,
							    G_PARAM_READWRITE));
	g_object_class_install_property(object_class, PROP_QT_IMM,
					g_param_spec_string("qtimm",
							    _("Qt immodule"),
							    _("A module name used for Qt"),
							    NULL,
							    G_PARAM_READWRITE));
	g_object_class_install_property(object_class, PROP_XIM,
					g_param_spec_string("xim",
							    _("XIM name"),
							    _("XIM server name"),
							    NULL,
							    G_PARAM_READWRITE));
	g_object_class_install_property(object_class, PROP_IGNORE_FLAG,
					g_param_spec_boolean("ignore",
							     _("Ignore flag"),
							     _("A flag to not list up."),
							     FALSE,
							     G_PARAM_READWRITE));
	g_object_class_install_property(object_class, PROP_XIM_PROG,
					g_param_spec_string("xim_prog",
							    _("XIM server program name"),
							    _("XIM server program to be run"),
							    NULL,
							    G_PARAM_READWRITE));
	g_object_class_install_property(object_class, PROP_XIM_PROG_ARGS,
					g_param_spec_string("xim_args",
							    _("XIM server program's options"),
							    _("Command line options for XIM server program"),
							    NULL,
							    G_PARAM_READWRITE));
	g_object_class_install_property(object_class, PROP_PREFS_PROG,
					g_param_spec_string("prefs_prog",
							    _("Preference program name"),
							    _("Preference program to be run"),
							    NULL,
							    G_PARAM_READWRITE));
	g_object_class_install_property(object_class, PROP_PREFS_PROG_ARGS,
					g_param_spec_string("prefs_args",
							    _("Preference program's options"),
							    _("Command line options for preference program"),
							    NULL,
							    G_PARAM_READWRITE));
	g_object_class_install_property(object_class, PROP_AUX_PROG,
					g_param_spec_string("aux_prog",
							    _("Auxiliary program name"),
							    _("Auxiliary program to be run"),
							    NULL,
							    G_PARAM_READWRITE));
	g_object_class_install_property(object_class, PROP_AUX_PROG_ARGS,
					g_param_spec_string("aux_args",
							    _("Auxiliary program's options"),
							    _("Command line options for auxiliary program"),
							    NULL,
							    G_PARAM_READWRITE));
	g_object_class_install_property(object_class, PROP_LONG_DESC,
					g_param_spec_string("long_desc",
							    _("Long Description"),
							    _("Long Description"),
							    NULL,
							    G_PARAM_READWRITE));
	g_object_class_install_property(object_class, PROP_IS_SYSTEM_DEFAULT,
					g_param_spec_boolean("is_system_default",
							     _("System Default"),
							     _("Whether or not IM is a system default."),
							     FALSE,
							     G_PARAM_READWRITE));
	g_object_class_install_property(object_class, PROP_IS_USER_DEFAULT,
					g_param_spec_boolean("is_user_default",
							     _("User Default"),
							     _("Whether or not IM is an user default."),
							     FALSE,
							     G_PARAM_READWRITE));
}

static void
imsettings_info_init(IMSettingsInfo *info)
{
	IMSettingsInfoPrivate *priv = IMSETTINGS_INFO_GET_PRIVATE (info);

	priv->filename = NULL;
	priv->gtkimm = NULL;
	priv->qtimm = NULL;
	priv->xim = NULL;
	priv->xim_prog = NULL;
	priv->xim_args = NULL;
	priv->prefs_prog = NULL;
	priv->prefs_args = NULL;
	priv->aux_prog = NULL;
	priv->aux_args = NULL;
	priv->short_desc = NULL;
	priv->long_desc = NULL;
}

static GPtrArray *
imsettings_info_update_filename_list(const gchar *_path)
{
	const gchar *path, *name;
	GDir *dir;
	GError *error = NULL;
	gchar *filename;
	struct stat st;
	size_t len, suffixlen = strlen(XINPUT_SUFFIX);
	GPtrArray *retval = NULL;

	if (_path == NULL)
		path = XINPUT_PATH;
	else
		path = _path;

	if ((dir = g_dir_open(path, 0, &error)) == NULL) {
		g_warning("%s\n", error->message);

		return NULL;
	}
	retval = g_ptr_array_new();
	while ((name = g_dir_read_name(dir)) != NULL) {
		len = strlen(name);
		if (len < suffixlen ||
		    strcmp(&name[len - suffixlen], XINPUT_SUFFIX) != 0)
			continue;
		filename = g_build_filename(path, name, NULL);
		if (lstat(filename, &st) == 0) {
			if (S_ISREG (st.st_mode) && !S_ISLNK (st.st_mode))
				g_ptr_array_add(retval, g_strdup(filename));
		}
		g_free(filename);
	}
	g_dir_close(dir);

	if (retval->len == 0) {
		g_ptr_array_free(retval, TRUE);
		retval = NULL;
	}

	return retval;
}

/*
 * Public functions
 */
void
_imsettings_info_init(void)
{
	if (_imsettings_filename_list)
		_imsettings_info_finalize();

	_imsettings_filename_list = imsettings_info_update_filename_list(NULL);
}

void
_imsettings_info_finalize(void)
{
	gint i;

	for (i = 0; i < _imsettings_filename_list->len; i++)
		g_free(g_ptr_array_index(_imsettings_filename_list, i));

	g_ptr_array_free(_imsettings_filename_list, TRUE);
	_imsettings_filename_list = NULL;
}

GPtrArray *
_imsettings_info_get_filename_list(void)
{
	return _imsettings_filename_list;
}

IMSettingsInfo *
imsettings_info_new(const gchar *filename)
{
	g_return_val_if_fail (filename != NULL, NULL);
	g_return_val_if_fail (g_file_test(filename, G_FILE_TEST_EXISTS), NULL);

	return g_object_new(IMSETTINGS_TYPE_INFO,
			    "filename", filename,
			    NULL);
}

IMSettingsInfo *
imsettings_info_new_with_lang(const gchar *filename,
			      const gchar *language)
{
	g_return_val_if_fail (filename != NULL, NULL);
	g_return_val_if_fail (language != NULL, NULL);
	g_return_val_if_fail (g_file_test(filename, G_FILE_TEST_EXISTS), NULL);

	return g_object_new(IMSETTINGS_TYPE_INFO,
			    "language", language,
			    "filename", filename,
			    NULL);
}

#define _IMSETTINGS_DEFUNC_PROPERTY(_t_,_n_,_m_,_v_)			\
	_t_								\
	imsettings_info_get_ ## _n_(IMSettingsInfo *info)		\
	{								\
		IMSettingsInfoPrivate *priv;				\
									\
		g_return_val_if_fail (IMSETTINGS_IS_INFO (info), (_v_)); \
									\
		priv = IMSETTINGS_INFO_GET_PRIVATE (info);		\
									\
		return priv->_m_;					\
	}

_IMSETTINGS_DEFUNC_PROPERTY (const gchar *, language, language, NULL)
_IMSETTINGS_DEFUNC_PROPERTY (const gchar *, filename, filename, NULL)
_IMSETTINGS_DEFUNC_PROPERTY (const gchar *, gtkimm, gtkimm, NULL)
_IMSETTINGS_DEFUNC_PROPERTY (const gchar *, qtimm, qtimm, NULL)
_IMSETTINGS_DEFUNC_PROPERTY (const gchar *, xim, xim, NULL)
_IMSETTINGS_DEFUNC_PROPERTY (const gchar *, xim_program, xim_prog, NULL)
_IMSETTINGS_DEFUNC_PROPERTY (const gchar *, xim_args, xim_args, NULL)
_IMSETTINGS_DEFUNC_PROPERTY (const gchar *, prefs_program, prefs_prog, NULL)
_IMSETTINGS_DEFUNC_PROPERTY (const gchar *, prefs_args, prefs_args, NULL)
_IMSETTINGS_DEFUNC_PROPERTY (const gchar *, aux_program, aux_prog, NULL)
_IMSETTINGS_DEFUNC_PROPERTY (const gchar *, aux_args, aux_args, NULL)

const gchar *
imsettings_info_get_short_desc(IMSettingsInfo *info)
{
	IMSettingsInfoPrivate *priv;

	g_return_val_if_fail (IMSETTINGS_IS_INFO (info), NULL);

	priv = IMSETTINGS_INFO_GET_PRIVATE (info);

	if (priv->short_desc == NULL || priv->short_desc[0] == 0)
		return priv->xim;

	return priv->short_desc;
}

_IMSETTINGS_DEFUNC_PROPERTY (const gchar *,long_desc, long_desc, NULL)

const gchar *
imsettings_info_get_supported_language(IMSettingsInfo *info)
{
	g_return_val_if_fail (IMSETTINGS_IS_INFO (info), NULL);

	return NULL;
}

gboolean
imsettings_info_is_visible(IMSettingsInfo *info)
{
	IMSettingsInfoPrivate *priv;

	g_return_val_if_fail (IMSETTINGS_IS_INFO (info), FALSE);

	priv = IMSETTINGS_INFO_GET_PRIVATE (info);

	return !priv->ignore;
}

gboolean
imsettings_info_is_system_default(IMSettingsInfo *info)
{
	IMSettingsInfoPrivate *priv;

	g_return_val_if_fail (IMSETTINGS_IS_INFO (info), FALSE);

	priv = IMSETTINGS_INFO_GET_PRIVATE (info);

	return priv->is_system_default;
}

gboolean
imsettings_info_is_user_default(IMSettingsInfo *info)
{
	IMSettingsInfoPrivate *priv;

	g_return_val_if_fail (IMSETTINGS_IS_INFO (info), FALSE);

	priv = IMSETTINGS_INFO_GET_PRIVATE (info);

	return priv->is_user_default;
}

gboolean
imsettings_info_is_xim(IMSettingsInfo *info)
{
	IMSettingsInfoPrivate *priv;

	g_return_val_if_fail (IMSETTINGS_IS_INFO (info), FALSE);

	priv = IMSETTINGS_INFO_GET_PRIVATE (info);

	return priv->is_xim;
}

gboolean
imsettings_info_compare(const IMSettingsInfo *info1,
			const IMSettingsInfo *info2)
{
	IMSettingsInfoPrivate *priv1, *priv2;

	g_return_val_if_fail (IMSETTINGS_IS_INFO (info1), FALSE);
	g_return_val_if_fail (IMSETTINGS_IS_INFO (info2), FALSE);

	if (info1 == info2)
		return TRUE;

	priv1 = IMSETTINGS_INFO_GET_PRIVATE (info1);
	priv2 = IMSETTINGS_INFO_GET_PRIVATE (info2);

#define _cmp(_o_)							\
	((!priv1->_o_ && !priv2->_o_) ||				\
	 (priv1->_o_ && priv2->_o_ && strcmp(priv1->_o_, priv2->_o_) == 0))

#if GNOME_ENABLE_DEBUG
	g_return_val_if_fail (_cmp (gtkimm), FALSE);
	g_return_val_if_fail (_cmp (qtimm), FALSE);
	g_return_val_if_fail (_cmp (xim), FALSE);
	g_return_val_if_fail (_cmp (xim_prog), FALSE);
	g_return_val_if_fail (_cmp (xim_args), FALSE);
	g_return_val_if_fail (_cmp (prefs_prog), FALSE);
	g_return_val_if_fail (_cmp (prefs_args), FALSE);
	g_return_val_if_fail (_cmp (aux_prog), FALSE);
	g_return_val_if_fail (_cmp (aux_args), FALSE);
	g_return_val_if_fail (_cmp (short_desc), FALSE);
	g_return_val_if_fail (_cmp (long_desc), FALSE);
	g_return_val_if_fail (priv1->ignore == priv2->ignore, FALSE);

	return TRUE;
#else
	return (_cmp (gtkimm) &&
		_cmp (qtimm) &&
		_cmp (xim) &&
		_cmp (xim_prog) &&
		_cmp (xim_args) &&
		_cmp (prefs_prog) &&
		_cmp (prefs_args) &&
		_cmp (aux_prog) &&
		_cmp (aux_args) &&
		_cmp (short_desc) &&
		_cmp (long_desc) &&
		(priv1->ignore == priv2->ignore));
#endif

#undef _cmp
}
