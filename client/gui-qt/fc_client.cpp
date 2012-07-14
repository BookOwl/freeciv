/**********************************************************************
 Freeciv - Copyright (C) 1996-2005 - Freeciv Development Team
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
***********************************************************************/

#ifdef HAVE_CONFIG_H
#include <fc_config.h>
#endif

#include "fc_client.h"

extern QApplication *qapp;

/****************************************************************************
  Constructor
****************************************************************************/
fc_client::fc_client() : QObject()
{
  main_window = new QMainWindow;
  central_wdg = new QWidget;
  central_layout = new QGridLayout;

  // General part not related to any single page
  observer_mode =  false;
  create_dock_widgets();
  menu_bar = new mr_menu();
  menu_bar->setup_menus();
  main_window->setMenuBar(menu_bar);
  status_bar = main_window->statusBar();
  status_bar_label = new QLabel;
  status_bar_label->setAlignment(Qt::AlignCenter);
  status_bar->addWidget(status_bar_label, 1);
  set_status_bar(tr(_("Welcome to Freeciv")));

  // PAGE_MAIN
  pages[PAGE_MAIN] = new QWidget(central_wdg);
  page = PAGE_MAIN;
  create_main_page();

  // PAGE_START
  pages[PAGE_START] = new QWidget(central_wdg);
  create_start_page();

  // PAGE_SCENARIO
  pages[PAGE_SCENARIO] = new QWidget(central_wdg);
  create_scenario_page();

  // PAGE_LOAD
  pages[PAGE_LOAD] = new QWidget(central_wdg);
  create_load_page();

  // PAGE_NETWORK
  pages[PAGE_NETWORK] = new QWidget(central_wdg);
  create_network_page();
  meta_scan_timer = new QTimer;
  lan_scan_timer = new QTimer;
  pages[PAGE_NETWORK]->setVisible(false);

  // PAGE_GAME
  pages[PAGE_GAME] = new QWidget(central_wdg);
  init_mapcanvas_and_overview();
  map_canvas_resized (MAPVIEW_WIDTH, MAPVIEW_HEIGHT);
  create_game_page();
  pages[PAGE_GAME]->setVisible(false);

  // PAGE_GGZ
  pages[PAGE_GGZ] = NULL;
  central_layout->addLayout(pages_layout[PAGE_MAIN], 1, 1);
  central_layout->addLayout(pages_layout[PAGE_NETWORK], 1, 1);
  central_layout->addLayout(pages_layout[PAGE_LOAD], 1, 1);
  central_layout->addLayout(pages_layout[PAGE_SCENARIO], 1, 1);
  central_layout->addLayout(pages_layout[PAGE_START], 1, 1);
  central_layout->addLayout(pages_layout[PAGE_GAME], 1, 1);
  central_wdg->setLayout(central_layout);
  main_window->setCentralWidget(central_wdg);
  main_window->addDockWidget(Qt::RightDockWidgetArea,
                             dock_widget[ (int) OUTPUT_DOCK_WIDGET]);
  main_window->addDockWidget(Qt::BottomDockWidgetArea,
                             dock_widget[ (int) MESSAGE_DOCK_WIDGET]);
  dock_widget[ (int) MESSAGE_DOCK_WIDGET]->hide();
  main_window->setVisible(true);
}

/****************************************************************************
  Destructor
****************************************************************************/
fc_client::~fc_client()
{
  delete main_window;
}

/****************************************************************************
  Main part of gui-qt
****************************************************************************/
void fc_client::main(QApplication *qapp)
{

  qRegisterMetaType<QTextCursor> ("QTextCursor");

  real_output_window_append(_("This is Qt-client for Freeciv."), NULL, -1);
  chat_welcome_message();

  real_output_window_append(_("In this early Qt-client development phase "
                              "the only way to connect server is via"
                              "commandline autoconnect parameter \"-a\""),
                            NULL, -1);

  set_client_state(C_S_DISCONNECTED);

  startTimer(TIMER_INTERVAL);
  qapp->exec();

  free_mapcanvas_and_overview();
  tileset_free_tiles(tileset);
}

/****************************************************************************
  Appends text to chat window
****************************************************************************/
void fc_client::append_output_window(const QString &str)
{
  output_window->append(str);
}

/****************************************************************************
  Return whether chatline should be active on page.
****************************************************************************/
bool fc_client::chat_active_on_page(enum client_pages check)
{
  if (check == PAGE_START || check == PAGE_GAME) {
    return true;
  }

  return false;
}

/****************************************************************************
  Switch from one client page to another.
****************************************************************************/
void fc_client::switch_page(enum client_pages new_page)
{
  bool chat_old_act;
  bool chat_new_act;

  main_window->menuBar()->setVisible(false);
  chat_old_act = chat_active_on_page(page);
  chat_new_act = chat_active_on_page(new_page);

  if (chat_new_act && !chat_old_act) {
    chat_line->setReadOnly(false);
  }

  if (!chat_new_act && chat_old_act) {
    chat_line->setReadOnly(true);
  }

  for (int i = 0; i < PAGE_GGZ + 1; i++) {
    if (i == new_page) {
      show_children(pages_layout[i], true);
    } else {
      show_children(pages_layout[i], false);
    }
  }

  page = new_page;

  if (page == PAGE_START) {
    show_dock_widget( (int) OUTPUT_DOCK_WIDGET);
  }

  if (page == PAGE_GAME) {
    main_window->menuBar()->setVisible(true);
    show_dock_widget( (int) MESSAGE_DOCK_WIDGET);
    show_dock_widget( (int) OUTPUT_DOCK_WIDGET);
    mapview_wdg->setFocus();
  }
}

/****************************************************************************
  Returns currently open page
****************************************************************************/
enum client_pages fc_client::current_page()
{
  return page;
}

/****************************************************************************
  Add notifier for server input
****************************************************************************/
void fc_client::add_server_source(int sock)
{
  server_notifier = new QSocketNotifier(sock, QSocketNotifier::Read);

  connect(server_notifier, SIGNAL(activated(int)), this, SLOT(server_input(int)));
}

/****************************************************************************
  There is input from server
****************************************************************************/
void fc_client::server_input(int sock)
{
  input_from_server(sock);
}

/****************************************************************************
  Enter pressed on chatline
****************************************************************************/
void fc_client::chat()
{
  send_chat(chat_line->text().toUtf8().data());
  real_output_window_append(chat_line->text().toUtf8().data(), NULL, -1);
  chat_line->clear();
}



/****************************************************************************
  Timer event handling
****************************************************************************/
void fc_client::timerEvent(QTimerEvent *event)
{
  // Prevent current timer from repeating with possibly wrong interval
  killTimer(event->timerId());

  // Call timer callback in client common code and
  // start new timer with correct interval
  startTimer(real_timer_callback() * 1000);
}

/****************************************************************************
  Quit App
****************************************************************************/
void fc_client::quit()
{
  qapp->quit();
}

/****************************************************************************
 * Switchs page to PAGE_MAIN
 ***************************************************************************/
void fc_client::slot_switch_to_main_page()
{
  switch_page(PAGE_MAIN);
  show_dock_widget( (int) OUTPUT_DOCK_WIDGET);
}

/****************************************************************************
  Switchs page to PAGE_LOAD
****************************************************************************/
void fc_client::slot_switch_to_load_page()
{
  update_load_page();
  switch_page(PAGE_LOAD);
  hide_dock_widgets();
}

/****************************************************************************
  Switchs page to PAGE_SCENARIO
****************************************************************************/
void fc_client::slot_switch_to_scenario_page()
{
  update_scenarios_page();
  switch_page(PAGE_SCENARIO);
  hide_dock_widgets();
}

/****************************************************************************
  Switchs page to PAGE_START
****************************************************************************/
void fc_client::slot_switch_to_start_page()
{
  switch_page(PAGE_START);
  show_dock_widget( (int) OUTPUT_DOCK_WIDGET);
}

/****************************************************************************
  Switchs page to PAGE_GAME
****************************************************************************/
void fc_client::slot_switch_to_game_page()
{
  switch_page(PAGE_GAME);
  show_dock_widget( (int) OUTPUT_DOCK_WIDGET);
  show_dock_widget( (int) MESSAGE_DOCK_WIDGET);
  main_window->menuBar()->setVisible(true);
  mapview_wdg->setFocus();
}

/****************************************************************************
  Switchs page to PAGE_NETWORK
****************************************************************************/
void fc_client::slot_switch_to_network_page()
{
  char buf[256];

  switch_page(PAGE_NETWORK);
  hide_dock_widgets();
  update_network_lists();
  connect_host_edit->setText(server_host);
  fc_snprintf(buf, sizeof (buf), "%d", server_port);
  connect_port_edit->setText(buf);
  connect_login_edit->setText(user_name);
}

/****************************************************************************
  Disconnect from server and return to MAIN PAGE
****************************************************************************/
void fc_client::slot_disconnect()
{
  if (client.conn.used) {
    disconnect_from_server();
  }

  switch_page(PAGE_MAIN);
  show_dock_widget( (int) OUTPUT_DOCK_WIDGET);
}

/****************************************************************************
  User clicked Observe button in START_PAGE
****************************************************************************/
void fc_client::slot_pregame_observe()
{
  if (observer_mode == false) {
    send_chat("/observe");
  } else {
    send_chat("/take -");
  }

  observer_mode = !observer_mode;
}

/****************************************************************************
  User clicked Start in START_PAGE
****************************************************************************/
void fc_client::slot_pregame_start()
{
  send_chat("/start");
}

/****************************************************************************
 * Shows all widgets for given layout
 ***************************************************************************/
void fc_client::show_children(const QLayout* layout, bool show)
{
  QLayoutItem *item = NULL;
  QWidget *widget = NULL;

  if (layout) {
    for (int i = 0; i < layout->count(); i++) {
      item = layout->itemAt(i);
      widget = item ? item->widget() : 0;

      if (widget) {
        widget->setVisible(show);
      }

      if (item->layout()) {
        show_children(item->layout(), show);
      }
    }
  }
}

/****************************************************************************
  Called when map view has been resized
****************************************************************************/
void map_view::resizeEvent(QResizeEvent* event)
{
  QSize size;

  size = event->size();

  if (C_S_RUNNING == client_state()) {
    map_canvas_resized(size.width(), size.height());
  }
}
