/***********************************************************************
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

#ifndef FC__CHATLINE_H
#define FC__CHATLINE_H

#ifdef HAVE_CONFIG_H
#include <fc_config.h>
#endif

extern "C" {
#include "chatline_g.h"
}

// gui-qt
#include "fonts.h"
#include "listener.h"

//Qt
#include <QEvent>
#include <QStringList>
#include <QTextBrowser>
#include <QLineEdit>
#include <QCheckBox>

class QPushButton;
class chat_listener;
QString apply_tags(QString str, const struct text_tag_list *tags,
                   QColor bg_color);
template<> std::set<chat_listener *> listener<chat_listener>::instances;
/***************************************************************************
  Listener for chat. See listener<> for information about how to use it
***************************************************************************/
class chat_listener : public listener<chat_listener>
{
  // History is shared among all instances...
  static QStringList history;
  // ...but each has its own position.
  int position;

  // Chat completion word list.
  static QStringList word_list;

public:
  // Special value meaning "end of history".
  static const int HISTORY_END = -1;

  static void update_word_list();

  explicit chat_listener();

  virtual void chat_message_received(const QString &,
                                     const struct text_tag_list *);
  virtual void chat_word_list_changed(const QStringList &);

  void send_chat_message(const QString &message);

  int position_in_history() { return position; }
  QString back_in_history();
  QString forward_in_history();
  void reset_history_position();

  QStringList current_word_list() { return word_list; }
};

/***************************************************************************
  Chat input widget
***************************************************************************/
class chat_input : public QLineEdit, private chat_listener
{
  Q_OBJECT

private slots:
  void send();

public:
  explicit chat_input(QWidget *parent = nullptr);

  virtual void chat_word_list_changed(const QStringList &);

  bool event(QEvent *event);
};

/***************************************************************************
  Class for chat widget
***************************************************************************/
class chatwdg : public QWidget, private chat_listener
{
  Q_OBJECT
public:
  chatwdg(QWidget *parent);
  void append(const QString &str);
  chat_input *chat_line;
  void make_link(struct tile *ptile);
  void update_widgets();
  int default_size(int lines);
  void scroll_to_bottom();
  void update_font();
private slots:
  void state_changed(int state);
  void rm_links();
  void anchor_clicked(const QUrl &link);
protected:
  void paint(QPainter *painter, QPaintEvent *event);
  void paintEvent(QPaintEvent *event);
  bool eventFilter(QObject *obj, QEvent *event);
private:
  void chat_message_received(const QString &message,
                             const struct text_tag_list *tags);

  QTextBrowser *chat_output;
  QPushButton *remove_links;
  QCheckBox *cb;

};

class version_message_event : public QEvent
{
  QString message;
public:
  explicit version_message_event(const QString &msg);
  QString get_message() const { return message; }
};

#endif                        /* FC__CHATLINE_H */
