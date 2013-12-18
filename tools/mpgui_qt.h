/********************************************************************** 
 Freeciv - Copyright (C) 1996 - A Kjeldberg, L Gregersen, P Unold
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
***********************************************************************/

#ifndef FC__MPGUI_QT_H
#define FC__MPGUI_QT_H

#ifdef HAVE_CONFIG_H
#include <fc_config.h>
#endif

// Qt
#include <QObject>
#include <QLabel>
#include <QLineEdit>

// tools
#include "modinst.h"

struct QTableWidget;

class mpgui : public QObject
{
  Q_OBJECT

  public:
    void setup(QWidget *central);
    void display_msg(const char *msg);
    void setup_list(const char *name, const char *URL,
                    const char *version, const char *license,
                    enum modpack_type type, const char *notes);
    void refresh_list_versions();

  private slots:
    void URL_given();
    void row_selected(int, int);
    void row_download(int, int);

  private:

    QLineEdit *URLedit;
    QLabel *msg_dspl;
    QTableWidget *mplist_table;
};

#endif // FC__MPGUI_QT_H
