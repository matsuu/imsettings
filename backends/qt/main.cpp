/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
// main.cpp
// Copyright (C) 2008 Red Hat, Inc. All rights reserved.
//
// Authors:
//   Akira TAGOH  <tagoh@redhat.com>
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330,
// Boston, MA 02111-1307, USA.
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cstdlib>
#include <QtCore/QCoreApplication>
#include <QtCore/QFileInfo>
#include "imsettings/imsettings.h"
#include "qt-imsettings.h"
#include "qt-imsettings-glue.h"


int
main(int    argc,
     char **argv)
{
	QCoreApplication app(argc, argv);
	IMSettingsQt imsettings;
	const QDBusConnectionInterface *iface;
	bool arg_replace = FALSE;

	if (argc > 1) {
		for (int i = 1; i < argc; i++) {
			if (strcmp(argv[i], "--replace") == 0) {
				arg_replace = TRUE;
			} else if (strcmp(argv[i], "--help") == 0) {
				QFileInfo fi(argv[0]);

				fprintf(stderr, "Usage: %s [--replace]\n", fi.baseName().toLatin1().data());
				exit(1);
			} else {
				fprintf(stderr, "Unknown option %s\n", argv[i]);
				exit(1);
			}
		}
	}

	new IMSettingsQtDBusAdaptor(&imsettings);

	iface = QDBusConnection::sessionBus().interface();
	QObject::connect(iface, SIGNAL (NameOwnerChanged(QString, QString, QString)),
			 &imsettings, SLOT (NameOwnerChanged(QString, QString, QString)));
	QObject::connect(&imsettings, SIGNAL (disconnected()),
			 &app, SLOT (quit()));
	if (((QDBusConnectionInterface *)iface)->registerService(IMSETTINGS_QT_SERVICE_DBUS,
								 (arg_replace ?
								  QDBusConnectionInterface::ReplaceExistingService :
								  QDBusConnectionInterface::DontQueueService),
								 QDBusConnectionInterface::AllowReplacement)) {
	} else {
		printf("Failed to register the service for %s\n", IMSETTINGS_QT_SERVICE_DBUS);
		exit(1);
	}
	if (QDBusConnection::sessionBus().registerObject(IMSETTINGS_QT_PATH_DBUS, &imsettings) == false) {
		printf("Failed to register the object to %s\n", IMSETTINGS_QT_PATH_DBUS);
		exit(1);
	}

	return app.exec();
}
