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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <packets.h>
#include <log.h>
#include <capability.h>
#include <mem.h>

/**************************************************************************
Swap bytes on an integer considered as 16 bits
**************************************************************************/
static unsigned int swab_int16(unsigned int val)
{
  return ((val & 0xFF) << 8) | ((val & 0xFF00) >> 8);
}

/**************************************************************************
Swap bytes on an integer considered as 32 bits
**************************************************************************/
static unsigned int swab_int32(unsigned int val)
{
  return ((val & 0xFF) << 24) | ((val & 0xFF00) << 8)
    | ((val & 0xFF0000) >> 8) | ((val & 0xFF000000) >> 24);
}

/**************************************************************************
Swap bytes on an pointed to integer considered as 16 bits
**************************************************************************/
#ifdef UNUSED
static void swab_pint16(int *ptr)
{
  (*ptr) = swab_int16(*ptr);
}
#endif

/**************************************************************************
Swap bytes on an pointed to integer considered as 32 bits
**************************************************************************/
static void swab_pint32(int *ptr)
{
  (*ptr) = swab_int32(*ptr);
}

/**************************************************************************
...
**************************************************************************/
void *get_packet_from_connection(struct connection *pc, int *ptype)
{
  int len, type;

  if(pc->buffer.ndata<4)
    return NULL;           /* length and type not read */

  get_int16(pc->buffer.data, &len);
  get_int8(pc->buffer.data+2, &type);

  if(pc->first_packet) {
    /* the first packet better be short: */
    freelog(LOG_DEBUG, "first packet type %d len %d", type, len);
    if(len > 1024) {
      freelog(LOG_NORMAL, "connection from %s detected as old byte order",
	      pc->addr);
      pc->byte_swap = 1;
    } else {
      pc->byte_swap = 0;
    }
    pc->first_packet = 0;
  }

  if(pc->byte_swap) {
    len = swab_int16(len);
    /* so the packet gets processed (removed etc) properly: */
    put_int16(pc->buffer.data, len);
  }

  if(len > pc->buffer.ndata)
    return NULL;           /* not all data has been read */

  freelog(LOG_DEBUG, "packet type %d len %d", type, len);

  *ptype=type;

  switch(type) {

  case PACKET_REQUEST_JOIN_GAME:
    return receive_packet_req_join_game(pc);

  case PACKET_JOIN_GAME_REPLY:
    return receive_packet_join_game_reply(pc);

  case PACKET_SERVER_SHUTDOWN:
    return receive_packet_generic_message(pc);

  case PACKET_UNIT_INFO:
    return receive_packet_unit_info(pc);

   case PACKET_CITY_INFO:
    return receive_packet_city_info(pc);

  case PACKET_MOVE_UNIT:
    return receive_packet_move_unit(pc);

  case PACKET_TURN_DONE:
    return receive_packet_generic_message(pc);

  case PACKET_BEFORE_NEW_YEAR:
    return receive_packet_before_new_year(pc);
  case PACKET_NEW_YEAR:
    return receive_packet_new_year(pc);

  case PACKET_TILE_INFO:
    return receive_packet_tile_info(pc);

  case PACKET_SELECT_RACE:
  case PACKET_REMOVE_UNIT:
  case PACKET_REMOVE_CITY:
  case PACKET_GAME_STATE:
  case PACKET_REPORT_REQUEST:
  case PACKET_REMOVE_PLAYER:  
  case PACKET_CITY_REFRESH:
  case PACKET_INCITE_INQ:
    return receive_packet_generic_integer(pc);
    
  case PACKET_ALLOC_RACE:
    return receive_packet_alloc_race(pc);

  case PACKET_SHOW_MESSAGE:
    return receive_packet_generic_message(pc);

  case PACKET_PLAYER_INFO:
    return receive_packet_player_info(pc);

  case PACKET_GAME_INFO:
    return receive_packet_game_info(pc);

  case PACKET_MAP_INFO:
    return receive_packet_map_info(pc);

  case PACKET_CHAT_MSG:
  case PACKET_PAGE_MSG:
    return receive_packet_generic_message(pc);
    
  case PACKET_CITY_SELL:
  case PACKET_CITY_BUY:
  case PACKET_CITY_CHANGE:
  case PACKET_CITY_MAKE_SPECIALIST:
  case PACKET_CITY_MAKE_WORKER:
  case PACKET_CITY_CHANGE_SPECIALIST:
  case PACKET_CITY_RENAME:
    return receive_packet_city_request(pc);

  case PACKET_PLAYER_RATES:
  case PACKET_PLAYER_REVOLUTION:
  case PACKET_PLAYER_GOVERNMENT:
  case PACKET_PLAYER_RESEARCH:
  case PACKET_PLAYER_TECH_GOAL:
    return receive_packet_player_request(pc);

  case PACKET_UNIT_BUILD_CITY:
  case PACKET_UNIT_DISBAND:
  case PACKET_UNIT_CHANGE_HOMECITY:
  case PACKET_UNIT_ESTABLISH_TRADE:
  case PACKET_UNIT_HELP_BUILD_WONDER:
  case PACKET_UNIT_GOTO_TILE:
  case PACKET_UNIT_AUTO:
  case PACKET_UNIT_UNLOAD:
  case PACKET_UNIT_UPGRADE:
  case PACKET_UNIT_NUKE:
    return receive_packet_unit_request(pc);
  case PACKET_UNITTYPE_UPGRADE:
    return receive_packet_unittype_info(pc);
  case PACKET_UNIT_COMBAT:
    return receive_packet_unit_combat(pc);
  case PACKET_NUKE_TILE:
    return receive_packet_nuke_tile(pc);
  case PACKET_DIPLOMAT_ACTION:
    return receive_packet_diplomat_action(pc);

  case PACKET_DIPLOMACY_INIT_MEETING:
  case PACKET_DIPLOMACY_CREATE_CLAUSE:
  case PACKET_DIPLOMACY_REMOVE_CLAUSE:
  case PACKET_DIPLOMACY_CANCEL_MEETING:
  case PACKET_DIPLOMACY_ACCEPT_TREATY:
  case PACKET_DIPLOMACY_SIGN_TREATY:
    return receive_packet_diplomacy_info(pc);

  case PACKET_INCITE_COST:
  case PACKET_CITY_OPTIONS:
    return receive_packet_generic_values(pc);

  case PACKET_RULESET_TECH:
    return receive_packet_ruleset_tech(pc);
  case PACKET_RULESET_UNIT:
    return receive_packet_ruleset_unit(pc);
  case PACKET_RULESET_BUILDING:
    return receive_packet_ruleset_building(pc);

  case PACKET_SPACESHIP_INFO:
    return receive_packet_spaceship_info(pc);

  case PACKET_SPACESHIP_ACTION:
    return receive_packet_spaceship_action(pc);

  default:
    freelog(LOG_NORMAL, "unknown packet type received");
    remove_packet_from_buffer(&pc->buffer);
    return NULL;
  };
}

/**************************************************************************
...
**************************************************************************/
unsigned char *get_int8(unsigned char *buffer, int *val)
{
  if(val) {
    *val=(*buffer);
  }
  return buffer+1;
}

/**************************************************************************
...
**************************************************************************/
unsigned char *put_int8(unsigned char *buffer, int val)
{
  *buffer++=val&0xff;
  return buffer;
}

/**************************************************************************
...
**************************************************************************/
unsigned char *get_int16(unsigned char *buffer, int *val)
{
  if(val) {
    unsigned short x;
    memcpy(&x,buffer,2);
    *val=ntohs(x);
  }
  return buffer+2;
}


/**************************************************************************
...
**************************************************************************/
unsigned char *put_int16(unsigned char *buffer, int val)
{
  unsigned short x = htons(val);
  memcpy(buffer,&x,2);
  return buffer+2;
}


/**************************************************************************
...
**************************************************************************/
unsigned char *get_int32(unsigned char *buffer, int *val)
{
  if(val) {
    unsigned long x;
    memcpy(&x,buffer,4);
    *val=ntohl(x);
  }
  return buffer+4;
}


/**************************************************************************
...
**************************************************************************/
unsigned char *put_int32(unsigned char *buffer, int val)
{
  unsigned long x = htonl(val);
  memcpy(buffer,&x,4);
  return buffer+4;
}

/**************************************************************************
...
**************************************************************************/
unsigned char *put_string(unsigned char *buffer, char *mystring)
{
  int len = strlen(mystring) + 1;
  memcpy(buffer, mystring, len);
  return buffer+len;
}


/**************************************************************************
...
**************************************************************************/
unsigned char *get_string(unsigned char *buffer, char *mystring)
{
  unsigned char *c;
  int len;

  /* avoid using strlen (or strcpy) on an (unsigned char*)  --dwp */
  for(c=buffer; *c; c++) ;
  len = c-buffer+1;
  if(mystring) {
    memcpy(mystring, buffer, len);
  }
  return buffer+len;
}

/**************************************************************************
...
**************************************************************************/
unsigned char *put_city_map(unsigned char *buffer, char *str)
{
  const static int index[20]=
      {1,2,3, 5,6,7,8,9, 10,11, 13,14, 15,16,17,18,19, 21,22,23 };
  int i;

  for(i=0;i<20;i+=5)
    *buffer++ = (str[index[i]]-'0')*81 + (str[index[i+1]]-'0')*27 +
	        (str[index[i+2]]-'0')*9 + (str[index[i+3]]-'0')*3 +
	        (str[index[i+4]]-'0')*1;

  return buffer;
}

/**************************************************************************
...
**************************************************************************/
unsigned char *get_city_map(unsigned char *buffer, char *str)
{
  const static int index[21]=
      {1,2,3, 5,6,7,8,9, 10,11, 13,14, 15,16,17,18,19, 21,22,23 };
  int i,j;

  if(!str)  return buffer+4;

  str[0]='2'; str[4]='2'; str[12]='1'; 
  str[20]='2'; str[24]='2'; str[25]='\0';
  for(i=0;i<20;)  {
    j=*buffer++;
    str[index[i++]]='0'+j/81; j%=81;
    str[index[i++]]='0'+j/27; j%=27;
    str[index[i++]]='0'+j/9; j%=9;
    str[index[i++]]='0'+j/3; j%=3;
    str[index[i++]]='0'+j;
  };

  return buffer;
}
    
/**************************************************************************
...
**************************************************************************/
unsigned char *put_bit_string(unsigned char *buffer, char *str)
{
  int i,b,n;
  int data;

  n=strlen(str);
  *buffer++=n;
  for(i=0;i<n;)  {
    data=0;
    for(b=0;b<8 && i<n;b++,i++)
      if(str[i]=='1') data|=(1<<b);
    *buffer++=data;
  }

  return buffer;
}

/**************************************************************************
...
**************************************************************************/
unsigned char *get_bit_string(unsigned char *buffer, char *str)
{
  int i,b,n;
  int data;

  n=*buffer++;
  for(i=0;i<n;)  {
    data=*buffer++;
    for(b=0;b<8 && i<n;b++,i++)
      if(data&(1<<b))  str[i]='1'; else str[i]='0';
  }
  str[n]='\0';

  return buffer;
}


/*************************************************************************
...
**************************************************************************/
int send_packet_diplomacy_info(struct connection *pc, enum packet_type pt,
			       struct packet_diplomacy_info *packet)
{
  unsigned char buffer[MAX_PACKET_SIZE], *cptr;
  cptr=put_int8(buffer+2, pt);
  
  cptr=put_int32(cptr, packet->plrno0);
  cptr=put_int32(cptr, packet->plrno1);
  cptr=put_int32(cptr, packet->plrno_from);
  cptr=put_int32(cptr, packet->clause_type);
  cptr=put_int32(cptr, packet->value);
  put_int16(buffer, cptr-buffer);

  return send_connection_data(pc, buffer, cptr-buffer);
}

/*************************************************************************
...
**************************************************************************/
struct packet_diplomacy_info *
receive_packet_diplomacy_info(struct connection *pc)
{
  unsigned char *cptr;
  struct packet_diplomacy_info *preq=
    fc_malloc(sizeof(struct packet_diplomacy_info));

  cptr=get_int16(pc->buffer.data, NULL);
  cptr=get_int8(cptr, NULL);

  cptr=get_int32(cptr, &preq->plrno0);
  cptr=get_int32(cptr, &preq->plrno1);
  cptr=get_int32(cptr, &preq->plrno_from);
  cptr=get_int32(cptr, &preq->clause_type);
  cptr=get_int32(cptr, &preq->value);
  remove_packet_from_buffer(&pc->buffer);
  return preq;
}


/*************************************************************************
...
**************************************************************************/
int send_packet_diplomat_action(struct connection *pc, 
				struct packet_diplomat_action *packet)
{
  unsigned char buffer[MAX_PACKET_SIZE], *cptr;
  cptr=put_int8(buffer+2, PACKET_DIPLOMAT_ACTION);
  
  cptr=put_int8(cptr, packet->action_type);
  cptr=put_int16(cptr, packet->diplomat_id);
  cptr=put_int16(cptr, packet->target_id);
  cptr=put_int16(cptr, packet->value);
  put_int16(buffer, cptr-buffer);

  return send_connection_data(pc, buffer, cptr-buffer);
}

/*************************************************************************
...
**************************************************************************/
struct packet_diplomat_action *
receive_packet_diplomat_action(struct connection *pc)
{
  unsigned char *cptr;
  struct packet_diplomat_action *preq=
    fc_malloc(sizeof(struct packet_diplomat_action));

  cptr=get_int16(pc->buffer.data, NULL);
  cptr=get_int8(cptr, NULL);

  cptr=get_int8(cptr, &preq->action_type);
  cptr=get_int16(cptr, &preq->diplomat_id);
  cptr=get_int16(cptr, &preq->target_id);
  cptr=get_int16(cptr, &preq->value);
  remove_packet_from_buffer(&pc->buffer);
  return preq;
}

/*************************************************************************
...
**************************************************************************/
int send_packet_nuke_tile(struct connection *pc, 
			  struct packet_nuke_tile *packet)
{
  unsigned char buffer[MAX_PACKET_SIZE], *cptr;
  cptr=put_int8(buffer+2, PACKET_NUKE_TILE);
  
  cptr=put_int8(cptr, packet->x);
  cptr=put_int8(cptr, packet->y);
  put_int16(buffer, cptr-buffer);

  return send_connection_data(pc, buffer, cptr-buffer);
}


/*************************************************************************
...
**************************************************************************/
struct packet_nuke_tile *
receive_packet_nuke_tile(struct connection *pc)
{
  unsigned char *cptr;
  struct packet_nuke_tile *preq=
    fc_malloc(sizeof(struct packet_nuke_tile));

  cptr=get_int16(pc->buffer.data, NULL);
  cptr=get_int8(cptr, NULL);

  cptr=get_int8(cptr, &preq->x);
  cptr=get_int8(cptr, &preq->y);
  remove_packet_from_buffer(&pc->buffer);
  return preq;
}




/*************************************************************************
...
**************************************************************************/
int send_packet_unit_combat(struct connection *pc, 
			    struct packet_unit_combat *packet)
{
  unsigned char buffer[MAX_PACKET_SIZE], *cptr;
  cptr=put_int8(buffer+2, PACKET_UNIT_COMBAT);
  
  cptr=put_int16(cptr, packet->attacker_unit_id);
  cptr=put_int16(cptr, packet->defender_unit_id);
  cptr=put_int8(cptr, packet->attacker_hp);
  cptr=put_int8(cptr, packet->defender_hp);
  cptr=put_int8(cptr, packet->make_winner_veteran);
  put_int16(buffer, cptr-buffer);

  return send_connection_data(pc, buffer, cptr-buffer);
}


/*************************************************************************
...
**************************************************************************/
struct packet_unit_combat *
receive_packet_unit_combat(struct connection *pc)
{
  unsigned char *cptr;
  struct packet_unit_combat *preq=
    fc_malloc(sizeof(struct packet_unit_combat));

  cptr=get_int16(pc->buffer.data, NULL);
  cptr=get_int8(cptr, NULL);

  cptr=get_int16(cptr, &preq->attacker_unit_id);
  cptr=get_int16(cptr, &preq->defender_unit_id);
  cptr=get_int8(cptr, &preq->attacker_hp);
  cptr=get_int8(cptr, &preq->defender_hp);
  cptr=get_int8(cptr, &preq->make_winner_veteran);
  remove_packet_from_buffer(&pc->buffer);
  return preq;
}


/*************************************************************************
...
**************************************************************************/
int send_packet_unit_request(struct connection *pc, 
			     struct packet_unit_request *packet,
			     enum packet_type req_type)
{
  unsigned char buffer[MAX_PACKET_SIZE], *cptr;
  cptr=put_int8(buffer+2, req_type);
  cptr=put_int16(cptr, packet->unit_id);
  cptr=put_int16(cptr, packet->city_id);
  cptr=put_int8(cptr, packet->x);
  cptr=put_int8(cptr, packet->y);
  cptr=put_string(cptr, packet->name);
  put_int16(buffer, cptr-buffer);

  return send_connection_data(pc, buffer, cptr-buffer);
}


/*************************************************************************
...
**************************************************************************/
struct packet_unit_request *
receive_packet_unit_request(struct connection *pc)
{
  unsigned char *cptr;
  struct packet_unit_request *preq=
    fc_malloc(sizeof(struct packet_unit_request));

  cptr=get_int16(pc->buffer.data, NULL);
  cptr=get_int8(cptr, NULL);
  cptr=get_int16(cptr, &preq->unit_id);
  cptr=get_int16(cptr, &preq->city_id);
  cptr=get_int8(cptr, &preq->x);
  cptr=get_int8(cptr, &preq->y);
  cptr=get_string(cptr, preq->name);
  remove_packet_from_buffer(&pc->buffer);
  return preq;
}



/*************************************************************************
...
**************************************************************************/
int send_packet_player_request(struct connection *pc, 
			       struct packet_player_request *packet,
			       enum packet_type req_type)
{
  unsigned char buffer[MAX_PACKET_SIZE], *cptr;
  cptr=put_int8(buffer+2, req_type);
  cptr=put_int8(cptr, packet->tax);
  cptr=put_int8(cptr, packet->luxury);
  cptr=put_int8(cptr, packet->science);
  cptr=put_int8(cptr, packet->government);
  cptr=put_int8(cptr, packet->tech);
  put_int16(buffer, cptr-buffer);

  return send_connection_data(pc, buffer, cptr-buffer);
}


/*************************************************************************
...
**************************************************************************/
struct packet_player_request *
receive_packet_player_request(struct connection *pc)
{
  unsigned char *cptr;
	int length;
  struct packet_player_request *preq=
    fc_malloc(sizeof(struct packet_player_request));

  cptr=get_int16(pc->buffer.data, &length);
  cptr=get_int8(cptr, NULL);
  cptr=get_int8(cptr, &preq->tax);
  cptr=get_int8(cptr, &preq->luxury);
  cptr=get_int8(cptr, &preq->science);
  cptr=get_int8(cptr, &preq->government);
  cptr=get_int8(cptr, &preq->tech);
  remove_packet_from_buffer(&pc->buffer);
  return preq;
}





/*************************************************************************
...
**************************************************************************/
int send_packet_city_request(struct connection *pc, 
			     struct packet_city_request *packet,
			     enum packet_type req_type)
{
  unsigned char buffer[MAX_PACKET_SIZE], *cptr;
  cptr=put_int8(buffer+2, req_type);
  cptr=put_int16(cptr, packet->city_id);
  cptr=put_int8(cptr, packet->build_id);
  cptr=put_int8(cptr, packet->is_build_id_unit_id?1:0);
  cptr=put_int8(cptr, packet->worker_x);
  cptr=put_int8(cptr, packet->worker_y);
  cptr=put_int8(cptr, packet->specialist_from);
  cptr=put_int8(cptr, packet->specialist_to);
  cptr=put_string(cptr, packet->name);
  put_int16(buffer, cptr-buffer);

  return send_connection_data(pc, buffer, cptr-buffer);
}


/*************************************************************************
...
**************************************************************************/
struct packet_city_request *
receive_packet_city_request(struct connection *pc)
{
  unsigned char *cptr;
  struct packet_city_request *preq=
    fc_malloc(sizeof(struct packet_city_request));

  cptr=get_int16(pc->buffer.data, NULL);
  cptr=get_int8(cptr, NULL);

  cptr=get_int16(cptr, &preq->city_id);
  cptr=get_int8(cptr, &preq->build_id);
  cptr=get_int8(cptr, &preq->is_build_id_unit_id);
  cptr=get_int8(cptr, &preq->worker_x);
  cptr=get_int8(cptr, &preq->worker_y);
  cptr=get_int8(cptr, &preq->specialist_from);
  cptr=get_int8(cptr, &preq->specialist_to);
  cptr=get_string(cptr, preq->name);
  
  remove_packet_from_buffer(&pc->buffer);
  return preq;
}


/*************************************************************************
...
**************************************************************************/
int send_packet_player_info(struct connection *pc, struct packet_player_info *pinfo)
{
  unsigned char buffer[MAX_PACKET_SIZE], *cptr;
  cptr=put_int8(buffer+2, PACKET_PLAYER_INFO);
  cptr=put_int8(cptr, pinfo->playerno);
  cptr=put_string(cptr, pinfo->name);
  cptr=put_int8(cptr, pinfo->government);
  cptr=put_int32(cptr, pinfo->embassy);
  cptr=put_int8(cptr, pinfo->race);
  cptr=put_int8(cptr, pinfo->turn_done?1:0);
  cptr=put_int16(cptr, pinfo->nturns_idle);
  cptr=put_int8(cptr, pinfo->is_alive?1:0);
  
  cptr=put_int32(cptr, pinfo->gold);
  cptr=put_int8(cptr, pinfo->tax);
  cptr=put_int8(cptr, pinfo->science);
  cptr=put_int8(cptr, pinfo->luxury);

  cptr=put_int32(cptr, pinfo->researched);
  cptr=put_int32(cptr, pinfo->researchpoints);
  cptr=put_int8(cptr, pinfo->researching);
  cptr=put_bit_string(cptr, (char*)pinfo->inventions);
  cptr=put_int16(cptr, pinfo->future_tech);
  
  cptr=put_int8(cptr, pinfo->is_connected?1:0);
  cptr=put_string(cptr, pinfo->addr);
  cptr=put_int8(cptr, pinfo->revolution);
  cptr=put_int8(cptr, pinfo->tech_goal);
  cptr=put_int8(cptr, pinfo->ai?1:0);

  /* if (pc && has_capability("clientcapabilities", pc->capability)) */
  cptr=put_string(cptr, pinfo->capability);

  put_int16(buffer, cptr-buffer);

  return send_connection_data(pc, buffer, cptr-buffer);
}


/*************************************************************************
...
**************************************************************************/
struct packet_player_info *
receive_packet_player_info(struct connection *pc)
{
  unsigned char *cptr;
  int length;
  struct packet_player_info *pinfo=
     fc_malloc(sizeof(struct packet_player_info));

  cptr=get_int16(pc->buffer.data, &length);
  cptr=get_int8(cptr, NULL);

  cptr=get_int8(cptr,  &pinfo->playerno);
  cptr=get_string(cptr, pinfo->name);
  cptr=get_int8(cptr,  &pinfo->government);
  cptr=get_int32(cptr,  &pinfo->embassy);
  cptr=get_int8(cptr,  &pinfo->race);
  cptr=get_int8(cptr,  &pinfo->turn_done);
  cptr=get_int16(cptr,  &pinfo->nturns_idle);
  cptr=get_int8(cptr,  &pinfo->is_alive);
  
  cptr=get_int32(cptr, &pinfo->gold);
  cptr=get_int8(cptr, &pinfo->tax);
  cptr=get_int8(cptr, &pinfo->science);
  cptr=get_int8(cptr, &pinfo->luxury);

  cptr=get_int32(cptr, &pinfo->researched); /* signed */
  cptr=get_int32(cptr, &pinfo->researchpoints);
  cptr=get_int8(cptr, &pinfo->researching);
  cptr=get_bit_string(cptr, (char*)pinfo->inventions);
  cptr=get_int16(cptr, &pinfo->future_tech);

  cptr=get_int8(cptr, &pinfo->is_connected);
  cptr=get_string(cptr, pinfo->addr);
  cptr=get_int8(cptr, &pinfo->revolution);
  cptr=get_int8(cptr, &pinfo->tech_goal);
  cptr=get_int8(cptr, &pinfo->ai);

  /* if (has_capability("clientcapabilities", pc->capability)) */
  cptr=get_string(cptr, pinfo->capability);
  
  remove_packet_from_buffer(&pc->buffer);
  return pinfo;
}




/*************************************************************************
...
**************************************************************************/
int send_packet_game_info(struct connection *pc, 
			  struct packet_game_info *pinfo)
{
  unsigned char buffer[MAX_PACKET_SIZE], *cptr;
  int i;
  
  cptr=put_int8(buffer+2, PACKET_GAME_INFO);
  cptr=put_int16(cptr, pinfo->gold);
  cptr=put_int32(cptr, pinfo->tech);
  cptr=put_int8(cptr, pinfo->techlevel);

  cptr=put_int32(cptr, pinfo->skill_level);
  cptr=put_int16(cptr, pinfo->timeout);
  cptr=put_int32(cptr, pinfo->end_year);
  cptr=put_int32(cptr, pinfo->year);
  cptr=put_int8(cptr, pinfo->min_players);
  cptr=put_int8(cptr, pinfo->max_players);
  cptr=put_int8(cptr, pinfo->nplayers);
  cptr=put_int8(cptr, pinfo->player_idx);
  cptr=put_int32(cptr, pinfo->globalwarming);
  cptr=put_int32(cptr, pinfo->heating);
  cptr=put_int8(cptr, pinfo->cityfactor);
  cptr=put_int8(cptr, pinfo->diplcost);
  cptr=put_int8(cptr, pinfo->freecost);
  cptr=put_int8(cptr, pinfo->conquercost);
  cptr=put_int8(cptr, pinfo->unhappysize);
  cptr=put_int8(cptr, pinfo->rail_food);
  cptr=put_int8(cptr, pinfo->rail_prod);
  cptr=put_int8(cptr, pinfo->rail_trade);
  
  for(i=0; i<A_LAST; i++)
    cptr=put_int8(cptr, pinfo->global_advances[i]);
  for(i=0; i<B_LAST; i++)
    cptr=put_int16(cptr, pinfo->global_wonders[i]);
  cptr=put_int8(cptr, pinfo->techpenalty);
  cptr=put_int8(cptr, pinfo->foodbox);
  cptr=put_int8(cptr, pinfo->civstyle);
  cptr=put_int8(cptr, pinfo->spacerace);

  cptr=put_int8(cptr, pinfo->aqueduct_size);
  cptr=put_int8(cptr, pinfo->sewer_size);
  cptr=put_int8(cptr, pinfo->rtech.get_bonus_tech);
  cptr=put_int8(cptr, pinfo->rtech.boat_fast);
  cptr=put_int8(cptr, pinfo->rtech.cathedral_plus);
  cptr=put_int8(cptr, pinfo->rtech.cathedral_minus);
  cptr=put_int8(cptr, pinfo->rtech.colosseum_plus);
  
  put_int16(buffer, cptr-buffer);

  return send_connection_data(pc, buffer, cptr-buffer);
}

/*************************************************************************
...
**************************************************************************/
struct packet_game_info *receive_packet_game_info(struct connection *pc)
{
  int i;
  unsigned char *cptr;
  struct packet_game_info *pinfo=
     fc_malloc(sizeof(struct packet_game_info));

  cptr=get_int16(pc->buffer.data, NULL);
  cptr=get_int8(cptr, NULL);

  cptr=get_int16(cptr, &pinfo->gold);
  cptr=get_int32(cptr, &pinfo->tech);
  cptr=get_int8(cptr, &pinfo->techlevel);
  cptr=get_int32(cptr, &pinfo->skill_level);
  cptr=get_int16(cptr, &pinfo->timeout);
  cptr=get_int32(cptr, &pinfo->end_year);
  cptr=get_int32(cptr, &pinfo->year);
  cptr=get_int8(cptr, &pinfo->min_players);
  cptr=get_int8(cptr, &pinfo->max_players);
  cptr=get_int8(cptr, &pinfo->nplayers);
  cptr=get_int8(cptr, &pinfo->player_idx);
  cptr=get_int32(cptr, &pinfo->globalwarming);
  cptr=get_int32(cptr, &pinfo->heating);
  cptr=get_int8(cptr, &pinfo->cityfactor);
  cptr=get_int8(cptr, &pinfo->diplcost);
  cptr=get_int8(cptr, &pinfo->freecost);
  cptr=get_int8(cptr, &pinfo->conquercost);
  cptr=get_int8(cptr, &pinfo->unhappysize);
  cptr=get_int8(cptr, &pinfo->rail_food);
  cptr=get_int8(cptr, &pinfo->rail_prod);
  cptr=get_int8(cptr, &pinfo->rail_trade);
  
  for(i=0; i<A_LAST; i++)
    cptr=get_int8(cptr, &pinfo->global_advances[i]);
  for(i=0; i<B_LAST; i++)
    cptr=get_int16(cptr, &pinfo->global_wonders[i]);
  cptr=get_int8(cptr, &pinfo->techpenalty);
  cptr=get_int8(cptr, &pinfo->foodbox);
  cptr=get_int8(cptr, &pinfo->civstyle);
  cptr=get_int8(cptr, &pinfo->spacerace);

  cptr=get_int8(cptr, &pinfo->aqueduct_size);
  cptr=get_int8(cptr, &pinfo->sewer_size);
  cptr=get_int8(cptr, &pinfo->rtech.get_bonus_tech);
  cptr=get_int8(cptr, &pinfo->rtech.boat_fast);
  cptr=get_int8(cptr, &pinfo->rtech.cathedral_plus);
  cptr=get_int8(cptr, &pinfo->rtech.cathedral_minus);
  cptr=get_int8(cptr, &pinfo->rtech.colosseum_plus);

  remove_packet_from_buffer(&pc->buffer);
  return pinfo;
}

/*************************************************************************
...
**************************************************************************/
int send_packet_map_info(struct connection *pc, 
			 struct packet_map_info *pinfo)
{
  unsigned char buffer[MAX_PACKET_SIZE], *cptr;

  cptr=put_int8(buffer+2, PACKET_MAP_INFO);
  cptr=put_int8(cptr, pinfo->xsize);
  cptr=put_int8(cptr, pinfo->ysize);
  cptr=put_int8(cptr, pinfo->is_earth?1:0);
  put_int16(buffer, cptr-buffer);

  return send_connection_data(pc, buffer, cptr-buffer);
}

/*************************************************************************
...
**************************************************************************/
struct packet_map_info *receive_packet_map_info(struct connection *pc)
{
  unsigned char *cptr;
  struct packet_map_info *pinfo=
     fc_malloc(sizeof(struct packet_map_info));

  cptr=get_int16(pc->buffer.data, NULL);
  cptr=get_int8(cptr, NULL);

  cptr=get_int8(cptr, &pinfo->xsize);
  cptr=get_int8(cptr, &pinfo->ysize);
  cptr=get_int8(cptr, &pinfo->is_earth);

  remove_packet_from_buffer(&pc->buffer);
  return pinfo;
}

/*************************************************************************
...
**************************************************************************/
struct packet_tile_info *
receive_packet_tile_info(struct connection *pc)
{
  unsigned char *cptr;
  struct packet_tile_info *packet=
    fc_malloc(sizeof(struct packet_tile_info));

  cptr=get_int16(pc->buffer.data, NULL);
  cptr=get_int8(cptr, NULL);
  cptr=get_int8(cptr, &packet->x);
  cptr=get_int8(cptr, &packet->y);
  cptr=get_int8(cptr, &packet->type);
  cptr=get_int8(cptr, &packet->special);
  cptr=get_int8(cptr, &packet->known);
  remove_packet_from_buffer(&pc->buffer);

  return packet;
}

struct packet_unittype_info *
receive_packet_unittype_info(struct connection *pc)
{
  unsigned char *cptr;
  struct packet_unittype_info *packet=
    fc_malloc(sizeof(struct packet_unittype_info));

  cptr=get_int16(pc->buffer.data, NULL);
  cptr=get_int8(cptr, NULL);
  cptr=get_int8(cptr, &packet->type);
  cptr=get_int8(cptr, &packet->action);
  remove_packet_from_buffer(&pc->buffer);

  return packet;
}

/*************************************************************************
...
**************************************************************************/
int send_packet_tile_info(struct connection *pc, 
			  struct packet_tile_info *pinfo)

{
  unsigned char buffer[MAX_PACKET_SIZE], *cptr;

  cptr=put_int8(buffer+2, PACKET_TILE_INFO);
  cptr=put_int8(cptr, pinfo->x);
  cptr=put_int8(cptr, pinfo->y);
  cptr=put_int8(cptr, pinfo->type);
  cptr=put_int8(cptr, pinfo->special);
  cptr=put_int8(cptr, pinfo->known);
  put_int16(buffer, cptr-buffer);

  return send_connection_data(pc, buffer, cptr-buffer);
}

/**************************************************************************
...
**************************************************************************/
int send_packet_new_year(struct connection *pc, struct 
			 packet_new_year *request)
{
  unsigned char buffer[MAX_PACKET_SIZE], *cptr;
  cptr=put_int8(buffer+2, PACKET_NEW_YEAR);
  cptr=put_int32(cptr, request->year);
  put_int16(buffer, cptr-buffer);
  return send_connection_data(pc, buffer, cptr-buffer);
}

/**************************************************************************
...
**************************************************************************/
int send_packet_unittype_info(struct connection *pc, int type, int action)
{
  unsigned char buffer[MAX_PACKET_SIZE], *cptr;
  cptr=put_int8(buffer+2, PACKET_UNITTYPE_UPGRADE);
  cptr=put_int8(cptr, type);
  cptr=put_int8(cptr, action);
  put_int16(buffer, cptr-buffer);
  return send_connection_data(pc, buffer, cptr-buffer);
}

/**************************************************************************
...
**************************************************************************/
struct packet_before_new_year *
receive_packet_before_new_year(struct connection *pc)
{
  unsigned char *cptr;
  struct packet_before_new_year *packet=
    fc_malloc(sizeof(struct packet_before_new_year));

  cptr=get_int16(pc->buffer.data, NULL);
  cptr=get_int8(cptr, NULL);
  remove_packet_from_buffer(&pc->buffer);
  return packet;
}

int send_packet_before_end_year(struct connection *pc)
{
  unsigned char buffer[MAX_PACKET_SIZE], *cptr;
  cptr=put_int8(buffer+2, PACKET_BEFORE_NEW_YEAR);
  put_int16(buffer, cptr-buffer);
  return send_connection_data(pc, buffer, cptr-buffer);
}

/*************************************************************************
...
**************************************************************************/
int send_packet_unit_info(struct connection *pc, 
			    struct packet_unit_info *req)
{
  unsigned char buffer[MAX_PACKET_SIZE], *cptr;
  unsigned char pack;

  cptr=put_int8(buffer+2, PACKET_UNIT_INFO);
  cptr=put_int16(cptr, req->id);
  pack=(req->owner)|(req->veteran?0x10:0)|(req->ai?0x20:0);
  cptr=put_int8(cptr, pack);
  cptr=put_int8(cptr, req->x);
  cptr=put_int8(cptr, req->y);
  cptr=put_int16(cptr, req->homecity);
  cptr=put_int8(cptr, req->type);
  cptr=put_int8(cptr, req->movesleft);
  cptr=put_int8(cptr, req->hp);
  pack=(req->upkeep&0x3)|((req->unhappiness&0x3)<<2)|((req->activity&0xf)<<4);
  cptr=put_int8(cptr, pack);
  cptr=put_int8(cptr, req->activity_count);
  cptr=put_int8(cptr, req->goto_dest_x);
  cptr=put_int8(cptr, req->goto_dest_y);
  if(req->fuel) cptr=put_int8(cptr, req->fuel);
  
  put_int16(buffer, cptr-buffer);
  return send_connection_data(pc, buffer, cptr-buffer);
}

/*************************************************************************
...
**************************************************************************/
int send_packet_city_info(struct connection *pc, struct packet_city_info *req)
{
  unsigned char buffer[MAX_PACKET_SIZE], *cptr;
  int data;
  cptr=put_int8(buffer+2, PACKET_CITY_INFO);
  cptr=put_int16(cptr, req->id);
  cptr=put_int8(cptr, req->owner);
  cptr=put_int8(cptr, req->x);
  cptr=put_int8(cptr, req->y);
  cptr=put_string(cptr, req->name);
  
  cptr=put_int8(cptr, req->size);
  cptr=put_int8(cptr, req->ppl_happy);
  cptr=put_int8(cptr, req->ppl_content);
  cptr=put_int8(cptr, req->ppl_unhappy);
  cptr=put_int8(cptr, req->ppl_elvis);
  cptr=put_int8(cptr, req->ppl_scientist);
  cptr=put_int8(cptr, req->ppl_taxman);

  cptr=put_int8(cptr, req->food_prod);
  cptr=put_int8(cptr, req->food_surplus);
  cptr=put_int16(cptr, req->shield_prod);
  cptr=put_int16(cptr, req->shield_surplus);
  cptr=put_int16(cptr, req->trade_prod);
  cptr=put_int16(cptr, req->corruption);

  cptr=put_int16(cptr, req->luxury_total);
  cptr=put_int16(cptr, req->tax_total);
  cptr=put_int16(cptr, req->science_total);

  cptr=put_int16(cptr, req->food_stock);
  cptr=put_int16(cptr, req->shield_stock);
  cptr=put_int16(cptr, req->pollution);
  cptr=put_int8(cptr, req->currently_building);

  data=req->is_building_unit?1:0;
  data|=req->did_buy?2:0;
  data|=req->did_sell?4:0;
  data|=req->was_happy?8:0;
  data|=req->airlift?16:0;
  data|=req->diplomat_investigate?32:0; /* gentler implementation -- Syela */
  cptr=put_int8(cptr, data);

  cptr=put_city_map(cptr, (char*)req->city_map);
  cptr=put_bit_string(cptr, (char*)req->improvements);

  /* if(pc && has_capability("autoattack1", pc->capability)) */
  /* only 8 options allowed before need to extend protocol */
  cptr=put_int8(cptr, req->city_options);
  
  for(data=0;data<4;data++)  {
    if(req->trade[data])  {
      cptr=put_int16(cptr, req->trade[data]);
      cptr=put_int8(cptr,req->trade_value[data]);
    }
  }

  put_int16(buffer, cptr-buffer);

  return send_connection_data(pc, buffer, cptr-buffer);
}

/*************************************************************************
...
**************************************************************************/
struct packet_city_info *
receive_packet_city_info(struct connection *pc)
{
  unsigned char *cptr;
  struct packet_city_info *packet=
    fc_malloc(sizeof(struct packet_city_info));
  int length,data;

  cptr=get_int16(pc->buffer.data, &length);
  cptr=get_int8(cptr, NULL);
  cptr=get_int16(cptr, &packet->id);
  cptr=get_int8(cptr, &packet->owner);
  cptr=get_int8(cptr, &packet->x);
  cptr=get_int8(cptr, &packet->y);
  cptr=get_string(cptr, packet->name);
  
  cptr=get_int8(cptr, &packet->size);
  cptr=get_int8(cptr, &packet->ppl_happy);
  cptr=get_int8(cptr, &packet->ppl_content);
  cptr=get_int8(cptr, &packet->ppl_unhappy);
  cptr=get_int8(cptr, &packet->ppl_elvis);
  cptr=get_int8(cptr, &packet->ppl_scientist);
  cptr=get_int8(cptr, &packet->ppl_taxman);
  
  cptr=get_int8(cptr, &packet->food_prod);
  cptr=get_int8(cptr, &packet->food_surplus);
  if(packet->food_surplus > 127) packet->food_surplus-=256;
  cptr=get_int16(cptr, &packet->shield_prod);
  cptr=get_int16(cptr, &packet->shield_surplus);
  if(packet->shield_surplus > 32767) packet->shield_surplus-=65536;
  cptr=get_int16(cptr, &packet->trade_prod);
  cptr=get_int16(cptr, &packet->corruption);

  cptr=get_int16(cptr, &packet->luxury_total);
  cptr=get_int16(cptr, &packet->tax_total);
  cptr=get_int16(cptr, &packet->science_total);
  
  cptr=get_int16(cptr, &packet->food_stock);
  cptr=get_int16(cptr, &packet->shield_stock);
  cptr=get_int16(cptr, &packet->pollution);
  cptr=get_int8(cptr, &packet->currently_building);

  cptr=get_int8(cptr, &data);
  packet->is_building_unit = data&1;
  packet->did_buy = (data>>=1)&1;
  packet->did_sell = (data>>=1)&1;
  packet->was_happy = (data>>=1)&1;
  packet->airlift = (data>>=1)&1;
  packet->diplomat_investigate = (data>>=1)&1;
  
  cptr=get_city_map(cptr, (char*)packet->city_map);
  cptr=get_bit_string(cptr, (char*)packet->improvements);

  /* if(has_capability("autoattack1", pc->capability)) */
  cptr=get_int8(cptr, &packet->city_options);

  for(data=0;data<4;data++)  {
    if(pc->buffer.data+length-cptr < 3)  break;
    cptr=get_int16(cptr, &packet->trade[data]);
    cptr=get_int8(cptr, &packet->trade_value[data]);
  }
  for(;data<4;data++) packet->trade_value[data]=packet->trade[data]=0;

  remove_packet_from_buffer(&pc->buffer);

  return packet;
}

/*************************************************************************
...
**************************************************************************/
struct packet_unit_info *
receive_packet_unit_info(struct connection *pc)
{
  unsigned char *cptr;
  int pack;
  int length;
  struct packet_unit_info *packet=
    fc_malloc(sizeof(struct packet_unit_info));

  cptr=get_int16(pc->buffer.data, &length);
  cptr=get_int8(cptr, NULL);
  cptr=get_int16(cptr, &packet->id);
  cptr=get_int8(cptr, &pack);
  packet->owner=pack&0x0f;
  packet->veteran=(pack&0x10)?1:0; packet->ai=(pack&0x20)?1:0;
  cptr=get_int8(cptr, &packet->x);
  cptr=get_int8(cptr, &packet->y);
  cptr=get_int16(cptr, &packet->homecity);
  cptr=get_int8(cptr, &packet->type);
  cptr=get_int8(cptr, &packet->movesleft);
  cptr=get_int8(cptr, &packet->hp);
  cptr=get_int8(cptr, &pack);
  packet->activity=pack>>4; packet->upkeep=pack&0x3;
  packet->unhappiness=(pack>>2)&0x3;
  cptr=get_int8(cptr, &packet->activity_count);
  cptr=get_int8(cptr, &packet->goto_dest_x);
  cptr=get_int8(cptr, &packet->goto_dest_y);
  if(pc->buffer.data+length-cptr >= 1)
    cptr=get_int8(cptr, &packet->fuel);
  else
    packet->fuel=0;

  remove_packet_from_buffer(&pc->buffer);

  return packet;
}

/**************************************************************************
...
**************************************************************************/
struct packet_new_year *
receive_packet_new_year(struct connection *pc)
{
  unsigned char *cptr;
  struct packet_new_year *packet=
    fc_malloc(sizeof(struct packet_new_year));

  cptr=get_int16(pc->buffer.data, NULL);
  cptr=get_int8(cptr, NULL);
  cptr=get_int32(cptr, &packet->year);
  remove_packet_from_buffer(&pc->buffer);

  return packet;
}



/**************************************************************************
...
**************************************************************************/
int send_packet_move_unit(struct connection *pc, struct 
			  packet_move_unit *request)
{
  unsigned char buffer[MAX_PACKET_SIZE], *cptr;

  cptr=put_int8(buffer+2, PACKET_MOVE_UNIT);
  cptr=put_int8(cptr, request->x);
  cptr=put_int8(cptr, request->y);
  cptr=put_int16(cptr, request->unid);
  put_int16(buffer, cptr-buffer);

  return send_connection_data(pc, buffer, cptr-buffer);
}



/**************************************************************************
...
**************************************************************************/
struct packet_move_unit *receive_packet_move_unit(struct connection *pc)
{
  unsigned char *cptr;
  struct packet_move_unit *packet=
    fc_malloc(sizeof(struct packet_move_unit));

  cptr=get_int16(pc->buffer.data, NULL);
  cptr=get_int8(cptr, NULL);
  cptr=get_int8(cptr, &packet->x);
  cptr=get_int8(cptr, &packet->y);
  cptr=get_int16(cptr, &packet->unid);
  remove_packet_from_buffer(&pc->buffer);

  return packet;
}

/**************************************************************************
...
**************************************************************************/
int send_packet_req_join_game(struct connection *pc, struct 
			      packet_req_join_game *request)
{
  unsigned char buffer[MAX_PACKET_SIZE], *cptr;
  cptr=put_int8(buffer+2, PACKET_REQUEST_JOIN_GAME);
  cptr=put_string(cptr, request->name);
  cptr=put_int32(cptr, request->major_version);
  cptr=put_int32(cptr, request->minor_version);
  cptr=put_int32(cptr, request->patch_version);
  cptr=put_string(cptr, request->capability);
  put_int16(buffer, cptr-buffer);

  return send_connection_data(pc, buffer, cptr-buffer);
}

/**************************************************************************
...
**************************************************************************/
int send_packet_join_game_reply(struct connection *pc, struct 
			        packet_join_game_reply *reply)
{
  unsigned char buffer[MAX_PACKET_SIZE], *cptr;
  cptr=put_int8(buffer+2, PACKET_JOIN_GAME_REPLY);
  cptr=put_int32(cptr, reply->you_can_join);
  /* if other end is byte swapped, you_can_join should be 0,
     which is 0 even if bytes are swapped! --dwp */
  cptr=put_string(cptr, reply->message);
  cptr=put_string(cptr, reply->capability);

  /* so that old clients will understand the reply: */
  if(pc->byte_swap) {
    put_int16(buffer, swab_int16(cptr-buffer));
  } else {
    put_int16(buffer, cptr-buffer);
  }

  return send_connection_data(pc, buffer, cptr-buffer);
}

/**************************************************************************
...
**************************************************************************/
int send_packet_generic_message(struct connection *pc, int type,
				struct packet_generic_message *packet)
{
  unsigned char buffer[MAX_PACKET_SIZE], *cptr;
  cptr=put_int8(buffer+2, type);
  cptr=put_int8(cptr, packet->x);
  cptr=put_int8(cptr, packet->y);
  cptr=put_int32(cptr, packet->event);

  cptr=put_string(cptr, packet->message);
  put_int16(buffer, cptr-buffer);

  return send_connection_data(pc, buffer, cptr-buffer);
}

/**************************************************************************
...
**************************************************************************/
int send_packet_generic_integer(struct connection *pc, int type,
				struct packet_generic_integer *packet)
{
  unsigned char buffer[MAX_PACKET_SIZE], *cptr;
  cptr=put_int8(buffer+2, type);
  cptr=put_int32(cptr, packet->value);
  put_int16(buffer, cptr-buffer);
  return send_connection_data(pc, buffer, cptr-buffer);
}

/**************************************************************************
...
**************************************************************************/
struct packet_req_join_game *
receive_packet_req_join_game(struct connection *pc)
{
  unsigned char *cptr;
  struct packet_req_join_game *packet=
    fc_malloc(sizeof(struct packet_req_join_game));

  cptr=get_int16(pc->buffer.data, NULL);
  cptr=get_int8(cptr, NULL);
  cptr=get_string(cptr, packet->name);
  cptr=get_int32(cptr, &packet->major_version);
  cptr=get_int32(cptr, &packet->minor_version);
  cptr=get_int32(cptr, &packet->patch_version);
  cptr=get_string(cptr, packet->capability);

  if(pc->byte_swap) {
    swab_pint32(&packet->major_version);
    swab_pint32(&packet->minor_version);
    swab_pint32(&packet->patch_version);
  }

  remove_packet_from_buffer(&pc->buffer);

  return packet;
}

/**************************************************************************
...
**************************************************************************/
struct packet_join_game_reply *
receive_packet_join_game_reply(struct connection *pc)
{
  unsigned char *cptr;
  struct packet_join_game_reply *packet=
    fc_malloc(sizeof(struct packet_join_game_reply));

  cptr=get_int16(pc->buffer.data, NULL);
  cptr=get_int8(cptr, NULL);
  
  cptr=get_int32(cptr, &packet->you_can_join);
  cptr=get_string(cptr, packet->message);
  cptr=get_string(cptr, packet->capability);

  remove_packet_from_buffer(&pc->buffer);

  return packet;
}

/**************************************************************************
...
**************************************************************************/
struct packet_generic_message *
receive_packet_generic_message(struct connection *pc)
{
  unsigned char *cptr;
  struct packet_generic_message *packet=
    fc_malloc(sizeof(struct packet_generic_message));

  cptr=get_int16(pc->buffer.data, NULL);
  cptr=get_int8(cptr, NULL);
  cptr=get_int8(cptr, &packet->x);
  cptr=get_int8(cptr, &packet->y);
  cptr=get_int32(cptr, &packet->event);

  cptr=get_string(cptr, packet->message);
  
  remove_packet_from_buffer(&pc->buffer);

  return packet;
}

/**************************************************************************
...
**************************************************************************/
struct packet_generic_integer *
receive_packet_generic_integer(struct connection *pc)
{
  unsigned char *cptr;
  struct packet_generic_integer *packet=
    fc_malloc(sizeof(struct packet_generic_integer));

  cptr=get_int16(pc->buffer.data, NULL);
  cptr=get_int8(cptr, NULL);
  cptr=get_int32(cptr, &packet->value);

  remove_packet_from_buffer(&pc->buffer);

  return packet;
}

/**************************************************************************
...
**************************************************************************/
int send_packet_alloc_race(struct connection *pc, 
			   struct packet_alloc_race *packet)
{
  unsigned char buffer[MAX_PACKET_SIZE], *cptr;
  cptr=put_int8(buffer+2, PACKET_ALLOC_RACE);
  cptr=put_int32(cptr, packet->race_no);
  cptr=put_string(cptr, packet->name);
  put_int16(buffer, cptr-buffer);

  return send_connection_data(pc, buffer, cptr-buffer);
}

/**************************************************************************
...
**************************************************************************/
struct packet_alloc_race *
receive_packet_alloc_race(struct connection *pc)
{
  unsigned char *cptr;
  struct packet_alloc_race *packet=
    fc_malloc(sizeof(struct packet_alloc_race));

  cptr=get_int16(pc->buffer.data, NULL);
  cptr=get_int8(cptr, NULL);
  cptr=get_int32(cptr, &packet->race_no);
  cptr=get_string(cptr, packet->name);

  remove_packet_from_buffer(&pc->buffer);

  return packet;
}

/**************************************************************************
...
**************************************************************************/
int send_packet_generic_values(struct connection *pc, int type,
			       struct packet_generic_values *req)
{
  unsigned char buffer[MAX_PACKET_SIZE], *cptr;
  
  cptr=put_int8(buffer+2, type);
  cptr=put_int16(cptr, req->id);
  cptr=put_int32(cptr, req->value1);
  cptr=put_int32(cptr, req->value2);

  put_int16(buffer, cptr-buffer);

  return send_connection_data(pc, buffer, cptr-buffer);
}

/**************************************************************************
...
**************************************************************************/
struct packet_generic_values *
receive_packet_generic_values(struct connection *pc)
{
  unsigned char *cptr;
  int length;
  struct packet_generic_values *packet=
    fc_malloc(sizeof(struct packet_generic_values));

  cptr=get_int16(pc->buffer.data, &length);
  cptr=get_int8(cptr, NULL);
  cptr=get_int16(cptr, &packet->id);
  if(pc->buffer.data+length-cptr >= 4)
    cptr=get_int32(cptr, &packet->value1);
  else packet->value1=0;
  if(pc->buffer.data+length-cptr >= 4)
    cptr=get_int32(cptr, &packet->value2);
  else packet->value2=0;

  remove_packet_from_buffer(&pc->buffer);

  return packet;
}

/**************************************************************************
...
**************************************************************************/
int send_packet_ruleset_unit(struct connection *pc,
			     struct packet_ruleset_unit *packet)
{
  unsigned char buffer[MAX_PACKET_SIZE], *cptr;
  cptr=put_int8(buffer+2, PACKET_RULESET_UNIT);
  
  cptr=put_int8(cptr, packet->id);
  cptr=put_int8(cptr, packet->graphics);
  cptr=put_int8(cptr, packet->move_type);
  cptr=put_int16(cptr, packet->build_cost);
  cptr=put_int8(cptr, packet->attack_strength);
  cptr=put_int8(cptr, packet->defense_strength);
  cptr=put_int8(cptr, packet->move_rate);
  cptr=put_int8(cptr, packet->tech_requirement);
  cptr=put_int8(cptr, packet->vision_range);
  cptr=put_int8(cptr, packet->transport_capacity);
  cptr=put_int8(cptr, packet->hp);
  cptr=put_int8(cptr, packet->firepower);
  cptr=put_int8(cptr, packet->obsoleted_by);
  cptr=put_int8(cptr, packet->fuel);
  cptr=put_int32(cptr, packet->flags);
  cptr=put_int32(cptr, packet->roles);
  cptr=put_string(cptr, packet->name);
  put_int16(buffer, cptr-buffer);

  return send_connection_data(pc, buffer, cptr-buffer);
}

/**************************************************************************
...
**************************************************************************/
struct packet_ruleset_unit *
receive_packet_ruleset_unit(struct connection *pc)
{
  unsigned char *cptr;
  struct packet_ruleset_unit *packet=
    fc_malloc(sizeof(struct packet_ruleset_unit));

  cptr=get_int16(pc->buffer.data, NULL);
  cptr=get_int8(cptr, NULL);

  cptr=get_int8(cptr, &packet->id);
  cptr=get_int8(cptr, &packet->graphics);
  cptr=get_int8(cptr, &packet->move_type);
  cptr=get_int16(cptr, &packet->build_cost);
  cptr=get_int8(cptr, &packet->attack_strength);
  cptr=get_int8(cptr, &packet->defense_strength);
  cptr=get_int8(cptr, &packet->move_rate);
  cptr=get_int8(cptr, &packet->tech_requirement);
  cptr=get_int8(cptr, &packet->vision_range);
  cptr=get_int8(cptr, &packet->transport_capacity);
  cptr=get_int8(cptr, &packet->hp);
  cptr=get_int8(cptr, &packet->firepower);
  cptr=get_int8(cptr, &packet->obsoleted_by);
  if (packet->obsoleted_by>127) packet->obsoleted_by-=256;
  cptr=get_int8(cptr, &packet->fuel);
  cptr=get_int32(cptr, &packet->flags);
  cptr=get_int32(cptr, &packet->roles);
  cptr=get_string(cptr, packet->name);

  remove_packet_from_buffer(&pc->buffer);

  return packet;
}


/**************************************************************************
...
**************************************************************************/
int send_packet_ruleset_tech(struct connection *pc,
			     struct packet_ruleset_tech *packet)
{
  unsigned char buffer[MAX_PACKET_SIZE], *cptr;
  cptr=put_int8(buffer+2, PACKET_RULESET_TECH);
  
  cptr=put_int8(cptr, packet->id);
  cptr=put_int8(cptr, packet->req[0]);
  cptr=put_int8(cptr, packet->req[1]);
  cptr=put_string(cptr, packet->name);
  put_int16(buffer, cptr-buffer);

  return send_connection_data(pc, buffer, cptr-buffer);
}

/**************************************************************************
...
**************************************************************************/
struct packet_ruleset_tech *
receive_packet_ruleset_tech(struct connection *pc)
{
  unsigned char *cptr;
  struct packet_ruleset_tech *packet=
    fc_malloc(sizeof(struct packet_ruleset_tech));

  cptr=get_int16(pc->buffer.data, NULL);
  cptr=get_int8(cptr, NULL);

  cptr=get_int8(cptr, &packet->id);
  cptr=get_int8(cptr, &packet->req[0]);
  cptr=get_int8(cptr, &packet->req[1]);
  cptr=get_string(cptr, packet->name);

  remove_packet_from_buffer(&pc->buffer);

  return packet;
}

/**************************************************************************
...
**************************************************************************/
int send_packet_ruleset_building(struct connection *pc,
			     struct packet_ruleset_building *packet)
{
  unsigned char buffer[MAX_PACKET_SIZE], *cptr;
  cptr=put_int8(buffer+2, PACKET_RULESET_BUILDING);
  
  cptr=put_int8(cptr, packet->id);
  cptr=put_int8(cptr, packet->is_wonder);
  cptr=put_int8(cptr, packet->tech_requirement);
  cptr=put_int16(cptr, packet->build_cost);
  cptr=put_int8(cptr, packet->shield_upkeep);
  cptr=put_int8(cptr, packet->obsolete_by);
  cptr=put_int8(cptr, packet->variant);
  cptr=put_string(cptr, packet->name);
  put_int16(buffer, cptr-buffer);

  return send_connection_data(pc, buffer, cptr-buffer);
}

/**************************************************************************
...
**************************************************************************/
struct packet_ruleset_building *
receive_packet_ruleset_building(struct connection *pc)
{
  unsigned char *cptr;
  struct packet_ruleset_building *packet=
    fc_malloc(sizeof(struct packet_ruleset_building));

  cptr=get_int16(pc->buffer.data, NULL);
  cptr=get_int8(cptr, NULL);

  cptr=get_int8(cptr, &packet->id);
  cptr=get_int8(cptr, &packet->is_wonder);
  cptr=get_int8(cptr, &packet->tech_requirement);
  cptr=get_int16(cptr, &packet->build_cost);
  cptr=get_int8(cptr, &packet->shield_upkeep);
  cptr=get_int8(cptr, &packet->obsolete_by);
  cptr=get_int8(cptr, &packet->variant);
  cptr=get_string(cptr, packet->name);

  remove_packet_from_buffer(&pc->buffer);

  return packet;
}

/**************************************************************************
...
**************************************************************************/
int send_packet_spaceship_info(struct connection *pc,
			       struct packet_spaceship_info *packet)
{
  unsigned char buffer[MAX_PACKET_SIZE], *cptr;
  cptr=put_int8(buffer+2, PACKET_SPACESHIP_INFO);
  
  cptr=put_int8(cptr, packet->player_num);
  cptr=put_int8(cptr, packet->sship_state);
  cptr=put_int8(cptr, packet->structurals);
  cptr=put_int8(cptr, packet->components);
  cptr=put_int8(cptr, packet->modules);
  cptr=put_int8(cptr, packet->fuel);
  cptr=put_int8(cptr, packet->propulsion);
  cptr=put_int8(cptr, packet->habitation);
  cptr=put_int8(cptr, packet->life_support);
  cptr=put_int8(cptr, packet->solar_panels);
  cptr=put_int16(cptr, packet->launch_year);
  cptr=put_int8(cptr, (packet->population/1000));
  cptr=put_int32(cptr, packet->mass);
  cptr=put_int32(cptr, (int) (packet->support_rate*10000));
  cptr=put_int32(cptr, (int) (packet->energy_rate*10000));
  cptr=put_int32(cptr, (int) (packet->success_rate*10000));
  cptr=put_int32(cptr, (int) (packet->travel_time*10000));
  cptr=put_bit_string(cptr, (char*)packet->structure);

  put_int16(buffer, cptr-buffer);
  return send_connection_data(pc, buffer, cptr-buffer);
}

/**************************************************************************
...
**************************************************************************/
struct packet_spaceship_info *
receive_packet_spaceship_info(struct connection *pc)
{
  int tmp;
  unsigned char *cptr;
  struct packet_spaceship_info *packet=
    fc_malloc(sizeof(struct packet_spaceship_info));
  
  cptr=get_int16(pc->buffer.data, NULL);
  cptr=get_int8(cptr, NULL);

  cptr=get_int8(cptr, &packet->player_num);
  cptr=get_int8(cptr, &packet->sship_state);
  cptr=get_int8(cptr, &packet->structurals);
  cptr=get_int8(cptr, &packet->components);
  cptr=get_int8(cptr, &packet->modules);
  cptr=get_int8(cptr, &packet->fuel);
  cptr=get_int8(cptr, &packet->propulsion);
  cptr=get_int8(cptr, &packet->habitation);
  cptr=get_int8(cptr, &packet->life_support);
  cptr=get_int8(cptr, &packet->solar_panels);
  cptr=get_int16(cptr, &packet->launch_year);
  
  if(packet->launch_year > 32767) packet->launch_year-=65536;
  
  cptr=get_int8(cptr, &packet->population);
  packet->population *= 1000;
  cptr=get_int32(cptr, &packet->mass);
  
  cptr=get_int32(cptr, &tmp);
  packet->support_rate = tmp * 0.0001;
  cptr=get_int32(cptr, &tmp);
  packet->energy_rate = tmp * 0.0001;
  cptr=get_int32(cptr, &tmp);
  packet->success_rate = tmp * 0.0001;
  cptr=get_int32(cptr, &tmp);
  packet->travel_time = tmp * 0.0001;

  cptr=get_bit_string(cptr, (char*)packet->structure);
  
  remove_packet_from_buffer(&pc->buffer);

  return packet;
}

/**************************************************************************
...
**************************************************************************/
int send_packet_spaceship_action(struct connection *pc,
				 struct packet_spaceship_action *packet)
{
  unsigned char buffer[MAX_PACKET_SIZE], *cptr;
  cptr=put_int8(buffer+2, PACKET_SPACESHIP_ACTION);
  
  cptr=put_int8(cptr, packet->action);
  cptr=put_int8(cptr, packet->num);

  put_int16(buffer, cptr-buffer);
  return send_connection_data(pc, buffer, cptr-buffer);
}

/**************************************************************************
...
**************************************************************************/
struct packet_spaceship_action *
receive_packet_spaceship_action(struct connection *pc)
{
  unsigned char *cptr;
  struct packet_spaceship_action *packet=
    fc_malloc(sizeof(struct packet_spaceship_action));
  
  cptr=get_int16(pc->buffer.data, NULL);
  cptr=get_int8(cptr, NULL);

  cptr=get_int8(cptr, &packet->action);
  cptr=get_int8(cptr, &packet->num);

  remove_packet_from_buffer(&pc->buffer);

  return packet;
}


/**************************************************************************
remove the packet from the buffer
**************************************************************************/
void remove_packet_from_buffer(struct socket_packet_buffer *buffer)
{
  int len;
  get_int16(buffer->data, &len);
  memcpy(buffer->data, buffer->data+len, buffer->ndata-len);
  buffer->ndata-=len;
}

/********************************************************************
...
********************************************************************/
void connection_do_buffer(struct connection *pc)
{
  if(pc)
    pc->send_buffer.do_buffer_sends++;
}

/********************************************************************
...
********************************************************************/
void connection_do_unbuffer(struct connection *pc)
{
  if(pc) {
    pc->send_buffer.do_buffer_sends--;
    if(pc->send_buffer.do_buffer_sends==0)
      flush_connection_send_buffer(pc);
  }
}

/********************************************************************
  flush'em
********************************************************************/
void flush_connection_send_buffer(struct connection *pc)
{
  if(pc) {
    if(pc->send_buffer.ndata) {
      if(write(pc->sock, pc->send_buffer.data, pc->send_buffer.ndata)!=
	 pc->send_buffer.ndata) {
	freelog(LOG_NORMAL, "failed writing data to socket");
      }
      pc->send_buffer.ndata=0;
    }
  }
}


/********************************************************************
  write data to socket
********************************************************************/
int send_connection_data(struct connection *pc, unsigned char *data, int len)
{
  if(pc) {
    if(pc->send_buffer.do_buffer_sends) {
      if(10*MAX_PACKET_SIZE-pc->send_buffer.ndata >= len) { /* room for more?*/
	memcpy(pc->send_buffer.data+pc->send_buffer.ndata, data, len);
	pc->send_buffer.ndata+=len;
      }
      else {
	flush_connection_send_buffer(pc);
	memcpy(pc->send_buffer.data, data, len);
	pc->send_buffer.ndata=len;
      }
    }
    else {
      if(write(pc->sock, data, len)!=len) {
	freelog(LOG_NORMAL, "failed writing data to socket");
	return -1;
      }
      
    }

  }
  return 0;
}



/********************************************************************
read data from socket, and check if a packet is ready

returns: -1  an error occured - you should close the socket
          >0 : #bytes read 
********************************************************************/
int read_socket_data(int sock, struct socket_packet_buffer *buffer)
{
  int didget;

  if((didget=read(sock, buffer->data+buffer->ndata, 
		  MAX_PACKET_SIZE-buffer->ndata))<=0)
    return -1;
 
  buffer->ndata+=didget;
  
  freelog(LOG_DEBUG, "didget:%d", didget); 

  return didget;
}


