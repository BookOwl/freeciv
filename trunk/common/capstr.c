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

#include <string.h>
#include <stdlib.h>		/* getenv() */

#include "connection.h"		/* MAX_LEN_CAPSTR */
#include "support.h"

#include "capstr.h"

static char our_capability_internal[MAX_LEN_CAPSTR];
const char * const our_capability = our_capability_internal;

/* Capabilities: original author: Mitch Davis (mjd@alphalink.com.au)
 *
 * The capability string is a string clients and servers trade to find
 * out if they can talk to each other, and using which protocol version,
 * and which features and behaviors to expect.  The string is a list of
 * words, separated by whitespace and/or commas, where each word indicates
 * a capability that this version of Freeciv understands.
 * If a capability word is mandatory, it should start with a "+".
 *
 * eg, #define CAPABILITY "+1.6, MapScroll, +AutoSettlers"
 *
 * Client and server functions can test these strings for a particular
 * capability by calling the functions in capability.c
 *
 * Each executable has a string our_capability (above), which gives the
 * capabilities of the running executable.  This is normally initialised
 * with CAPABILITY, but can be changed at run-time by setting the
 * FREECIV_CAPS environment variable, though that is probably mainly
 * useful for testing purposes.
 *
 * For checking the connections of other executables, each
 * "struct connection" has a capability string, which gives the
 * capability of the executable at the other end of the connection.
 * So for the client, the capability of the server is in
 * aconnection.capability, and for the server, the capabilities of 
 * connected clients are in game.players[i]->conn.capability
 * The client now also knows the capabilities of other clients,
 * via game.players[i]->conn.capability.
 *
 * Note the connection struct is a parameter to the functions to send and
 * receive packets, which may be convenient for adjusting how a packet is
 * sent or interpreted based on the capabilities of the connection.
 *
 * At the time of a major release, the capability string may be
 * simplified; eg, the example string above could be replaced by "+1.7".
 * (This should probably only happen if a mandatory capability has
 * been introduced since the previous release.)
 * Whoever makes such a change has responsibility to search the Freeciv
 * code, and look for places where people are using has_capability.
 * If you're taking a capability out of the string, because now every
 * client and server supports it, then you should take out the
 * if(has_capability()) code so that this code is always executed.
 *
 * (The savefile and ruleset files have strings which are used similarly,
 * and checked by the same has_capability function, but the strings there
 * are not directly related to the capability strings discussed here.)
 */

#define CAPABILITY "+1.11 diplomat_investigate_fix production_change_fix" \
" game_ruleset nuclear_fallout land_channel_requirement event_wonder_obsolete" \
" event00_fix conn_info gen_impr_oversights diplo_move_city packet_short_city" \
" indef_impr_types worklist_true_ids shared_vision activity_patrol" \
" gen_granary_size"

/* "+1.11" is protocol for 1.11.0 stable release.

   "diplomat_investigate_fix" extends the protocol so that diplomat
   investigation of a city reveals the correct supported and present
   unit lists.

   "production_change_fix" extends the protocol so that city production
   changes are correctly accounted for, allowing for recovery of penalty
   when changing back to original class.

   "game_ruleset" is use of game.ruleset instead of civstyle option.

   "nuclear_fallout" is protocol extension for having nuclear fallout
   be distinct from industrial pollution.

   "land_channel_requirement" extends the protocol for the requirement
   of a minimum number of ocean tiles adjacent to a land tile wished to
   be changed to ocean.

   "event_wonder_obsolete" is a message event type, letting players know
   when one of their wonders has been made obsolete by a tech advance
   anywhere in the world.
   
   "event00_fix" is that client understands that negative positions for
   events means no position.  Previous clients expect (0,0), which is
   also a genuine position.

   "conn_info" is protocol changes to tell clients more information
   about other connections to the server, including about multiple
   connections per player.

   "gen_impr_oversights" extends the protocol to include information
   which addresses a few oversights in the original design.

   "diplo_move_city" means server knows DIPLOMAT_MOVE activity for
   diplomats to move into (allied) cities.

   "packet_short_city" is smaller packet often sent instead of
   packet_city_info.

   "indef_impr_types" extends the protocol to allow for an indefinite
   number of improvement types.

   "worklist_true_ids" changes the worklist internals and extends
   the protocol so that unit and improvement IDs are communicated as
   their true values (units are no longer B_LAST + unit_id).  A flag
   field is added to discriminate between unit and improvement and to
   specify the end of the list.

   "shared_vision" is the ability to negotiate and display info about
   shared vision.

   "activity_patrol" is the patrol activity and the ability to send a goto
   route from the client to the server (for both goto and patrol activities).

   "gen_granary_size" adds the option to change the way the size of the 
   food storage box is calculated.
*/

void init_our_capability(void)
{
  char *s;

  s = getenv("FREECIV_CAPS");
  if (s == NULL) {
    s = CAPABILITY;
  }
  sz_strlcpy(our_capability_internal, s);
}
