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
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <signal.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SIGNAL_H
#include <sys/signal.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_PWD_H
#include <pwd.h>
#endif

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef HAVE_SYS_UIO_H
#include <sys/uio.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#ifdef HAVE_LIBREADLINE
#include <readline/readline.h>
#include <readline/history.h>
#endif

#include "log.h"
#include "mem.h"
#include "netintf.h"
#include "packets.h"
#include "shared.h"
#include "support.h"

#include "civserver.h"
#include "console.h"
#include "stdinhand.h"

#include "sernet.h"

struct connection connections[MAX_NUM_CONNECTIONS];

#ifdef GENERATING_MAC      /* mac network globals */
TEndpointInfo serv_info;
EndpointRef serv_ep;
#else
static int sock;
extern int port;
#endif
extern int force_end_of_sniff;

#ifdef __VMS
#include <descrip.h>
#include <iodef.h>
#include <stsdef.h>
#include <starlet.h>
#include <lib$routines.h>
#include <efndef.h>
static unsigned long int tt_chan;
static char input_char = 0;
static char got_input = 0;
void user_interrupt_callback();
#endif


static int server_accept_connection(int sockfd);


#ifdef HAVE_LIBREADLINE
/****************************************************************************/

#define HISTORY_FILENAME  ".civserver_history"
#define HISTORY_LENGTH    100

static char *history_file = NULL;

static int readline_handled_input = 0;

/*****************************************************************************
...
*****************************************************************************/
static void handle_readline_input_callback(char *line)
{
  if (line) {
    if (*line)
      add_history(line);
    con_prompt_enter();		/* just got an 'Enter' hit */
    handle_stdin_input((struct connection*)NULL, line);
  }
  readline_handled_input = 1;
}

#endif
/*****************************************************************************
...
*****************************************************************************/
void close_connection(struct connection *pconn)
{
  /* safe to do these even if not in lists: */
  conn_list_unlink(&game.all_connections, pconn);
  conn_list_unlink(&game.est_connections, pconn);
  conn_list_unlink(&game.game_connections, pconn);
  
  close(pconn->sock);
  pconn->used = 0;
  pconn->established = 0;
  pconn->player = NULL;
  pconn->access_level = ALLOW_NONE;
  free(pconn->buffer);
  free(pconn->send_buffer);
  pconn->buffer = NULL;
  pconn->send_buffer = NULL;
}

/*****************************************************************************
...
*****************************************************************************/
void close_connections_and_socket(void)
{
  int i;
  
  for(i=0; i<MAX_NUM_CONNECTIONS; i++) {
    if(connections[i].used) {
      close_connection(&connections[i]);
    }
  }
  close(sock);

#ifdef HAVE_LIBREADLINE
  if (history_file) {
    write_history(history_file);
    history_truncate_file(history_file, HISTORY_LENGTH);
  }
#endif
}

/*****************************************************************************
  Used for errors and other strangeness.  As well as some direct uses, is
  passed to packet routines as callback for when packet sending has a write
  error.  Closes the connection cleanly, calling lost_connection_to_client()
  to clean up server variables, notify other clients, etc.
*****************************************************************************/
static void close_socket_callback(struct connection *pc)
{
  lost_connection_to_client(pc);
  close_connection(pc);
}

/*****************************************************************************
Get and handle:
- new connections,
- input from connections,
- input from server operator in stdin
Returns:
  0 if went past end-of-turn timeout
  2 if force_end_of_sniff found to be set
  1 otherwise (got and processed something?)
This function also handles prompt printing, via the con_prompt_*
functions.  That is, other functions should not need to do so.  --dwp
*****************************************************************************/
int sniff_packets(void)
{
  int i;
  int max_desc;
  fd_set readfs, exceptfs;
  struct timeval tv;
  static int year;
#ifdef SOCKET_ZERO_ISNT_STDIN
  char buf[BUF_SIZE+1];
  char *bufptr = buf;
#endif

  con_prompt_init();

#ifdef HAVE_LIBREADLINE
  {
    static int readline_initialized = 0;

    if (!readline_initialized) {
      char *home_dir = user_home_dir();
      if (home_dir) {
	history_file =
	  fc_malloc(strlen(home_dir) + 1 + strlen(HISTORY_FILENAME) + 1);
	if (history_file) {
	  strcpy(history_file, home_dir);
	  strcat(history_file, "/");
	  strcat(history_file, HISTORY_FILENAME);
	  using_history();
	  read_history(history_file);
	}
      }

      rl_initialize();
      rl_callback_handler_install("> ", handle_readline_input_callback);
      rl_attempted_completion_function = freeciv_completion;

      readline_initialized = 1;
    }
  }
#endif

  if(year!=game.year) {
    if (server_state == RUN_GAME_STATE) year=game.year;
  }
  if (!game.timeout)
    game.turn_start = time(NULL);
  
  while(1) {
    con_prompt_on();		/* accepting new input */
    
    if(force_end_of_sniff) {
      force_end_of_sniff=0;
      con_prompt_off();
      return 2;
    }
    
    tv.tv_sec=1;
    tv.tv_usec=0;
    
    MY_FD_ZERO(&readfs);
    MY_FD_ZERO(&exceptfs);
#ifndef SOCKET_ZERO_ISNT_STDIN
    FD_SET(0, &readfs);	
#endif
    FD_SET(sock, &readfs);
    FD_SET(sock, &exceptfs);
    max_desc=sock;
    
    for(i=0; i<MAX_NUM_CONNECTIONS; i++) {
      if(connections[i].used) {
	FD_SET(connections[i].sock, &readfs);
	FD_SET(connections[i].sock, &exceptfs);
        max_desc=MAX(connections[i].sock, max_desc);
      }
    }
    con_prompt_off();		/* output doesn't generate a new prompt */
    
    if(select(max_desc+1, &readfs, NULL, &exceptfs, &tv)==0) { /* timeout */
      send_server_info_to_metaserver(0,0);
      if((game.timeout) 
	&& (time(NULL)>game.turn_start + game.timeout)
	&& (server_state == RUN_GAME_STATE)){
	con_prompt_off();
	return 0;
      }
#ifdef __VMS
      {
	struct { short numchars; char firstchar; char reserved; int reserved2; } ttchar;
	unsigned long status;
	status = sys$qiow(EFN$C_ENF,tt_chan,IO$_SENSEMODE|IO$M_TYPEAHDCNT,0,0,0,
			  &ttchar,sizeof(ttchar),0,0,0,0);
	if (!$VMS_STATUS_SUCCESS(status)) lib$stop(status);
	if (ttchar.numchars)
	  FD_SET(0, &readfs);
	else
	  continue;
      }
#else
#ifdef SOCKET_ZERO_ISNT_STDIN
    if (feof(stdin))
#endif
      continue;
#endif
    }
    if (!game.timeout)
      game.turn_start = time(NULL);

    if(FD_ISSET(sock, &exceptfs)) {	     /* handle Ctrl-Z suspend/resume */
      continue;
    }
    if(FD_ISSET(sock, &readfs)) {	     /* new players connects */
      freelog(LOG_VERBOSE, "got new connection");
      if(server_accept_connection(sock)==-1) {
	freelog(LOG_NORMAL, "failed accepting connection");
      }
    }
    for(i=0; i<MAX_NUM_CONNECTIONS; i++) {   /* check for freaky players */
      if(connections[i].used && FD_ISSET(connections[i].sock, &exceptfs)) {
 	freelog(LOG_NORMAL, "cut connection %s due to exception data",
		conn_description(&connections[i]));
	close_socket_callback(&connections[i]);
      }
    }
#ifndef SOCKET_ZERO_ISNT_STDIN
    if(FD_ISSET(0, &readfs)) {    /* input from server operator */
#ifdef HAVE_LIBREADLINE
      rl_callback_read_char();
      if (readline_handled_input) {
	readline_handled_input = 0;
	con_prompt_enter_clear();
      }
      continue;
#else
      int didget;
      char buf[BUF_SIZE+1];
      
      if((didget=read(0, buf, BUF_SIZE))==-1) {
	freelog(LOG_FATAL, "read from stdin failed");
	exit(1);
      }
      *(buf+didget)='\0';
      con_prompt_enter();	/* will need a new prompt, regardless */
      handle_stdin_input((struct connection *)NULL, buf);
#endif
    }
#else
    if(!feof(stdin)) {    /* input from server operator */
      /* fetch chars until \n or run out of space in buffer */
      while ((*bufptr=fgetc(stdin)) != EOF) {
          if (*bufptr == '\n') *bufptr = '\0';
          if (*bufptr == '\0') {
              bufptr = buf;
              con_prompt_enter(); /* will need a new prompt, regardless */
              handle_stdin_input((struct connection *)NULL, buf);
              break;
          }
          if ((bufptr-buf) <= BUF_SIZE) bufptr++; /* prevent overrun */
      }
    }
#endif
    else {                             /* input from a player */
      for(i=0; i<MAX_NUM_CONNECTIONS; i++) {
  	struct connection *pconn = &connections[i];
	if(pconn->used && FD_ISSET(pconn->sock, &readfs)) {
	  if(read_socket_data(pconn->sock, pconn->buffer)>=0) {
	    char *packet;
	    int type;
	    while((packet=get_packet_from_connection(pconn, &type))) {
	      handle_packet_input(pconn, packet, type);
	    }
	  } else {
	    close_socket_callback(pconn);
	  }
	}
      }
    }
    break;
  }
  con_prompt_off();

  if((game.timeout) 
     && (time(NULL)>game.turn_start + game.timeout))
    return 0;
  return 1;
}

/********************************************************************
  Make up a name for the connection, before we get any data from
  it to use as a sensible name.  Name will be 'c' + integer,
  guaranteed not to be the same as any other connection name,
  nor player name nor user name, nor connection id (avoid possible
  confusions).   Returns pointer to static buffer, and fills in
  (*id) with chosen value.
********************************************************************/
static const char *makeup_connection_name(int *id)
{
  static unsigned short i = 0;
  static char name[MAX_LEN_NAME];

  for(;;) {
    if (i==(unsigned short)-1) i++;              /* don't use 0 */
    my_snprintf(name, sizeof(name), "c%u", (unsigned int)++i);
    if (!find_player_by_name(name)
	&& !find_player_by_user(name)
	&& !find_conn_by_id(i)
	&& !find_conn_by_name(name)) {
      *id = i;
      return name;
    }
  }
}
  
/********************************************************************
  Server accepts connection from client:
  Low level socket stuff, and basic-initialize the connection struct.
  Returns 0 on success, -1 on failure (bad accept(), or too many
  connections).
********************************************************************/
static int server_accept_connection(int sockfd)
{
  int fromlen;
  int new_sock;
  struct sockaddr_in fromend;
  struct hostent *from;
  int i;

  fromlen = sizeof(fromend);

  if ((new_sock=accept(sockfd, (struct sockaddr *) &fromend, &fromlen)) == -1) {
    freelog(LOG_VERBOSE, "accept failed: %s", mystrerror(errno));
    return -1;
  }

  my_nonblock(new_sock);

  from=gethostbyaddr((char*)&fromend.sin_addr, sizeof(fromend.sin_addr),
		     AF_INET);

  for(i=0; i<MAX_NUM_CONNECTIONS; i++) {
    struct connection *pconn = &connections[i];
    if (!pconn->used) {
      pconn->used = 1;
      pconn->sock = new_sock;
      pconn->established = 0;
      pconn->observer = 0;
      pconn->player = NULL;
      pconn->buffer = new_socket_packet_buffer();
      pconn->send_buffer = new_socket_packet_buffer();
      pconn->first_packet = 1;
      pconn->byte_swap = 0;
      pconn->capability[0] = '\0';
      pconn->access_level = (i?default_access_level:first_access_level);

      sz_strlcpy(pconn->name, makeup_connection_name(&pconn->id));
      sz_strlcpy(pconn->addr,
		 (from ? from->h_name : inet_ntoa(fromend.sin_addr)));

      conn_list_insert_back(&game.all_connections, pconn);
  
      freelog(LOG_VERBOSE, "connection (%s) from %s", pconn->name, pconn->addr);
      return 0;
    }
  }

  freelog(LOG_FATAL, "maximum number of connections reached");
  return -1;
}

/********************************************************************
 open server socket to be used to accept client connections
********************************************************************/
int server_open_socket(void)
{
  /* setup socket address */
  struct sockaddr_in src;
  int opt;

  /* broken pipes are ignored. */
#ifdef HAVE_SIGPIPE
  signal (SIGPIPE, SIG_IGN);
#endif

  if((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    freelog(LOG_FATAL, "socket failed: %s", mystrerror(errno));
    exit(1);
  }

  opt=1; 
  if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, 
		(char*)&opt, sizeof(opt)) == -1) {
    freelog(LOG_FATAL, "SO_REUSEADDR failed: %s", mystrerror(errno));
  }

  memset(&src, 0, sizeof(src));
  src.sin_family = AF_INET;
  src.sin_addr.s_addr = htonl(INADDR_ANY);
  src.sin_port = htons(port);

  if(bind(sock, (struct sockaddr *) &src, sizeof (src)) == -1) {
    freelog(LOG_FATAL, "bind failed: %s", mystrerror(errno));
    exit(1);
  }

  if(listen(sock, MAX_NUM_CONNECTIONS) == -1) {
    freelog(LOG_FATAL, "listen failed: %s", mystrerror(errno));
    exit(1);
  }

  close_socket_set_callback(close_socket_callback);
  return 0;
}


/********************************************************************
...
********************************************************************/
void init_connections(void)
{
  int i;
  for(i=0; i<MAX_NUM_CONNECTIONS; i++) { 
    struct connection *pconn = &connections[i];
    pconn->used = 0;
    conn_list_init(&pconn->self);
    conn_list_insert(&pconn->self, pconn);
  }
#ifdef __VMS
  {
    unsigned long status;
    $DESCRIPTOR (tt_desc, "SYS$INPUT");
    status = sys$assign(&tt_desc,&tt_chan,0,0);
    if (!$VMS_STATUS_SUCCESS(status)) lib$stop(status);
  }
#endif
}
