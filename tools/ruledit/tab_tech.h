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

#ifndef FC__TAB_TECH_H
#define FC__TAB_TECH_H

#ifdef HAVE_CONFIG_H
#include <fc_config.h>
#endif

// Qt
#include <QWidget>

// common
#include "tech.h"

class QGridLayout;
class QLabel;
class QLineEdit;
class QListWidget;
class QMenu;
class QToolButton;

class ruledit_gui;

class tab_tech : public QWidget
{
  Q_OBJECT

  public:
    explicit tab_tech(QWidget *parent, ruledit_gui *ui_in);

  private:
    ruledit_gui *ui;
    void update_tech_info(struct advance *adv);
    QString tech_name(struct advance *padv);
    void techs_to_menu(QMenu *fill_menu);
    QMenu *prepare_req_button(QToolButton *button, enum tech_req rn);

    QLabel *name;
    QLabel *rname;
    QToolButton *req1_button;
    QToolButton *req2_button;
    QToolButton *root_req_button;
    QMenu *req1;
    QMenu *req2;
    QMenu *root_req;
    QListWidget *tech_list;

    struct advance *selected;

  private slots:
    void select_tech();
    void req1_jump();
    void req2_jump();
    void root_req_jump();
    void req1_menu(QAction *action);
    void req2_menu(QAction *action);
    void root_req_menu(QAction *action);
};


#endif // FC__TAB_TECH_H
