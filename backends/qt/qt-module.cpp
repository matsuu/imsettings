/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * qt-module.c
 * Copyright (C) 2008-2010 Red Hat, Inc. All rights reserved.
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

#include <QtCore/QSettings>
#include "imsettings-info.h"

G_BEGIN_DECLS

void module_switch_im(IMSettingsInfo *info);

/*< private >*/

/*< public >*/
void
module_switch_im(IMSettingsInfo *info)
{
	QSettings s("Trolltech");
	const gchar *qtimm = imsettings_info_get_qtimm(info);

	if (!qtimm || qtimm[0] == 0) {
		g_warning("Invalid Qt immodule in: %s",
			  imsettings_info_get_filename(info));
		return;
	}

	s.setValue(QLatin1String("/Qt/DefaultInputMethod"), qtimm);

	g_log(G_LOG_DOMAIN, G_LOG_LEVEL_INFO,
	      "Setting up %s as Qt immodule", qtimm);
}

G_END_DECLS
