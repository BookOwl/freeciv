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

#ifndef FC__TAB_MISC_H
#define FC__TAB_MISC_H

// Qt
#include <QWidget>

class QLineEdit;

class ruledit_gui;

class tab_misc : public QWidget
{
  Q_OBJECT

  public:
    explicit tab_misc(QWidget *parent, ruledit_gui *ui_in);

  private slots:
    void savedir_given();

  private:
    ruledit_gui *ui;
    QLineEdit *name;
    QLineEdit *savedir;
};


#endif // FC__TAB_MISC_H
