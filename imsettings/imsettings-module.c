/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * imsettings-module.c
 * Copyright (C) 2010 Red Hat, Inc. All rights reserved.
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
#include <gmodule.h>
#include "imsettings-module.h"

#define IMSETTINGS_MODULE_GET_PRIVATE(_o_)	(G_TYPE_INSTANCE_GET_PRIVATE ((_o_), IMSETTINGS_TYPE_MODULE, IMSettingsModulePrivate))

enum {
	PROP_0,
	PROP_NAME,
	LAST_PROP
};
struct _IMSettingsModulePrivate {
	GModule  *module;
	gchar    *name;
	gpointer  callback;
};

G_DEFINE_TYPE (IMSettingsModule, imsettings_module, G_TYPE_OBJECT);

/*< private >*/
static void
imsettings_module_set_property(GObject      *object,
			       guint         prop_id,
			       const GValue *value,
			       GParamSpec   *pspec)
{
	IMSettingsModule *module = IMSETTINGS_MODULE (object);
	IMSettingsModulePrivate *priv = module->priv;

	switch (prop_id) {
	    case PROP_NAME:
		    g_free(priv->name);
		    priv->name = g_strdup(g_value_get_string(value));
		    break;
	    default:
		    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		    break;
	}
}

static void
imsettings_module_get_property(GObject    *object,
			       guint       prop_id,
			       GValue     *value,
			       GParamSpec *pspec)
{
	IMSettingsModule *module = IMSETTINGS_MODULE (object);
	IMSettingsModulePrivate *priv = module->priv;

	switch (prop_id) {
	    case PROP_NAME:
		    g_value_set_string(value,
				       priv->name);
		    break;
	    default:
		    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		    break;
	}
}

static void
imsettings_module_finalize(GObject *object)
{
	IMSettingsModule *module = IMSETTINGS_MODULE (object);
	IMSettingsModulePrivate *priv = module->priv;

	if (priv->module)
		g_log(G_LOG_DOMAIN, G_LOG_LEVEL_INFO,
		      "Unloading imesttings module: %s",
		      priv->name);
	g_free(priv->name);
	if (priv->module) {
		g_module_close(priv->module);
	}

	if (G_OBJECT_CLASS (imsettings_module_parent_class)->finalize)
		G_OBJECT_CLASS (imsettings_module_parent_class)->finalize(object);
}

static void
imsettings_module_class_init(IMSettingsModuleClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private(klass, sizeof (IMSettingsModulePrivate));

	object_class->set_property = imsettings_module_set_property;
	object_class->get_property = imsettings_module_get_property;
	object_class->finalize     = imsettings_module_finalize;

	/* properties */
	g_object_class_install_property(object_class, PROP_NAME,
					g_param_spec_string("name",
							    _("Name"),
							    _("A module name for imsettings backend"),
							    NULL,
							    G_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY));
}

static void
imsettings_module_init(IMSettingsModule *module)
{
	module->priv = IMSETTINGS_MODULE_GET_PRIVATE (module);
}

/*< public >*/

/**
 * imsettings_module_new:
 * @name:
 *
 * FIXME
 *
 * Returns:
 */
IMSettingsModule *
imsettings_module_new(const gchar *name)
{
	gchar *n, *module;
	IMSettingsModule *retval;

	g_return_val_if_fail (name != NULL && name[0] != 0, NULL);

	n = g_path_get_basename(name);
	if (strncmp(n, "libimsettings-", 14) == 0) {
		gsize len = strlen(&n[14]);
		gsize mlen = strlen(G_MODULE_SUFFIX) + 1;

		if (len > mlen &&
		    strcmp(&n[14 + len - mlen], "." G_MODULE_SUFFIX) == 0) {
			module = g_strndup(&n[14], len - mlen);
			module[len - mlen] = 0;
		} else {
			goto copy_it;
		}
	} else {
	  copy_it:
		module = g_strdup(n);
	}

	retval = IMSETTINGS_MODULE (g_object_new(IMSETTINGS_TYPE_MODULE,
						 "name", module,
						 NULL));

	g_free(module);
	g_free(n);

	return retval;
}

/**
 * imsettings_module_get_name:
 * @module:
 *
 * FIXME
 *
 * Returns:
 */
const gchar *
imsettings_module_get_name(IMSettingsModule *module)
{
	IMSettingsModulePrivate *priv;

	g_return_val_if_fail (IMSETTINGS_IS_MODULE (module), NULL);

	priv = module->priv;

	return priv->name;
}

/**
 * imsettings_module_load:
 * @module:
 *
 * FIXME
 *
 * Returns:
 */
gboolean
imsettings_module_load(IMSettingsModule *module)
{
	IMSettingsModulePrivate *priv;
	gchar **path_list, *modulename, *s, *path, *fullpath;
	const gchar *p;
	gint i;
	gsize len;
	gboolean retval = FALSE;

	g_return_val_if_fail (IMSETTINGS_IS_MODULE (module), FALSE);

	priv = module->priv;
	modulename = g_strdup_printf("libimsettings-%s.so", priv->name);
	p = g_getenv("IMSETTINGS_MODULE_PATH");
	if (!p) {
		path_list = g_strsplit(IMSETTINGS_MODULE_PATH,
				       G_SEARCHPATH_SEPARATOR_S,
				       -1);
	} else {
		path_list = g_strsplit(p, G_SEARCHPATH_SEPARATOR_S, -1);
	}

	for (i = 0; path_list[i] != NULL && !retval; i++) {
		s = path_list[i];

		while (*s && g_ascii_isspace(*s))
			s++;
		len = strlen(s);
		while (len > 0 && g_ascii_isspace(s[len - 1]))
			len--;
		path = g_strndup(s, len);
		if (path[0] != 0) {
			fullpath = g_build_filename(path, modulename, NULL);

			priv->module = g_module_open(fullpath,
						     G_MODULE_BIND_LAZY|G_MODULE_BIND_LOCAL);
			if (priv->module) {
				gpointer mod_cb;

				g_module_symbol(priv->module,
						"module_switch_im",
						&mod_cb);

				if (!mod_cb) {
					g_warning(g_module_error());
					goto next;
				}
				priv->callback = mod_cb;
				g_log(G_LOG_DOMAIN, G_LOG_LEVEL_INFO,
				      "Loading imsettings module: %s",
				      fullpath);
				retval = TRUE;
			}

		  next:
			g_free(fullpath);
		}
		g_free(path);
	}
	g_free(modulename);
	g_strfreev(path_list);

	return retval;
}

/**
 * imsettings_module_switch_im:
 * @module:
 *
 * FIXME
 */
void
imsettings_module_switch_im(IMSettingsModule *module,
			    IMSettingsInfo   *info)
{
	IMSettingsModulePrivate *priv;

	g_return_if_fail (IMSETTINGS_IS_MODULE (module));

	priv = module->priv;

	((IMSettingsModuleCallback)priv->callback) (info);
}
