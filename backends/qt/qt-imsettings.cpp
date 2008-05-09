/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * qt-imsettings.cpp
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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <QtGui/QInputContextFactory>
#include "imsettings/imsettings.h"
#include "qt-imsettings.h"

IMSettingsQt::IMSettingsQt()
	: m_settings(new QSettings("Trolltech"))
{
}

IMSettingsQt::~IMSettingsQt()
{
	delete m_settings;
}

bool
IMSettingsQt::ChangeTo(const QString &module)
{
	m_settings->setValue(QLatin1String("/Qt/DefaultInputMethod"), module);
	qDebug("ChangeTo: %s", module.toLatin1().data());

	return true;
}

void
IMSettingsQt::NameOwnerChanged(const QString &name,
			       const QString &old_owner,
			       const QString &new_owner)
{
	if (QString::compare(name, IMSETTINGS_QT_INTERFACE_DBUS) == 0) {
		qDebug("OwnerChanged: `%s'->`%s' for %s",
		       old_owner.toLatin1().data(),
		       new_owner.toLatin1().data(),
		       name.toLatin1().data());

		if (m_owner.isEmpty())
			m_owner = new_owner;
		if (!old_owner.isEmpty() && QString::compare(old_owner, m_owner) == 0) {
			qDebug("disconnected");
			emit disconnected();
		}
	}
}
