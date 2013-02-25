
[spec]

; Format and options of this spec file:
options = "+Freeciv-spec-Devel-2013.Feb.13"

[info]

; "Karapooze" -- The unit set based on "Cimpletoon" collection of units by YD.

artists = "
    YD
"

[file]
gfx = "cimpletoon/orient_units"

[grid_main]

x_top_left = 1
y_top_left = 1
dx = 64
dy = 48
pixel_border = 1

tiles = { "row", "column", "tag"
  0,  0, "u.settlers_sw"
  0,  1, "u.settlers_w"
  0,  2, "u.settlers_nw"
  0,  3, "u.settlers_n"
  0,  4, "u.settlers_ne"
  0,  5, "u.settlers_e"
  0,  6, "u.settlers_se"
  0,  7, "u.settlers_s"

  1,  0, "u.warriors_sw"
  1,  1, "u.warriors_w"
  1,  2, "u.warriors_nw"
  1,  3, "u.warriors_n"
  1,  4, "u.warriors_ne"
  1,  5, "u.warriors_e"
  1,  6, "u.warriors_se"
  1,  7, "u.warriors_s"

  2,  0, "u.explorer_sw"
  2,  1, "u.explorer_w"
  2,  2, "u.explorer_nw"
  2,  3, "u.explorer_n"
  2,  4, "u.explorer_ne"
  2,  5, "u.explorer_e"
  2,  6, "u.explorer_se"
  2,  7, "u.explorer_s"

  3,  0, "u.worker_sw"
  3,  1, "u.worker_w"
  3,  2, "u.worker_nw"
  3,  3, "u.worker_n"
  3,  4, "u.worker_ne"
  3,  5, "u.worker_e"
  3,  6, "u.worker_se"
  3,  7, "u.worker_s"

  4,  0, "u.horsemen_sw"
  4,  1, "u.horsemen_w"
  4,  2, "u.horsemen_nw"
  4,  3, "u.horsemen_n"
  4,  4, "u.horsemen_ne"
  4,  5, "u.horsemen_e"
  4,  6, "u.horsemen_se"
  4,  7, "u.horsemen_s"

  5,  0, "u.archers_sw"
  5,  1, "u.archers_w"
  5,  2, "u.archers_nw"
  5,  3, "u.archers_n"
  5,  4, "u.archers_ne"
  5,  5, "u.archers_e"
  5,  6, "u.archers_se"
  5,  7, "u.archers_s"

  6,  0, "u.phalanx_sw"
  6,  1, "u.phalanx_w"
  6,  2, "u.phalanx_nw"
  6,  3, "u.phalanx_n"
  6,  4, "u.phalanx_ne"
  6,  5, "u.phalanx_e"
  6,  6, "u.phalanx_se"
  6,  7, "u.phalanx_s"

  7,  0, "u.trireme_sw"
  7,  1, "u.trireme_w"
  7,  2, "u.trireme_nw"
  7,  3, "u.trireme_n"
  7,  4, "u.trireme_ne"
  7,  5, "u.trireme_e"
  7,  6, "u.trireme_se"
  7,  7, "u.trireme_s"

  8,  0, "u.chariot_sw"
  8,  1, "u.chariot_w"
  8,  2, "u.chariot_nw"
  8,  3, "u.chariot_n"
  8,  4, "u.chariot_ne"
  8,  5, "u.chariot_e"
  8,  6, "u.chariot_se"
  8,  7, "u.chariot_s"

  9,  0, "u.catapult_sw"
  9,  1, "u.catapult_w"
  9,  2, "u.catapult_nw"
  9,  3, "u.catapult_n"
  9,  4, "u.catapult_ne"
  9,  5, "u.catapult_e"
  9,  6, "u.catapult_se"
  9,  7, "u.catapult_s"

  10,  0, "u.legion_sw"
  10,  1, "u.legion_w"
  10,  2, "u.legion_nw"
  10,  3, "u.legion_n"
  10,  4, "u.legion_ne"
  10,  5, "u.legion_e"
  10,  6, "u.legion_se"
  10,  7, "u.legion_s"

  11,  0, "u.diplomat_sw"
  11,  1, "u.diplomat_w"
  11,  2, "u.diplomat_nw"
  11,  3, "u.diplomat_n"
  11,  4, "u.diplomat_ne"
  11,  5, "u.diplomat_e"
  11,  6, "u.diplomat_se"
  11,  7, "u.diplomat_s"

  12,  0, "u.caravan_sw"
  12,  1, "u.caravan_w"
  12,  2, "u.caravan_nw"
  12,  3, "u.caravan_n"
  12,  4, "u.caravan_ne"
  12,  5, "u.caravan_e"
  12,  6, "u.caravan_se"
  12,  7, "u.caravan_s"

  13,  0, "u.pikemen_sw"
  13,  1, "u.pikemen_w"
  13,  2, "u.pikemen_nw"
  13,  3, "u.pikemen_n"
  13,  4, "u.pikemen_ne"
  13,  5, "u.pikemen_e"
  13,  6, "u.pikemen_se"
  13,  7, "u.pikemen_s"

  14,  0, "u.knights_sw"
  14,  1, "u.knights_w"
  14,  2, "u.knights_nw"
  14,  3, "u.knights_n"
  14,  4, "u.knights_ne"
  14,  5, "u.knights_e"
  14,  6, "u.knights_se"
  14,  7, "u.knights_s"

  15,  0, "u.caravel_sw"
  15,  1, "u.caravel_w"
  15,  2, "u.caravel_nw"
  15,  3, "u.caravel_n"
  15,  4, "u.caravel_ne"
  15,  5, "u.caravel_e"
  15,  6, "u.caravel_se"
  15,  7, "u.caravel_s"

  16,  0, "u.galleon_sw"
  16,  1, "u.galleon_w"
  16,  2, "u.galleon_nw"
  16,  3, "u.galleon_n"
  16,  4, "u.galleon_ne"
  16,  5, "u.galleon_e"
  16,  6, "u.galleon_se"
  16,  7, "u.galleon_s"

  17,  0, "u.frigate_sw"
  17,  1, "u.frigate_w"
  17,  2, "u.frigate_nw"
  17,  3, "u.frigate_n"
  17,  4, "u.frigate_ne"
  17,  5, "u.frigate_e"
  17,  6, "u.frigate_se"
  17,  7, "u.frigate_s"

  18,  0, "u.ironclad_sw"
  18,  1, "u.ironclad_w"
  18,  2, "u.ironclad_nw"
  18,  3, "u.ironclad_n"
  18,  4, "u.ironclad_ne"
  18,  5, "u.ironclad_e"
  18,  6, "u.ironclad_se"
  18,  7, "u.ironclad_s"

  19,  0, "u.musketeers_sw"
  19,  1, "u.musketeers_w"
  19,  2, "u.musketeers_nw"
  19,  3, "u.musketeers_n"
  19,  4, "u.musketeers_ne"
  19,  5, "u.musketeers_e"
  19,  6, "u.musketeers_se"
  19,  7, "u.musketeers_s"

  20,  0, "u.dragoons_sw"
  20,  1, "u.dragoons_w"
  20,  2, "u.dragoons_nw"
  20,  3, "u.dragoons_n"
  20,  4, "u.dragoons_ne"
  20,  5, "u.dragoons_e"
  20,  6, "u.dragoons_se"
  20,  7, "u.dragoons_s"

  21,  0, "u.cannon_sw"
  21,  1, "u.cannon_w"
  21,  2, "u.cannon_nw"
  21,  3, "u.cannon_n"
  21,  4, "u.cannon_ne"
  21,  5, "u.cannon_e"
  21,  6, "u.cannon_se"
  21,  7, "u.cannon_s"

  22,  0, "u.engineers_sw"
  22,  1, "u.engineers_w"
  22,  2, "u.engineers_nw"
  22,  3, "u.engineers_n"
  22,  4, "u.engineers_ne"
  22,  5, "u.engineers_e"
  22,  6, "u.engineers_se"
  22,  7, "u.engineers_s"

  23,  0, "u.transport_sw"
  23,  1, "u.transport_w"
  23,  2, "u.transport_nw"
  23,  3, "u.transport_n"
  23,  4, "u.transport_ne"
  23,  5, "u.transport_e"
  23,  6, "u.transport_se"
  23,  7, "u.transport_s"

  24,  0, "u.destroyer_sw"
  24,  1, "u.destroyer_w"
  24,  2, "u.destroyer_nw"
  24,  3, "u.destroyer_n"
  24,  4, "u.destroyer_ne"
  24,  5, "u.destroyer_e"
  24,  6, "u.destroyer_se"
  24,  7, "u.destroyer_s"

  25,  0, "u.riflemen_sw"
  25,  1, "u.riflemen_w"
  25,  2, "u.riflemen_nw"
  25,  3, "u.riflemen_n"
  25,  4, "u.riflemen_ne"
  25,  5, "u.riflemen_e"
  25,  6, "u.riflemen_se"
  25,  7, "u.riflemen_s"

  26,  0, "u.cavalry_sw"
  26,  1, "u.cavalry_w"
  26,  2, "u.cavalry_nw"
  26,  3, "u.cavalry_n"
  26,  4, "u.cavalry_ne"
  26,  5, "u.cavalry_e"
  26,  6, "u.cavalry_se"
  26,  7, "u.cavalry_s"

  27,  0, "u.alpine_troops_sw"
  27,  1, "u.alpine_troops_w"
  27,  2, "u.alpine_troops_nw"
  27,  3, "u.alpine_troops_n"
  27,  4, "u.alpine_troops_ne"
  27,  5, "u.alpine_troops_e"
  27,  6, "u.alpine_troops_se"
  27,  7, "u.alpine_troops_s"

  28,  0, "u.freight_sw"
  28,  1, "u.freight_w"
  28,  2, "u.freight_nw"
  28,  3, "u.freight_n"
  28,  4, "u.freight_ne"
  28,  5, "u.freight_e"
  28,  6, "u.freight_se"
  28,  7, "u.freight_s"

  29,  0, "u.spy_sw"
  29,  1, "u.spy_w"
  29,  2, "u.spy_nw"
  29,  3, "u.spy_n"
  29,  4, "u.spy_ne"
  29,  5, "u.spy_e"
  29,  6, "u.spy_se"
  29,  7, "u.spy_s"

  30,  0, "u.cruiser_sw"
  30,  1, "u.cruiser_w"
  30,  2, "u.cruiser_nw"
  30,  3, "u.cruiser_n"
  30,  4, "u.cruiser_ne"
  30,  5, "u.cruiser_e"
  30,  6, "u.cruiser_se"
  30,  7, "u.cruiser_s"

  31,  0, "u.battleship_sw"
  31,  1, "u.battleship_w"
  31,  2, "u.battleship_nw"
  31,  3, "u.battleship_n"
  31,  4, "u.battleship_ne"
  31,  5, "u.battleship_e"
  31,  6, "u.battleship_se"
  31,  7, "u.battleship_s"

  32,  0, "u.submarine_sw"
  32,  1, "u.submarine_w"
  32,  2, "u.submarine_nw"
  32,  3, "u.submarine_n"
  32,  4, "u.submarine_ne"
  32,  5, "u.submarine_e"
  32,  6, "u.submarine_se"
  32,  7, "u.submarine_s"

  33,  0, "u.marines_sw"
  33,  1, "u.marines_w"
  33,  2, "u.marines_nw"
  33,  3, "u.marines_n"
  33,  4, "u.marines_ne"
  33,  5, "u.marines_e"
  33,  6, "u.marines_se"
  33,  7, "u.marines_s"

  34,  0, "u.partisan_sw"
  34,  1, "u.partisan_w"
  34,  2, "u.partisan_nw"
  34,  3, "u.partisan_n"
  34,  4, "u.partisan_ne"
  34,  5, "u.partisan_e"
  34,  6, "u.partisan_se"
  34,  7, "u.partisan_s"

  35,  0, "u.artillery_sw"
  35,  1, "u.artillery_w"
  35,  2, "u.artillery_nw"
  35,  3, "u.artillery_n"
  35,  4, "u.artillery_ne"
  35,  5, "u.artillery_e"
  35,  6, "u.artillery_se"
  35,  7, "u.artillery_s"

  36,  0, "u.fighter_sw"
  36,  1, "u.fighter_w"
  36,  2, "u.fighter_nw"
  36,  3, "u.fighter_n"
  36,  4, "u.fighter_ne"
  36,  5, "u.fighter_e"
  36,  6, "u.fighter_se"
  36,  7, "u.fighter_s"

  37,  0, "u.aegis_cruiser_sw"
  37,  1, "u.aegis_cruiser_w"
  37,  2, "u.aegis_cruiser_nw"
  37,  3, "u.aegis_cruiser_n"
  37,  4, "u.aegis_cruiser_ne"
  37,  5, "u.aegis_cruiser_e"
  37,  6, "u.aegis_cruiser_se"
  37,  7, "u.aegis_cruiser_s"

  38,  0, "u.carrier_sw"
  38,  1, "u.carrier_w"
  38,  2, "u.carrier_nw"
  38,  3, "u.carrier_n"
  38,  4, "u.carrier_ne"
  38,  5, "u.carrier_e"
  38,  6, "u.carrier_se"
  38,  7, "u.carrier_s"

  39,  0, "u.armor_sw"
  39,  1, "u.armor_w"
  39,  2, "u.armor_nw"
  39,  3, "u.armor_n"
  39,  4, "u.armor_ne"
  39,  5, "u.armor_e"
  39,  6, "u.armor_se"
  39,  7, "u.armor_s"

  40,  0, "u.mech_inf_sw"
  40,  1, "u.mech_inf_w"
  40,  2, "u.mech_inf_nw"
  40,  3, "u.mech_inf_n"
  40,  4, "u.mech_inf_ne"
  40,  5, "u.mech_inf_e"
  40,  6, "u.mech_inf_se"
  40,  7, "u.mech_inf_s"

  41,  0, "u.howitzer_sw"
  41,  1, "u.howitzer_w"
  41,  2, "u.howitzer_nw"
  41,  3, "u.howitzer_n"
  41,  4, "u.howitzer_ne"
  41,  5, "u.howitzer_e"
  41,  6, "u.howitzer_se"
  41,  7, "u.howitzer_s"

  42,  0, "u.paratroopers_sw"
  42,  1, "u.paratroopers_w"
  42,  2, "u.paratroopers_nw"
  42,  3, "u.paratroopers_n"
  42,  4, "u.paratroopers_ne"
  42,  5, "u.paratroopers_e"
  42,  6, "u.paratroopers_se"
  42,  7, "u.paratroopers_s"

  43,  0, "u.helicopter_sw"
  43,  1, "u.helicopter_w"
  43,  2, "u.helicopter_nw"
  43,  3, "u.helicopter_n"
  43,  4, "u.helicopter_ne"
  43,  5, "u.helicopter_e"
  43,  6, "u.helicopter_se"
  43,  7, "u.helicopter_s"

  44,  0, "u.bomber_sw"
  44,  1, "u.bomber_w"
  44,  2, "u.bomber_nw"
  44,  3, "u.bomber_n"
  44,  4, "u.bomber_ne"
  44,  5, "u.bomber_e"
  44,  6, "u.bomber_se"
  44,  7, "u.bomber_s"

  45,  0, "u.awacs_sw"
  45,  1, "u.awacs_w"
  45,  2, "u.awacs_nw"
  45,  3, "u.awacs_n"
  45,  4, "u.awacs_ne"
  45,  5, "u.awacs_e"
  45,  6, "u.awacs_se"
  45,  7, "u.awacs_s"

  46,  0, "u.nuclear_sw"
  46,  1, "u.nuclear_w"
  46,  2, "u.nuclear_nw"
  46,  3, "u.nuclear_n"
  46,  4, "u.nuclear_ne"
  46,  5, "u.nuclear_e"
  46,  6, "u.nuclear_se"
  46,  7, "u.nuclear_s"

  47,  0, "u.cruise_missile_sw"
  47,  1, "u.cruise_missile_w"
  47,  2, "u.cruise_missile_nw"
  47,  3, "u.cruise_missile_n"
  47,  4, "u.cruise_missile_ne"
  47,  5, "u.cruise_missile_e"
  47,  6, "u.cruise_missile_se"
  47,  7, "u.cruise_missile_s"

  48,  0, "u.stealth_bomber_sw"
  48,  1, "u.stealth_bomber_w"
  48,  2, "u.stealth_bomber_nw"
  48,  3, "u.stealth_bomber_n"
  48,  4, "u.stealth_bomber_ne"
  48,  5, "u.stealth_bomber_e"
  48,  6, "u.stealth_bomber_se"
  48,  7, "u.stealth_bomber_s"

  49,  0, "u.stealth_fighter_sw"
  49,  1, "u.stealth_fighter_w"
  49,  2, "u.stealth_fighter_nw"
  49,  3, "u.stealth_fighter_n"
  49,  4, "u.stealth_fighter_ne"
  49,  5, "u.stealth_fighter_e"
  49,  6, "u.stealth_fighter_se"
  49,  7, "u.stealth_fighter_s"

  50,  0, "u.leader_sw"
  50,  1, "u.leader_w"
  50,  2, "u.leader_nw"
  50,  3, "u.leader_n"
  50,  4, "u.leader_ne"
  50,  5, "u.leader_e"
  50,  6, "u.leader_se"
  50,  7, "u.leader_s"

  51,  0, "u.barbarian_leader_sw"
  51,  1, "u.barbarian_leader_w"
  51,  2, "u.barbarian_leader_nw"
  51,  3, "u.barbarian_leader_n"
  51,  4, "u.barbarian_leader_ne"
  51,  5, "u.barbarian_leader_e"
  51,  6, "u.barbarian_leader_se"
  51,  7, "u.barbarian_leader_s"

  52,  0, "u.fanatics_sw"
  52,  1, "u.fanatics_w"
  52,  2, "u.fanatics_nw"
  52,  3, "u.fanatics_n"
  52,  4, "u.fanatics_ne"
  52,  5, "u.fanatics_e"
  52,  6, "u.fanatics_se"
  52,  7, "u.fanatics_s"

  53,  0, "u.crusaders_sw"
  53,  1, "u.crusaders_w"
  53,  2, "u.crusaders_nw"
  53,  3, "u.crusaders_n"
  53,  4, "u.crusaders_ne"
  53,  5, "u.crusaders_e"
  53,  6, "u.crusaders_se"
  53,  7, "u.crusaders_s"

  54,  0, "u.elephants_sw"
  54,  1, "u.elephants_w"
  54,  2, "u.elephants_nw"
  54,  3, "u.elephants_n"
  54,  4, "u.elephants_ne"
  54,  5, "u.elephants_e"
  54,  6, "u.elephants_se"
  54,  7, "u.elephants_s"

}
