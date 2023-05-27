/* Ogle - A video player
 * Copyright (C) 2000, 2001 Björn Englund, Håkan Hjort
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <X11/Xlib.h>
#include <string.h>

#include "debug_print.h"
/**
 * Does what it sounds like. 
 */

Bool
look_for_good_xscreensaver()
{
  int i = 0;
  char response[500];
  pid_t pid;
  int mypipe[2];
  int br;

  if(pipe(mypipe) == -1)
    {
      perror("pipe");
      exit(EXIT_FAILURE);
    }

  pid = fork();
  switch (pid)
    {
    case -1:
      /* Fork failed! */
      perror("fork");
      exit(EXIT_FAILURE);
      break;
    case 0:
      /* This is the xscreensaver-command process */

      close(mypipe[0]);               /* close read end of pipe               */
      dup2(mypipe[1], STDOUT_FILENO); /* make 1 same as write-to end of pipe  */
      dup2(mypipe[1], STDERR_FILENO); /* make 2 same as write-to end of pipe  */
      close(mypipe[1]);               /* close excess fildes                  */
      execlp("xscreensaver-command", "xscreensaver-command", "--version", NULL);

      /* still around?  exec failed. */

      if(errno == ENOENT)
         fprintf(stderr,"ENOENT\n");
      else
         perror("exec");
      exit(EXIT_FAILURE);  /* no flush */
    default:
      /* We're the parent, carry on. */
      break;
    }

  /* Don't care about status, parse output instead. */
  waitpid(pid, NULL, 0);

  if((br = read(mypipe[0], response+i, 500-i)) < 0) {
    br = 0;
  }
  
  response[br] = '\0';
  
  close(mypipe[0]);

  if(!strncmp(response, "xscreensaver-command: no screensaver is running", 46))
    {
      fprintf(stderr,
              "Xscreensaver not running.\n");
      return(False);
    }
  else if(!strncmp(response, "ENOENT", 6))
    {
     fprintf(stderr,
              "xscreensaver-command not found.\n");
     return(False);
    }
  else if(!strncmp(response, "XScreenSaver ", 13))
    {
      if (atof(response+13) >= 3.34)
       {
         /* We win */

         /* Strip trailing newline */
         response[strlen(response)-1] = 0;
	 
	 DNOTE("Found a running xscreensaver, version \"%s\" (>= 3.34).\n",
	       response+13);
         return(True);
       }
     else
       {
         DNOTE("%s", "Found too old version of xscreensaver!\n");
         DNOTE("%s", "\tGet at least version 3.34!\n");
         return(False);
       }
    }
  else
    {
      DNOTE("Got weird data from xscreensaver-command:\"%s\"\n",
	    response);
      return(False);
    }
}

/**
 *  This is the function that actually tells the screensaver not
 *  to do anything for another while.
 */

void
nudge_xscreensaver()
{
  static pid_t pid = 0;

//  fprintf(stderr, "Muting the screensaver...\n");

  /* Wait for the last one we spawned. */

  if(pid != 0)
    {
      waitpid(pid, NULL, 0);
    }

  pid = fork();
  switch (pid)
    {
    case -1:
      /* Fork failed! */
      perror("fork");
      exit(EXIT_FAILURE);
      break;
    case 0:
      /* This is the xscreensaver-command process */

      close(STDERR_FILENO);       /* close stderr. I'm not interested.    */
      close(STDOUT_FILENO);       /* close stdout. I'm not interested.    */
      execlp("xscreensaver-command",
             "xscreensaver-command", "-deactivate", NULL);
      perror("exec");       /* still around?  exec failed           */
      _exit(EXIT_FAILURE);  /* no flush                             */
      break;
    default:
      /* Main process */
      break;
   }
}

