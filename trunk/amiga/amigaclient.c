/* This is a preparse main function for freeciv client.
The file client/civclient.c must be compiled with main defined to civ_main,
or this will not work.

This method is hopefully a lot better portable than the previous method, which
depended on init, exit, cleanup and destructor methods of compiler.

The main() functions either gets
1) C-style commandline arguments or
2) argc == 0 and argv == WBStartup message.

If your compiler does not support this, create a preparse function, which
produces that results. Maybe rename main() to main2() and name your preparse
function main() and call main2() afterwards. This depends on your compiler.
*/

#ifdef __SASC /* remove usage of strerror prototype */
#define strerror strerror_unuse
#endif

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>

#include <libraries/usergroup.h>
#include <workbench/startup.h>

#include <clib/alib_protos.h>

#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/icon.h>
#include <proto/socket.h>
#include <proto/usergroup.h>
#ifdef MIAMI_SDK
#include <bsdsocket/socketbasetags.h>
#else /* AmiTCP */
#include <amitcp/socketbasetags.h>
#endif

#define MUIMASTER_NAME    "muimaster.library"
#define MUIMASTER_VMIN    11

#define SOCKETNAME "bsdsocket.library"
#define SOCKETVERSION 3     /* minimum bsdsocket version to use */
#define USERGROUPVERSION 1

/* the external definitions */
extern int civ_main(int, void *); /* The prototype of the REAL freeciv main function */
extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;

struct Library *SocketBase = 0;
struct Library *UserGroupBase = 0;
struct Library *GuiGFXBase = 0;
struct Library *MUIMasterBase = 0;
struct Library *DataTypesBase = 0;
struct Library *IconBase;
static char *stdargv[1] = {"civclient"}; /* standard arg, if WB parsing failed */

#ifdef __SASC
/* Stack for the server */
__near LONG __stack = 120000;
#endif

/*********************************************************/

static void civ_exitfunc(void)
{
  if(MUIMasterBase) CloseLibrary(MUIMasterBase);
  if(UserGroupBase) CloseLibrary(UserGroupBase);
  if(SocketBase) CloseLibrary(SocketBase);
  if(GuiGFXBase) CloseLibrary(GuiGFXBase);
  if(DataTypesBase) CloseLibrary(DataTypesBase);
}

int main(int argc, char **argv)
{
  int ret = 20;

  /* using malloc needs to free calls */
  if(!argc) /* called from WB */
  {
    if(IconBase = OpenLibrary("icon.library", 0L))
    {
      struct DiskObject *dob;
      struct WBArg *wba = ((struct WBStartup *) argv)->sm_ArgList;
      BPTR dir = CurrentDir(wba->wa_Lock);

      if((dob = GetDiskObject(wba->wa_Name)))
      {
        int i = 1, j;
      
        if(dob->do_ToolTypes)
        {
          for(j = 0; dob->do_ToolTypes[j]; ++j)
          {
            if(*dob->do_ToolTypes[j] != '(' && *dob->do_ToolTypes[j] != ' ' &&
            *dob->do_ToolTypes[j] != ';')
            {
              ++argc;
              if(strchr(dob->do_ToolTypes[j], '='))
                ++argc;
            }
          }
        }

        ++argc; /* the program name */
        if((argv = (char**)malloc(sizeof(char*)*argc)))
        {
          argv[0] = stdargv[0]; /* the file name */

          for(j = 0; argc && dob->do_ToolTypes[j]; ++j)
          {
            char *ttype = dob->do_ToolTypes[j], *dest;
            int len = strlen(ttype), u;
            char *second = strchr(ttype,'=');

            if(*ttype == '(' || *ttype == ' ' || *ttype == ';')
              ;
            else if((dest = argv[i++] = (char*)malloc(len+4)))
            {
              if(second)
                len = second-ttype;
              dest[0] = dest[1] = '-';
              strcpy(dest+2,ttype);
              dest[2+len] = 0;

              for(u = 0; dest[u]; ++u)
                dest[u]=tolower(dest[u]);
              if(second)
                argv[i++] = dest+3+len;
            }
            else
              argc = 0; /* error */
          }
        }
        else
          argc=0; /* error */

        FreeDiskObject(dob);
      } /* get disk object */
      CurrentDir(dir);
      CloseLibrary(IconBase);
    } /* OpenLibary */
  } /* WB start */

  atexit(civ_exitfunc); /* we need to free the stuff on exit()! */
  if((SocketBase = OpenLibrary(SOCKETNAME,SOCKETVERSION)))
  {
    if((GuiGFXBase = OpenLibrary("guigfx.library",16)))
    {
      if(GuiGFXBase->lib_Version == 16)
      {
        if(GuiGFXBase->lib_Revision < 2)
        {
          printf("guigfx.library version 16 with revision smaller than 2 found.\nIf the graphics look corrupt, please install a newer version\n");
        }
      }
      if((DataTypesBase = OpenLibrary("datatypes.library",39)))
      {
        int h_errno;
        SocketBaseTags(SBTM_SETVAL(SBTC_ERRNOPTR(sizeof(errno))), &errno,
                       SBTM_SETVAL(SBTC_HERRNOLONGPTR), &h_errno,
                       SBTM_SETVAL(SBTC_LOGTAGPTR), "civclient",
                       TAG_END);
        
        if((UserGroupBase = OpenLibrary(USERGROUPNAME, USERGROUPVERSION)))
        {
          ug_SetupContextTags("civclient",
                   UGT_INTRMASK, SIGBREAKB_CTRL_C,
                   UGT_ERRNOPTR(sizeof(errno)), &errno,
                   TAG_END);

          if((MUIMasterBase = OpenLibrary(MUIMASTER_NAME,MUIMASTER_VMIN)))
          {
            /* Reserve 0 for stdin */
            Dup2Socket(-1,0);

            /* all went well, call main function */
            if(!argc)
              ret = civ_main(1, stdargv);
            else
              ret = civ_main(argc, argv);

          }
          else
            printf("Couldn't open " MUIMASTER_NAME "!\n");
        }
        else
          printf("Couldn't open " USERGROUPNAME "!\n");
      }
      else
        printf("Couldn't open datatypes.library version 39\n");
    }
    else
      printf("Couldn't open guigfx.library version 16!\n");
  }
  else
    printf("Couldn't open " SOCKETNAME "!\nPlease start a TCP/IP stack.\n");

  exit (ret);
}

/**************************************************************************
 usleep() function. (because select() is too slow and not needed to be
 emulated for the client). Complete.
**************************************************************************/
void usleep(unsigned long usec)
{
  TimeDelay(0,0,usec);
}

/**************************************************************************
 strerror() which also understand the non ANSI C errors. Complete.
 Note. SAS uses another prototype definition.
**************************************************************************/
#ifdef __SASC /* remove usage of strerror prototype */
#undef strerror
#endif

const char *strerror(unsigned int error)
{
  ULONG taglist[3];

  taglist[0] = SBTM_GETVAL(SBTC_ERRNOSTRPTR);
  taglist[1] = error;
  taglist[2] = TAG_END;

  SocketBaseTagList((struct TagItem *)taglist);
  return (char *)taglist[1];
}

/**************************************************************************
 select() emulation (only sockets)
**************************************************************************/
#ifdef MIAMI_SDK
long select(long nfds,fd_set *readfds, fd_set *writefds, fd_set *exeptfds, void *timeout)
#else
int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exeptfds, struct timeval *timeout)
#endif
{
  return WaitSelect(nfds, readfds, writefds, exeptfds, timeout, NULL);
}

/**************************************************************************
 read() emulation. Only for real bsd sockets (created with socket()).
 (The server doesn't use it for other descriptors)
**************************************************************************/
int read(int fd, char *buf, int len)
{
  if(fd) return recv(fd,buf,len,0);
  return -1;
}

/**************************************************************************
 write() emulation. Only for real bsd sockets (created with socket()).
 (The server doesn't use it for other descriptors)
**************************************************************************/
int write(int fd, char *buf, int len)
{
  if(fd) return send(fd,buf,len,0);
  return 0;
}

/**************************************************************************
 close() emulation. Only for real bsd sockets (created with socket()).
 (The server doesn't use it for other descriptors)
**************************************************************************/
void close(int fd)
{
  if(fd) CloseSocket(fd);
}

/**************************************************************************
 ioctl() stub
**************************************************************************/
int ioctl(int fd, unsigned int request, char *argp)
{
  return IoctlSocket(fd, request, argp);
}

