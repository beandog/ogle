/* Ogle - A video player
 * Copyright (C) 2000, 2001 Håkan Hjort
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <X11/Xlib.h>

#include <ogle/dvdcontrol.h>
#include "debug_print.h"

#include "xsniffer.h"
#include "interpret_config.h"
#include "bindings.h"


DVDNav_t *nav;
char *dvd_path;
int bookmarks_autosave = 0;
int bookmarks_autoload = 0;

int msgqid;
extern int win;

char *program_name;
int dlevel;

void usage()
{
  fprintf(stderr, "Usage: %s [-m <msgid>] path\n", 
          program_name);
}

void autoload_bookmark(void) {
  DVDBookmark_t *bm;
  unsigned char id[16];
  char *state = NULL;
  int n;

  if(!bookmarks_autoload) {
    return;
  }

  if(DVDGetDiscID(nav, id) != DVD_E_Ok) {
    NOTE("%s", "GetDiscID failed\n");
    return;
  }
  
  if((bm = DVDBookmarkOpen(id, NULL, 0)) == NULL) {
    if(errno != ENOENT) {
      NOTE("%s", "BookmarkOpen failed: ");
      perror("");
    }
    return;
  }
  
  n = DVDBookmarkGetNr(bm);
  
  if(n == -1) {
    NOTE("%s", "DVDBookmarkGetNr failed\n");
  } else if(n > 0) {
    char *appinfo;
    if(DVDBookmarkGet(bm, n-1, &state, NULL,
		      "common", &appinfo) != -1) {
      if(state) {
	if(appinfo && !strcmp(appinfo, "autobookmark")) {
	  if(DVDSetState(nav, state) != DVD_E_Ok) {
	    NOTE("%s", "DVDSetState failed\n");
	  }
	}
	free(state);
      }
      if(appinfo) {
	free(appinfo);
      }
    } else {
      NOTE("%s", "BookmarkGet failed\n");
    }
  }
  DVDBookmarkClose(bm);
}


int main (int argc, char *argv[])
{
  DVDResult_t res;

  int c;
  
  program_name = argv[0];
  GET_DLEVEL();

  /* Parse command line options */
  while ((c = getopt(argc, argv, "m:h?")) != EOF) {
    switch (c) {
    case 'm':
      msgqid = atoi(optarg);
      break;
    case 'h':
    case '?':
      usage();
      return 1;
    }
  }
  
  if(argc - optind > 1){
    usage();
    exit(1);
  }
  
  
  if(msgqid !=-1) { // ignore sending data.
    sleep(1);
    res = DVDOpenNav(&nav, msgqid);
    if(res != DVD_E_Ok ) {
      DVDPerror("DVDOpen:", res);
      exit(1);
    }
  }

  interpret_config();

  if(argc - optind == 1) {
    if(dvd_path != NULL) {
      free(dvd_path);
    }
    dvd_path = argv[optind];
  }
  
  if(dvd_path == NULL) {
    fprintf(stderr, "ERROR: dvd_cli: You must specify the path to your dvd device, either in the oglerc or on the command line\n");
    exit(-1);
  }
  
  res = DVDSetDVDRoot(nav, dvd_path);
  if(res != DVD_E_Ok) {
    DVDPerror("DVDSetDVDRoot:", res);
    exit(1);
  }
  
  DVDRequestInput(nav,
		  INPUT_MASK_KeyPress | INPUT_MASK_ButtonPress |
		  INPUT_MASK_PointerMotion);
  
  autoload_bookmark();
  
  xsniff_mouse(NULL);
  exit(0);
}


