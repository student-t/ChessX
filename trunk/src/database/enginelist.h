/****************************************************************************
*   Copyright (C) 2010 by Michal Rudolf <mrudolf@kdewebdev.org>           *
****************************************************************************/

#ifndef ENGINELIST_H
#define ENGINELIST_H

#include <QtCore>
#include "enginedata.h"

class EngineList : public QList<EngineData>
{
public:
    EngineList();
};

#endif // ENGINELIST_H
