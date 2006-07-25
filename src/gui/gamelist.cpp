/***************************************************************************
                          gamelist.cpp  -  Game List window
                             -------------------
    begin                : Sun 23 Jul 2006
    copyright            : (C) 2006 Michal Rudolf <mrudolf@kdewebdev.org>
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <qapplication.h>
#include <qhbox.h>
#include <qlayout.h>
#include <qheader.h>
#include <qlistview.h>
#include <qscrollbar.h>

#include "database.h"
#include "gamelist.h"

GameList::GameList(QWidget* parent, const char* name) : QWidget(parent, name), m_count(0)
{
  setCaption(tr("Game list"));
  setMinimumSize(700, 400);

  QHBoxLayout* hbox = new QHBoxLayout(this);

  m_list = new QListView(this);
  m_list->addColumn(tr("Index"));
  m_list->addColumn(tr("White"));
  m_list->addColumn(tr("Black"));
  m_list->addColumn(tr("Event"));
  m_list->addColumn(tr("Site"));
  m_list->addColumn(tr("Round"));
  m_list->addColumn(tr("Date"));
  m_list->addColumn(tr("Result"));
  m_list->addColumn(tr("ECO"));
  m_list->addColumn(tr("Length"));
  m_list->setColumnAlignment(Index, Qt::AlignRight);
  m_list->setColumnAlignment(Round, Qt::AlignRight);
  m_list->setColumnAlignment(Length, Qt::AlignRight);
  m_list->setAllColumnsShowFocus(true);
  m_list->setHScrollBarMode(QScrollView::AlwaysOff);
  m_list->setVScrollBarMode(QScrollView::AlwaysOff);
  hbox->addWidget(m_list);

  new QListViewItem(m_list, "");
  m_itemHeight = m_list->firstChild()->height();
  m_list->clear();
  m_list->setSorting(-1);

  m_scroll = new QScrollBar(Qt::Vertical, this);
  hbox->addWidget(m_scroll);

  /* Keyboard filter */
  m_list->installEventFilter(this);

  connect(m_scroll, SIGNAL(sliderMoved(int)), SLOT(scrollList(int)));
  connect(m_scroll, SIGNAL(valueChanged(int)), SLOT(scrollList(int)));

  connect(m_list, SIGNAL(doubleClicked(QListViewItem*, const QPoint&, int)), SLOT(itemSelected(QListViewItem*)));
  connect(m_list, SIGNAL(returnPressed(QListViewItem*)), SLOT(itemSelected(QListViewItem*)));

  resize(sizeHint());
}

void GameList::addItem(int index)
{
  Game g;
  if (!m_database || !m_database->load(index, g))
    return;
  QListViewItem* item = new QListViewItem(m_list, m_list->lastItem());
  item->setText(Index, QString::number(index+1));
  item->setText(White, g.tag("White"));
  item->setText(Black, g.tag("Black"));
  item->setText(Event, g.tag("Event"));
  item->setText(Site, g.tag("Site"));
  item->setText(Round, g.tag("Round"));
  item->setText(Date, g.tag("Date"));
  item->setText(Result, g.tag("Result").left(3));
  item->setText(Length, QString::number((g.ply() + 1) / 2));
  item->setText(ECO, g.tag("ECO"));
}

void GameList::resizeEvent(QResizeEvent* event)
{
  setItemCount(m_count);
  QWidget::resizeEvent(event);
}

void GameList::setItemCount(int count)
{
  m_count = count;
  if (!count)
    return;
  m_pageSize = (m_list->size().height() - m_list->header()->height() - 4) / m_itemHeight;
  m_scroll->setMaxValue(m_count - 1);
  m_scroll->setPageStep(m_pageSize - 1);
  scrollList(m_scroll->value());
}

void GameList::scrollList(int page)
{
  int start = page;
  if (start > m_count - m_pageSize)
    start = m_count - m_pageSize;
  m_list->clear();
  for (int i = 0; i < m_pageSize; i++)
    addItem(start + i);
  QListViewItem* current = m_list->firstChild();
  if (page > start)
    for (int i = 0; i < page - start; i++)
      current = current->itemBelow();
  m_list->setSelected(current, true);
}

bool GameList::eventFilter(QObject* o, QEvent* e)
{
  if (o == m_list && (e->type() == QEvent::KeyPress || e->type() == QEvent::Wheel))
  {
    qApp->sendEvent(m_scroll, e);
    return true;
  }
  return false;
}

void GameList::itemSelected(QListViewItem* item)
{
  emit selected(item->text(Index).toInt() - 1);
}

void GameList::setDatabase(Database* database)
{
  m_database = database;
  setItemCount(m_database->count());
}

