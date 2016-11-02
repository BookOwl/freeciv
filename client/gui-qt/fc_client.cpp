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

// Qt
#include <QApplication>
#include <QFormLayout>
#include <QMainWindow>
#include <QLineEdit>
#include <QScrollBar>
#include <QStackedLayout>
#include <QStatusBar>
#include <QStyleFactory>
#include <QTabBar>
#include <QTextEdit>

// client
#include "connectdlg_common.h"

// common
#include "game.h"

// gui-qt
#include "fonts.h"
#include "fc_client.h"
#include "gui_main.h"
#include "optiondlg.h"
#include "sidebar.h"
#include "sprite.h"

fc_icons* fc_icons::m_instance = 0;
fc_font* fc_font::m_instance = 0;
extern "C" {
  bool get_turn_done_button_state();
  void real_science_report_dialog_update(void);
}
extern void write_shortcuts();

QString current_theme;

/****************************************************************************
  Constructor
****************************************************************************/
fc_client::fc_client() : QMainWindow()
{
  QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));
  /**
   * Somehow freeciv-client-common asks to switch to page when all widgets
   * haven't been created yet by Qt, even constructor finished job,
   * so we null all Qt objects, so while switching we will know if they
   * were created.
   * After adding new QObjects null them here.
   */
  main_wdg = NULL;
  connect_lan = NULL;
  connect_metaserver = NULL;
  central_layout = NULL;
  output_window = NULL;
  scenarios_view = NULL;
  connect_host_edit = NULL;
  connect_port_edit = NULL;
  connect_login_edit = NULL;
  connect_password_edit = NULL;
  connect_confirm_password_edit = NULL;
  button = NULL;
  button_box = NULL;
  server_notifier = NULL;
  chat_line = NULL;
  lan_widget = NULL;
  wan_widget = NULL;
  info_widget = NULL;
  saves_load = NULL;
  scenarios_load = NULL;
  meta_scan_timer = NULL;
  lan_scan_timer = NULL;
  status_bar = NULL;
  status_bar_label = NULL;
  menu_bar = NULL;
  mapview_wdg = NULL;
  sidebar_wdg = nullptr;
  msgwdg = NULL;
  infotab = NULL;
  central_wdg = NULL;
  game_tab_widget = NULL;
  start_players_tree = NULL;
  unit_sel = NULL;
  info_tile_wdg = NULL;
  opened_dialog = NULL;
  current_file = "";
  status_bar_queue.clear();
  quitting = false;
  pre_vote = NULL;
  x_vote = NULL;
  gtd = NULL;
  update_info_timer = nullptr;
  game_layout = nullptr;
  unitinfo_wdg = nullptr;
  for (int i = 0; i <= PAGE_GAME; i++) {
    pages_layout[i] = NULL;
    pages[i] = NULL;
  }
  init();
}

/****************************************************************************
  Initializes layouts for all pages
****************************************************************************/
void fc_client::init()
{
  fc_font::instance()->init_fonts();
  read_settings();
  QApplication::setFont(*fc_font::instance()->get_font(fonts::default_font));
  QString path;
  central_wdg = new QWidget;
  central_layout = new QStackedLayout;

  // General part not related to any single page
  menu_bar = new mr_menu();
  corner_wid = new fc_corner(this);
  if (gui_options.gui_qt_show_titlebar == false) {
    menu_bar->setCornerWidget(corner_wid);
  }
  setMenuBar(menu_bar);
  status_bar = statusBar();
  status_bar_label = new QLabel;
  status_bar_label->setAlignment(Qt::AlignCenter);
  status_bar->addWidget(status_bar_label, 1);
  set_status_bar(_("Welcome to Freeciv"));
  create_cursors();
  switch_page_mapper = new QSignalMapper;
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
  pages[PAGE_NETWORK]->setVisible(false);

  // PAGE_GAME
  gui_options.gui_qt_allied_chat_only = true;
  path = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
  if (path.isEmpty() == false) {
    QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, path);
  }
  pages[PAGE_GAME] = new QWidget(central_wdg);
  init_mapcanvas_and_overview();
  create_game_page();
  
  pages[PAGE_GAME + 1] = new QWidget(central_wdg);
  create_loading_page();

  pages_layout[PAGE_GAME]->setContentsMargins(0, 0, 0, 0);

  pages[PAGE_MAIN]->setLayout(pages_layout[PAGE_MAIN]);
  pages[PAGE_NETWORK]->setLayout(pages_layout[PAGE_NETWORK]);
  pages[PAGE_LOAD]->setLayout(pages_layout[PAGE_LOAD]);
  pages[PAGE_SCENARIO]->setLayout(pages_layout[PAGE_SCENARIO]);
  pages[PAGE_START]->setLayout(pages_layout[PAGE_START]);
  pages[PAGE_GAME]->setLayout(pages_layout[PAGE_GAME]);
  pages[PAGE_GAME + 1]->setLayout(pages_layout[PAGE_GAME + 1]);

  central_layout->addWidget(pages[PAGE_MAIN]);
  central_layout->addWidget(pages[PAGE_NETWORK]);
  central_layout->addWidget(pages[PAGE_LOAD]);
  central_layout->addWidget(pages[PAGE_SCENARIO]);
  central_layout->addWidget(pages[PAGE_START]);
  central_layout->addWidget(pages[PAGE_GAME]);
  central_layout->addWidget(pages[PAGE_GAME + 1]);

  central_wdg->setLayout(central_layout);
  setCentralWidget(central_wdg);

  connect(switch_page_mapper, SIGNAL(mapped( int)),
                this, SLOT(switch_page(int)));
  resize(pages[PAGE_MAIN]->minimumSizeHint());
  setVisible(true);

  game_tab_widget->init();
  chat_listener::listen();

}

/****************************************************************************
  Destructor
****************************************************************************/
fc_client::~fc_client()
{
  status_bar_queue.clear();
}


/****************************************************************************
  Main part of gui-qt
****************************************************************************/
void fc_client::main(QApplication *qapp)
{
  qRegisterMetaType<QTextCursor>("QTextCursor");
  qRegisterMetaType<QTextBlock>("QTextBlock");
  fc_allocate_ow_mutex();
  real_output_window_append(_("This is Qt-client for Freeciv."), NULL, -1);
  fc_release_ow_mutex();
  chat_welcome_message(true);

  set_client_state(C_S_DISCONNECTED);

  startTimer(TIMER_INTERVAL);
  connect(qapp, SIGNAL(aboutToQuit()), this, SLOT(closing()));
  qapp->exec();

  free_mapcanvas_and_overview();
  tileset_free_tiles(tileset);
}

/****************************************************************************
  Returns status if fc_client is being closed
****************************************************************************/
bool fc_client::is_closing()
{
  return quitting;
}

/****************************************************************************
  Called when fc_client is going to quit
****************************************************************************/
void fc_client::closing()
{
  quitting = true;
}

/****************************************************************************
  Slot to send fake chat messages. Do not use in new code.
****************************************************************************/
void fc_client::send_fake_chat_message(const QString &message)
{
  send_chat_message(message);
}

/****************************************************************************
  Appends text to chat window
****************************************************************************/
void fc_client::chat_message_received(const QString &message,
                                      const struct text_tag_list *tags)
{
  QTextCursor cursor;
  QColor col = output_window->palette().color(QPalette::Text);
  QString str = apply_tags(message, tags, col);

  if (output_window != NULL) {
    output_window->append(str);
    output_window->verticalScrollBar()->setSliderPosition(
                              output_window->verticalScrollBar()->maximum());
  }
}

/****************************************************************************
  Updates the application's default font when it changes
****************************************************************************/
void fc_client::update_font(const QString &name, const QFont &font)
{
  if (name == fonts::default_font) {
    QApplication::setFont(font);
  }
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
  Argument is int cause QSignalMapper doesn't want to work with enum
  Because chat widget is in 2 layouts we need to switch between them here
  (addWidget removes it from prevoius layout automatically)
****************************************************************************/
void fc_client::switch_page(int new_pg)
{
  char buf[256];
  enum client_pages new_page;
  int i_page;

  new_page = static_cast<client_pages>(new_pg);

  if ((new_page == PAGE_SCENARIO || new_page == PAGE_LOAD)
       && !is_server_running()) {
    current_file = "";
    client_start_server_and_set_page(new_page);
    return;
  }

  if (page == PAGE_NETWORK){
    destroy_server_scans();
  }
  menuBar()->setVisible(false);
  if (status_bar != nullptr) {
    status_bar->setVisible(true);
  }

  central_layout->setCurrentWidget(pages[new_pg]);
  page = new_page;
  i_page = new_page;
  switch (i_page) {
  case PAGE_MAIN:
    break;
  case PAGE_START:
    pre_vote->hide();
    voteinfo_gui_update();
    break;
  case PAGE_LOAD:
    update_load_page();
    break;
  case PAGE_GAME:
    if (gui_options.gui_qt_show_titlebar == false) {
      setWindowFlags(Qt::Window | Qt::CustomizeWindowHint);
    }
    showMaximized();
    /* For MS Windows, it might ingore first */
    showMaximized();
    gui()->infotab->chtwdg->update_widgets();
    status_bar->setVisible(false);
    if (gui_options.gui_qt_fullscreen){
      gui()->showFullScreen();
      gui()->game_tab_widget->showFullScreen();
    }
    menuBar()->setVisible(true);
    mapview_wdg->setFocus();
    center_on_something();
    voteinfo_gui_update();
    update_info_label();
    minimapview_wdg->reset();
    overview_size_changed();
    update_sidebar_tooltips();
    real_science_report_dialog_update();
    break;
  case PAGE_SCENARIO:
    update_scenarios_page();
    break;
  case PAGE_NETWORK:
    update_network_lists();
    set_connection_state(LOGIN_TYPE);
    connect_host_edit->setText(server_host);
    fc_snprintf(buf, sizeof(buf), "%d", server_port);
    connect_port_edit->setText(buf);
    connect_login_edit->setText(user_name);
    connect_password_edit->setDisabled(true);
    connect_confirm_password_edit->setDisabled(true);
    break;
  case (PAGE_GAME + 1):
    break;
  default:
    if (client.conn.used) {
      disconnect_from_server();
    }
    set_client_page(PAGE_MAIN);
    break;
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

  connect(server_notifier, SIGNAL(activated(int)), this, 
          SLOT(server_input(int)));
}

/****************************************************************************
  Event handler
****************************************************************************/
bool fc_client::event(QEvent *event)
{
  if (event->type() == QEvent::User) {
    version_message_event vmevt =
        dynamic_cast<version_message_event&>(*event);
    set_status_bar(vmevt.get_message());
    return true;
  } else {
    return QMainWindow::event(event);
  }
}

/****************************************************************************
  Closes main window
****************************************************************************/
void fc_client::closeEvent(QCloseEvent *event) {
  popup_quit_dialog();
  event->ignore();
}

/****************************************************************************
  Removes notifier
****************************************************************************/
void fc_client::remove_server_source()
{
  delete server_notifier;
}

/****************************************************************************
  There is input from server
****************************************************************************/
void fc_client::server_input(int sock)
{
#if defined(Q_WS_WIN)
  server_notifier->setEnabled(false);
#endif
  input_from_server(sock);
#if defined(Q_WS_WIN)
  server_notifier->setEnabled(true);
#endif
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
  QApplication *qapp = current_app();

  if (qapp != nullptr) {
    qapp->quit();
  }
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
}

/****************************************************************************
  User clicked Observe button in START_PAGE
****************************************************************************/
void fc_client::slot_pregame_observe()
{
  if (client_is_observer() || client_is_global_observer()) {
    if (game.info.is_new_game) {
      send_chat("/take -");
    } else {
      send_chat("/detach");
    }
    obs_button->setText(_("Don't Observe"));
  } else {
    send_chat("/observe");
    obs_button->setText(_("Observe"));
  }
}

/****************************************************************************
  User clicked Start in START_PAGE
****************************************************************************/
void fc_client::slot_pregame_start()
{
  if (can_client_control()) {
    dsend_packet_player_ready(&client.conn,
                              player_number(client_player()),
                              !client_player()->is_ready);
  } else {
    dsend_packet_player_ready(&client.conn, 0, TRUE);
  }
}


/****************************************************************************
  Finds not used index on game_view_tab and returns it
****************************************************************************/
void fc_client::gimme_place(QWidget *widget, QString str)
{
  QString x;

  x = opened_repo_dlgs.key(widget);

  if (x.isEmpty()) {
    opened_repo_dlgs.insert(str, widget);
    return;
  }
  log_error("Failed to find place for new tab widget");
  return;
}

/****************************************************************************
  Checks if given report is opened, if you create new report as tab on game
  page, figure out some original string and put in in repodlg.h as comment to
  that QWidget class.
****************************************************************************/
bool fc_client::is_repo_dlg_open(QString str)
{
  QWidget *w;

  w = opened_repo_dlgs.value(str);

  if (w == NULL) {
    return false;
  }

  return true;
}

/****************************************************************************
  Returns index on game tab page of given report dialog 
****************************************************************************/
int fc_client::gimme_index_of(QString str)
{
  int i;
  QWidget *w;

  if (str == "MAP") {
    return 0;
  }

  w = opened_repo_dlgs.value(str);
  i = game_tab_widget->indexOf(w);
  return i;
}

/****************************************************************************
  Loads qt-specific options
****************************************************************************/
void fc_client::read_settings()
{
  QSettings s(QSettings::IniFormat, QSettings::UserScope,
              "freeciv-qt-client");
  if (s.contains("Fonts-set") == false) {
    configure_fonts();
  }
  if (s.contains("Chat-xsize")) {
    qt_settings.chat_width = s.value("Chat-xsize").toInt();
  } else {
    qt_settings.chat_width = 33;
  }
  if (s.contains("Chat-ysize")) {
    qt_settings.chat_height = s.value("Chat-ysize").toInt();
  } else {
    qt_settings.chat_height = 33;
  }
  if (s.contains("unit_x")) {
    qt_settings.unit_info_pos_x = s.value("unit_x").toInt();
  } else {
    qt_settings.unit_info_pos_x = 330;
  }
  if (s.contains("unit_y")) {
    qt_settings.unit_info_pos_y = s.value("unit_y").toInt();
  } else {
    qt_settings.unit_info_pos_y = 0;
  }
  if (s.contains("City-dialog")) {
    qt_settings.city_geometry = s.value("City-dialog").toByteArray();
  }
  if (s.contains("splitter1")) {
    qt_settings.city_splitter1 = s.value("splitter1").toByteArray();
  }
  if (s.contains("splitter2")) {
    qt_settings.city_splitter2 = s.value("splitter2").toByteArray();
  }
  if (s.contains("splitter3")) {
    qt_settings.city_splitter3 = s.value("splitter3").toByteArray();
  }
  qt_settings.player_repo_sort_col = -1;
  qt_settings.city_repo_sort_col = -1;

}

/****************************************************************************
  Save qt-specific options
****************************************************************************/
void fc_client::write_settings()
{
  QSettings s(QSettings::IniFormat, QSettings::UserScope,
              "freeciv-qt-client");
  s.setValue("Fonts-set", true);
  s.setValue("Chat-xsize", qt_settings.chat_width);
  s.setValue("Chat-ysize", qt_settings.chat_height);
  s.setValue("City-dialog", qt_settings.city_geometry);
  s.setValue("splitter1", qt_settings.city_splitter1);
  s.setValue("splitter2", qt_settings.city_splitter2);
  s.setValue("splitter3", qt_settings.city_splitter3);
  s.setValue("unit_x", qt_settings.unit_info_pos_x);
  s.setValue("unit_y", qt_settings.unit_info_pos_y);
  write_shortcuts();
}

/****************************************************************************
  Shows/closes unit selection widget
****************************************************************************/
void fc_client::toggle_unit_sel_widget(struct tile *ptile)
{
  if (unit_sel != NULL) {
    unit_sel->close();
    delete unit_sel;
    unit_sel = new units_select(ptile, gui()->mapview_wdg);
    unit_sel->show();
  } else {
    unit_sel = new units_select(ptile, gui()->mapview_wdg);
    unit_sel->show();
  }
}

/****************************************************************************
  Update unit selection widget if open
****************************************************************************/
void fc_client::update_unit_sel()
{
  if (unit_sel != NULL){
    unit_sel->update_units();
    unit_sel->create_pixmap();
    unit_sel->update();
  }
}

/****************************************************************************
  Closes unit selection widget.
****************************************************************************/
void fc_client::popdown_unit_sel()
{
  if (unit_sel != nullptr){
    unit_sel->close();
    delete unit_sel;
    unit_sel = nullptr;
  }
}

/****************************************************************************
  Removes report dialog string from the list marking it as closed
****************************************************************************/
void fc_client::remove_repo_dlg(QString str)
{
  /* if app is closing opened_repo_dlg is already deleted */
  if (is_closing() == false) {
    opened_repo_dlgs.remove(str);
  }
}

/****************************************************************************
  Popups client options
****************************************************************************/
void fc_client::popup_client_options()
{
  option_dialog_popup(_("Set local options"), client_optset);
}


void fc_client::create_cursors(void)
{
  enum cursor_type curs;
  int cursor;
  QPixmap *pix;
  int hot_x, hot_y;
  struct sprite *sprite;
  int frame;
  QCursor *c;
  for (cursor = 0; cursor < CURSOR_LAST; cursor++) {
    for (frame = 0; frame < NUM_CURSOR_FRAMES; frame++) {
      curs = static_cast<cursor_type>(cursor);
      sprite = get_cursor_sprite(tileset, curs, &hot_x, &hot_y, frame);
      pix = sprite->pm;
      c = new QCursor(*pix, hot_x, hot_y);
      fc_cursors[cursor][frame] = c;
    }
  }
}


/****************************************************************************
  Contructor for corner widget (used for menubar)
****************************************************************************/
fc_corner::fc_corner(QMainWindow *qmw): QWidget()
{
  QHBoxLayout *hb;
  QPushButton *qpb;
  int h;
  QFont *f = fc_font::instance()->get_font(fonts::default_font);

  if (f->pointSize() > 0) {
    h = f->pointSize();
  } else {
    h = f->pixelSize();
  }
  mw = qmw;
  hb = new QHBoxLayout();
  qpb = new QPushButton(fc_icons::instance()->get_icon("cmin"), "");
  qpb->setFixedSize(h, h);
  connect(qpb, SIGNAL(clicked()), SLOT(minimize()));
  hb->addWidget(qpb);
  qpb = new QPushButton(fc_icons::instance()->get_icon("cmax"), "");
  qpb->setFixedSize(h, h);
  connect(qpb, SIGNAL(clicked()), SLOT(maximize()));
  hb->addWidget(qpb);
  qpb = new QPushButton(fc_icons::instance()->get_icon("cclose"), "");
  qpb->setFixedSize(h, h);
  connect(qpb, SIGNAL(clicked()), SLOT(close_fc()));
  hb->addWidget(qpb);
  setLayout(hb);
}

/****************************************************************************
  Slot for closing freeciv via corner widget
****************************************************************************/
void fc_corner::close_fc()
{
  mw->close();
}

/****************************************************************************
  Slot for maximizing freeciv window via corner widget
****************************************************************************/
void fc_corner::maximize()
{
  if (mw->isMaximized() == false) {
    mw->showMaximized();
  } else {
    mw->showNormal();
  }
}

/****************************************************************************
  Slot for minimizing freeciv window via corner widget
****************************************************************************/
void fc_corner::minimize()
{
  mw->showMinimized();
}

/****************************************************************************
  Game tab widget constructor
****************************************************************************/
fc_game_tab_widget::fc_game_tab_widget(): QStackedWidget()
{
}

/****************************************************************************
  Init default settings for game_tab_widget
****************************************************************************/
void fc_game_tab_widget::init()
{
  connect(this, SIGNAL(currentChanged(int)), SLOT(current_changed(int)));
}


/****************************************************************************
  Icon provider constructor
****************************************************************************/
fc_icons::fc_icons()
{
}

/****************************************************************************
  Returns instance of fc_icons
****************************************************************************/
fc_icons *fc_icons::instance()
{
  if (!m_instance)
    m_instance = new fc_icons;
  return m_instance;
}

/****************************************************************************
  Deletes fc_icons instance
****************************************************************************/
void fc_icons::drop()
{
  if (m_instance) {
    delete m_instance;
    m_instance = 0;
  }
}

/****************************************************************************
  Returns icon by given name
****************************************************************************/
QIcon fc_icons::get_icon(const QString &id)
{
  QIcon icon;
  QString str;

  str = QString("themes") + DIR_SEPARATOR + "gui-qt" + DIR_SEPARATOR;
  /* Try custom icon from theme */
  icon.addFile(fileinfoname(get_data_dirs(),
                            QString(str.toLocal8Bit().data() + current_theme
                                    + DIR_SEPARATOR
                                    + id + ".png").toLocal8Bit().data()));
  str = str + "icons" + DIR_SEPARATOR;
  /* Try icon from icons dir */
  if (icon.isNull()) {
  icon.addFile(fileinfoname(get_data_dirs(),
                            QString(str.toLocal8Bit().data()
                                    + id + ".png").toLocal8Bit().data()));
  }
  return QIcon(icon);
}

/****************************************************************************
  Returns pixmap by given name, pixmap needs to be deleted by someone else
****************************************************************************/
QPixmap* fc_icons::get_pixmap(const QString &id)
{
  QPixmap *pm;
  bool status;
  QString str;

  pm = new QPixmap;
  str = QString("themes") + DIR_SEPARATOR + "gui-qt" + DIR_SEPARATOR;
  status = pm->load(fileinfoname(get_data_dirs(),
                                 QString(str + current_theme
                                 + DIR_SEPARATOR
                                 + id + ".png").toLocal8Bit().data()));

  if (status == false) {
    str = str + "icons" + DIR_SEPARATOR;
    pm->load(fileinfoname(get_data_dirs(), QString(str
                          + id + ".png").toLocal8Bit().data()));
  }
  return pm;
}

/****************************************************************************
  Returns path for icon
****************************************************************************/
QString fc_icons::get_path(const QString &id)
{
  QString str;
  
  str = QString("themes") + DIR_SEPARATOR + "gui-qt"
        + DIR_SEPARATOR + "icons" + DIR_SEPARATOR;
  return fileinfoname(get_data_dirs(),
                      QString(str + id + ".png").toLocal8Bit().data());
}

/****************************************************************************
  Resize event for all game tab widgets
****************************************************************************/
void fc_game_tab_widget::resizeEvent(QResizeEvent *event)
{
  QSize size;
  size = event->size();
  if (C_S_RUNNING <= client_state()) {
    gui()->sidebar_wdg->resize_me(size.width(), size.height());
    map_canvas_resized(size.width(), size.height());
    size.setWidth(event->size().width() - gui()->sidebar_wdg->width());
    gui()->infotab->resize((size.width()
                             * gui()->qt_settings.chat_width) / 100,
                             (size.height()
                             * gui()->qt_settings.chat_height) / 100);
    gui()->infotab->move(0 , size.height() - gui()->infotab->height());
    gui()->infotab->restore_chat();
    gui()->minimapview_wdg->move(event->size().width() -
                                 gui()->minimapview_wdg->width() - 10,
                                 size.height() -
                                 gui()->minimapview_wdg->height() - 10);
    gui()->x_vote->move(width() / 2 - gui()->x_vote->width() / 2, 0);
    gui()->update_sidebar_tooltips();
    side_disable_endturn(get_turn_done_button_state());
    gui()->mapview_wdg->resize(event->size().width(),size.height());
    update_unit_info_label(nullptr);
  }
  event->setAccepted(true);
}

/****************************************************************************
  Tab has been changed
****************************************************************************/
void fc_game_tab_widget::current_changed(int index)
{
  QList<fc_sidewidget*> objs;
  fc_sidewidget *sw;

  if (gui()->is_closing()) {
    return;
  }
  objs = gui()->sidebar_wdg->objects;

  foreach(sw, objs) {
    sw->update_final_pixmap();
  }
  currentWidget()->hide();
  widget(index)->show();

  /* Set focus to map instead sidebar */
  if (gui()->mapview_wdg && gui()->current_page() == PAGE_GAME
     && index == 0) {
    gui()->mapview_wdg->setFocus();
  }

}

/****************************************************************************
  Init's layout and default values for options in START_PAGE
****************************************************************************/
void pregame_options::init()
{
  QFormLayout *layout;
  QHBoxLayout *hbox = nullptr;
  QPushButton *but;
  int level;

  layout = new QFormLayout(this);
  nation = new QPushButton(this);
  max_players = new QSpinBox(this);
  ailevel = new QComboBox(this);
  cruleset = new QComboBox(this);
  max_players->setRange(1, MAX_NUM_PLAYERS);

  // Text and icon set by update_buttons()
  connect(nation, &QPushButton::clicked,
          this, &pregame_options::pick_nation);

  for (level = 0; level < AI_LEVEL_COUNT; level++) {
    if (is_settable_ai_level(static_cast<ai_level>(level))) {
      const char *level_name = ai_level_translated_name(
                                        static_cast<ai_level>(level));
      ailevel->addItem(level_name, level);
    }
  }
  ailevel->setCurrentIndex(-1);

  connect(max_players, SIGNAL(valueChanged(int)),
          SLOT(max_players_change(int)));
  connect(ailevel, SIGNAL(currentIndexChanged(int)),
          SLOT(ailevel_change(int)));
  connect(cruleset, SIGNAL(currentIndexChanged(int)),
          SLOT(ruleset_change(int)));

  but = new QPushButton;
  but->setText(_("More Game Options"));
  but->setIcon(fc_icons::instance()->get_icon("preferences-other"));
  QObject::connect(but, SIGNAL(clicked()), this,
                   SLOT(popup_server_options()));

  layout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
  layout->addRow(_("Nation:"), nation);
  layout->addRow(_("Rules:"), cruleset);

  hbox = new QHBoxLayout();
  hbox->addWidget(max_players);
  hbox->addWidget(ailevel);
  layout->addRow(_("Players:"), hbox);
  layout->addWidget(but);
  setLayout(layout);

  update_buttons();
}

/****************************************************************************
  Update the ruleset list
****************************************************************************/
void pregame_options::set_rulesets(int num_rulesets, char **rulesets)
{
  int i;
  int def_idx = -1;

  cruleset->clear();
  cruleset->blockSignals(true);
  for (i = 0; i < num_rulesets; i++){
    cruleset->addItem(rulesets[i], i);
    if (!strcmp("default", rulesets[i])) {
      def_idx = i;
    }
  }

  /* HACK: server should tell us the current ruleset. */
  cruleset->setCurrentIndex(def_idx);
  cruleset->blockSignals(false);
}

/****************************************************************************
  Sets the value of the "aifill" option. Doesn't send the new value to the
  server
****************************************************************************/
void pregame_options::set_aifill(int aifill)
{
  max_players->blockSignals(true);
  max_players->setValue(aifill);
  max_players->blockSignals(false);
}

/****************************************************************************
  Updates the buttons whenever the game state has changed
****************************************************************************/
void pregame_options::update_buttons()
{
  struct sprite *psprite = nullptr;
  QPixmap *pixmap = nullptr;
  const struct player *pplayer = client_player();

  // Update the "Select Nation" button
  if (pplayer != nullptr) {
    if (pplayer->nation != nullptr) {
      nation->setText(nation_adjective_for_player(pplayer));
      psprite = get_nation_shield_sprite(tileset, pplayer->nation);
      pixmap = psprite->pm;
      nation->setIconSize(pixmap->size());
      nation->setIcon(QIcon(*pixmap));
    } else {
      nation->setText(_("Random"));
      nation->setIcon(fc_icons::instance()->get_icon("flush-random"));
    }
  }
}

/****************************************************************************
  Slot for changing aifill value
****************************************************************************/
void pregame_options::max_players_change(int i)
{
  option_int_set(optset_option_by_name(server_optset, "aifill"), i);
}

/****************************************************************************
  Slot for changing level of AI
****************************************************************************/
void pregame_options::ailevel_change(int i)
{
  int k;
  const char *name;

  k = ailevel->currentData().toInt();
  name = ai_level_cmd(static_cast<ai_level>(k));
  send_chat_printf("/%s", name);
}

/****************************************************************************
  Slot for changing ruleset
****************************************************************************/
void pregame_options::ruleset_change(int i)
{
  if (!cruleset->currentText().isEmpty()) {
    set_ruleset(cruleset->currentText().toLocal8Bit().data());
  }
}

/****************************************************************************
  Slot for picking a nation
****************************************************************************/
void pregame_options::pick_nation()
{
  popup_races_dialog(client_player());
}

/****************************************************************************
  Popups client options
****************************************************************************/
void pregame_options::popup_server_options()
{
  option_dialog_popup(_("Set server options"), server_optset);
}

