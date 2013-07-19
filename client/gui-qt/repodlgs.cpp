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

#ifdef HAVE_CONFIG_H
#include <fc_config.h>
#endif

// common
#include "control.h"
#include "research.h"

// client
#include "repodlgs_common.h"
#include "reqtree.h"
#include "sprite.h"
#include "text.h"

// gui-qt
#include "cityrep.h"
#include "qtg_cxxside.h"

#include "repodlgs.h"

/****************************************************************************
  Compare unit_items (used for techs) by name
****************************************************************************/
bool comp_less_than(const qlist_item &q1, const qlist_item &q2)
{
  return (q1.tech_str < q2.tech_str);
}

/****************************************************************************
  Constructor for research diagram
****************************************************************************/
research_diagram::research_diagram(QWidget *parent): QWidget(parent)
{
  req = create_reqtree(client_player(), true);
  get_reqtree_dimensions(req, &width, &height);
  pcanvas = qtg_canvas_create(width, height);
  pcanvas->map_pixmap.fill(Qt::transparent);
}

/****************************************************************************
  Destructor for research diagram
****************************************************************************/
research_diagram::~research_diagram()
{
  qtg_canvas_free(pcanvas);
  destroy_reqtree(req);
}

/****************************************************************************
  Recreates whole diagram and schedules update
****************************************************************************/
void research_diagram::update_reqtree()
{
  destroy_reqtree(req);
  req = create_reqtree(client_player(), true);
  draw_reqtree(req, pcanvas, 0, 0, 0, 0, width, height);
  update();
}

/****************************************************************************
  Mouse handler for research_diagram
****************************************************************************/
void research_diagram::mousePressEvent(QMouseEvent *event)
{
  Tech_type_id tech = get_tech_on_reqtree(req, event->x(), event->y());

  if (event->button() == Qt::LeftButton && can_client_issue_orders()) {
    switch (player_invention_state(client_player(), tech)) {
    case TECH_PREREQS_KNOWN:
      dsend_packet_player_research(&client.conn, tech);
      break;
    case TECH_UNKNOWN:
      dsend_packet_player_tech_goal(&client.conn, tech);
      break;
    case TECH_KNOWN:
      break;
    }
  }
}

/****************************************************************************
  Paint event for research_diagram
****************************************************************************/
void research_diagram::paintEvent(QPaintEvent *event)
{
  QPainter painter;

  painter.begin(this);
  painter.drawPixmap(0, 0, width, height, pcanvas->map_pixmap);
  painter.end();
}

/****************************************************************************
  Returns size of research_diagram
****************************************************************************/
QSize research_diagram::size()
{
  QSize s;

  s.setWidth(width);;
  s.setHeight(height);
  return s;
}

/****************************************************************************
  Consctructor for science_report
****************************************************************************/
science_report::science_report(): QWidget()
{
  QSize size;
  QSizePolicy size_expanding_policy(QSizePolicy::Expanding,
                                    QSizePolicy::Expanding);
  QSizePolicy size_fixed_policy(QSizePolicy::Minimum, QSizePolicy::Minimum);

  goal_combo = new QComboBox();
  info_label = new QLabel();
  progress = new QProgressBar();
  progress_label = new QLabel();
  researching_combo = new QComboBox();
  sci_layout = new QGridLayout();
  res_diag = new research_diagram();
  scroll = new QScrollArea();

  curr_list = NULL;
  goal_list = NULL;
  progress->setTextVisible(true);
  progress_label->setSizePolicy(size_fixed_policy);
  sci_layout->addWidget(progress_label, 0, 0, 1, 8);
  sci_layout->addWidget(researching_combo, 1, 0, 1, 4);
  researching_combo->setSizePolicy(size_fixed_policy);
  sci_layout->addWidget(progress, 1, 5, 1, 4);
  progress->setSizePolicy(size_fixed_policy);
  sci_layout->addWidget(goal_combo, 2, 0, 1, 4);
  goal_combo->setSizePolicy(size_fixed_policy);
  sci_layout->addWidget(info_label, 2, 5, 1, 4);
  info_label->setSizePolicy(size_fixed_policy);

  size = res_diag->size();
  res_diag->setMinimumSize(size);
  scroll->setAutoFillBackground(true);
  scroll->setPalette(QPalette(QColor(215,215,215)));
  scroll->setWidget(res_diag);
  scroll->setSizePolicy(size_expanding_policy);
  sci_layout->addWidget(scroll, 4, 0, 1, 10);

  QObject::connect(researching_combo, SIGNAL(currentIndexChanged(int)),
                   SLOT(current_tech_changed(int)));

  QObject::connect(goal_combo, SIGNAL(currentIndexChanged(int)),
                   SLOT(goal_tech_changed(int)));

  setLayout(sci_layout);

}

/****************************************************************************
  Destructor for science report
  Removes "SCI" string marking it as closed
  And frees given index on list marking it as ready for new widget
****************************************************************************/
science_report::~science_report()
{
  gui()->remove_repo_dlg("SCI");
  gui()->remove_place(index);
}

/****************************************************************************
  Updates science_report and marks it as opened
  It has to be called soon after constructor.
  It could be in constructor but compiler will yell about not used variable
****************************************************************************/
void science_report::init(bool raise)
{
  index = gui()->gimme_place();
  gui()->add_game_tab(this, _("Research"), index);
  if (raise == false) {
    gui()->game_tab_widget->change_color(index, Qt::red);
  } else {
    gui()->game_tab_widget->setCurrentIndex(index);
  }
  gui()->add_repo_dlg("SCI");
  update_report();
}

/****************************************************************************
  Schedules paint event in some qt queue
****************************************************************************/
void science_report::redraw()
{
  update();
}

/****************************************************************************
  Updates all important widgets on science_report
****************************************************************************/
void science_report::update_report()
{

  struct player_research *research = player_research_get(client_player());
  const char *text;
  int total;
  int done = research->bulbs_researched;
  QVariant qvar, qres;
  double not_used;
  QString str;
  qlist_item item;

  fc_assert_ret(NULL != research);

  if (curr_list) {
    delete curr_list;
  }

  if (goal_list) {
    delete goal_list;
  }

  if (research->researching != A_UNSET) {
    total = total_bulbs_required(client.conn.playing);
  } else  {
    total = -1;
  }

  curr_list = new QList<qlist_item>;
  goal_list = new QList<qlist_item>;
  progress_label->setText(science_dialog_text());
  progress_label->setAlignment(Qt::AlignHCenter);
  info_label->setAlignment(Qt::AlignHCenter);
  info_label->setText(get_science_goal_text(research->tech_goal));
  text = get_science_target_text(&not_used);
  str = QString::fromUtf8(text);
  progress->setFormat(str);
  progress->setMinimum(0);
  progress->setMaximum(total);

  if (done <= total) {
    progress->setValue(done);
  } else {
    done = total;
    progress->setValue(done);
  }

  /** Collect all techs which are reachable in the next step. */
  advance_index_iterate(A_FIRST, i) {
    if (TECH_PREREQS_KNOWN == research->inventions[i].state) {
      item.tech_str
      =
        QString::fromUtf8(advance_name_translation(advance_by_number(i)));
      item.id = i;
      curr_list->append(item);
    }
  } advance_index_iterate_end;


  /** Collect all techs which are reachable in next 10 steps. */
  advance_index_iterate(A_FIRST, i) {
    if (player_invention_reachable(client_player(), i, false)
        && TECH_KNOWN != research->inventions[i].state
        && (i == research->tech_goal
            || 10 >= research->inventions[i].num_required_techs)) {
      item.tech_str
         = QString::fromUtf8(advance_name_translation(advance_by_number(i)));
      item.id = i;
      goal_list->append(item);
    }
  } advance_index_iterate_end;

  /** sort both lists */
   qSort(goal_list->begin(), goal_list->end(), comp_less_than);
   qSort(curr_list->begin(), curr_list->end(), comp_less_than);

  /** fill combo boxes */
  researching_combo->blockSignals(true);
  goal_combo->blockSignals(true);

  researching_combo->clear();
  goal_combo->clear();
  for (int i = 0; i < curr_list->count(); i++) {
    qvar = curr_list->at(i).id;
    researching_combo->insertItem(i, curr_list->at(i).tech_str, qvar);
  }

  for (int i = 0; i < goal_list->count(); i++) {
    qvar = goal_list->at(i).id;
    goal_combo->insertItem(i, goal_list->at(i).tech_str, qvar);
  }

  /** set current tech and goal */
  qres = research->researching;

  if (qres == A_UNSET) {
    researching_combo->insertItem(0, _("None"), A_UNSET);
    researching_combo->setCurrentIndex(0);
  } else {
    for (int i = 0; i < researching_combo->count(); i++) {
      qvar = researching_combo->itemData(i);

      if (qvar == qres) {
        researching_combo->setCurrentIndex(i);
      }
    }
  }

  qres = research->tech_goal;

  if (qres == A_UNSET) {
    goal_combo->insertItem(0, _("None"), A_UNSET);
    goal_combo->setCurrentIndex(0);
  } else {
    for (int i = 0; i < goal_combo->count(); i++) {
      qvar = goal_combo->itemData(i);

      if (qvar == qres) {
        goal_combo->setCurrentIndex(i);
      }
    }
  }

  researching_combo->blockSignals(false);
  goal_combo->blockSignals(false);

  update_reqtree();
}

/****************************************************************************
  Calls update for research_diagram
****************************************************************************/
void science_report::update_reqtree()
{
  res_diag->update_reqtree();
}

/****************************************************************************
  Slot used when combo box with current tech changes
****************************************************************************/
void science_report::current_tech_changed(int index)
{
  QVariant qvar;
  qvar = researching_combo->itemData(index);

  if (researching_combo->hasFocus()) {
    if (can_client_issue_orders()) {
      dsend_packet_player_research(&client.conn, qvar.toInt());
    }
  }
}

/****************************************************************************
  Slot used when combo box with goal have been changed
****************************************************************************/
void science_report::goal_tech_changed(int index)
{
  QVariant qvar;
  qvar = goal_combo->itemData(index);

  if (goal_combo->hasFocus()) {
    if (can_client_issue_orders()) {
      dsend_packet_player_tech_goal(&client.conn, qvar.toInt());
    }
  }
}

/**************************************************************************
  Update the science report.
**************************************************************************/
void real_science_report_dialog_update(void)
{
  int i;
  science_report *sci_rep;
  QWidget *w;

  if (gui()->is_repo_dlg_open("SCI")) {
    i = gui()->gimme_index_of("SCI");
    fc_assert(i != -1);
    w = gui()->game_tab_widget->widget(i);
    sci_rep = reinterpret_cast<science_report*>(w);
    sci_rep->update_report();
  }
}

/**************************************************************************
  Constructor for units report
**************************************************************************/
units_report::units_report(): QWidget()
{
  QGridLayout *units_layout= new QGridLayout;
  units_widget = new QTableWidget;
  find_button = new QPushButton;
  upgrade_button = new QPushButton;
  QStringList slist;
  slist << _("Unit Type") << Q_("?Upgradable unit [short]:U") << _("In-Prog") 
        << _("Active") << _("Shield") << _("Food") << _("Gold");
  units_widget->setColumnCount(slist.count());
  units_widget->setHorizontalHeaderLabels(slist);
  units_widget->setProperty("showGrid", "false");
  units_widget->setProperty("selectionBehavior", "SelectRows");
  units_widget->setEditTriggers(QAbstractItemView::NoEditTriggers);
  units_widget->horizontalHeader()->resizeSections(QHeaderView::
                                                   ResizeToContents);
  units_widget->verticalHeader()->setVisible(false);
  units_widget->setSelectionMode(QAbstractItemView::SingleSelection);
  find_button->setText(_("Find _Nearest"));
  find_button->setEnabled(false);
  upgrade_button->setText(_("Upgrade"));
  upgrade_button->setEnabled(false);
  units_layout->addWidget(units_widget, 1, 0, 5, 5);
  units_layout->addWidget(find_button, 0, 0, 1, 1);
  units_layout->addWidget(upgrade_button, 0, 1, 1, 1);

  connect(find_button, SIGNAL(pressed()), SLOT(find_units()));
  connect(upgrade_button, SIGNAL(pressed()), SLOT(upgrade_units()));
  connect(units_widget->selectionModel(),
          SIGNAL(selectionChanged(const QItemSelection &,
                                  const QItemSelection &)),
          SLOT(selection_changed(const QItemSelection &,
                                 const QItemSelection &)));
  setLayout(units_layout);
}


/****************************************************************************
  Destructor for units report
****************************************************************************/
units_report::~units_report()
{
  gui()->remove_repo_dlg("UNI");
  gui()->remove_place(index);
}

/****************************************************************************
  Updates units_report and marks it as opened
  It has to be called soon after constructor.
****************************************************************************/
void units_report::init()
{
  curr_row = -1;
  index = gui()->gimme_place();
  gui()->add_game_tab(this, _("Units"), index);
  gui()->game_tab_widget->setCurrentIndex(index);
  gui()->add_repo_dlg("UNI");
}

/**************************************************************************
  Updates all widgets in units reports
**************************************************************************/
void units_report::update_report()
{
  struct urd_info {
    int active_count;
    int building_count;
    int upkeep[O_LAST];
  };
  int output;
  int row, column;
  Unit_type_id utype_id;
  QTableWidgetItem *unit_item;
  struct urd_info unit_array[utype_count()];
  struct urd_info unit_totals;
  struct urd_info *info;
  QVariant qvar;

  units_widget->setRowCount(0);
  units_widget->clearContents();
  memset(unit_array, '\0', sizeof(unit_array));
  memset(&unit_totals, '\0', sizeof(unit_totals));
  /* Count units. */
  players_iterate(pplayer) {
    if (client_has_player() && pplayer != client_player()) {
      continue;
    }

    unit_list_iterate(pplayer->units, punit) {
      info = unit_array + utype_index(unit_type(punit));

      if (0 != punit->homecity) {
        for (output = 0; output < O_LAST; output++) {
          info->upkeep[output] += punit->upkeep[output];
        }
      }
      info->active_count++;
    } unit_list_iterate_end;
    city_list_iterate(pplayer->cities, pcity) {
      if (VUT_UTYPE == pcity->production.kind) {
        int num_units;
        info = unit_array + utype_index(pcity->production.value.utype);
        /* Account for build slots in city */
        (void) city_production_build_units(pcity, true, &num_units);
        /* Unit is in progress even if it won't be done this turn */
        num_units = MAX(num_units, 1);
        info->building_count += num_units;
      }
    } city_list_iterate_end;
  } players_iterate_end;
  row = 0;
  unit_type_iterate(utype) {
    utype_id = utype_index(utype);
    info = unit_array + utype_id;

    if (0 == info->active_count && 0 == info->building_count) {
      continue;                 /* We don't need a row for this type. */
    }
    units_widget->setRowCount(row + 1);
    for (column = 0; column < 7; column++) {
      unit_item = new QTableWidgetItem;
      unit_item->setTextAlignment(Qt::AlignHCenter);
      switch (column) {
      case 0:
        unit_item->setTextAlignment(Qt::AlignLeft);
        unit_item->setText(utype_name_translation(utype));
        qvar = utype_id;
        unit_item->setData(Qt::UserRole, qvar);
        break;
      case 1:
        if ((client_has_player()
             && NULL != can_upgrade_unittype(client_player(), utype))) {
          unit_item->setCheckState(Qt::Checked);
        } else {
          unit_item->setCheckState(Qt::Unchecked);
        }
        break;
      case 2:
        unit_item->setText(QString::number(info->building_count));
        break;
      case 3:
        unit_item->setText(QString::number(info->active_count));
        break;
      case 4:
        unit_item->setText(QString::number(info->upkeep[O_SHIELD]));
        break;
      case 5:
        unit_item->setText(QString::number(info->upkeep[O_FOOD]));
        break;
      case 6:
        unit_item->setText(QString::number(info->upkeep[O_GOLD]));
        break;
      }
      units_widget->setItem(row, column, unit_item);
    }
    /* Update totals. */
    unit_totals.active_count += info->active_count;
    for (output = 0; output < O_LAST; output++) {
      unit_totals.upkeep[output] += info->upkeep[output];
    }
    unit_totals.building_count += info->building_count;

    row++;
  } unit_type_iterate_end;
  row++;
  units_widget->setRowCount(row);
  for (column = 0; column < 7; column++) {
    unit_item = new QTableWidgetItem;
    unit_item->setTextAlignment(Qt::AlignHCenter);
    switch (column) {
    case 0:
      unit_item->setTextAlignment(Qt::AlignLeft);
      unit_item->setText(_("Totals:"));
      break;
    case 1:
      unit_item->setCheckState(Qt::Unchecked);
      break;
    case 2:
      unit_item->setText(QString::number(unit_totals.building_count));
      break;
    case 3:
      unit_item->setText(QString::number(unit_totals.active_count));
      break;
    case 4:
      unit_item->setText(QString::number(unit_totals.upkeep[O_SHIELD]));
      break;
    case 5:
      unit_item->setText(QString::number(unit_totals.upkeep[O_FOOD]));
      break;
    case 6:
      unit_item->setText(QString::number(unit_totals.upkeep[O_GOLD]));
      break;
    }
    units_widget->setItem(row - 1, column, unit_item);
  }
  units_widget->resizeRowsToContents();
  max_row = row - 1;
}

/**************************************************************************
  Finds closest unit
**************************************************************************/
struct unit *units_report::find_nearest_unit(const struct unit_type *utype,
                                             struct tile *ptile)
{
  struct unit *best_candidate = NULL;
  int best_dist = FC_INFINITY, dist;

  players_iterate(pplayer) {
    if (client_has_player() && pplayer != client_player()) {
      continue;
    }

    unit_list_iterate(pplayer->units, punit) {
      if (utype == unit_type(punit)
          && FOCUS_AVAIL == punit->client.focus_status
          && 0 < punit->moves_left
          && !punit->done_moving && !punit->ai_controlled) {
        dist = sq_map_distance(unit_tile(punit), ptile);
        if (dist < best_dist) {
          best_candidate = punit;
          best_dist = dist;
        }
      }
    } unit_list_iterate_end;
  } players_iterate_end;
  return best_candidate;
}

/**************************************************************************
  Action for button 'find units' in units report
**************************************************************************/
void units_report::find_units()
{
  struct tile *ptile;
  struct unit *punit;
  struct unit_type *utype;

  utype = utype_by_number(uid);
  ptile = get_center_tile_mapcanvas();
  if ((punit = find_nearest_unit(utype, ptile))) {
    center_tile_mapcanvas(unit_tile(punit));

    if (ACTIVITY_IDLE == punit->activity
        || ACTIVITY_SENTRY == punit->activity) {
      if (can_unit_do_activity(punit, ACTIVITY_IDLE)) {
        unit_focus_set_and_select(punit);
      }
    }
  }
}

/**************************************************************************
  Upgrades selected units after clicking 'upgrade' button
**************************************************************************/
void units_report::upgrade_units()
{

  struct unit_type *utype;
  struct unit_type *upgrade;
  utype = utype_by_number(uid);
  int price;
  QMessageBox ask;
  QString s1, s2;
  int ret;
  char buf[1024];
  char buf2[2048];

  upgrade = can_upgrade_unittype(client_player(), utype);
  price = unit_upgrade_price(client_player(), utype, upgrade);
  fc_snprintf(buf, ARRAY_SIZE(buf), PL_("Treasury contains %d gold.",
                                        "Treasury contains %d gold.",
                                        client_player()->economic.gold),
              client_player()->economic.gold);
  fc_snprintf(buf2, ARRAY_SIZE(buf2),
              PL_("Upgrade as many %s to %s as possible "
                  "for %d gold each?\n%s",
                  "Upgrade as many %s to %s as possible "
                  "for %d gold each?\n%s", price),
              utype_name_translation(utype),
              utype_name_translation(upgrade), price, buf);
  s2 = QString(buf2);
  ask.setText(s2);
  ask.setStandardButtons(QMessageBox::Cancel | QMessageBox::Ok);
  ask.setDefaultButton(QMessageBox::Cancel);
  ask.setWindowTitle(_("Upgrade Obsolete Units"));
  ret = ask.exec();

  switch (ret) {
  case QMessageBox::Cancel:
    return;
  case QMessageBox::Ok:
    dsend_packet_unit_type_upgrade(&client.conn, utype_number(utype));
    return;
  }
}

/**************************************************************************
  Action for changed selection in units report
**************************************************************************/
void units_report::selection_changed(const QItemSelection& sl,
                                     const QItemSelection& ds)
{
  QTableWidgetItem *itm;
  int i;
  QVariant qvar;
  struct unit_type *utype;
  find_button->setEnabled(false);
  upgrade_button->setEnabled(false);

  if (sl.isEmpty()) {
    return;
  }

  curr_row = sl.indexes().at(0).row();
  if (curr_row >= 0 && curr_row < max_row) {
    itm = units_widget->item(curr_row, 0);
    qvar = itm->data(Qt::UserRole);
    uid = qvar.toInt();
    utype = utype_by_number(uid);
    itm = units_widget->item(curr_row, 3);
    i = itm->text().toInt();
    if (i > 0) {
      find_button->setEnabled(true);
    }
    if (can_client_issue_orders()
        && NULL != can_upgrade_unittype(client_player(), utype)) {
      upgrade_button->setEnabled(true);
    }
  }
}

/**************************************************************************
  Constructor for eceonmy report
**************************************************************************/
eco_report::eco_report(): QWidget()
{
  QGridLayout *eco_layout = new QGridLayout;
  eco_widget = new QTableWidget;
  disband_button = new QPushButton;
  sell_button = new QPushButton;
  sell_redun_button = new QPushButton;
  eco_label = new QLabel;

  QStringList slist;
  slist << _("Type") << Q_("?Building or Unit type:Name") << _("Redundant")
      << _("Count") << _("Cost") << _("U Total");
  eco_widget->setColumnCount(slist.count());
  eco_widget->setHorizontalHeaderLabels(slist);
  eco_widget->setProperty("showGrid", "false");
  eco_widget->setProperty("selectionBehavior", "SelectRows");
  eco_widget->setEditTriggers(QAbstractItemView::NoEditTriggers);
  eco_widget->horizontalHeader()->resizeSections(QHeaderView::Stretch);
  eco_widget->verticalHeader()->setVisible(false);
  eco_widget->setSelectionMode(QAbstractItemView::SingleSelection);
  disband_button->setText(_("_Disband"));
  disband_button->setEnabled(false);
  sell_button->setText(_("Sell _All"));
  sell_button->setEnabled(false);
  sell_redun_button->setText(_("Sell _Redundant"));
  sell_redun_button->setEnabled(false);
  eco_layout->addWidget(eco_widget, 1, 0, 5, 5);
  eco_layout->addWidget(disband_button, 0, 0, 1, 1);
  eco_layout->addWidget(sell_button, 0, 1, 1, 1);
  eco_layout->addWidget(sell_redun_button, 0, 2, 1, 1);
  eco_layout->addWidget(eco_label, 6, 0, 1, 5);

  connect(disband_button, SIGNAL(pressed()), SLOT(disband_units()));
  connect(sell_button, SIGNAL(pressed()), SLOT(sell_buildings()));
  connect(sell_redun_button, SIGNAL(pressed()), SLOT(sell_redundant()));
  connect(eco_widget->selectionModel(),
          SIGNAL(selectionChanged(const QItemSelection &,
                                  const QItemSelection &)),
          SLOT(selection_changed(const QItemSelection &,
                                 const QItemSelection &)));
  setLayout(eco_layout);
}

/**************************************************************************
  Destructor for economy report
**************************************************************************/
eco_report::~eco_report()
{
  gui()->remove_repo_dlg("ECO");
  gui()->remove_place(index);
}

/**************************************************************************
  Initializes place in tab for economy report
**************************************************************************/
void eco_report::init()
{
  curr_row = -1;
  index = gui()->gimme_place();
  gui()->add_game_tab(this, _("Economy"), index);
  gui()->game_tab_widget->setCurrentIndex(index);
  gui()->add_repo_dlg("ECO");
}

/**************************************************************************
  Refresh all widgets for economy report
**************************************************************************/
void eco_report::update_report()
{
  struct improvement_entry building_entries[B_LAST];
  struct unit_entry unit_entries[U_LAST];
  int entries_used, building_total, unit_total, tax, i, j;
  char buf[256];
  QTableWidgetItem *item;
  QFont f = QApplication::font();
  int h;
  QFontMetrics fm(f);
  h = fm.height() + 6;
  QPixmap pix_scaled;
  eco_widget->setRowCount(0);
  eco_widget->clearContents();
  get_economy_report_data(building_entries, &entries_used,
                          &building_total, &tax);
  for (i = 0; i < entries_used; i++) {
    struct improvement_entry *pentry = building_entries + i;
    struct impr_type *pimprove = pentry->type;
    QPixmap *pix = get_building_sprite(tileset, pimprove)->pm;
    pix_scaled = pix->scaledToHeight(h);
    cid cid = cid_encode_building(pimprove);

    eco_widget->insertRow(i);
    for (j = 0; j < 6; j++) {
      item = new QTableWidgetItem;
      item->setTextAlignment(Qt::AlignHCenter);
      switch (j) {
      case 0:
        item->setData(Qt::DecorationRole, pix_scaled);
        item->setData(Qt::UserRole, cid);
        break;
      case 1:
        item->setTextAlignment(Qt::AlignLeft);
        item->setText(improvement_name_translation(pimprove));
        break;
      case 2:
        item->setText(QString::number(pentry->redundant));
        break;
      case 3:
        item->setText(QString::number(pentry->count));
        break;
      case 4:
        item->setText(QString::number(pentry->cost));
        break;
      case 5:
        item->setText(QString::number(pentry->total_cost));
        break;
      }
      eco_widget->setItem(i, j, item);
    }
  }
  max_row = i;
  get_economy_report_units_data(unit_entries, &entries_used, &unit_total);
  for (i = 0; i < entries_used; i++) {
    struct unit_entry *pentry = unit_entries + i;
    struct unit_type *putype = pentry->type;
    QPixmap *pix = get_unittype_sprite(tileset, putype,
                                       direction8_invalid(), true)->pm;
    cid cid = cid_encode_unit(putype);

    eco_widget->insertRow(i + max_row);
    for (j = 0; j < 6; j++) {
      item = new QTableWidgetItem;
      item->setTextAlignment(Qt::AlignHCenter);
      switch (j) {
      case 0:
        item->setData(Qt::DecorationRole, *pix);
        item->setData(Qt::UserRole, cid);
        break;
      case 1:
        item->setTextAlignment(Qt::AlignLeft);
        item->setText(utype_name_translation(putype));
        break;
      case 2:
        item->setText(QString::number(0));
        break;
      case 3:
        item->setText(QString::number(pentry->count));
        break;
      case 4:
        item->setText(QString::number(pentry->cost));
        break;
      case 5:
        item->setText(QString::number(pentry->total_cost));
        break;
      }
      eco_widget->setItem(max_row + i, j, item);
    }
  }
  max_row = max_row + i;
  fc_snprintf(buf, sizeof(buf), _("Income: %d    Total Costs: %d"),
              tax, building_total + unit_total);
  eco_label->setText(buf);
}

/**************************************************************************
  Action for selection changed in economy report
**************************************************************************/
void eco_report::selection_changed(const QItemSelection & sl,
                                   const QItemSelection & ds)
{
  QTableWidgetItem *itm;
  int i;
  QVariant qvar;
  struct universal selected;
  struct impr_type *pimprove;
  disband_button->setEnabled(false);
  sell_button->setEnabled(false);
  sell_redun_button->setEnabled(false);

  if (sl.isEmpty()) {
    return;
  }

  curr_row = sl.indexes().at(0).row();
  if (curr_row >= 0 && curr_row <= max_row) {
    itm = eco_widget->item(curr_row, 0);
    qvar = itm->data(Qt::UserRole);
    uid = qvar.toInt();
    selected = cid_decode(uid);
    switch (selected.kind) {
    case VUT_IMPROVEMENT:
      pimprove = selected.value.building;
      counter = eco_widget->item(curr_row, 3)->text().toInt();
      if (can_sell_building(pimprove)) {
        sell_button->setEnabled(true);
      }
      itm = eco_widget->item(curr_row, 2);
      i = itm->text().toInt();
      if (i > 0) {
        sell_redun_button->setEnabled(true);
      }
      break;
    case VUT_UTYPE:
      counter = eco_widget->item(curr_row, 3)->text().toInt();
      disband_button->setEnabled(true);
      break;
    default:
      log_error("Not supported type: %d.", selected.kind);
    }
  }
}

/**************************************************************************
  Disband pointed units (in economy report)
**************************************************************************/
void eco_report::disband_units()
{
  struct universal selected;

  char buf[1024];
  QString s;
  QMessageBox ask;
  int ret;
  selected = cid_decode(uid);
  struct unit_type *putype = selected.value.utype;
  fc_snprintf(buf, ARRAY_SIZE(buf),
              _("Do you really wish to disband "
                "every %s (%d total)?"),
              utype_name_translation(putype), counter);

  s = QString(buf);
  ask.setText(s);
  ask.setStandardButtons(QMessageBox::Cancel | QMessageBox::Ok);
  ask.setDefaultButton(QMessageBox::Cancel);
  ask.setWindowTitle(_("Disband Units"));
  ret = ask.exec();
  switch (ret) {
  case QMessageBox::Cancel:
    return;
  case QMessageBox::Ok:
    disband_all_units(putype, false, buf, sizeof(buf));
    break;
  }
  s = QString(buf);
  ask.setText(s);
  ask.setStandardButtons(QMessageBox::Ok);
  ask.setWindowTitle(_("Disband Results"));
  ask.exec();
}

/**************************************************************************
  Sell all pointed builings
**************************************************************************/
void eco_report::sell_buildings()
{
  struct universal selected;
  char buf[1024];
  QString s;
  QMessageBox ask;
  int ret;
  selected = cid_decode(uid);
  struct impr_type *pimprove = selected.value.building;
  fc_snprintf(buf, ARRAY_SIZE(buf),
              _("Do you really wish to sell "
                "every %s (%d total)?"),
              improvement_name_translation(pimprove), counter);

  s = QString(buf);
  ask.setText(s);
  ask.setStandardButtons(QMessageBox::Cancel | QMessageBox::Ok);
  ask.setDefaultButton(QMessageBox::Cancel);
  ask.setWindowTitle(_("Sell Improvements"));
  ret = ask.exec();
  switch (ret) {
  case QMessageBox::Cancel:
    return;
  case QMessageBox::Ok:
    sell_all_improvements(pimprove, false, buf, sizeof(buf));
    break;
  }
  s = QString(buf);
  ask.setText(s);
  ask.setStandardButtons(QMessageBox::Ok);
  ask.setWindowTitle(_("Sell-Off: Results"));
  ask.exec();
}

/**************************************************************************
  Sells redundant buildings
**************************************************************************/
void eco_report::sell_redundant()
{
  struct universal selected;
  char buf[1024];
  QString s;
  QMessageBox ask;
  int ret;
  selected = cid_decode(uid);
  struct impr_type *pimprove = selected.value.building;
  fc_snprintf(buf, ARRAY_SIZE(buf),
              _("Do you really wish to sell "
                "every redundant %s (%d total)?"),
              improvement_name_translation(pimprove), counter);

  s = QString(buf);
  ask.setText(s);
  ask.setStandardButtons(QMessageBox::Cancel | QMessageBox::Ok);
  ask.setDefaultButton(QMessageBox::Cancel);
  ask.setWindowTitle(_("Sell Improvements"));
  ret = ask.exec();
  switch (ret) {
  case QMessageBox::Cancel:
    return;
  case QMessageBox::Ok:
    sell_all_improvements(pimprove, true, buf, sizeof(buf));
    break;
  }
  s = QString(buf);
  ask.setText(s);
  ask.setStandardButtons(QMessageBox::Ok);
  ask.setWindowTitle(_("Sell-Off: Results"));
  ask.exec();
}

/**************************************************************************
  Constructor for endgame report
**************************************************************************/
endgame_report::endgame_report(const struct packet_endgame_report *packet): QWidget()
{
  QGridLayout *end_layout = new QGridLayout;
  end_widget = new QTableWidget;
  unsigned int i;

  players = 0;
  const size_t col_num = packet->category_num + 3;
  QStringList slist;
  slist << _("Player") << _("Nation") << _("Score");
  for (i = 0 ; i < col_num - 3; i++) {
    slist << Q_(packet->category_name[i]);
  }
  end_widget->setColumnCount(slist.count());
  end_widget->setHorizontalHeaderLabels(slist);
  end_widget->setProperty("showGrid", "false");
  end_widget->setProperty("selectionBehavior", "SelectRows");
  end_widget->setEditTriggers(QAbstractItemView::NoEditTriggers);
  end_widget->verticalHeader()->setVisible(false);
  end_widget->setSelectionMode(QAbstractItemView::SingleSelection);
  end_layout->addWidget(end_widget, 1, 0, 5, 5);
  setLayout(end_layout);

}

/**************************************************************************
  Destructor for endgame report
**************************************************************************/
endgame_report::~endgame_report()
{
  gui()->remove_repo_dlg("END");
  gui()->remove_place(index);
}

/**************************************************************************
  Initializes place in tab for endgame report
**************************************************************************/
void endgame_report::init()
{
  index = gui()->gimme_place();
  gui()->add_game_tab(this, _("Score"), index);
  gui()->game_tab_widget->setCurrentIndex(index);
  gui()->add_repo_dlg("END");
}

/**************************************************************************
  Refresh all widgets for economy report
**************************************************************************/
void endgame_report::update_report(const struct packet_endgame_player *packet)
{
  QTableWidgetItem *item;
  QPixmap *pix;
  unsigned int i;
  const struct player *pplayer = player_by_number(packet->player_id);
  const size_t col_num = packet->category_num + 3;
  end_widget->insertRow(players);
  for (i = 0; i < col_num; i++) {
      item = new QTableWidgetItem;
      switch (i) {
      case 0:
        item->setText(player_name(pplayer));
        break;
      case 1:
        pix = get_nation_flag_sprite(tileset, nation_of_player(pplayer))->pm;
        if (pix != NULL){
        item->setData(Qt::DecorationRole, *pix);
        }
        break;
      case 2:
        item->setText(QString::number(packet->score));
        item->setTextAlignment(Qt::AlignHCenter);
        break;
      default:
        item->setText(QString::number(packet->category_score[i-3]));
        item->setTextAlignment(Qt::AlignHCenter);
        break;
      }
      end_widget->setItem(players, i, item);
  }
  players++;
  end_widget->resizeRowsToContents();
}

/**************************************************************************
  Display the science report.  Optionally raise it.
  Typically triggered by F6.
**************************************************************************/
void science_report_dialog_popup(bool raise)
{

  science_report *sci_rep;
  int i;
  QWidget *w;

  if (client_is_global_observer()){
    return;
  }
  if (!gui()->is_repo_dlg_open("SCI")) {
    sci_rep = new science_report;
    sci_rep->init(raise);
  } else {
    i = gui()->gimme_index_of("SCI");
    fc_assert(i != -1);
    if (gui()->game_tab_widget->currentIndex() == i) {
      return;
    }
    w = gui()->game_tab_widget->widget(i);
    sci_rep = reinterpret_cast<science_report*>(w);
    if (raise == false) {
      gui()->game_tab_widget->change_color(i, Qt::red);
    } else {
    gui()->game_tab_widget->setCurrentWidget(sci_rep);
    }
  }
}

/**************************************************************************
  Update the economy report.
**************************************************************************/
void real_economy_report_dialog_update(void)
{
  int i;
  eco_report *eco_rep;
  QWidget *w;

  if (gui()->is_repo_dlg_open("ECO")) {
    i = gui()->gimme_index_of("ECO");
    fc_assert(i != -1);
    w = gui()->game_tab_widget->widget(i);
    eco_rep = reinterpret_cast<eco_report*>(w);
    eco_rep->update_report();
  }
}

/**************************************************************************
  Display the economy report.  Optionally raise it.
  Typically triggered by F5.
**************************************************************************/
void economy_report_dialog_popup(bool raise)
{
  int i;
  eco_report *eco_rep;
  QWidget *w;
  if (!gui()->is_repo_dlg_open("ECO")) {
    eco_rep = new eco_report;
    eco_rep->init();
    eco_rep->update_report();
  }
  else {
    i = gui()->gimme_index_of("ECO");
    fc_assert(i != -1);
    w = gui()->game_tab_widget->widget(i);
    eco_rep = reinterpret_cast<eco_report*>(w);
    gui()->game_tab_widget->setCurrentWidget(eco_rep);
  }
}

/**************************************************************************
  Update the units report.
**************************************************************************/
void real_units_report_dialog_update(void)
{
  int i;
  units_report *units_rep;
  QWidget *w;

  if (gui()->is_repo_dlg_open("UNI")) {
    i = gui()->gimme_index_of("UNI");
    fc_assert(i != -1);
    w = gui()->game_tab_widget->widget(i);
    units_rep = reinterpret_cast<units_report*>(w);
    units_rep->update_report();
  }
}

/**************************************************************************
  Display the units report.  Optionally raise it.
  Typically triggered by F2.
**************************************************************************/
void units_report_dialog_popup(bool raise)
{
  int i;
  units_report *ur;
  QWidget *w;
    if (!gui()->is_repo_dlg_open("UNI")) {
    units_report *units_rep = new units_report;
    units_rep->init();
    units_rep->update_report();
  } else {
    i = gui()->gimme_index_of("UNI");
    fc_assert(i != -1);
    w = gui()->game_tab_widget->widget(i);
    ur = reinterpret_cast<units_report*>(w);
    gui()->game_tab_widget->setCurrentWidget(ur);
  }
}

/****************************************************************************
  Show a dialog with player statistics at endgame.
****************************************************************************/
void endgame_report_dialog_start(const struct packet_endgame_report *packet)
{
  endgame_report *end_rep;
  end_rep = new endgame_report(packet);
  end_rep->init();
}

/****************************************************************************
  Received endgame report information about single player.
****************************************************************************/
void endgame_report_dialog_player(const struct packet_endgame_player *packet)
{
  int i;
  endgame_report *end_rep;
  QWidget *w;

  if (gui()->is_repo_dlg_open("END")) {
    i = gui()->gimme_index_of("END");
    fc_assert(i != -1);
    w = gui()->game_tab_widget->widget(i);
    end_rep = reinterpret_cast<endgame_report*>(w);
    end_rep->update_report(packet);
  }
}

/****************************************************************************
  Resize and redraw the requirement tree.
****************************************************************************/
void science_report_dialog_redraw(void)
{
  int i;
  science_report *sci_rep;
  QWidget *w;

  if (gui()->is_repo_dlg_open("SCI")) {
    i = gui()->gimme_index_of("SCI");
    fc_assert(i != -1);
    w = gui()->game_tab_widget->widget(i);
    sci_rep = reinterpret_cast<science_report*>(w);
    sci_rep->redraw();
  }
}
