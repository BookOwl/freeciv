/********************************************************************** 
Freeciv - Copyright (C) 2004 - The Freeciv Project
   This program is free software; you can redistribute it and / or modify
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

#include <assert.h>  
#include <fcntl.h>
#include <stdio.h>
#include <signal.h>             /* SIGTERM and kill */
#include <string.h>
#include <time.h>

#ifdef WIN32_NATIVE
#include <windows.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>		/* fchmod */
#endif

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>		/* fchmod */
#endif

#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#include "fcintl.h"
#include "mem.h"
#include "netintf.h"
#include "rand.h"
#include "registry.h"
#include "support.h"
#include "civclient.h"
#include "climisc.h"
#include "clinet.h"
#include "packhand_gen.h"

#include "chatline_common.h"
#include "connectdlg_g.h"
#include "connectdlg_common.h"
#include "tilespec.h"

#define WAIT_BETWEEN_TRIES 100000 /* usecs */ 
#define NUMBER_OF_TRIES 500
  
#ifdef WIN32_NATIVE
/* FIXME: this is referenced directly in gui-win32/connectdlg.c. */
HANDLE server_process = INVALID_HANDLE_VALUE;
HANDLE loghandle = INVALID_HANDLE_VALUE;
#else
static pid_t server_pid = - 1;
#endif

char player_name[MAX_LEN_NAME];
char *current_filename = NULL;

static bool client_has_hack = FALSE;

int internal_server_port;

const char *skill_level_names[NUM_SKILL_LEVELS] = { 
  N_("novice"),
  N_("easy"), 
  N_("normal"), 
  N_("hard")
 ,N_("experimental")
};

/************************************************************************** 
The general chain of events:

Two distinct paths are taken depending on the choice of mode: 

if the user selects the multi- player mode, then a packet_req_join_game 
packet is sent to the server. It is either successful or not. The end.

If the user selects a single- player mode (either a new game or a save game) 
then: 
   1. the packet_req_join_game is sent.
   2. on receipt, if we can join, then a challenge packet is sent to the
      server, so we can get hack level control.
   3. if we can't get hack, then we get dumped to multi- player mode. If
      we can, then:
      a. for a new game, we send a series of packet_generic_message packets
         with commands to start the game.
      b. for a saved game, we send the load command with a 
         packet_generic_message, then we send a PACKET_PLAYER_LIST_REQUEST.
         the response to this request will tell us if the game was loaded or
         not. if not, then we send another load command. if so, then we send
         a series of packet_generic_message packets with commands to start 
         the game.
**************************************************************************/ 

/************************************************************************** 
Tests if the client has started the server.
**************************************************************************/ 
bool is_server_running()
{ 
#ifdef WIN32_NATIVE
  return (server_process != INVALID_HANDLE_VALUE);
#else    
  return (server_pid > 0);
#endif
} 

/************************************************************************** 
  Returns TRUE if the client has hack access.
**************************************************************************/
bool can_client_access_hack(void)
{
  return client_has_hack;
}

/************************************************************************** 
Kills the server if the client has started it (FIXME: atexit handler?)
**************************************************************************/ 
void client_kill_server()
{
  if (is_server_running()) {
#ifdef WIN32_NATIVE
    TerminateProcess(server_process, 0);
    CloseHandle(server_process);
    if (loghandle != INVALID_HANDLE_VALUE) {
      CloseHandle(loghandle);
    }
    server_process = INVALID_HANDLE_VALUE;
    loghandle = INVALID_HANDLE_VALUE;
#else
    kill(server_pid, SIGTERM);
    waitpid(server_pid, NULL, WUNTRACED);
    server_pid = - 1;
#endif    
  }
  client_has_hack = FALSE;
}   

/**************************************************************** 
forks a server if it can. returns FALSE is we find we couldn't start
the server.
This is so system-intensive that it's *nix only.  VMS and Windows 
code will come later 
*****************************************************************/ 
bool client_start_server(void)
{
#if !defined(HAVE_WORKING_FORK) && !defined(WIN32_NATIVE)
  /* Can't do much without fork */
  return FALSE;
#else
  char buf[512];
  int connect_tries = 0;
# ifdef WIN32_NATIVE
  STARTUPINFO si;
  PROCESS_INFORMATION pi;
  
  char options[512];
  char cmdline1[512];
  char cmdline2[512];
  char cmdline3[512];
  char logcmdline[512];
  char scriptcmdline[512];
# endif

  /* only one server (forked from this client) shall be running at a time */
  /* This also resets client_has_hack. */
  client_kill_server();

  append_output_window(_("Starting server..."));

  /* find a free port */ 
  internal_server_port = find_next_free_port(DEFAULT_SOCK_PORT);

# ifdef HAVE_WORKING_FORK
  server_pid = fork();
  
  if (server_pid == 0) {
    int fd, argc = 0;
    const int max_nargs = 13;
    char *argv[max_nargs + 1], port_buf[32];

    /* inside the child */

    /* Set up the command-line parameters. */
    my_snprintf(port_buf, sizeof(port_buf), "%d", internal_server_port);
    argv[argc++] = "civserver";
    argv[argc++] = "-p";
    argv[argc++] = port_buf;
    argv[argc++] = "-q";
    argv[argc++] = "1";
    argv[argc++] = "-e";
    if (logfile) {
      argv[argc++] = "--debug";
      argv[argc++] = "3";
      argv[argc++] = "--log";
      argv[argc++] = logfile;
    }
    if (scriptfile) {
      argv[argc++] = "--read";
      argv[argc++] = scriptfile;
    }
    argv[argc] = NULL;
    assert(argc <= max_nargs);

    /* avoid terminal spam, but still make server output available */ 
    fclose(stdout);
    fclose(stderr);

    /* include the port to avoid duplication */
    if (logfile) {
      fd = open(logfile, O_WRONLY | O_CREAT);

      if (fd != 1) {
        dup2(fd, 1);
      }
      if (fd != 2) {
        dup2(fd, 2);
      }
      fchmod(1, 0644);
    }

    /* If it's still attatched to our terminal, things get messed up, 
      but civserver needs *something* */ 
    fclose(stdin);
    fd = open("/dev/null", O_RDONLY);
    if (fd != 0) {
      dup2(fd, 0);
    }

    /* these won't return on success */ 
    execvp("./ser", argv);
    execvp("./server/civserver", argv);
    execvp("civserver", argv);
    
    /* This line is only reached if civserver cannot be started, 
     * so we kill the forked process.
     * Calling exit here is dangerous due to X11 problems (async replies) */ 
    _exit(1);
  } 
# else
#  ifdef WIN32_NATIVE
  if (logfile) {
    loghandle = CreateFile(logfile, GENERIC_WRITE, FILE_SHARE_READ, NULL,
			   OPEN_ALWAYS, 0, NULL);
  }

  ZeroMemory(&si, sizeof(si));
  si.cb = sizeof(si);
  si.hStdOutput = loghandle;
  si.hStdInput = INVALID_HANDLE_VALUE;
  si.hStdError = loghandle;
  si.dwFlags = STARTF_USESTDHANDLES;

  /* Set up the command-line parameters. */
  logcmdline[0] = 0;
  scriptcmdline[0] = 0;

  if (logfile) {
    my_snprintf(logcmdline, sizeof(logcmdline), " --debug 3 --log %s",
		logfile);
  }
  if (scriptfile) {
    my_snprintf(scriptcmdline, sizeof(scriptcmdline),  " --read %s",
		scriptfile);
  }

  my_snprintf(options, sizeof(options), "-p %d -q 1 -e%s%s",
	      internal_server_port, logcmdline, scriptcmdline);
  my_snprintf(cmdline1, sizeof(cmdline1), "./ser %s", options);
  my_snprintf(cmdline2, sizeof(cmdline2), "./server/civserver %s", options);
  my_snprintf(cmdline3, sizeof(cmdline3), "civserver %s", options);

  if (!CreateProcess(NULL, cmdline1, NULL, NULL, TRUE,
		     DETACHED_PROCESS | NORMAL_PRIORITY_CLASS,
		     NULL, NULL, &si, &pi) 
      && !CreateProcess(NULL, cmdline2, NULL, NULL, TRUE,
			DETACHED_PROCESS | NORMAL_PRIORITY_CLASS,
			NULL, NULL, &si, &pi) 
      && !CreateProcess(NULL, cmdline3, NULL, NULL, TRUE,
			DETACHED_PROCESS | NORMAL_PRIORITY_CLASS,
			NULL, NULL, &si, &pi)) {
    append_output_window(_("Couldn't start the server. You'll have to "
			   "start one manually. Sorry..."));
    return FALSE;
  }
  
  server_process = pi.hProcess;

#  endif
# endif
 
  /* a reasonable number of tries */ 
  while (connect_to_server(user_name, "localhost", internal_server_port, 
                           buf, sizeof(buf)) == -1) {
    myusleep(WAIT_BETWEEN_TRIES);
#ifdef HAVE_WORKING_FORK
#ifndef WIN32_NATIVE
    if (waitpid(server_pid, NULL, WNOHANG) != 0) {
      break;
    }
#endif
#endif
    if (connect_tries++ > NUMBER_OF_TRIES) {
      break;
    }
  }

  /* weird, but could happen, if server doesn't support new startup stuff
   * capabilities won't help us here... */ 
  if (!aconnection.used) {
    /* possible that server is still running. kill it */ 
    client_kill_server();

    append_output_window(_("Couldn't connect to the server. We probably "
                           "couldn't start it from here. You'll have to "
                           "start one manually. Sorry..."));
    return FALSE;
  }

  /* We set the topology to match the view.
   *
   * When a typical player launches a game, he wants the map orientation to
   * match the tileset orientation.  So if you use an isometric tileset you
   * get an iso-map and for a classic tileset you get a classic map.  In
   * both cases the map wraps in the X direction by default.
   *
   * This works with hex maps too now.  A hex map always has is_isometric
   * set.  An iso-hex map has hex_height != 0, while a non-iso hex map
   * has hex_width != 0.
   *
   * Setting the option here is a bit of a hack, but so long as the client
   * has sufficient permissions to do so (it doesn't have HACK access yet) it
   * is safe enough.  Note that if you load a savegame the topology will be
   * set but then overwritten during the load. */
  my_snprintf(buf, sizeof(buf), "/set topology %d",
	      (TF_WRAPX
	       | ((is_isometric && hex_height == 0) ? TF_ISO : 0)
	       | ((hex_width != 0 || hex_height != 0) ? TF_HEX : 0)));
  send_chat(buf);

  return TRUE;
#endif
}

/**************************************************************** 
if the client is capable of 'wanting hack', then the server will 
send the client a filename in the packet_join_game_reply packet.

this function creates the file with a suitably random number in it 
and then sends the filename and number to the server. If the server 
can open and read the number, then the client is given hack access.
*****************************************************************/ 
void send_client_wants_hack(char * filename)
{
  struct section_file file;

  /* find some entropy */ 
  int entropy = myrand(MAX_UINT32) - time(NULL);

  /* we don't want zero */ 
  if (entropy == 0) {
    entropy++;
  }

  section_file_init(&file);
  secfile_insert_int(&file, entropy, "challenge.entropy");
  section_file_save(&file, filename, 0);      
  section_file_free(&file);

  /* tell the server what we put into the file */ 
  dsend_packet_single_want_hack_req(&aconnection, entropy);
}

/**************************************************************** 
handle response (by the server) if the client has got hack or not.
*****************************************************************/ 
void handle_single_want_hack_reply(bool you_have_hack)
{
  if (you_have_hack) {
    append_output_window(_("We have control of the server "
                         "(command access level hack)"));
    client_has_hack = TRUE;
  } else if (is_server_running()) {
    /* only output this if we started the server and we NEED hack */
    append_output_window(_("We can't take control of server, "
                         "attempting to kill it."));
    client_kill_server();
  }
}

/**************************************************************** 
send server commands to start a saved game.
*****************************************************************/ 
void send_start_saved_game(void)
{   
  char buf[MAX_LEN_MSG];

  send_chat("/set timeout 0");
  send_chat("/set autotoggle 1");
  my_snprintf(buf, sizeof(buf), "/take \"%s\" \"%s\"",
      	      user_name, player_name);
  send_chat(buf);
  send_chat("/start");
}

/**************************************************************** 
send server command to save game.
*****************************************************************/ 
void send_save_game(char *filename)
{   
  char message[MAX_LEN_MSG];

  if (filename) {
    my_snprintf(message, MAX_LEN_MSG, "/save %s", filename);
  } else {
    my_snprintf(message, MAX_LEN_MSG, "/save");
  }

  send_chat(message);
}

/**************************************************************** 
 Should be called by gui after disconnecting from local server
*****************************************************************/ 
void disconnected_from_local_server() 
{
  char buf[1024];
  assert(is_server_running());
  my_snprintf(buf, sizeof(buf), 
              _("A local server is still running on port %d. Use "
                "\"Connect to Network Game\" to connect to it."),
	      internal_server_port);
  append_output_window(buf);
}
