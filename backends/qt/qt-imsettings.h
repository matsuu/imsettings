/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * qt-imsettings.h
 * Copyright (C) 2008-2009 Red Hat, Inc. All rights reserved.
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
#ifndef __IMSETTINGS_QT_IMSETTINGS_H__
#define __IMSETTINGS_QT_IMSETTINGS_H__

#include <QtCore/QObject>
#include <QtCore/QSettings>
#include <QtCore/QString>


class IMSettingsQt : public QObject {
	Q_OBJECT;

public:
	IMSettingsQt();
	virtual ~IMSettingsQt();

private:
	QSettings *m_settings;
	QString    m_owner;

private Q_SLOTS:
	bool ChangeTo(const QString &module);
	void NameOwnerChanged(const QString &name, const QString &old_owner, const QString &new_owner);

Q_SIGNALS:
	void disconnected(void);
};

#endif /* __IMSETTINGS_QT_IMSETTINGS_H__ */
