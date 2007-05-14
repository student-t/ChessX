/***************************************************************************
                          common  -  description
                             -------------------
    begin                : 26/02/2007
    copyright            : (C) 2006 Marius Roets
                           <saidinwielder@sourceforge.net>
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
/**
Unit tests for the IndexItem class
*/

#ifndef INDEXTEST_H
#define INDEXTEST_H

#include <QtTest/QtTest>
#include "pgndatabase.h"


class IndexTest : public QObject
{

 Q_OBJECT

private slots:
  void initTestCase();
  void init();
  void cleanup();
  void cleanupTestCase();

  void testIndex();
  void testIndexFromDatabase();

};

#endif 

