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

#ifdef HAVE_CONFIG_H
#include <fc_config.h>
#endif

// Qt
#include <QApplication>
#include <QDesktopWidget>
#include <QHeaderView>
#include <QMessageBox>
#include <QRadioButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSplitter>
#include <QVBoxLayout>
#include <QWidgetAction>

// utility
#include "support.h"

// client
#include "citydlg_common.h"
#include "climisc.h"
#include "control.h"
#include "global_worklist.h"
#include "helpdata.h"
#include "mapview_common.h"
#include "movement.h"
#include "sprite.h"
#include "text.h"
#include "tilespec.h"

// common
#include "citizens.h"
#include "city.h"
#include "game.h"

//gui-qt
#include "citydlg.h"
#include "colors.h"
#include "hudwidget.h"

//agents
#include "cma_fec.h"

extern QApplication *qapp;
static bool city_dlg_created = false; /** defines if dialog for city has been
                                       * already created. It's created only
                                       * once per client
                                       */
static city_dialog *city_dlg;
extern QString split_text(QString text, bool cut);
extern QString cut_helptext(QString text);

/****************************************************************************
  Custom progressbar constructor
****************************************************************************/
progress_bar::progress_bar(QWidget *parent): QProgressBar(parent)
{
  m_timer.start();
  startTimer(50);
  create_region();
  sfont = new QFont;
}

/****************************************************************************
  Custom progressbar resize event
****************************************************************************/
void progress_bar::resizeEvent(QResizeEvent *event)
{
  create_region();
}

/****************************************************************************
  Timer event used to animate progress
****************************************************************************/
void progress_bar::timerEvent(QTimerEvent *event)
{
  if ((value() != minimum() && value() < maximum())
      || (0 == minimum() && 0 == maximum())) {
    m_animate_step = m_timer.elapsed() / 50;
    update();
  }
}

/****************************************************************************
  Paint event for custom progress bar
****************************************************************************/
void progress_bar::paintEvent(QPaintEvent *event)
{
  QPainter p;
  QString str;
  QLinearGradient g, gx;
  QColor c;
  QRect r, rx, r2;
  int max;
  int f_size;
  int point_size = sfont->pointSize();
  int pixel_size = sfont->pixelSize();

  if (point_size < 0) {
    f_size = pixel_size;
  } else {
    f_size = point_size;
  }

  rx.setX(0);
  rx.setY(0);
  rx.setWidth(width());
  rx.setHeight(height());
  p.begin(this);
  p.drawLine(rx.topLeft(), rx.topRight());
  p.drawLine(rx.bottomLeft(), rx.bottomRight());

  max = maximum();

  if (max == 0) {
    max = 1;
  }

  r = QRect(0, 0, width() * value() / max, height());

  gx = QLinearGradient(0 , 0, 0, height());
  c = QColor(palette().color(QPalette::Highlight));
  gx.setColorAt(0, c);
  gx.setColorAt(0.5, QColor(40, 40, 40));
  gx.setColorAt(1, c);
  p.fillRect(r, QBrush(gx));
  p.setClipRegion(reg.translated(m_animate_step % 32, 0));

  g = QLinearGradient(0 , 0, width(), height());
  c.setAlphaF(0.1);
  g.setColorAt(0, c);
  c.setAlphaF(0.9);
  g.setColorAt(1, c);
  p.fillRect(r, QBrush(g));

  p.setClipping(false);
  r2 = QRect(width() * value() / max, 0, width(), height());
  c = palette().color(QPalette::Window);
  p.fillRect(r2, c);

  /* draw text */
  c = palette().color(QPalette::Text);
  p.setPen(c);
  sfont->setCapitalization(QFont::AllUppercase);
  sfont->setBold(true);
  p.setFont(*sfont);

  if (text().contains('\n')) {
    QString s1, s2;
    int i, j;

    i = text().indexOf('\n');
    s1 = text().left(i);
    s2 = text().right(text().count() - i);

    if (2 * f_size >= 2 * height() / 3) {
      if (point_size < 0) {
        sfont->setPixelSize(height() / 4);
      } else  {
        sfont->setPointSize(height() / 4);
      }
    }

    j = height() - 2 * f_size;
    p.setCompositionMode(QPainter::CompositionMode_ColorDodge);
    QFontMetrics fm(*sfont);

    if (fm.width(s1) > rx.width()) {
      s1 = fm.elidedText(s1, Qt::ElideRight, rx.width());
    }

    i = rx.width() - fm.width(s1);
    i = qMax(0, i);
    p.drawText(i / 2, j / 3 + f_size, s1);

    if (fm.width(s2) > rx.width()) {
      s2 = fm.elidedText(s2, Qt::ElideRight, rx.width());
    }

    i = rx.width() - fm.width(s2);
    i = qMax(0, i);

    p.drawText(i / 2, height() - j / 3, s2);
  } else {
    QString s;
    int i, j;
    s = text();
    j = height() - f_size;
    p.setCompositionMode(QPainter::CompositionMode_ColorDodge);
    QFontMetrics fm(*sfont);

    if (fm.width(s) > rx.width()) {
      s = fm.elidedText(s, Qt::ElideRight, rx.width());
    }

    i = rx.width() - fm.width(s);
    i = qMax(0, i);
    p.drawText(i / 2, j / 2 + f_size, s);
  }

  p.end();
}

/****************************************************************************
  Creates region with diagonal lines
****************************************************************************/
void progress_bar::create_region()
{
  int offset;
  QRect   r(-50, 0, width() + 50, height());
  int chunk_width = 16;
  int size = width()  + 50;
  reg = QRegion();

  for (offset = 0; offset < (size * 2); offset += (chunk_width * 2)) {
    QPolygon a;

    a.setPoints(4, r.x(), r.y() + offset,
                r.x() + r.width(), (r.y() + offset) - size,
                r.x() + r.width(),
                (r.y() + offset + chunk_width) - size,
                r.x(), r.y() + offset + chunk_width);
    reg += QRegion(a);
  }

}

/****************************************************************************
  Draws X on pixmap pointing its useless
****************************************************************************/
static void pixmap_put_x(QPixmap *pix)
{
  QPen pen(QColor(0, 0, 0));
  QPainter p;

  pen.setWidth(2);
  p.begin(pix);
  p.setRenderHint(QPainter::Antialiasing);
  p.setPen(pen);
  p.drawLine(0, 0, pix->width(), pix->height());
  p.drawLine(pix->width(), 0, 0, pix->height());
  p.end();
}

/****************************************************************************
  Improvement item constructor
****************************************************************************/
impr_item::impr_item(QWidget *parent, impr_type *building,
                     struct city *city): QLabel(parent)
{
  setParent(parent);
  pcity = city;
  impr = building;
  impr_pixmap = nullptr;
  struct sprite *sprite;
  sprite = get_building_sprite(tileset , building);

  if (sprite != nullptr) {
    impr_pixmap = qtg_canvas_create(sprite->pm->width(),
                                    sprite->pm->height());
    impr_pixmap->map_pixmap.fill(Qt::transparent);
    pixmap_copy(&impr_pixmap->map_pixmap, sprite->pm, 0 , 0, 0, 0,
                sprite->pm->width(), sprite->pm->height());
  }

  setFixedWidth(impr_pixmap->map_pixmap.width() + 4);
  setFixedHeight(impr_pixmap->map_pixmap.height());
  setToolTip(get_tooltip_improvement(building).trimmed());
}

/****************************************************************************
  Improvement item destructor
****************************************************************************/
impr_item::~impr_item()
{
  if (impr_pixmap) {
    qtg_canvas_free(impr_pixmap);
  }
}

/****************************************************************************
  Sets pixmap to improvemnt item
****************************************************************************/
void impr_item::init_pix()
{
  setPixmap(impr_pixmap->map_pixmap);
  update();
}

/****************************************************************************
  Mouse enters widget
****************************************************************************/
void impr_item::enterEvent(QEvent *event)
{
  struct sprite *sprite;
  QPainter p;

  if (impr_pixmap) {
    qtg_canvas_free(impr_pixmap);
  }

  if (impr) {
    sprite = get_building_sprite(tileset , impr);
    impr_pixmap = qtg_canvas_create(sprite->pm->width(),
                                    sprite->pm->height());
    impr_pixmap->map_pixmap.fill(QColor(palette().color(QPalette::Highlight)));
    pixmap_copy(&impr_pixmap->map_pixmap, sprite->pm, 0 , 0, 0, 0,
                sprite->pm->width(), sprite->pm->height());
  }

  init_pix();
}

/****************************************************************************
  Mouse leaves widget
****************************************************************************/
void impr_item::leaveEvent(QEvent *event)
{
  struct sprite *sprite;

  if (impr_pixmap) {
    qtg_canvas_free(impr_pixmap);
  }

  if (impr) {
    sprite = get_building_sprite(tileset , impr);
    impr_pixmap = qtg_canvas_create(sprite->pm->width(),
                                    sprite->pm->height());
    impr_pixmap->map_pixmap.fill(Qt::transparent);
    pixmap_copy(&impr_pixmap->map_pixmap, sprite->pm, 0 , 0, 0, 0,
                sprite->pm->width(), sprite->pm->height());
  }

  init_pix();
}

/****************************************************************************
  Improvement list constructor
****************************************************************************/
impr_info::impr_info(QWidget *parent): QFrame(parent)
{
  setParent(parent);
  layout = new QHBoxLayout(this);
  init_layout();
}

/****************************************************************************
  Inits improvement list constructor
****************************************************************************/
void impr_info::init_layout()
{
  QSizePolicy size_fixed_policy(QSizePolicy::Fixed,
                                QSizePolicy::MinimumExpanding,
                                QSizePolicy::Slider);

  setSizePolicy(size_fixed_policy);
  setLayout(layout);
}

/****************************************************************************
  Improvement list destructor
****************************************************************************/
impr_info::~impr_info()
{

}

/****************************************************************************
  Adds improvement item to list
****************************************************************************/
void impr_info::add_item(impr_item *item)
{
  impr_list.append(item);
}

/****************************************************************************
  Clears layout on improvement list
****************************************************************************/
void impr_info::clear_layout()
{
  int i = impr_list.count();
  impr_item *ui;
  int j;
  setUpdatesEnabled(false);
  setMouseTracking(false);

  for (j = 0; j < i; j++) {
    ui = impr_list[j];
    layout->removeWidget(ui);
    delete ui;
  }

  while (!impr_list.empty()) {
    impr_list.removeFirst();
  }

  setMouseTracking(true);
  setUpdatesEnabled(true);
}

/****************************************************************************
  Updates list of improvements
****************************************************************************/
void impr_info::update_buildings()
{
  int i = impr_list.count();
  int j;
  int h = 0;
  impr_item *ui;

  setUpdatesEnabled(false);
  hide();

  for (j = 0; j < i; j++) {
    ui = impr_list[j];
    h = ui->height();
    layout->addWidget(ui, 0, Qt::AlignVCenter);
  }

  if (impr_list.count() > 0) {
    parentWidget()->parentWidget()->setFixedHeight(city_dlg->scroll_height
                                                   + h + 6);
  } else {
    parentWidget()->parentWidget()->setFixedHeight(0);
  }

  show();
  setUpdatesEnabled(true);
  layout->update();
  updateGeometry();
}

/****************************************************************************
  Double click event on improvement item
****************************************************************************/
void impr_item::mouseDoubleClickEvent(QMouseEvent *event)
{
  hud_message_box ask(city_dlg);
  QString s;
  QVariant qvar;
  char buf[256];
  int price;
  int ret;

  if (!can_client_issue_orders()) {
    return;
  }

  if (event->button() == Qt::LeftButton) {
    if (test_player_sell_building_now(client.conn.playing, pcity,
                                      impr) != TR_SUCCESS) {
      return;
    }

    price = impr_sell_gold(impr);
    fc_snprintf(buf, ARRAY_SIZE(buf),
                PL_("Sell %s for %d gold?",
                    "Sell %s for %d gold?", price),
                city_improvement_name_translation(pcity, impr), price);

    s = QString(buf);
    ask.set_text_title(s, (_("Sell improvement?")));
    ask.setStandardButtons(QMessageBox::Cancel | QMessageBox::Ok);
    ret = ask.exec();

    switch (ret) {
    case QMessageBox::Cancel:
      return;

    case QMessageBox::Ok:
      city_sell_improvement(pcity, improvement_number(impr));
      break;
    }
  }
}


/****************************************************************************
  Class representing one unit, allows context menu, holds pixmap for it
****************************************************************************/
unit_item::unit_item(QWidget *parent, struct unit *punit,
                     bool supp, int hppy_cost) : QLabel()
{
  setParent(parent);
  happy_cost = hppy_cost;
  supported = supp;
  unit_pixmap = NULL;
  qunit = punit;

  if (punit) {
    if (supported) {
      unit_pixmap = qtg_canvas_create(tileset_full_tile_width(tileset),
                                      tileset_unit_with_upkeep_height(tileset));
    } else {
      unit_pixmap = qtg_canvas_create(tileset_full_tile_width(tileset),
                                      tileset_unit_height(tileset));
    }

    unit_pixmap->map_pixmap.fill(Qt::transparent);
    put_unit(punit, unit_pixmap, 1.0, 0, 0);

    if (supported) {
      put_unit_city_overlays(punit, unit_pixmap, 0,
                             tileset_unit_layout_offset_y(tileset),
                             punit->upkeep, happy_cost);
    }
  }

  create_actions();
  setFixedWidth(unit_pixmap->map_pixmap.width() + 4);
  setFixedHeight(unit_pixmap->map_pixmap.height());
  setToolTip(unit_description(qunit));
}

/****************************************************************************
  Sets pixmap for unit_item class
****************************************************************************/
void unit_item::init_pix()
{
  setPixmap(unit_pixmap->map_pixmap);
  update();
}

/****************************************************************************
  Destructor for unit item
****************************************************************************/
unit_item::~unit_item()
{
  if (unit_pixmap) {
    qtg_canvas_free(unit_pixmap);
  }
}

/****************************************************************************
  Context menu handler
****************************************************************************/
void unit_item::contextMenuEvent(QContextMenuEvent *event)
{
  QMenu *menu = new QMenu(gui()->central_wdg);

  if (!can_client_issue_orders()) {
    return;
  }

  if (unit_owner(qunit) != client_player()) {
    return;
  }

  menu->addAction(activate);
  menu->addAction(activate_and_close);

  if (sentry) {
    menu->addAction(sentry);
  }

  if (fortify) {
    menu->addAction(fortify);
  }

  if (change_home) {
    menu->addAction(change_home);
  }

  if (load) {
    menu->addAction(load);
  }

  if (unload) {
    menu->addAction(unload);
  }

  if (unload_trans) {
    menu->addAction(unload_trans);
  }

  if (disband_action) {
    menu->addAction(disband_action);
  }

  if (upgrade) {
    menu->addAction(upgrade);
  }

  menu->popup(event->globalPos());
}

/****************************************************************************
  Initializes context menu
****************************************************************************/
void unit_item::create_actions()
{
  struct unit_list *qunits;

  if (unit_owner(qunit) != client_player() || !can_client_issue_orders()) {
    return;
  }

  qunits = unit_list_new();
  unit_list_append(qunits, qunit);
  activate = new QAction(_("Activate unit"), this);
  connect(activate, SIGNAL(triggered()), this, SLOT(activate_unit()));
  activate_and_close = new QAction(_("Activate and close dialog"), this);
  connect(activate_and_close, SIGNAL(triggered()), this,
          SLOT(activate_and_close_dialog()));

  if (can_unit_do_activity(qunit, ACTIVITY_SENTRY)) {
    sentry = new QAction(_("Sentry unit"), this);
    connect(sentry, SIGNAL(triggered()), this, SLOT(sentry_unit()));
  } else {
    sentry = NULL;
  }

  if (can_unit_do_activity(qunit, ACTIVITY_FORTIFYING)) {
    fortify = new QAction(_("Fortify unit"), this);
    connect(fortify, SIGNAL(triggered()), this, SLOT(fortify_unit()));
  } else {
    fortify = NULL;
  }

  if (!unit_has_type_flag(qunit, UTYF_UNDISBANDABLE)) {
    disband_action = new QAction(_("Disband unit"), this);
    connect(disband_action, SIGNAL(triggered()), this, SLOT(disband()));
  } else {
    disband_action = NULL;
  }

  if (can_unit_change_homecity(qunit)) {
    change_home = new QAction(_("Change homecity"), this);
    connect(change_home, SIGNAL(triggered()), this, SLOT(change_homecity()));
  } else {
    change_home = NULL;
  }

  if (units_can_load(qunits)) {
    load = new QAction(_("Load"), this);
    connect(load, SIGNAL(triggered()), this, SLOT(load_unit()));
  } else {
    load = NULL;
  }

  if (units_can_unload(qunits)) {
    unload = new QAction(_("Unload"), this);
    connect(unload, SIGNAL(triggered()), this, SLOT(unload_unit()));
  } else {
    unload = NULL;
  }

  if (units_are_occupied(qunits)) {
    unload_trans = new QAction(_("Unload All From Transporter"), this);
    connect(unload_trans, SIGNAL(triggered()), this, SLOT(unload_all()));
  } else {
    unload_trans = NULL;
  }

  if (units_can_upgrade(qunits)) {
    upgrade = new QAction(_("Upgrade Unit"), this);
    connect(upgrade, SIGNAL(triggered()), this, SLOT(upgrade_unit()));
  } else {
    upgrade = NULL;
  }

  unit_list_destroy(qunits);
}

/****************************************************************************
  Popups MessageBox  for disbanding unit and disbands it
****************************************************************************/
void unit_item::disband()
{
  struct unit_list *punits;
  struct unit *punit = player_unit_by_number(client_player(), qunit->id);

  if (punit == nullptr) {
    return;
  }

  punits = unit_list_new();
  unit_list_append(punits, punit);
  popup_disband_dialog(punits);
  unit_list_destroy(punits);
}

/****************************************************************************
  Loads unit into some tranport
****************************************************************************/
void unit_item::load_unit()
{
  request_unit_load(qunit, NULL, unit_tile(qunit));
}

/****************************************************************************
  Unloads unit
****************************************************************************/
void unit_item::unload_unit()
{
  request_unit_unload(qunit);
}

/****************************************************************************
  Unloads all untis from transporter
****************************************************************************/
void unit_item::unload_all()
{
  request_unit_unload_all(qunit);
}

/****************************************************************************
  Upgrades unit
****************************************************************************/
void unit_item::upgrade_unit()
{
  struct unit_list *qunits;
  qunits = unit_list_new();
  unit_list_append(qunits, qunit);
  popup_upgrade_dialog(qunits);
  unit_list_destroy(qunits);
}

/****************************************************************************
  Changes homecity for given unit
****************************************************************************/
void unit_item::change_homecity()
{
  if (qunit) {
    request_unit_change_homecity(qunit);
  }
}

/****************************************************************************
  Activates unit and closes city dialog
****************************************************************************/
void unit_item::activate_and_close_dialog()
{
  if (qunit) {
    unit_focus_set(qunit);
    qtg_popdown_all_city_dialogs();
  }
}

/****************************************************************************
  Activates unit in city dialog
****************************************************************************/
void unit_item::activate_unit()
{
  if (qunit) {
    unit_focus_set(qunit);
  }
}

/****************************************************************************
  Fortifies unit in city dialog
****************************************************************************/
void unit_item::fortify_unit()
{
  if (qunit) {
    request_unit_fortify(qunit);
  }
}

/****************************************************************************
  Mouse entered widget
****************************************************************************/
void unit_item::enterEvent(QEvent *event)
{
  if (unit_pixmap) {
    delete unit_pixmap;
  }

  if (qunit) {
    if (supported) {
      unit_pixmap = qtg_canvas_create(tileset_full_tile_width(tileset),
                                      tileset_unit_with_upkeep_height(tileset));
    } else {
      unit_pixmap = qtg_canvas_create(tileset_full_tile_width(tileset),
                                      tileset_unit_height(tileset));
    }

    unit_pixmap->map_pixmap.fill(QColor(palette().color(QPalette::Highlight)));
    put_unit(qunit, unit_pixmap, 1.0, 0, 0);

    if (supported) {
      put_unit_city_overlays(qunit, unit_pixmap, 0,
                             tileset_unit_layout_offset_y(tileset),
                             qunit->upkeep, happy_cost);
    }
  }

  init_pix();
}

/****************************************************************************
  Mouse left widget
****************************************************************************/
void unit_item::leaveEvent(QEvent *event)
{
  if (unit_pixmap) {
    delete unit_pixmap;
  }

  if (qunit) {
    if (supported) {
      unit_pixmap = qtg_canvas_create(tileset_full_tile_width(tileset),
                                      tileset_unit_with_upkeep_height(tileset));
    } else {
      unit_pixmap = qtg_canvas_create(tileset_full_tile_width(tileset),
                                      tileset_unit_height(tileset));
    }

    unit_pixmap->map_pixmap.fill(Qt::transparent);
    put_unit(qunit, unit_pixmap, 1.0, 0, 0);

    if (supported) {
      put_unit_city_overlays(qunit, unit_pixmap, 0,
                             tileset_unit_layout_offset_y(tileset),
                             qunit->upkeep, happy_cost);
    }
  }

  init_pix();
}

/****************************************************************************
  Mouse press event -activates unit and closes dialog
****************************************************************************/
void unit_item::mousePressEvent(QMouseEvent *event)
{
  if (event->button() == Qt::LeftButton) {
    if (qunit) {
      unit_focus_set(qunit);
      qtg_popdown_all_city_dialogs();
    }
  }
}

/****************************************************************************
  Sentries unit in city dialog
****************************************************************************/
void unit_item::sentry_unit()
{
  if (qunit) {
    request_unit_sentry(qunit);
  }
}

/****************************************************************************
  Class representing list of units ( unit_item 's)
****************************************************************************/
unit_info::unit_info(bool supp) : QFrame()
{
  layout = new QHBoxLayout(this);
  init_layout();
  supports = supp;
}

/****************************************************************************
  Destructor for unit_info
****************************************************************************/
unit_info::~unit_info()
{
  qDeleteAll(unit_list);
  unit_list.clear();
}

/****************************************************************************
  Adds one unit to list
****************************************************************************/
void unit_info::add_item(unit_item *item)
{
  unit_list.append(item);
}

/****************************************************************************
  Initiazlizes layout ( layout needs to be changed after adding units )
****************************************************************************/
void unit_info::init_layout()
{
  QSizePolicy size_fixed_policy(QSizePolicy::Fixed,
                                QSizePolicy::MinimumExpanding,
                                QSizePolicy::Slider);
  setSizePolicy(size_fixed_policy);
  setLayout(layout);
}


/****************************************************************************
  Updates units
****************************************************************************/
void unit_info::update_units()
{
  int i = unit_list.count();
  int j;
  int h;
  unit_item *ui;

  setUpdatesEnabled(false);
  hide();

  for (j = 0; j < i; j++) {
    ui = unit_list[j];
    layout->addWidget(ui, 0, Qt::AlignVCenter);
  }

  h = tileset_unit_height(tileset) + 6;
  if (supports) {
    h = tileset_unit_with_upkeep_height(tileset) + 6;
  }

  if (unit_list.count() > 0) {
    parentWidget()->parentWidget()->setFixedHeight(city_dlg->scroll_height
                                                   + h);
  } else {
    parentWidget()->parentWidget()->setFixedHeight(0);
  }
  show();
  setUpdatesEnabled(true);
  layout->update();
  updateGeometry();
}

/****************************************************************************
  Cleans layout - run it before layout initialization
****************************************************************************/
void unit_info::clear_layout()
{
  int i = unit_list.count();
  unit_item *ui;
  int j;
  setUpdatesEnabled(false);
  setMouseTracking(false);

  for (j = 0; j < i; j++) {
    ui = unit_list[j];
    layout->removeWidget(ui);
    delete ui;
  }

  while (!unit_list.empty()) {
    unit_list.removeFirst();
  }

  setMouseTracking(true);
  setUpdatesEnabled(true);
}

/****************************************************************************
  city_label is used only for showing citizens icons
  and was created only to catch mouse events
****************************************************************************/
city_label::city_label(int t, QWidget *parent) : QLabel(parent)
{
  type = t;
}

/****************************************************************************
  Mouse handler for city_label
****************************************************************************/
void city_label::mousePressEvent(QMouseEvent *event)
{
  int citnum, i;
  int w = tileset_small_sprite_width(tileset);
  int num_citizens = pcity->size;

  if (cma_is_city_under_agent(pcity, NULL)) {
    return;
  }

  i = 1 + (num_citizens * 5 / 200);
  w = w / i;
  citnum = event->x() / w;

  if (!can_client_issue_orders()) {
    return;
  }

  city_rotate_specialist(pcity, citnum);
}

/****************************************************************************
  Just sets target city for city_label
****************************************************************************/
void city_label::set_city(city *pciti)
{
  pcity = pciti;
}

/****************************************************************************
  Used for showing tiles and workers view in city dialog
****************************************************************************/
city_map::city_map(QWidget *parent): QWidget(parent)
{
  setParent(parent);
  radius = 0;
  wdth = get_citydlg_canvas_width();
  hight = get_citydlg_canvas_height();
  cutted_width = wdth;
  cutted_height = hight;
  view = qtg_canvas_create(wdth, hight);
  view->map_pixmap.fill(Qt::black);
  miniview = qtg_canvas_create(0, 0);
  miniview->map_pixmap.fill(Qt::black);
  delta_x = 0;
  delta_y = 0;
  setContextMenuPolicy(Qt::CustomContextMenu);
  connect(this, SIGNAL(customContextMenuRequested(const QPoint &)),
          this, SLOT(context_menu(const QPoint &)));
}

/****************************************************************************
  Destructor for city map
****************************************************************************/
city_map::~city_map()
{
  qtg_canvas_free(view);
  qtg_canvas_free(miniview);
}

/****************************************************************************
  Redraws whole view in city_map
****************************************************************************/
void city_map::paintEvent(QPaintEvent *event)
{
  QPainter painter;
  QString str;

  painter.begin(this);
  painter.drawPixmap(0, 0, cutted_width, cutted_height,
                     miniview->map_pixmap);

  if (cma_is_city_under_agent(mcity, NULL)) {
    QPixmap pix(cutted_width, cutted_height);
    pix.fill(QColor(90, 90, 90, 90));
    painter.drawPixmap(0, 0, cutted_width, cutted_height, pix);
    painter.setPen(QColor(120, 120, 120));
    /* TRANS: %1 is custom string choosen by player. */
    str = QString(_("Governor %1"))
          .arg(cmafec_get_short_descr_of_city(mcity));
    painter.drawText(5, cutted_height - 10, str);
  }

  painter.end();
}

/****************************************************************************
  Calls function to put pixmap on view ( it doesn't draw on screen )
****************************************************************************/
void city_map::set_pixmap(struct city *pcity)
{
  int r, max_r;

  r = sqrt(city_map_radius_sq_get(pcity));

  if (radius != r) {
    max_r = sqrt(rs_max_city_radius_sq());
    radius = r;
    delete miniview;
    cutted_width = wdth * (r + 1) / max_r;
    cutted_height = hight * (r + 1) / max_r;
    cutted_width = qMin(cutted_width, wdth);
    cutted_height = qMin(cutted_height, hight);
    delta_x = (wdth - cutted_width) / 2;
    delta_y = (hight - cutted_height) / 2;
    miniview = qtg_canvas_create(cutted_width, cutted_height);
    miniview->map_pixmap.fill(Qt::black);
    setMinimumSize(cutted_width, cutted_height);
    setMaximumSize(cutted_width, cutted_height);
    parentWidget()->updateGeometry();
  }

  city_dialog_redraw_map(pcity, view);
  qtg_canvas_copy(miniview, view, delta_x, delta_y,
                  0, 0, cutted_width, cutted_height);
  mcity = pcity;
}

/****************************************************************************
  Used to change workers on view
****************************************************************************/
void city_map::mousePressEvent(QMouseEvent *event)
{
  int canvas_x, canvas_y, city_x, city_y;

  if (!can_client_issue_orders() || event->button() != Qt::LeftButton) {
    return;
  }

  canvas_x = event->x() + delta_x;
  canvas_y = event->y() + delta_y;

  if (canvas_to_city_pos(&city_x, &city_y, city_map_radius_sq_get(mcity),
                         canvas_x, canvas_y)) {
    city_toggle_worker(mcity, city_x, city_y);
  }
}

/****************************************************************************
  Context menu for setting worker tasks
****************************************************************************/
void city_map::context_menu(QPoint point)
{
  int canvas_x, canvas_y, city_x, city_y;
  QAction *act;
  QAction con_clear(_("Clear"), this);
  QAction con_irrig_tf(_("Irrigate"), this);
  QAction con_irrig(_("Irrigate"), this);
  QAction con_mine_tf(_("Plant"), this);
  QAction con_mine(_("Mine"), this);
  QAction con_road(_("Road"), this);
  QAction con_trfrm(_("Transform"), this);
  QMenu con_menu(this);
  QWidgetAction *wid_act;
  struct packet_worker_task task;
  struct terrain *pterr;
  struct tile *ptile;
  struct universal for_terr;
  struct worker_task *ptask;

  if (!can_client_issue_orders()) {
    return;
  }

  canvas_x = point.x() + delta_x;
  canvas_y = point.y() + delta_y;

  if (!canvas_to_city_pos(&city_x, &city_y, city_map_radius_sq_get(mcity),
                          canvas_x, canvas_y)) {
    return;
  }

  ptile = city_map_to_tile(mcity->tile, city_map_radius_sq_get(mcity),
                           city_x, city_y);
  task.city_id = mcity->id;
  pterr = tile_terrain(ptile);
  for_terr.kind = VUT_TERRAIN;
  for_terr.value.terrain = pterr;
  ptask = worker_task_list_get(mcity->task_reqs, 0);

  wid_act = new QWidgetAction(this);
  wid_act->setDefaultWidget(new QLabel(_("Autosettler activity:")));
  con_menu.addAction(wid_act);

  if (pterr->mining_result != pterr && pterr->mining_result != NULL
      && effect_cumulative_max(EFT_MINING_TF_POSSIBLE, &for_terr) > 0) {
    con_menu.addAction(&con_mine_tf);
  } else if (pterr->mining_result == pterr
             && effect_cumulative_max(EFT_MINING_POSSIBLE, &for_terr) > 0) {
    con_menu.addAction(&con_mine);
  }

  if (pterr->irrigation_result != pterr && pterr->irrigation_result != NULL
      && effect_cumulative_max(EFT_IRRIG_TF_POSSIBLE, &for_terr) > 0) {
    con_menu.addAction(&con_irrig_tf);
  } else if (pterr->irrigation_result == pterr
             && effect_cumulative_max(EFT_IRRIG_POSSIBLE, &for_terr) > 0) {
    con_menu.addAction(&con_irrig);
  }

  if (pterr->transform_result != pterr && pterr->transform_result != NULL
      && effect_cumulative_max(EFT_TRANSFORM_POSSIBLE, &for_terr) > 0) {
    con_menu.addAction(&con_trfrm);
  }

  if (next_extra_for_tile(ptile, EC_ROAD, city_owner(mcity), NULL) != NULL) {
    con_menu.addAction(&con_road);
  }

  if (ptask != NULL) {
    con_menu.addAction(&con_clear);
  }

  act = con_menu.exec(mapToGlobal(point));

  if (act) {
    bool target = FALSE;

    if (act == &con_road) {
      task.activity = ACTIVITY_GEN_ROAD;
      target = TRUE;
    } else if (act == &con_mine) {
      task.activity = ACTIVITY_MINE;
      target = TRUE;
    } else if (act == &con_mine_tf) {
      task.activity = ACTIVITY_MINE;
    } else if (act == &con_irrig) {
      task.activity = ACTIVITY_IRRIGATE;
      target = TRUE;
    } else if (act == &con_irrig_tf) {
      task.activity = ACTIVITY_IRRIGATE;
    } else if (act == &con_trfrm) {
      task.activity = ACTIVITY_TRANSFORM;
    }

    task.want = 100;

    if (target) {
      enum extra_cause cause = activity_to_extra_cause(task.activity);
      struct extra_type *tgt;

      tgt = next_extra_for_tile(ptile, cause, city_owner(mcity), NULL);

      if (tgt != NULL) {
        task.tgt = extra_index(tgt);
      } else {
        task.tgt = -1;
      }
    } else {
      task.tgt = -1;
    }

    task.tile_id = ptile->index;
    send_packet_worker_task(&client.conn, &task);
  }
}

/****************************************************************************
  Constructor for city_dialog, sets layouts, policies ...
****************************************************************************/
city_dialog::city_dialog(QWidget *parent): QDialog(parent)
{
  int info_nr;
  int iter;
  QFont f = QApplication::font();
  QFont *small_font;
  QFontMetrics fm(f);
  QGridLayout *gridl, *slider_grid;
  QGroupBox *group_box, *map_box, *prod_options,
            *qgbox, *qgbprod, *qsliderbox, *result_box;
  QHBoxLayout *hbox, *hbox_layout, *prod_option_layout,
              *v_layout, *work_but_layout;
  QHeaderView *header;
  QLabel *lab2, *label, *ql, *some_label;
  QPushButton *qpush2;
  QScrollArea *scroll, *scroll2, *scroll3;
  QSizePolicy size_expanding_policy(QSizePolicy::Expanding,
                                    QSizePolicy::Expanding);
  QSizePolicy size_fixed_policy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  QSlider *slider;
  QStringList info_list, str_list;
  QVBoxLayout *lefttop_layout, *units_layout, *worklist_layout,
              *right_layout, *vbox, *vbox_layout;
  QWidget *split_widget1, *split_widget2, *info_wdg;

  int h = 2 * fm.height() + 2;
  small_font = fc_font::instance()->get_font(fonts::city_label);

  happines_shown = false;
  central_splitter = new QSplitter;
  central_splitter->setOpaqueResize(true);
  central_left_splitter = new QSplitter;
  central_left_splitter->setOpaqueResize(true);
  prod_unit_splitter = new QSplitter;
  prod_unit_splitter->setOpaqueResize(true);

  setParent(parent);
  setMouseTracking(true);
  setWindowFlags(Qt::Dialog);
  selected_row_p = -1;
  pcity = NULL;
  lcity_name = new QPushButton(this);
  lcity_name->setToolTip(_("Click to change city name"));

  single_page_layout = new QHBoxLayout();
  single_page_layout->setContentsMargins(0, 0 ,0 ,0);
  size_expanding_policy.setHorizontalStretch(0);
  size_expanding_policy.setVerticalStretch(0);
  current_building = 0;

  /* map view */
  map_box = new QGroupBox(this);
  v_layout = new QHBoxLayout;

  /* City information widget texts about surpluses and so on */
  info_wdg = new QWidget(this);

  /* Fill info_wdg with labels */
  info_grid_layout = new QGridLayout(parent);
  info_list << _("Food:") << _("Prod:") << _("Trade:") << _("Gold:")
            << _("Luxury:") << _("Science:") << _("Granary:")
            << _("Change in:") << _("Corruption:") << _("Waste:")
            << _("Culture:") << _("Pollution:") << _("Plague Risk:");
  info_nr = info_list.count();
  info_wdg->setFont(*small_font);
  info_grid_layout->setSpacing(0);
  info_grid_layout->setMargin(0);

  for (iter = 0; iter < info_nr; iter++) {
    ql = new QLabel(info_list[iter], info_wdg);
    ql->setFont(*small_font);
    ql->setProperty(fonts::city_label, "true");
    info_grid_layout->addWidget(ql, iter, 0);
    qlt[iter] = new QLabel(info_wdg);
    qlt[iter]->setFont(*small_font);
    qlt[iter]->setProperty(fonts::city_label, "true");
    info_grid_layout->addWidget(qlt[iter], iter, 1);
    info_grid_layout->setRowStretch(iter, 0);
  }

  info_wdg->setLayout(info_grid_layout);

  /* Buy button */
  buy_button = new QPushButton();
  buy_button->setIcon(fc_icons::instance()->get_icon("help-donate"));
  connect(buy_button, SIGNAL(clicked()), SLOT(buy()));

  connect(lcity_name, SIGNAL(clicked()), SLOT(city_rename()));
  citizens_label = new city_label(FEELING_FINAL, this);
  citizen_pixmap = NULL;
  view = new city_map(this);

  /* City map group box */
  vbox_layout = new QVBoxLayout;
  hbox_layout = new QHBoxLayout;
  hbox_layout->addWidget(view);
  vbox_layout->addLayout(hbox_layout);
  vbox_layout->addWidget(lcity_name);
  map_box->setLayout(vbox_layout);
  map_box->setTitle(_("City map"));

  /* current/supported units/improvements widgets */
  supp_units = new QGroupBox();
  supp_units->setProperty("themed_groupbox", true);
  curr_units = new QGroupBox();
  curr_units->setProperty("themed_groupbox", true);
  curr_impr = new QGroupBox();
  curr_impr->setProperty("themed_groupbox", true);
  supported_units = new unit_info(true);
  scroll = new QScrollArea;
  scroll->setWidgetResizable(true);
  scroll->setMaximumHeight(tileset_unit_with_upkeep_height(tileset) + 6
                           + scroll->horizontalScrollBar()->height());
  scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  scroll->setWidget(supported_units);
  current_units = new unit_info(false);
  scroll2 = new QScrollArea;
  scroll2->setWidgetResizable(true);
  scroll2->setMaximumHeight(tileset_unit_height(tileset) + 6
                            + scroll2->horizontalScrollBar()->height());
  scroll2->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  scroll2->setWidget(current_units);
  scroll_height = scroll2->horizontalScrollBar()->height();
  city_buildings = new impr_info(this);
  scroll3 = new QScrollArea;
  scroll3->setWidgetResizable(true);
  scroll3->setMaximumHeight(tileset_unit_height(tileset) + 6
                            + scroll3->horizontalScrollBar()->height());
  scroll3->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  scroll3->setWidget(city_buildings);
  scroll->setProperty("city_scroll", true);
  scroll2->setProperty("city_scroll", true);
  scroll3->setProperty("city_scroll", true);

  lefttop_layout = new QVBoxLayout();
  units_layout = new QVBoxLayout();
  worklist_layout = new QVBoxLayout();
  right_layout = new QVBoxLayout();
  leftbot_layout = new QHBoxLayout();
  units_layout = new QVBoxLayout();
  left_layout = new QVBoxLayout();

  /* Checkboxes to show units/wonders/imrovements
   * on production list */
  prod_option_layout = new QHBoxLayout;
  show_buildings = new QCheckBox;
  show_buildings->setToolTip(_("Show buildings"));
  show_buildings->setChecked(true);
  label = new QLabel();
  label->setPixmap(*fc_icons::instance()->get_pixmap("building"));
  label->setToolTip(_("Show buildings"));
  prod_option_layout->addWidget(show_buildings, Qt::AlignLeft);
  prod_option_layout->addWidget(label, Qt::AlignLeft);
  prod_option_layout->addStretch(100);
  label = new QLabel();
  label->setPixmap(*fc_icons::instance()->get_pixmap("cunits"));
  label->setToolTip(_("Show units"));
  show_units = new QCheckBox;
  show_units->setToolTip(_("Show units"));
  show_units->setChecked(true);
  prod_option_layout->addWidget(show_units, Qt::AlignHCenter);
  prod_option_layout->addWidget(label, Qt::AlignHCenter);
  prod_option_layout->addStretch(100);
  label = new QLabel();
  label->setPixmap(*fc_icons::instance()->get_pixmap("wonder"));
  label->setToolTip(_("Show wonders"));
  show_wonders = new QCheckBox;
  show_wonders->setToolTip(_("Show wonders"));
  show_wonders->setChecked(true);
  prod_option_layout->addWidget(show_wonders);
  prod_option_layout->addWidget(label);
  prod_option_layout->addStretch(100);
  label = new QLabel();
  label->setPixmap(*fc_icons::instance()->get_pixmap("future"));
  label->setToolTip(_("Show future targets"));
  future_targets = new QCheckBox;
  future_targets->setToolTip(_("Show future targets"));
  future_targets->setChecked(false);
  prod_option_layout->addWidget(future_targets);
  prod_option_layout->addWidget(label, Qt::AlignRight);
  prod_options = new QGroupBox(this);
  prod_options->setLayout(prod_option_layout);

  /* prev/next and close buttons */
  button = new QPushButton;
  button->setIcon(fc_icons::instance()->get_icon("city-close"));
  button->setIconSize(QSize(56, 56));
  button->setToolTip(_("Close city dialog"));
  connect(button, SIGNAL(clicked()), SLOT(hide()));

  next_city_but = new QPushButton();
  next_city_but->setIcon(fc_icons::instance()->get_icon("city-right"));
  next_city_but->setIconSize(QSize(56, 56));
  next_city_but->setToolTip(_("Show next city"));
  connect(next_city_but, SIGNAL(clicked()), SLOT(next_city()));

  prev_city_but = new QPushButton();
  connect(prev_city_but, SIGNAL(clicked()), SLOT(prev_city()));
  prev_city_but->setIcon(fc_icons::instance()->get_icon("city-left"));
  prev_city_but->setIconSize(QSize(56, 56));
  prev_city_but->setToolTip(_("Show previous city"));

  happiness_button = new QPushButton();
  happiness_button->setIcon(fc_icons::instance()->get_icon("city-switch"));
  happiness_button->setIconSize(QSize(56, 28));
  connect(happiness_button, SIGNAL(clicked()), SLOT(show_happiness()));
  update_happiness_button();

  button->setFixedSize(64, 64);
  prev_city_but->setFixedSize(64, 64);
  next_city_but->setFixedSize(64, 64);
  happiness_button->setFixedSize(64, 32);
  vbox_layout = new QVBoxLayout;
  vbox_layout->addWidget(prev_city_but);
  vbox_layout->addWidget(next_city_but);
  vbox_layout->addWidget(button);
  vbox_layout->addWidget(happiness_button, Qt::AlignHCenter);
  hbox_layout = new QHBoxLayout;

  hbox_layout->addLayout(vbox_layout, Qt::AlignLeft);
  hbox_layout->addWidget(info_wdg, Qt::AlignLeft);
  hbox_layout->addWidget(map_box, Qt::AlignCenter);

  /* Layout with city view and buttons */
  lefttop_layout->addWidget(citizens_label, Qt::AlignHCenter);
  lefttop_layout->addLayout(hbox_layout);
  lefttop_layout->addStretch(1);

  /* Layout for units/buildings */
  v_layout = new QHBoxLayout;
  v_layout->addWidget(scroll3);
  curr_impr->setLayout(v_layout);
  curr_impr->setTitle(_("Improvements"));
  v_layout = new QHBoxLayout;
  v_layout->addWidget(scroll2);
  curr_units->setLayout(v_layout);
  v_layout = new QHBoxLayout;
  v_layout->addWidget(scroll);
  supp_units->setLayout(v_layout);

  units_layout->addWidget(curr_units);
  units_layout->addWidget(supp_units);
  units_layout->addWidget(curr_impr);

  vbox = new QVBoxLayout;
  vbox_layout = new QVBoxLayout;
  qgbprod = new QGroupBox;
  group_box = new QGroupBox(_("Worklist Option"));
  work_but_layout = new QHBoxLayout;
  work_next_but = new QPushButton(fc_icons::instance()->get_icon(
                                    "go-down"), "");
  work_prev_but = new QPushButton(fc_icons::instance()->get_icon(
                                    "go-up"), "");
  work_add_but = new QPushButton(fc_icons::instance()->get_icon(
                                   "list-add"), "");
  work_rem_but = new QPushButton(style()->standardIcon(
                                   QStyle::SP_DialogDiscardButton), "");
  work_but_layout->addWidget(work_add_but);
  work_but_layout->addWidget(work_next_but);
  work_but_layout->addWidget(work_prev_but);
  work_but_layout->addWidget(work_rem_but);
  but_menu_worklist = new QPushButton;
  production_combo_p = new progress_bar(parent);
  production_combo_p->setToolTip(_("Click to change current production"));
  p_table_p = new QTableWidget;

  r1 = new QRadioButton(_("Change"));
  r2 = new QRadioButton(_("Insert Before"));
  r3 = new QRadioButton(_("Insert After"));
  r4 = new QRadioButton(_("Add Last"));
  r4->setChecked(true);
  group_box->setLayout(vbox);


  p_table_p->setColumnCount(3);
  p_table_p->setProperty("showGrid", "false");
  p_table_p->setProperty("selectionBehavior", "SelectRows");
  p_table_p->setEditTriggers(QAbstractItemView::NoEditTriggers);
  p_table_p->verticalHeader()->setVisible(false);
  p_table_p->horizontalHeader()->setVisible(false);
  p_table_p->setSelectionMode(QAbstractItemView::SingleSelection);
  production_combo_p->setFixedHeight(h);
  p_table_p->setMinimumWidth(200);
  p_table_p->setSizePolicy(size_expanding_policy);
  p_table_p->setContextMenuPolicy(Qt::CustomContextMenu);
  header = p_table_p->horizontalHeader();
  header->setStretchLastSection(true);

  qgbprod->setTitle(_("Worklist"));
  vbox_layout->addWidget(prod_options);
  vbox_layout->addWidget(production_combo_p);
  vbox_layout->addWidget(buy_button);
  vbox_layout->addWidget(p_table_p);
  vbox_layout->addLayout(work_but_layout);
  qgbprod->setLayout(vbox_layout);

  but_menu_worklist->setText(_("Worklist menu"));
  but_menu_worklist->setIcon(style()->standardIcon(
                               QStyle::SP_FileLinkIcon));

  worklist_layout->addWidget(qgbprod);
  connect(p_table_p,
          SIGNAL(customContextMenuRequested(const QPoint &)), this,
          SLOT(display_worklist_menu(const QPoint &)));
  connect(but_menu_worklist, SIGNAL(clicked()), SLOT(delete_prod()));
  connect(production_combo_p, SIGNAL(clicked()), SLOT(show_targets()));
  connect(work_add_but, SIGNAL(clicked()), SLOT(show_targets_worklist()));
  connect(work_prev_but, SIGNAL(clicked()), SLOT(worklist_up()));
  connect(work_next_but, SIGNAL(clicked()), SLOT(worklist_down()));
  connect(work_rem_but, SIGNAL(clicked()), SLOT(worklist_del()));
  connect(p_table_p,
          SIGNAL(itemDoubleClicked(QTableWidgetItem *)),
          SLOT(dbl_click_p(QTableWidgetItem *)));
  connect(p_table_p->selectionModel(),
          SIGNAL(selectionChanged(const QItemSelection &,
                                  const QItemSelection &)),
          SLOT(item_selected(const QItemSelection &,
                             const QItemSelection &)));
  happiness_group = new QGroupBox(_("Happiness"));
  gridl = new QGridLayout;

  nationality_table = new QTableWidget;
  nationality_table->setColumnCount(3);
  nationality_table->setProperty("showGrid", "false");
  nationality_table->setProperty("selectionBehavior", "SelectRows");
  nationality_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  nationality_table->verticalHeader()->setVisible(false);
  nationality_table->horizontalHeader()->setStretchLastSection(true);

  info_list.clear();
  info_list << _("Cities:") << _("Luxuries:") << _("Buildings:")
            << _("Nationality:") << _("Units:") <<  _("Wonders:");
  info_nr = info_list.count();

  for (int i = 0; i < info_list.count(); i++) {
    lab_table[i] = new city_label(1 + i, this);
    gridl->addWidget(lab_table[i], i, 1, 1, 1);
    lab2 = new QLabel(this);
    lab2->setFont(*small_font);
    lab2->setProperty(fonts::city_label, "true");
    lab2->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    lab2->setText(info_list.at(i));
    gridl->addWidget(lab2, i, 0, 1, 1);
  }

  gridl->setSpacing(0);
  happiness_group->setLayout(gridl);


  happiness_layout = new QHBoxLayout;
  happiness_layout->addWidget(happiness_group);
  happiness_layout->addWidget(nationality_table);
  happiness_layout->setStretch(0, 10);
  happiness_widget = new QWidget();
  happiness_widget->setLayout(happiness_layout);
  qgbox = new QGroupBox(_("Presets:"));
  qsliderbox = new QGroupBox(_("Governor settings"));
  result_box = new QGroupBox(_("Results:"));
  hbox = new QHBoxLayout;
  gridl = new QGridLayout;
  slider_grid = new QGridLayout;

  qpush2
    = new QPushButton(style()->standardIcon(QStyle::SP_DialogSaveButton),
                      _("Save"));
  connect(qpush2, SIGNAL(pressed()), SLOT(save_cma()));

  cma_info_text = new QLabel;
  cma_info_text->setFont(*small_font);
  cma_info_text->setAlignment(Qt::AlignCenter);
  cma_table = new QTableWidget;
  cma_table->setColumnCount(1);
  cma_table->setProperty("showGrid", "false");
  cma_table->setProperty("selectionBehavior", "SelectRows");
  cma_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  cma_table->setSelectionMode(QAbstractItemView::SingleSelection);
  cma_table->setContextMenuPolicy(Qt::CustomContextMenu);
  cma_table->verticalHeader()->setVisible(false);
  cma_table->horizontalHeader()->setVisible(false);
  cma_table->horizontalHeader()->setSectionResizeMode(
    QHeaderView::Stretch);

  connect(cma_table->selectionModel(),
          SIGNAL(selectionChanged(const QItemSelection &,
                                  const QItemSelection &)),
          SLOT(cma_selected(const QItemSelection &,
                            const QItemSelection &)));
  connect(cma_table,
          SIGNAL(customContextMenuRequested(const QPoint &)), this,
          SLOT(cma_context_menu(const QPoint &)));
  connect(cma_table, SIGNAL(cellDoubleClicked(int, int)), this,
          SLOT(cma_double_clicked(int, int)));
  gridl->addWidget(cma_table, 0, 0, 1, 2);
  qgbox->setLayout(gridl);
  hbox->addWidget(cma_info_text);
  result_box->setLayout(hbox);
  str_list << _("Food") << _("Shield") << _("Trade") << _("Gold")
           << _("Luxury") << _("Science") << _("Celebrate");
  some_label = new QLabel(_("Minimal Surplus"));
  some_label->setAlignment(Qt::AlignCenter);
  slider_grid->addWidget(some_label, 0, 1, 1, 2);
  some_label = new QLabel(_("Priority"));
  some_label->setAlignment(Qt::AlignCenter);
  slider_grid->addWidget(some_label, 0, 3, 1, 2);

  for (int i = 0; i < str_list.count(); i++) {
    some_label = new QLabel(str_list.at(i));
    slider_grid->addWidget(some_label, i + 1, 0, 1, 1);
    some_label = new QLabel("0");
    some_label->setMinimumWidth(25);

    if (i != str_list.count() - 1) {
      slider = new QSlider(Qt::Horizontal);
      slider->setFocusPolicy(Qt::TabFocus);
      slider_tab[2 * i] = slider;
      slider->setRange(-20, 20);
      slider->setSingleStep(1);
      slider_grid->addWidget(some_label, i + 1, 1, 1, 1);
      slider_grid->addWidget(slider, i + 1, 2, 1, 1);
      slider->setProperty("FC", QVariant::fromValue((void *)some_label));

      connect(slider, SIGNAL(valueChanged(int)), SLOT(cma_slider(int)));
    } else {
      cma_celeb_checkbox = new QCheckBox;
      slider_grid->addWidget(cma_celeb_checkbox, i + 1, 2 , 1 , 1);
      connect(cma_celeb_checkbox,
              SIGNAL(stateChanged(int)), SLOT(cma_celebrate_changed(int)));
    }

    some_label = new QLabel("0");
    some_label->setMinimumWidth(25);
    slider = new QSlider(Qt::Horizontal);
    slider->setFocusPolicy(Qt::TabFocus);
    slider->setRange(0, 25);
    slider_tab[2 * i + 1] = slider;
    slider->setProperty("FC", QVariant::fromValue((void *)some_label));
    slider_grid->addWidget(some_label, i + 1, 3, 1, 1);
    slider_grid->addWidget(slider, i + 1, 4, 1, 1);
    connect(slider, SIGNAL(valueChanged(int)), SLOT(cma_slider(int)));
  }

  cma_enable_but = new QPushButton();
  cma_enable_but->setFocusPolicy(Qt::TabFocus);
  connect(cma_enable_but, SIGNAL(pressed()), SLOT(cma_enable()));
  slider_grid->addWidget(cma_enable_but, O_LAST + 4, 1, 1, 2);
  slider_grid->addWidget(qpush2, O_LAST + 4, 3, 1, 2);

  qsliderbox->setLayout(slider_grid);
  cma_result = new QLabel;
  cma_result_pix = new QLabel;

  hbox = new QHBoxLayout;
  hbox->addWidget(cma_result_pix);
  hbox->addWidget(cma_result);
  hbox->addStretch(10);
  right_layout->addWidget(qgbox);
  right_layout->addLayout(hbox);
  right_layout->addWidget(qsliderbox);

  split_widget1 = new QWidget;
  split_widget1->setLayout(worklist_layout);
  split_widget2 = new QWidget;
  split_widget2->setLayout(units_layout);
  prod_unit_splitter->addWidget(split_widget1);
  prod_unit_splitter->addWidget(split_widget2);
  prod_unit_splitter->setStretchFactor(0, 1);
  prod_unit_splitter->setStretchFactor(1, 5);
  prod_unit_splitter->setOrientation(Qt::Horizontal);
  leftbot_layout->addWidget(prod_unit_splitter);
  top_widget = new QWidget;
  top_widget->setLayout(lefttop_layout);

  prod_happ_widget = new QWidget;
  prod_happ_widget->setLayout(leftbot_layout);
  central_left_splitter->addWidget(top_widget);
  central_left_splitter->addWidget(prod_happ_widget);
  central_left_splitter->setStretchFactor(0, 1);
  central_left_splitter->setStretchFactor(1, 100);
  central_left_splitter->setOrientation(Qt::Vertical);
  left_layout->addWidget(central_left_splitter);

  split_widget1 = new QWidget(this);
  split_widget2 = new QWidget(this);
  split_widget1->setLayout(left_layout);
  split_widget2->setLayout(right_layout);
  central_splitter->addWidget(split_widget1);
  central_splitter->addWidget(split_widget2);
  central_splitter->setStretchFactor(0, 100);
  central_splitter->setStretchFactor(1, 1);
  central_splitter->setOrientation(Qt::Horizontal);
  single_page_layout->addWidget(central_splitter);
  setSizeGripEnabled(true);
  setLayout(single_page_layout);

  ::city_dlg_created = true;
}

/****************************************************************************
  Sets tooltip for happiness/pruction button switcher
****************************************************************************/
void city_dialog::update_happiness_button()
{
  if (happines_shown == false) {
    happiness_button->setToolTip(_("Show happiness information"));
  } else {
    happiness_button->setToolTip(_("Show city production"));
  }
}

/****************************************************************************
  Shows happiness tab
****************************************************************************/
void city_dialog::show_happiness()
{
  setUpdatesEnabled(false);

  if (happines_shown == false) {
    leftbot_layout->replaceWidget(prod_unit_splitter,
                                  happiness_widget,
                                  Qt::FindDirectChildrenOnly);
    prod_unit_splitter->hide();
    happiness_widget->show();
    happiness_widget->updateGeometry();
  } else {
    leftbot_layout->replaceWidget(happiness_widget,
                                  prod_unit_splitter,
                                  Qt::FindDirectChildrenOnly);
    prod_unit_splitter->show();
    prod_unit_splitter->updateGeometry();
    happiness_widget->hide();
  }

  setUpdatesEnabled(true);
  update();
  happines_shown = !happines_shown;
  update_happiness_button();
}


/****************************************************************************
  Updates buttons/widgets which should be enabled/disabled
****************************************************************************/
void city_dialog::update_disabled()
{
  if (NULL == client.conn.playing
      || city_owner(pcity) != client.conn.playing) {
    prev_city_but->setDisabled(true);
    next_city_but->setDisabled(true);
    buy_button->setDisabled(true);
    cma_enable_but->setDisabled(true);
    production_combo_p->setDisabled(true);
    but_menu_worklist->setDisabled(true);
    current_units->setDisabled(true);
    supported_units->setDisabled(true);
    view->setDisabled(true);

    if (!client_is_observer()) {
    }
  } else {
    prev_city_but->setEnabled(true);
    next_city_but->setEnabled(true);
    buy_button->setEnabled(true);
    cma_enable_but->setEnabled(true);
    production_combo_p->setEnabled(true);
    but_menu_worklist->setEnabled(true);
    current_units->setEnabled(true);
    supported_units->setEnabled(true);
    view->setEnabled(true);
  }

  if (can_client_issue_orders()) {
    cma_enable_but->setEnabled(true);
  } else  {
    cma_enable_but->setDisabled(true);
  }

  update_prod_buttons();
}

/****************************************************************************
  Update sensitivity of buttons in production tab
****************************************************************************/
void city_dialog::update_prod_buttons()
{
  work_next_but->setDisabled(true);
  work_prev_but->setDisabled(true);
  work_add_but->setDisabled(true);
  work_rem_but->setDisabled(true);

  if (client.conn.playing && city_owner(pcity) == client.conn.playing) {
    work_add_but->setEnabled(true);

    if (selected_row_p >= 0 && selected_row_p < p_table_p->rowCount()) {
      work_rem_but->setEnabled(true);
    }

    if (selected_row_p >= 0 && selected_row_p < p_table_p->rowCount() - 1) {
      work_next_but->setEnabled(true);
    }

    if (selected_row_p > 0 && selected_row_p < p_table_p->rowCount()) {
      work_prev_but->setEnabled(true);
    }
  }
}

/****************************************************************************
  City dialog destructor
****************************************************************************/
city_dialog::~city_dialog()
{
  if (citizen_pixmap) {
    delete citizen_pixmap;
  }

  cma_table->clear();
  p_table_p->clear();
  nationality_table->clear();
  current_units->clear_layout();
  supported_units->clear_layout();
  ::city_dlg_created = false;
}

/****************************************************************************
  Hide event
****************************************************************************/
void city_dialog::hideEvent(QHideEvent *event)
{
  gui()->qt_settings.city_geometry = saveGeometry();
  gui()->qt_settings.city_splitter1 = prod_unit_splitter->saveState();
  gui()->qt_settings.city_splitter2 = central_left_splitter->saveState();
  gui()->qt_settings.city_splitter3 = central_splitter->saveState();
}

/****************************************************************************
  Show event
****************************************************************************/
void city_dialog::showEvent(QShowEvent *event)
{
  if (gui()->qt_settings.city_geometry.isNull() == false) {
    restoreGeometry(gui()->qt_settings.city_geometry);
    prod_unit_splitter->restoreState(gui()->qt_settings.city_splitter1);
    central_left_splitter->restoreState(gui()->qt_settings.city_splitter2);
    central_splitter->restoreState(gui()->qt_settings.city_splitter3);
  }
}

/****************************************************************************
  Show event
****************************************************************************/
void city_dialog::closeEvent(QCloseEvent *event)
{
  gui()->qt_settings.city_geometry = saveGeometry();
  gui()->qt_settings.city_splitter1 = prod_unit_splitter->saveState();
  gui()->qt_settings.city_splitter2 = central_left_splitter->saveState();
  gui()->qt_settings.city_splitter3 = central_splitter->saveState();
}
/****************************************************************************
  City rename dialog input
****************************************************************************/
void city_dialog::city_rename()
{
  hud_input_box ask(gui()->central_wdg);

  ask.set_text_title_definput(_("What should we rename the city to?"),
                              _("Rename City"), city_name_get(pcity));
  if (ask.exec() == QDialog::Accepted) {
    ::city_rename(pcity, ask.input_edit.text().toLocal8Bit().data());
  }
}

/****************************************************************************
  Save cma dialog input
****************************************************************************/
void city_dialog::save_cma()
{
  struct cm_parameter param;
  QString text;
  hud_input_box ask(gui()->central_wdg);

  ask.set_text_title_definput(_("What should we name the preset?"),
                              _("Name new preset"),
                              _("new preset"));
  if (ask.exec() == QDialog::Accepted) {
    text = ask.input_edit.text().toLocal8Bit().data();
    if (!text.isEmpty()) {
      param.allow_disorder = false;
      param.allow_specialists = true;
      param.require_happy = cma_celeb_checkbox->isChecked();
      param.happy_factor = slider_tab[2 * O_LAST + 1]->value();

      for (int i = O_FOOD; i < O_LAST; i++) {
        param.minimal_surplus[i] = slider_tab[2 * i]->value();
        param.factor[i] = slider_tab[2 * i + 1]->value();
      }

      cmafec_preset_add(text.toLocal8Bit().data(), &param);
      update_cma_tab();
    }
  }
}

/****************************************************************************
  Enables cma slot, triggered by clicked button or changed cma
****************************************************************************/
void city_dialog::cma_enable()
{
  if (cma_is_city_under_agent(pcity, NULL)) {
    cma_release_city(pcity);
    return;
  }

  cma_changed();
  update_cma_tab();
}

/****************************************************************************
  Sliders moved and cma has been changed
****************************************************************************/
void city_dialog::cma_changed()
{
  struct cm_parameter param;

  param.allow_disorder = false;
  param.allow_specialists = true;
  param.require_happy = cma_celeb_checkbox->isChecked();
  param.happy_factor = slider_tab[2 * O_LAST + 1]->value();

  for (int i = O_FOOD; i < O_LAST; i++) {
    param.minimal_surplus[i] = slider_tab[2 * i]->value();
    param.factor[i] = slider_tab[2 * i + 1]->value();
  }

  cma_put_city_under_agent(pcity, &param);
}

/****************************************************************************
  Double click on some row ( column is unused )
****************************************************************************/
void city_dialog::cma_double_clicked(int row, int column)
{
  const struct cm_parameter *param;

  param = cmafec_preset_get_parameter(row);

  if (cma_is_city_under_agent(pcity, NULL)) {
    cma_release_city(pcity);
  }

  cma_put_city_under_agent(pcity, param);
}

/****************************************************************************
  CMA has been selected from list
****************************************************************************/
void city_dialog::cma_selected(const QItemSelection &sl,
                               const QItemSelection &ds)
{
  const struct cm_parameter *param;
  QModelIndex index;
  QModelIndexList indexes = sl.indexes();

  if (indexes.isEmpty() || cma_table->signalsBlocked()) {
    return;
  }

  index = indexes.at(0);
  int ind = index.row();

  if (cma_table->currentRow() == -1 || cmafec_preset_num() == 0) {
    return;
  }

  param = cmafec_preset_get_parameter(ind);
  update_sliders();

  if (cma_is_city_under_agent(pcity, NULL)) {
    cma_release_city(pcity);
    cma_put_city_under_agent(pcity, param);
  }
}

/****************************************************************************
  Updates sliders ( cma params )
****************************************************************************/
void city_dialog::update_sliders()
{
  const struct cm_parameter *param;
  int output;
  QVariant qvar;
  QLabel *label;

  if (cma_table->currentRow() == -1 || cmafec_preset_num() == 0) {
    return;
  }

  param = cmafec_preset_get_parameter(cma_table->currentRow());

  for (output = O_FOOD; output < 2 * O_LAST; output++) {
    slider_tab[output]->blockSignals(true);
  }

  for (output = O_FOOD; output < O_LAST; output++) {
    qvar = slider_tab[2 * output + 1]->property("FC");
    label = reinterpret_cast<QLabel *>(qvar.value<void *>());
    label->setText(QString::number(param->factor[output]));
    slider_tab[2 * output + 1]->setValue(param->factor[output]);
    qvar = slider_tab[2 * output]->property("FC");
    label = reinterpret_cast<QLabel *>(qvar.value<void *>());
    label->setText(QString::number(param->minimal_surplus[output]));
    slider_tab[2 * output]->setValue(param->minimal_surplus[output]);
  }

  slider_tab[2 * O_LAST + 1]->blockSignals(true);
  qvar = slider_tab[2 * O_LAST + 1]->property("FC");
  label = reinterpret_cast<QLabel *>(qvar.value<void *>());
  label->setText(QString::number(param->happy_factor));
  slider_tab[2 * O_LAST + 1]->setValue(param->happy_factor);
  slider_tab[2 * O_LAST + 1]->blockSignals(false);
  cma_celeb_checkbox->blockSignals(true);
  cma_celeb_checkbox->setChecked(param->require_happy);
  cma_celeb_checkbox->blockSignals(false);

  for (output = O_FOOD; output < 2 * O_LAST; output++) {
    slider_tab[output]->blockSignals(false);
  }
}

/****************************************************************************
  Updates cma tab
****************************************************************************/
void city_dialog::update_cma_tab()
{
  QString s;
  QTableWidgetItem *item;
  struct cm_parameter param;
  QPixmap pix;
  int i;

  cma_table->clear();
  cma_table->setRowCount(0);

  for (i = 0; i < cmafec_preset_num(); i++) {
    item = new QTableWidgetItem;
    item->setText(cmafec_preset_get_descr(i));
    cma_table->insertRow(i);
    cma_table->setItem(i, 0, item);
  }

  if (cmafec_preset_num() == 0) {
    cma_table->insertRow(0);
    item = new QTableWidgetItem;
    item->setText(_("No governor defined"));
    cma_table->setItem(0, 0, item);
  }

  if (cma_is_city_under_agent(pcity, NULL)) {
    s = QString(cmafec_get_short_descr_of_city(pcity));
    pix = style()->standardPixmap(QStyle::SP_DialogApplyButton);
    pix = pix.scaled(2 * pix.width(), 2 * pix.height(),
                     Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    cma_result_pix->setPixmap(pix);
    cma_result_pix->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    /* TRANS: %1 is custom string chosen by player */
    cma_result->setText(QString(_("<h3>Governor Enabled<br>(%1)</h3>")).arg(s));
    cma_result->setAlignment(Qt::AlignCenter);
  } else {
    pix = style()->standardPixmap(QStyle::SP_DialogCancelButton);
    pix = pix.scaled(1.6 * pix.width(), 1.6 * pix.height(),
                     Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    cma_result_pix->setPixmap(pix);
    cma_result_pix->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    cma_result->setText(QString(_("<h3>Governor Disabled</h3>")));
    cma_result->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  }

  if (cma_is_city_under_agent(pcity, NULL)) {
    cmafec_get_fe_parameter(pcity, &param);
    i = cmafec_preset_get_index_of_parameter(const_cast < const struct
        cm_parameter *const >(&param));

    if (i >= 0 && i < cma_table->rowCount()) {
      cma_table->blockSignals(true);
      cma_table->setCurrentCell(i, 0);
      cma_table->blockSignals(false);
    }

    cma_enable_but->setText(_("Disable"));
  } else {
    cma_enable_but->setText(_("Enable"));
  }

  update_sliders();
  update_results_text();
}

/****************************************************************************
  Removes selected CMA
****************************************************************************/
void city_dialog::cma_remove()
{
  int i;
  hud_message_box ask(city_dlg);
  int ret;

  i = cma_table->currentRow();

  if (i == -1 || cmafec_preset_num() == 0) {
    return;
  }

  ask.set_text_title(_("Remove this preset?"), cmafec_preset_get_descr(i));
  ask.setStandardButtons(QMessageBox::Cancel | QMessageBox::Ok);
  ask.setDefaultButton(QMessageBox::Cancel);
  ret = ask.exec();

  switch (ret) {
  case QMessageBox::Cancel:
    return;

  case QMessageBox::Ok:
    cmafec_preset_remove(i);
    update_cma_tab();
    break;
  }
}

/****************************************************************************
  CMA option 'celebrate' qcheckbox state has been changed
****************************************************************************/
void city_dialog::cma_celebrate_changed(int val)
{
  if (cma_is_city_under_agent(pcity, NULL)) {
    cma_changed();
    update_cma_tab();
  }
}

/****************************************************************************
  CMA options on slider has been changed
****************************************************************************/
void city_dialog::cma_slider(int value)
{
  QVariant qvar;
  QSlider *slider;
  QLabel *label;
  QString s;

  slider = qobject_cast<QSlider *>(sender());
  qvar = slider->property("FC");

  if (qvar.isNull() || !qvar.isValid()) {
    return;
  }

  label = reinterpret_cast<QLabel *>(qvar.value<void *>());
  label->setText(QString::number(value));

  if (cma_is_city_under_agent(pcity, NULL)) {
    cma_changed();
    update_cma_tab();
  }
}

/****************************************************************************
  Updates text result in cma tab
****************************************************************************/
void city_dialog::update_results_text()
{

  QString str;
  struct cm_parameter param;
  struct cm_result *result = cm_result_new(pcity);

  cmafec_get_fe_parameter(pcity, &param);
  cm_result_from_main_map(result, pcity);
  str = cmafec_get_result_descr(pcity, result, &param);
  cma_info_text->setText(str);

  if (!cma_is_city_under_agent(pcity, NULL)) {
    cma_info_text->setDisabled(true);
  } else {
    cma_info_text->setDisabled(false);
  }

  cm_result_destroy(result);
}

/****************************************************************************
  Received signal about changed qcheckbox - disband at size 1
****************************************************************************/
void city_dialog::disband_state_changed(int state)
{
  bv_city_options new_options;

  BV_CLR_ALL(new_options);

  if (state == Qt::Checked) {
    BV_SET(new_options, CITYO_DISBAND);
  } else if (state == Qt::Unchecked) {
    BV_CLR(new_options, CITYO_DISBAND);
  }

  if (!client_is_observer()) {
    dsend_packet_city_options_req(&client.conn, pcity->id, new_options);
  }
}

/****************************************************************************
  Context menu on governor tab in city worklist
****************************************************************************/
void city_dialog::cma_context_menu(const QPoint &p)
{
  QMenu cma_menu(this);
  QAction *cma_del_item;
  QAction *act;

  cma_del_item = cma_menu.addAction(_("Remove Governor"));
  act = cma_menu.exec(QCursor::pos());

  if (act == cma_del_item) {
    cma_remove();
  }

}

/****************************************************************************
  Context menu on production tab in city worklist
****************************************************************************/
void city_dialog::display_worklist_menu(const QPoint &p)
{
  bool worklist_defined = true;
  cid c_id;
  int which_menu;
  QAction *act;
  QAction *action;
  QAction *disband;
  QAction *wl_save;
  QAction wl_clear(_("Clear"), 0);
  QAction wl_empty(_("(no worklists defined)"), 0);
  QMap<QString, cid> list;
  QMap<QString, cid>::const_iterator map_iter;
  QMenu *add_menu;
  QMenu *insert_menu;
  QMenu list_menu(this);
  QMenu *options_menu;
  QVariant qvar;

  if (!can_client_issue_orders()) {
    return;
  }

  add_menu = list_menu.addMenu(_("Change worklist"));
  insert_menu = list_menu.addMenu(_("Insert worklist"));
  connect(&wl_clear, SIGNAL(triggered()), SLOT(clear_worklist()));
  list_menu.addAction(&wl_clear);
  list.clear();

  global_worklists_iterate(pgwl) {
    list.insert(global_worklist_name(pgwl), global_worklist_id(pgwl));
  } global_worklists_iterate_end;

  if (list.count() == 0) {
    add_menu->addAction(&wl_empty);
    insert_menu->addAction(&wl_empty);
    worklist_defined = false;
  }

  map_iter = list.constBegin();

  while (map_iter != list.constEnd()) {
    action = add_menu->addAction(map_iter.key());
    action->setData(map_iter.value());
    action->setProperty("FC", 1);
    action = insert_menu->addAction(map_iter.key());
    action->setData(map_iter.value());
    action->setProperty("FC", 2);
    map_iter++;
  }

  wl_save = list_menu.addAction(_("Save worklist"));
  options_menu = list_menu.addMenu(_("Options"));
  disband = options_menu->addAction(_("Disband at size 1"));
  disband->setCheckable(true);
  disband->setChecked(is_city_option_set(pcity, CITYO_DISBAND));

  act = 0;
  act = list_menu.exec(QCursor::pos());

  if (act) {
    if (act == disband) {
      int state;

      if (disband->isChecked()) {
        state = Qt::Checked;
      } else {
        state = Qt::Unchecked;
      }

      disband_state_changed(state);
    }
    if (act == wl_save) {
      save_worklist();
      return;
    }
    qvar = act->property("FC");

    if (!qvar.isValid() || qvar.isNull() || !worklist_defined) {
      return;
    }

    which_menu = qvar.toInt();
    qvar = act->data();
    c_id = qvar.toInt();

    if (which_menu == 1) { /* Change Worklist */
      city_set_queue(pcity,
                     global_worklist_get(global_worklist_by_id(c_id)));
    } else if (which_menu == 2) { /* Insert Worklist */
      if (worklist_defined) {
        city_queue_insert_worklist(pcity, selected_row_p + 1,
                                   global_worklist_get(global_worklist_by_id(
                                         c_id)));
      }
    }
  }
}


/****************************************************************************
  Enables/disables buy buttons depending on gold
****************************************************************************/
void city_dialog::update_buy_button()
{
  QString str;
  int value;

  buy_button->setDisabled(true);

  if (!client_is_observer() && client.conn.playing != NULL) {
    value = city_production_buy_gold_cost(pcity);
    str = QString(PL_("Buy (%1 gold)", "Buy (%1 gold)",
                      value)).arg(QString::number(value));

    if (client.conn.playing->economic.gold >= value && value != 0) {
      buy_button->setEnabled(true);
    }
  } else {
    str = QString(_("Buy"));
  }

  buy_button->setText(str);
}

/****************************************************************************
  Redraws citizens for city_label (citizens_label)
****************************************************************************/
void city_dialog::update_citizens()
{
  enum citizen_category categories[MAX_CITY_SIZE];
  int i, j, width, height;
  QPainter p;
  QPixmap *pix;
  int num_citizens = get_city_citizen_types(pcity, FEELING_FINAL, categories);
  int w = tileset_small_sprite_width(tileset);
  int h = tileset_small_sprite_height(tileset);

  i = 1 + (num_citizens * 5 / 200);
  w = w  / i;
  QRect source_rect(0, 0, w, h);
  QRect dest_rect(0, 0, w, h);
  width = w * num_citizens;
  height = h;

  if (citizen_pixmap) {
    delete citizen_pixmap;
  }

  citizen_pixmap = new QPixmap(width, height);

  for (j = 0, i = 0; i < num_citizens; i++, j++) {
    dest_rect.moveTo(i * w, 0);
    pix = get_citizen_sprite(tileset, categories[j], j, pcity)->pm;
    p.begin(citizen_pixmap);
    p.drawPixmap(dest_rect, *pix, source_rect);
    p.end();
  }

  citizens_label->set_city(pcity);
  citizens_label->setPixmap(*citizen_pixmap);

  lab_table[FEELING_FINAL]->setPixmap(*citizen_pixmap);
  lab_table[FEELING_FINAL]->setToolTip(text_happiness_wonders(pcity));

  for (int k = 0; k < FEELING_LAST - 1; k++) {
    lab_table[k]->set_city(pcity);
    num_citizens = get_city_citizen_types(pcity,
                                          static_cast<citizen_feeling>(k),
                                          categories);

    for (j = 0, i = 0; i < num_citizens; i++, j++) {
      dest_rect.moveTo(i * w, 0);
      pix = get_citizen_sprite(tileset, categories[j], j, pcity)->pm;
      p.begin(citizen_pixmap);
      p.drawPixmap(dest_rect, *pix, source_rect);
      p.end();
    }

    lab_table[k]->setPixmap(*citizen_pixmap);

    switch (k) {
    case FEELING_BASE:
      lab_table[k]->setToolTip(text_happiness_cities(pcity));
      break;

    case FEELING_LUXURY:
      lab_table[k]->setToolTip(text_happiness_luxuries(pcity));
      break;

    case FEELING_EFFECT :
      lab_table[k]->setToolTip(text_happiness_buildings(pcity));
      break;

    case FEELING_NATIONALITY:
      lab_table[k]->setToolTip(text_happiness_nationality(pcity));
      break;

    case FEELING_MARTIAL:
      lab_table[k]->setToolTip(text_happiness_units(pcity));
      break;

    default:
      break;
    }
  }
}

/****************************************************************************
  Various refresh after getting new info/reply from server
****************************************************************************/
void city_dialog::refresh()
{
  setUpdatesEnabled(false);
  production_combo_p->blockSignals(true);

  if (pcity) {
    view->set_pixmap(pcity);
    view->update();
    update_title();
    update_info_label();
    update_buy_button();
    update_citizens();
    update_building();
    update_improvements();
    update_units();
    update_nation_table();
    update_cma_tab();
    update_disabled();
  } else {
    destroy_city_dialog();
  }

  production_combo_p->blockSignals(false);
  setUpdatesEnabled(true);
  updateGeometry();
  update();
}


/****************************************************************************
  Updates nationality table in happiness tab
****************************************************************************/
void city_dialog::update_nation_table()
{
  QFont f = QApplication::font();
  QFontMetrics fm(f);
  QPixmap *pix = NULL;
  QPixmap pix_scaled;
  QString str;
  QStringList info_list;
  QTableWidgetItem *item;
  char buf[8];
  citizens nationality_i;
  int h;
  int i = 0;
  struct sprite *sprite;

  h = fm.height() + 6;
  nationality_table->clear();
  nationality_table->setRowCount(0);
  info_list.clear();
  info_list << _("#") << _("Flag") << _("Nation");
  nationality_table->setHorizontalHeaderLabels(info_list);

  citizens_iterate(pcity, pslot, nationality) {
    nationality_table->insertRow(i);

    for (int j = 0; j < nationality_table->columnCount(); j++) {
      item = new QTableWidgetItem;

      switch (j) {
      case 0:
        nationality_i = citizens_nation_get(pcity, pslot);

        if (nationality_i == 0) {
          str = "-";
        } else {
          fc_snprintf(buf, sizeof(buf), "%d", nationality_i);
          str = QString(buf);
        }

        item->setText(str);
        break;

      case 1:
        sprite = get_nation_flag_sprite(tileset,
                                        nation_of_player
                                        (player_slot_get_player(pslot)));

        if (sprite != NULL) {
          pix = sprite->pm;
          pix_scaled = pix->scaledToHeight(h);
          item->setData(Qt::DecorationRole, pix_scaled);
        } else {
          item->setText("FLAG MISSING");
        }
        break;

      case 2:
        item->setText(nation_adjective_for_player
                      (player_slot_get_player(pslot)));
        break;

      default:
        break;
      }
      nationality_table->setItem(i, j, item);
    }
    i++;
  } citizens_iterate_end;
  nationality_table->horizontalHeader()->setStretchLastSection(false);
  nationality_table->resizeColumnsToContents();
  nationality_table->resizeRowsToContents();
  nationality_table->horizontalHeader()->setStretchLastSection(true);
}

/****************************************************************************
  Updates information label ( food, prod ... surpluses ...)
****************************************************************************/
void city_dialog::update_info_label()
{
  int illness = 0;
  char buffer[512];
  char buf[2 * NUM_INFO_FIELDS][512];
  int granaryturns;

  enum { FOOD = 0, SHIELD = 2, TRADE = 4, GOLD = 6, LUXURY = 8, SCIENCE = 10,
         GRANARY = 12, GROWTH = 14, CORRUPTION = 16, WASTE = 18,
         CULTURE = 20, POLLUTION = 22, ILLNESS = 24
       };

  /* fill the buffers with the necessary info */
  fc_snprintf(buf[FOOD], sizeof(buf[FOOD]), "%3d (%+4d)",
              pcity->prod[O_FOOD], pcity->surplus[O_FOOD]);
  fc_snprintf(buf[SHIELD], sizeof(buf[SHIELD]), "%3d (%+4d)",
              pcity->prod[O_SHIELD] + pcity->waste[O_SHIELD],
              pcity->surplus[O_SHIELD]);
  fc_snprintf(buf[TRADE], sizeof(buf[TRADE]), "%3d (%+4d)",
              pcity->surplus[O_TRADE] + pcity->waste[O_TRADE],
              pcity->surplus[O_TRADE]);
  fc_snprintf(buf[GOLD], sizeof(buf[GOLD]), "%3d (%+4d)",
              pcity->prod[O_GOLD], pcity->surplus[O_GOLD]);
  fc_snprintf(buf[LUXURY], sizeof(buf[LUXURY]), "%3d",
              pcity->prod[O_LUXURY]);
  fc_snprintf(buf[SCIENCE], sizeof(buf[SCIENCE]), "%3d",
              pcity->prod[O_SCIENCE]);
  fc_snprintf(buf[GRANARY], sizeof(buf[GRANARY]), "%4d/%-4d",
              pcity->food_stock, city_granary_size(city_size_get(pcity)));

  get_city_dialog_output_text(pcity, O_FOOD, buf[FOOD + 1],
                              sizeof(buf[FOOD + 1]));
  get_city_dialog_output_text(pcity, O_SHIELD, buf[SHIELD + 1],
                              sizeof(buf[SHIELD + 1]));
  get_city_dialog_output_text(pcity, O_TRADE, buf[TRADE + 1],
                              sizeof(buf[TRADE + 1]));
  get_city_dialog_output_text(pcity, O_GOLD, buf[GOLD + 1],
                              sizeof(buf[GOLD + 1]));
  get_city_dialog_output_text(pcity, O_SCIENCE, buf[SCIENCE + 1],
                              sizeof(buf[SCIENCE + 1]));
  get_city_dialog_output_text(pcity, O_LUXURY, buf[LUXURY + 1],
                              sizeof(buf[LUXURY + 1]));
  get_city_dialog_culture_text(pcity, buf[CULTURE + 1],
                               sizeof(buf[CULTURE + 1]));
  get_city_dialog_pollution_text(pcity, buf[POLLUTION + 1],
                                 sizeof(buf[POLLUTION + 1]));
  get_city_dialog_illness_text(pcity, buf[ILLNESS + 1],
                               sizeof(buf[ILLNESS + 1]));

  granaryturns = city_turns_to_grow(pcity);

  if (granaryturns == 0) {
    /* TRANS: city growth is blocked.  Keep short. */
    fc_snprintf(buf[GROWTH], sizeof(buf[GROWTH]), _("blocked"));
  } else if (granaryturns == FC_INFINITY) {
    /* TRANS: city is not growing.  Keep short. */
    fc_snprintf(buf[GROWTH], sizeof(buf[GROWTH]), _("never"));
  } else {
    /* A negative value means we'll have famine in that many turns.
       But that's handled down below. */
    /* TRANS: city growth turns.  Keep short. */
    fc_snprintf(buf[GROWTH], sizeof(buf[GROWTH]),
                PL_("%d turn", "%d turns", abs(granaryturns)),
                abs(granaryturns));
  }

  fc_snprintf(buf[CORRUPTION], sizeof(buf[CORRUPTION]), "%4d",
              pcity->waste[O_TRADE]);
  fc_snprintf(buf[WASTE], sizeof(buf[WASTE]), "%4d", pcity->waste[O_SHIELD]);
  fc_snprintf(buf[CULTURE], sizeof(buf[CULTURE]), "%4d",
              pcity->client.culture);
  fc_snprintf(buf[POLLUTION], sizeof(buf[POLLUTION]), "%4d",
              pcity->pollution);

  if (!game.info.illness_on) {
    fc_snprintf(buf[ILLNESS], sizeof(buf[ILLNESS]), " -.-");
  } else {
    illness = city_illness_calc(pcity, NULL, NULL, NULL, NULL);
    /* illness is in tenth of percent */
    fc_snprintf(buf[ILLNESS], sizeof(buf[ILLNESS]), "%4.1f",
                (float) illness / 10.0);
  }

  get_city_dialog_output_text(pcity, O_FOOD, buffer, sizeof(buffer));

  for (int i = 0; i < NUM_INFO_FIELDS; i++) {
    int j = 2 * i;

    qlt[i]->setText(QString(buf[2 * i]));

    if (j != GROWTH && j != GRANARY && j != WASTE && j != CORRUPTION) {
      qlt[i]->setToolTip(QString(buf[2 * i + 1]));
    }
  }
}

/****************************************************************************
  Setups whole city dialog, public function
****************************************************************************/
void city_dialog::setup_ui(struct city *qcity)
{
  QPixmap q_pix = *get_icon_sprite(tileset, ICON_CITYDLG)->pm;
  QIcon q_icon =::QIcon(q_pix);

  setContentsMargins(0, 0 ,0 ,0);
  setWindowIcon(q_icon);
  pcity = qcity;
  production_combo_p->blockSignals(true);
  refresh();
  production_combo_p->blockSignals(false);

}


/****************************************************************************
  Removes selected item from city worklist
****************************************************************************/
void city_dialog::delete_prod()
{
  display_worklist_menu(QCursor::pos());
}

/****************************************************************************
  Double clicked item in worklist table in production tab
****************************************************************************/
void city_dialog::dbl_click_p(QTableWidgetItem *item)
{
  struct worklist queue;
  city_get_queue(pcity, &queue);

  if (selected_row_p < 0 || selected_row_p > worklist_length(&queue)) {
    return;
  }

  worklist_remove(&queue, selected_row_p);
  city_set_queue(pcity, &queue);
}

/****************************************************************************
  Updates layouts for supported and present units in city
****************************************************************************/
void city_dialog::update_units()
{
  unit_item *ui;
  struct unit_list *units;
  char buf[256];
  int n;
  int happy_cost;
  int free_unhappy = get_city_bonus(pcity, EFT_MAKE_CONTENT_MIL);
  supported_units->setUpdatesEnabled(false);
  supported_units->clear_layout();

  if (NULL != client.conn.playing
      && city_owner(pcity) != client.conn.playing) {
    units = pcity->client.info_units_supported;
  } else {
    units = pcity->units_supported;
  }

  unit_list_iterate(units, punit) {
    happy_cost = city_unit_unhappiness(punit, &free_unhappy);
    ui = new unit_item(this, punit, true, happy_cost);
    ui->init_pix();
    supported_units->add_item(ui);
  } unit_list_iterate_end;
  n = unit_list_size(units);
  fc_snprintf(buf, sizeof(buf), _("Supported units %d"), n);
  supp_units->setTitle(QString(buf));
  supported_units->update_units();
  supported_units->setUpdatesEnabled(true);
  current_units->setUpdatesEnabled(true);
  current_units->clear_layout();

  if (NULL != client.conn.playing
      && city_owner(pcity) != client.conn.playing) {
    units = pcity->client.info_units_present;
  } else {
    units = pcity->tile->units;
  }

  unit_list_iterate(units, punit) {
    ui = new unit_item(this , punit, false);
    ui->init_pix();
    current_units->add_item(ui);
  } unit_list_iterate_end;

  n = unit_list_size(units);
  fc_snprintf(buf, sizeof(buf), _("Present units %d"), n);
  curr_units->setTitle(QString(buf));

  current_units->update_units();
  current_units->setUpdatesEnabled(true);
}

/****************************************************************************
  Selection changed in production tab, in worklist tab
****************************************************************************/
void city_dialog::item_selected(const QItemSelection &sl,
                                const QItemSelection &ds)
{
  QModelIndex index;
  QModelIndexList indexes = sl.indexes();

  if (indexes.isEmpty()) {
    return;
  }

  index = indexes.at(0);
  selected_row_p = index.row();
  update_prod_buttons();
}

/****************************************************************************
  Changes city_dialog to next city after pushing next city button
****************************************************************************/
void city_dialog::next_city()
{
  int size, i, j;
  struct city *other_pcity = NULL;

  if (NULL == client.conn.playing) {
    return;
  }

  size = city_list_size(client.conn.playing->cities);

  if (size == 1) {
    return;
  }

  for (i = 0; i < size; i++) {
    if (pcity == city_list_get(client.conn.playing->cities, i)) {
      break;
    }
  }

  for (j = 1; j < size; j++) {
    other_pcity = city_list_get(client.conn.playing->cities,
                                (i + j + size) % size);
  }
  center_tile_mapcanvas(other_pcity->tile);
  qtg_real_city_dialog_popup(other_pcity);
}

/****************************************************************************
  Changes city_dialog to previous city after pushing prev city button
****************************************************************************/
void city_dialog::prev_city()
{
  int size, i, j;
  struct city *other_pcity = NULL;

  if (NULL == client.conn.playing) {
    return;
  }

  size = city_list_size(client.conn.playing->cities);

  if (size == 1) {
    return;
  }

  for (i = 0; i < size; i++) {
    if (pcity == city_list_get(client.conn.playing->cities, i)) {
      break;
    }
  }

  for (j = 1; j < size; j++) {
    other_pcity = city_list_get(client.conn.playing->cities,
                                (i - j + size) % size);
  }

  center_tile_mapcanvas(other_pcity->tile);
  qtg_real_city_dialog_popup(other_pcity);
}

/****************************************************************************
  Updates building improvement/unit
****************************************************************************/
void city_dialog::update_building()
{
  char buf[32];
  QString str;
  int cost = city_production_build_shield_cost(pcity);

  get_city_dialog_production(pcity, buf, sizeof(buf));
  production_combo_p->setRange(0, cost);

  if (pcity->shield_stock >= cost) {
    production_combo_p->setValue(cost);
  } else {
    production_combo_p->setValue(pcity->shield_stock);
  }

  production_combo_p->setAlignment(Qt::AlignCenter);
  str = QString(buf);
  str = str.simplified();

  production_combo_p->setFormat(QString("(%p%) %2\n%1")
                                .arg(city_production_name_translation(pcity),
                                     str));

  production_combo_p->updateGeometry();

}

/****************************************************************************
  Buy button. Shows message box asking for confirmation
****************************************************************************/
void city_dialog::buy()
{
  char buf[1024], buf2[1024];
  int ret;
  const char *name = city_production_name_translation(pcity);
  int value = city_production_buy_gold_cost(pcity);
  hud_message_box ask(city_dlg);

  if (!can_client_issue_orders()) {
    return;
  }

  fc_snprintf(buf2, ARRAY_SIZE(buf2), PL_("Treasury contains %d gold.",
                                        "Treasury contains %d gold.",
                                        client_player()->economic.gold),
              client_player()->economic.gold);
  fc_snprintf(buf, ARRAY_SIZE(buf), PL_("Buy %s for %d gold?",
                                        "Buy %s for %d gold?", value),
              name, value);
  ask.set_text_title(QString(buf), QString(buf2));
  ask.setStandardButtons(QMessageBox::Cancel | QMessageBox::Ok);
  ask.setDefaultButton(QMessageBox::Cancel);
  ret = ask.exec();

  switch (ret) {
  case QMessageBox::Cancel:
    return;
    break;

  case QMessageBox::Ok:
    city_buy_production(pcity);
    break;
  }
}

/****************************************************************************
  Updates list of improvements
****************************************************************************/
void city_dialog::update_improvements()
{
  QFont f = QApplication::font();
  QFontMetrics fm(f);
  QPixmap *pix = NULL;
  QPixmap pix_scaled;
  QString str;
  QTableWidgetItem *qitem;
  struct sprite *sprite;
  int h, cost, item, targets_used, col, upkeep;
  struct item items[MAX_NUM_PRODUCTION_TARGETS];
  struct universal targets[MAX_NUM_PRODUCTION_TARGETS];
  struct worklist queue;
  QSize size;
  impr_item *ui;

  upkeep = 0;
  city_buildings->setUpdatesEnabled(false);
  city_buildings->clear_layout();

  h = fm.height() + 6;
  targets_used = collect_already_built_targets(targets, pcity);
  name_and_sort_items(targets, targets_used, items, false, pcity);

  for (item = 0; item < targets_used; item++) {
    struct universal target = items[item].item;

    ui = new impr_item(this, target.value.building, pcity);
    ui->init_pix();
    city_buildings->add_item(ui);

    fc_assert_action(VUT_IMPROVEMENT == target.kind, continue);
    sprite = get_building_sprite(tileset, target.value.building);
    upkeep = upkeep + target.value.building->upkeep;
    if (sprite != NULL) {
      pix = sprite->pm;
      pix_scaled = pix->scaledToHeight(h);
    }
  }

  city_get_queue(pcity, &queue);
  p_table_p->setRowCount(worklist_length(&queue));

  for (int i = 0; i < worklist_length(&queue); i++) {
    struct universal target = queue.entries[i];

    if (VUT_UTYPE == target.kind) {
      str = utype_values_translation(target.value.utype);
      cost = utype_build_shield_cost(target.value.utype);
      sprite = get_unittype_sprite(tileset, target.value.utype,
                                   direction8_invalid(), true);
    } else {
      str = city_improvement_name_translation(pcity, target.value.building);
      sprite = get_building_sprite(tileset, target.value.building);

      if (improvement_has_flag(target.value.building, IF_GOLD)) {
        cost = -1;
      } else {
        cost = impr_build_shield_cost(target.value.building);
      }
    }

    for (col = 0; col < 3; col++) {
      qitem = new QTableWidgetItem();

      switch (col) {
      case 0:
        if (sprite) {
          pix = sprite->pm;
          pix_scaled = pix->scaledToHeight(h);
          qitem->setData(Qt::DecorationRole, pix_scaled);
        }
        break;

      case 1:
        qitem->setText(str);
        break;

      case 2:
        qitem->setTextAlignment(Qt::AlignRight);
        qitem->setText(QString::number(cost));
        break;
      }
      p_table_p->setItem(i, col, qitem);
    }
  }

  p_table_p->horizontalHeader()->setStretchLastSection(false);
  p_table_p->resizeColumnsToContents();
  p_table_p->resizeRowsToContents();
  p_table_p->horizontalHeader()->setStretchLastSection(true);

  city_buildings->update_buildings();
  city_buildings->setUpdatesEnabled(true);
  city_buildings->setUpdatesEnabled(true);

  curr_impr->setTitle(QString(_("Improvements - upkeep %1")).arg(upkeep));
}

/****************************************************************************
  Slot executed when user changed production in customized table widget
****************************************************************************/
void city_dialog::production_changed(int index)
{
  cid id;
  QVariant qvar;

  if (can_client_issue_orders()) {
    struct universal univ;

    id = qvar.toInt();
    univ = cid_production(id);
    city_change_production(pcity, &univ);
  }
}

/****************************************************************************
  Shows customized table widget with available items to produce
  Shows default targets in overview city page
****************************************************************************/
void city_dialog::show_targets()
{
  production_widget *pw;
  int when = 1;
  pw = new production_widget(this, pcity, future_targets->isChecked(),
                             when, selected_row_p, show_units->isChecked(),
                             false, show_wonders->isChecked(),
                             show_buildings->isChecked());
  pw->show();
}

/****************************************************************************
  Shows customized table widget with available items to produce
  Shows customized targets in city production page
****************************************************************************/
void city_dialog::show_targets_worklist()
{
  production_widget *pw;
  int when = 4;
  pw = new production_widget(this, pcity, future_targets->isChecked(),
                             when, selected_row_p, show_units->isChecked(),
                             false, show_wonders->isChecked(),
                             show_buildings->isChecked());
  pw->show();
}

/****************************************************************************
  Clears worklist in production page
****************************************************************************/
void city_dialog::clear_worklist()
{
  struct worklist empty;

  if (!can_client_issue_orders()) {
    return;
  }

  worklist_init(&empty);
  city_set_worklist(pcity, &empty);
}

/****************************************************************************
  Move current item on worklist up
****************************************************************************/
void city_dialog::worklist_up()
{
  QModelIndex index;
  struct worklist queue;
  struct universal *target = new universal;
  QVariant qvar;

  if (selected_row_p < 1 || selected_row_p >= p_table_p->rowCount()) {
    return;
  }

  city_get_queue(pcity, &queue);
  worklist_peek_ith(&queue, target, selected_row_p);
  worklist_remove(&queue, selected_row_p);
  worklist_insert(&queue, target, selected_row_p - 1);
  city_set_queue(pcity, &queue);
  index = p_table_p->model()->index(selected_row_p - 1, 0);
  p_table_p->setCurrentIndex(index);
  delete target;

}

/****************************************************************************
  Remove current item on worklist
****************************************************************************/
void city_dialog::worklist_del()
{
  QTableWidgetItem *item;

  if (selected_row_p < 0
      || selected_row_p >= p_table_p->rowCount()) {
    return;
  }

  item = p_table_p->item(selected_row_p, 0);
  dbl_click_p(item);
  update_prod_buttons();
}

/****************************************************************************
  Move current item on worklist down
****************************************************************************/
void city_dialog::worklist_down()
{
  QModelIndex index;
  struct worklist queue;
  struct universal *target = new universal;
  QVariant qvar;

  if (selected_row_p < 0 || selected_row_p >= p_table_p->rowCount() - 1) {
    return;
  }

  city_get_queue(pcity, &queue);
  worklist_peek_ith(&queue, target, selected_row_p);
  worklist_remove(&queue, selected_row_p);
  worklist_insert(&queue, target, selected_row_p + 1);
  city_set_queue(pcity, &queue);
  index = p_table_p->model()->index(selected_row_p + 1, 0);
  p_table_p->setCurrentIndex(index);
  delete target;
}

/****************************************************************************
  Save worklist
****************************************************************************/
void city_dialog::save_worklist()
{
  struct worklist queue;
  struct global_worklist *gw;
  QString text;
  hud_input_box ask(gui()->central_wdg);

  ask.set_text_title_definput(_("What should we name new worklist?"),
                              _("Save current worklist"),
                              _("New worklist"));
  if (ask.exec() == QDialog::Accepted) {
    text = ask.input_edit.text().toLocal8Bit().data();
    if (!text.isEmpty()) {
      gw = global_worklist_new(text.toLocal8Bit().data());
      city_get_queue(pcity, &queue);
      global_worklist_set(gw, &queue);
    }
  }
}

/****************************************************************************
  Puts city name and people count on title
****************************************************************************/
void city_dialog::update_title()
{
  QString buf;

  lcity_name->setText(QString(city_name_get(pcity)));

  if (city_unhappy(pcity)) {
    /* TRANS: city dialog title */
    buf = QString(_("%1 - %2 citizens - DISORDER")).arg(city_name_get(pcity),
          population_to_text(city_population(pcity)));
  } else if (city_celebrating(pcity)) {
    /* TRANS: city dialog title */
    buf = QString(_("%1 - %2 citizens - celebrating")).arg(city_name_get(pcity),
          population_to_text(city_population(pcity)));
  } else if (city_happy(pcity)) {
    /* TRANS: city dialog title */
    buf = QString(_("%1 - %2 citizens - happy")).arg(city_name_get(pcity),
          population_to_text(city_population(pcity)));
  } else {
    /* TRANS: city dialog title */
    buf = QString(_("%1 - %2 citizens")).arg(city_name_get(pcity),
          population_to_text(city_population(pcity)));
  }

  setWindowTitle(buf);
}

/**************************************************************************
  Pop up (or bring to the front) a dialog for the given city.  It may or
  may not be modal.
**************************************************************************/
void qtg_real_city_dialog_popup(struct city *pcity)
{
  if (!::city_dlg_created) {
    ::city_dlg = new city_dialog(gui()->mapview_wdg);
  }

  city_dlg->setup_ui(pcity);
  city_dlg->show();
  city_dlg->activateWindow();
  city_dlg->raise();
}

/****************************************************************************
  Closes city dialog
****************************************************************************/
void destroy_city_dialog()
{
  if (!::city_dlg_created) {
    return;
  }

  city_dlg->close();
  ::city_dlg_created = false;
}


/**************************************************************************
  Close the dialog for the given city.
**************************************************************************/
void qtg_popdown_city_dialog(struct city *pcity)
{
  if (!::city_dlg_created) {
    return;
  }

  destroy_city_dialog();
}

/**************************************************************************
  Close the dialogs for all cities.
**************************************************************************/
void qtg_popdown_all_city_dialogs()
{
  destroy_city_dialog();
}

/**************************************************************************
  Refresh (update) all data for the given city's dialog.
**************************************************************************/
void qtg_real_city_dialog_refresh(struct city *pcity)
{
  if (!::city_dlg_created) {
    return;
  }

  if (qtg_city_dialog_is_open(pcity)) {
    city_dlg->refresh();
  }
}

/**************************************************************************
  Updates city font
**************************************************************************/
void city_font_update()
{
  QList<QLabel *> l;
  QFont *f;

  if (!::city_dlg_created) {
    return;
  }

  l = city_dlg->findChildren<QLabel *>();

  f = fc_font::instance()->get_font(fonts::city_label);

  for (int i = 0; i < l.size(); ++i) {
    if (l.at(i)->property(fonts::city_label).isValid()) {
      l.at(i)->setFont(*f);
    }
  }
}
/**************************************************************************
  Update city dialogs when the given unit's status changes.  This
  typically means updating both the unit's home city (if any) and the
  city in which it is present (if any).
**************************************************************************/
void qtg_refresh_unit_city_dialogs(struct unit *punit)
{

  struct city *pcity_sup, *pcity_pre;

  pcity_sup = game_city_by_number(punit->homecity);
  pcity_pre = tile_city(punit->tile);

  qtg_real_city_dialog_refresh(pcity_sup);
  qtg_real_city_dialog_refresh(pcity_pre);

}

/**************************************************************************
  Return whether the dialog for the given city is open.
**************************************************************************/
bool qtg_city_dialog_is_open(struct city *pcity)
{
  if (!::city_dlg_created) {
    return false;
  }

  if (city_dlg->pcity == pcity && city_dlg->isVisible()) {
    return true;
  }

  return false;
}

/**************************************************************************
  Event filter for catching tooltip events
**************************************************************************/
bool fc_tooltip::eventFilter(QObject *obj, QEvent *ev)
{
  QHelpEvent *help_event;
  QString item_tooltip;
  QRect rect;

  if (ev->type() == QEvent::ToolTip) {
    QAbstractItemView *view = qobject_cast<QAbstractItemView *>(obj->parent());

    if (!view) {
      return false;
    }

    help_event = static_cast<QHelpEvent *>(ev);
    QPoint pos = help_event->pos();
    QModelIndex index = view->indexAt(pos);

    if (!index.isValid()) {
      return false;
    }

    item_tooltip = view->model()->data(index, Qt::ToolTipRole).toString();
    rect = view->visualRect(index);
    rect.setX(rect.x() + help_event->globalPos().x());
    rect.setY(rect.y() + help_event->globalPos().y());

    if (!item_tooltip.isEmpty()) {
      QToolTip::showText(help_event->globalPos(), item_tooltip, view, rect);
    } else {
      QToolTip::hideText();
    }

    return true;
  }

  return false;
}

QString bold(QString text)
{
  return QString("<b>" + text + "</b>");
}


/***************************************************************************
  Returns improvement properties to append in tooltip
***************************************************************************/
QString get_tooltip_improvement(impr_type *building)
{
  QString def_str;

  def_str = "<p style='white-space:pre'><b>"
            + QString(improvement_name_translation(building))
            + "</b>\n";
  def_str += QString(_("Cost: %1, Upkeep: %2\n\n"))
             .arg(impr_build_shield_cost(building))
             .arg(building->upkeep);
  return def_str;
}

/***************************************************************************
  Returns unit properties to append in tooltip
***************************************************************************/
QString get_tooltip_unit(struct unit_type *unit)
{
  QString def_str;

  def_str = "<b>" + QString(utype_name_translation(unit)) + "</b>\n";
  def_str += "<table><tr><td>" + bold(QString(_("Attack:"))) + " "
             + QString::number(unit->attack_strength)
             + QString("</td><td>") + bold(QString(_("Defense:"))) + " "
             + QString::number(unit->defense_strength)
             + QString("</td><td>") + bold(QString(_("Move:"))) + " "
             + QString(move_points_text(unit->move_rate, TRUE))
             + QString("</td></tr><tr><td>")
             + bold(QString(_("Cost:"))) + " "
             + QString::number(utype_build_shield_cost(unit))
             + QString("</td><td>") + bold(QString(_("Basic Upkeep:")))
             + " " + QString(helptext_unit_upkeep_str(unit))
             + QString("</td></tr><tr><td>")
             + bold(QString(_("Hitpoints:"))) + " "
             + QString::number(unit->hp)
             + QString("</td><td>") + bold(QString(_("FirePower:"))) + " "
             + QString::number(unit->firepower)
             + QString("</td><td>") + bold(QString(_("Vision:"))) + " "
             + QString::number((int) sqrt((double) unit->vision_radius_sq))
             + QString("</td></tr></table><p style='white-space:pre'>");
  return def_str;
};

/**************************************************************************
  Returns shortened help for given universal ( stored in qvar )
**************************************************************************/
QString get_tooltip(QVariant qvar)
{
  QString str, def_str, ret_str;
  QStringList sl;
  char buffer[8192];
  char buf2[1];

  buf2[0] = '\0';
  struct universal *target;
  target = reinterpret_cast<universal *>(qvar.value<void *>());

  if (target == NULL) {
  } else if (VUT_UTYPE == target->kind) {
    def_str = get_tooltip_unit(target->value.utype);
    str = helptext_unit(buffer, sizeof(buffer), client.conn.playing,
                        buf2, target->value.utype);
  } else {
    if (!improvement_has_flag(target->value.building, IF_GOLD)) {
      def_str = get_tooltip_improvement(target->value.building);
    }

    str = helptext_building(buffer, sizeof(buffer), client.conn.playing,
                            NULL, target->value.building);
  }

  /* Remove all lines from help which has '*' in first 3 chars */
  ret_str = cut_helptext(str);
  ret_str = split_text(ret_str, true);
  ret_str = ret_str.trimmed();
  ret_str = def_str + ret_str;

  return ret_str;
}

/***************************************************************************
  City item delegate constructor
***************************************************************************/
city_production_delegate::city_production_delegate(QPoint sh,
    QObject *parent,
    struct city *city)
  : QItemDelegate(parent)
{
  pd = sh;
  item_height = sh.y();
  pcity = city;
}

/***************************************************************************
  City item delgate paint event
***************************************************************************/
void city_production_delegate::paint(QPainter *painter,
                                     const QStyleOptionViewItem &option,
                                     const QModelIndex &index) const
{
  struct universal *target;
  QString name;
  QVariant qvar;
  QPixmap *pix;
  QPixmap pix_scaled;
  QRect rect1;
  QRect rect2;
  struct sprite *sprite;
  bool useless = false;
  bool is_coinage = false;
  bool is_neutral = false;
  bool is_sea = false;
  bool is_flying = false;
  bool is_unit = true;
  QPixmap pix_dec(option.rect.width(), option.rect.height());
  QStyleOptionViewItem opt;
  color col;
  QIcon icon = qapp->style()->standardIcon(QStyle::SP_DialogCancelButton);
  bool free_sprite = false;

  if (!option.rect.isValid()) {
    return;
  }

  qvar = index.data();

  if (qvar.isNull()) {
    return;
  }

  target = reinterpret_cast<universal *>(qvar.value<void *>());

  if (target == NULL) {
    col.qcolor = Qt::white;
    sprite = qtg_create_sprite(100, 100, &col);
    free_sprite = true;
    *sprite->pm = icon.pixmap(100, 100);
    name = _("Cancel");
    is_unit = false;
  } else if (VUT_UTYPE == target->kind) {
    name = utype_name_translation(target->value.utype);
    is_neutral = utype_has_flag(target->value.utype, UTYF_CIVILIAN);

    if (utype_move_type(target->value.utype) == UMT_SEA) {
      is_sea = true;
    }

    if (utype_move_type(target->value.utype) == UMT_BOTH) {
      is_flying = true;
    }

    sprite = get_unittype_sprite(tileset, target->value.utype,
                                 direction8_invalid(), true);
  } else {
    is_unit = false;
    name = improvement_name_translation(target->value.building);
    sprite = get_building_sprite(tileset, target->value.building);
    useless = is_improvement_redundant(pcity, target->value.building);
    is_coinage = improvement_has_flag(target->value.building, IF_GOLD);
  }

  if (sprite != NULL) {
    pix = sprite->pm;
    pix_scaled = pix->scaledToHeight(item_height - 2);

    if (useless) {
      pixmap_put_x(&pix_scaled);
    }
  }

  opt = QItemDelegate::setOptions(index, option);
  painter->save();
  opt.displayAlignment = Qt::AlignLeft;
  opt.textElideMode = Qt::ElideMiddle;
  QItemDelegate::drawBackground(painter, opt, index);
  rect1 = option.rect;
  rect1.setWidth(pix_scaled.width() + 4);
  rect2 = option.rect;
  rect2.setLeft(option.rect.left() + rect1.width());
  rect2.setTop(rect2.top() + (rect2.height()
                              - painter->fontMetrics().height()) / 2);
  QItemDelegate::drawDisplay(painter, opt, rect2, name);

  if (is_unit) {
    if (is_sea) {
      pix_dec.fill(QColor(0, 0, 255, 80));
    } else if (is_flying) {
      pix_dec.fill(QColor(220, 0, 0, 80));
    } else if (is_neutral) {
      pix_dec.fill(QColor(0, 120, 0, 40));
    } else {
      pix_dec.fill(QColor(0, 0, 150, 40));
    }

    QItemDelegate::drawDecoration(painter, option, option.rect, pix_dec);
  }

  if (is_coinage) {
    pix_dec.fill(QColor(255, 255, 0, 70));
    QItemDelegate::drawDecoration(painter, option, option.rect, pix_dec);
  }

  if (!pix_scaled.isNull()) {
    QItemDelegate::drawDecoration(painter, opt, rect1, pix_scaled);
  }

  drawFocus(painter, opt, option.rect);

  painter->restore();

  if (free_sprite == TRUE) {
    qtg_free_sprite(sprite);
  }
}

/****************************************************************************
  Draws focus for given item
****************************************************************************/
void city_production_delegate::drawFocus(QPainter *painter,
    const QStyleOptionViewItem &option,
    const QRect &rect) const
{
  QPixmap pix(option.rect.width(), option.rect.height());

  if ((option.state & QStyle::State_MouseOver) == 0 || !rect.isValid()) {
    return;
  }

  pix.fill(QColor(50, 50, 50, 50));
  QItemDelegate::drawDecoration(painter, option, option.rect, pix);
}


/***************************************************************************
  Size hint for city item delegate
***************************************************************************/
QSize city_production_delegate::sizeHint(const QStyleOptionViewItem &option,
    const QModelIndex &index) const
{
  QSize s;

  s.setWidth(pd.x());
  s.setHeight(pd.y());
  return s;
}

/****************************************************************************
  Production item constructor
****************************************************************************/
production_item::production_item(struct universal *ptarget,
                                 QObject *parent): QObject()
{
  setParent(parent);
  target = ptarget;
}

/****************************************************************************
  Production item destructor
****************************************************************************/
production_item::~production_item()
{
  /* allocated as renegade in model */
  if (target != NULL) {
    delete target;
  }
}

/****************************************************************************
  Returns stored data
****************************************************************************/
QVariant production_item::data() const
{
  return QVariant::fromValue((void *)target);
}

/****************************************************************************
  Sets data for item, must be declared.
****************************************************************************/
bool production_item::setData()
{
  return false;
}

/****************************************************************************
  Constructor for city production model
****************************************************************************/
city_production_model::city_production_model(struct city *pcity, bool f,
    bool su, bool sw, bool sb,
    QObject *parent)
  : QAbstractListModel(parent)
{
  show_units = su;
  show_wonders = sw;
  show_buildings = sb;
  mcity = pcity;
  future_t = f;
  populate();
}

/****************************************************************************
  Destructor for city production model
****************************************************************************/
city_production_model::~city_production_model()
{
  qDeleteAll(city_target_list);
  city_target_list.clear();
}

/****************************************************************************
  Returns data from model
****************************************************************************/
QVariant city_production_model::data(const QModelIndex &index, int role) const
{
  if (!index.isValid()) return QVariant();

  if (index.row() >= 0 && index.row() < rowCount() && index.column() >= 0
      && index.column() < columnCount()
      && (index.column() + index.row() * 3 < city_target_list.count())) {
    if (role == Qt::ToolTipRole) {
      return get_tooltip(city_target_list[index.row() * 3
                                          + index.column()]->data());
    }

    return city_target_list[index.row() * 3 + index.column()]->data();
  }

  return QVariant();
}

/****************************************************************************
  Fills model with data
****************************************************************************/
void city_production_model::populate()
{
  production_item *pi;
  struct universal targets[MAX_NUM_PRODUCTION_TARGETS];
  struct item items[MAX_NUM_PRODUCTION_TARGETS];
  struct universal *renegade;
  int item, targets_used;
  QString str;
  QFont f = QApplication::font();
  QFontMetrics fm(f);

  sh.setY(fm.height() * 2);
  sh.setX(0);

  qDeleteAll(city_target_list);
  city_target_list.clear();

  targets_used = collect_eventually_buildable_targets(targets, mcity,
                 future_t);
  name_and_sort_items(targets, targets_used, items, false, mcity);

  for (item = 0; item < targets_used; item++) {
    if (future_t || can_city_build_now(mcity, &items[item].item)) {
      renegade = new universal(items[item].item);

      /* renagade deleted in production_item destructor */
      if (VUT_UTYPE == renegade->kind) {
        str = utype_name_translation(renegade->value.utype);
        sh.setX(qMax(sh.x(), fm.width(str)));

        if (show_units == true) {
          pi = new production_item(renegade, this);
          city_target_list << pi;
        }
      } else {
        str = improvement_name_translation(renegade->value.building);
        sh.setX(qMax(sh.x(), fm.width(str)));

        if ((is_wonder(renegade->value.building) && show_wonders)
            || (is_improvement(renegade->value.building) && show_buildings)
            || (improvement_has_flag(renegade->value.building, IF_GOLD))) {
          pi = new production_item(renegade, this);
          city_target_list << pi;
        }
      }
    }
  }

  renegade = NULL;
  pi = new production_item(renegade, this);
  city_target_list << pi;
  sh.setX(2 * sh.y() + sh.x());
  sh.setX(qMin(sh.x(), 250));
}

/****************************************************************************
  Sets data in model
****************************************************************************/
bool city_production_model::setData(const QModelIndex &index,
                                    const QVariant &value, int role)
{
  if (!index.isValid() || role != Qt::DisplayRole || role != Qt::ToolTipRole)
    return false;

  if (index.row() >= 0 && index.row() < rowCount() && index.column() >= 0
      && index.column() < columnCount()) {
    bool change = city_target_list[index.row()]->setData();
    return change;
  }

  return false;
}

/****************************************************************************
  Constructor for production widget
  future - show future targets
  show_units - if to show units
  when - where to insert
  curr - current index to insert
  buy - buy if possible
****************************************************************************/
production_widget::production_widget(QWidget *parent, struct city *pcity,
                                     bool future, int when, int curr,
                                     bool show_units, bool buy,
                                     bool show_wonders,
                                     bool show_buildings): QTableView()
{
  QPoint pos, sh;
  int desk_width = QApplication::desktop()->width();
  int desk_height = QApplication::desktop()->height();
  fc_tt = new fc_tooltip(this);
  setAttribute(Qt::WA_DeleteOnClose);
  setWindowFlags(Qt::Popup);
  verticalHeader()->setVisible(false);
  horizontalHeader()->setVisible(false);
  setProperty("showGrid", false);
  curr_selection = curr;
  sh_units = show_units;
  pw_city = pcity;
  buy_it = buy;
  when_change = when;
  list_model = new city_production_model(pw_city, future, show_units,
                                         show_wonders, show_buildings, this);
  sh = list_model->sh;
  c_p_d = new city_production_delegate(sh, this, pw_city);
  setItemDelegate(c_p_d);
  setModel(list_model);
  viewport()->installEventFilter(fc_tt);
  installEventFilter(this);
  connect(selectionModel(), SIGNAL(selectionChanged(const QItemSelection &,
                                   const QItemSelection &)),
          SLOT(prod_selected(const QItemSelection &,
                             const QItemSelection &)));
  resizeRowsToContents();
  resizeColumnsToContents();
  setFixedWidth(3 * sh.x() + 6);
  setFixedHeight(list_model->rowCount()*sh.y() + 6);

  if (width() > desk_width) {
    setFixedWidth(desk_width);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
  } else {
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  }

  if (height() > desk_height) {
    setFixedHeight(desk_height);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
  } else {
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  }

  pos = QCursor::pos();

  if (pos.x() + width() > desk_width) {
    pos.setX(desk_width - width());
  } else if (pos.x() - width() < 0) {
    pos.setX(0);
  }

  if (pos.y() + height() > desk_height) {
    pos.setY(desk_height - height());
  } else if (pos.y() - height() < 0) {
    pos.setY(0);
  }

  move(pos);
  setMouseTracking(true);
  setFocus();
}

/****************************************************************************
  Mouse press event for production widget
****************************************************************************/
void production_widget::mousePressEvent(QMouseEvent *event)
{
  if (event->button() == Qt::RightButton) {
    close();
    return;
  }

  QAbstractItemView::mousePressEvent(event);
}

/****************************************************************************
  Event filter for production widget
****************************************************************************/
bool production_widget::eventFilter(QObject *obj, QEvent *ev)
{
  QRect pw_rect;
  QPoint br;

  if (obj != this)
    return false;

  if (ev->type() == QEvent::MouseButtonPress) {
    pw_rect.setTopLeft(pos());
    br.setX(pos().x() + width());
    br.setY(pos().y() + height());
    pw_rect.setBottomRight(br);

    if (!pw_rect.contains(QCursor::pos())) {
      close();
    }
  }

  return false;
}

/****************************************************************************
  Changed selection in production widget
****************************************************************************/
void production_widget::prod_selected(const QItemSelection &sl,
                                      const QItemSelection &ds)
{
  QModelIndexList indexes = selectionModel()->selectedIndexes();
  QModelIndex index;
  QVariant qvar;
  struct worklist queue;
  struct universal *target;

  if (indexes.isEmpty() || client_is_observer()) {
    return;
  }

  index = indexes.at(0);
  qvar = index.data(Qt::UserRole);

  if (qvar.isNull()) {
    return;
  }

  target = reinterpret_cast<universal *>(qvar.value<void *>());

  if (target != NULL) {
    city_get_queue(pw_city, &queue);

    switch (when_change) {
    case 0: /* Change current production */
      city_change_production(pw_city, target);

      if (city_can_buy(pw_city) && buy_it) {
        city_buy_production(pw_city);
      }

      break;

    case 1:                 /* Change current (selected on list)*/
      if (curr_selection < 0 || curr_selection > worklist_length(&queue)) {
        city_change_production(pw_city, target);
      } else {
        worklist_remove(&queue, curr_selection);
        worklist_insert(&queue, target, curr_selection);
        city_set_queue(pw_city, &queue);
      }
      break;

    case 2:                 /* Insert before */
      if (curr_selection < 0 || curr_selection > worklist_length(&queue)) {
        curr_selection = 0;
      }

      curr_selection--;
      curr_selection = qMax(0, curr_selection);
      worklist_insert(&queue, target, curr_selection);
      city_set_queue(pw_city, &queue);
      break;

    case 3:                 /* Insert after */
      if (curr_selection < 0 || curr_selection > worklist_length(&queue)) {
        city_queue_insert(pw_city, -1, target);
        break;
      }

      curr_selection++;
      worklist_insert(&queue, target, curr_selection);
      city_set_queue(pw_city, &queue);
      break;

    case 4:                 /* Add last */
      city_queue_insert(pw_city, -1, target);
      break;

    default:
      break;
    }
  }

  close();
  destroy();
}

/****************************************************************************
  Destructor for production widget
****************************************************************************/
production_widget::~production_widget()
{
  delete c_p_d;
  delete list_model;
  viewport()->removeEventFilter(fc_tt);
  removeEventFilter(this);
}

