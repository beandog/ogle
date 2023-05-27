#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>

#include <ogle/dvdcontrol.h>
#include <ogle/msgevents.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "debug_print.h"

#include "bindings.h"


extern DVDNav_t *nav;
extern int bookmarks_autosave;

int prevpg_timeout = 1;

int digit_timeout = 0;
int number_timeout = 0;

int default_skip_seconds = 15;
int skip_seconds = 15;

static int fs = 0;
static int isPaused = 0;
static double speed = 1.0;

struct action_number{
  int valid;
  int32_t nr;
  struct timeval timestamp;
};

static struct action_number user_nr = { 0, 0 };


void actionUpperButtonSelect(void *data)
{
  DVDUpperButtonSelect(nav);	  
}

void actionLowerButtonSelect(void *data)
{
  DVDLowerButtonSelect(nav);
}

void actionLeftButtonSelect(void *data)
{
  DVDLeftButtonSelect(nav);
}

void actionRightButtonSelect(void *data)
{
  DVDRightButtonSelect(nav);
}

void actionButtonActivate(void *data)
{
  struct action_number *user = (struct action_number *)data;
  if(user != NULL && user->valid && (user->nr >= 0)) {
    DVDButtonSelectAndActivate(nav, user->nr);
  } else { 
    DVDButtonActivate(nav);
  }
  if(user != NULL) {
    user->valid = 0;
  }
}

void actionMenuCallTitle(void *data)
{
  DVDMenuCall(nav, DVD_MENU_Title);
}

void actionMenuCallRoot(void *data)
{
  DVDMenuCall(nav, DVD_MENU_Root);
}

void actionMenuCallSubpicture(void *data)
{
  DVDMenuCall(nav, DVD_MENU_Subpicture);
}

void actionMenuCallAudio(void *data)
{
  DVDMenuCall(nav, DVD_MENU_Audio);
}

void actionMenuCallAngle(void *data)
{
  DVDMenuCall(nav, DVD_MENU_Angle);
}

void actionMenuCallPTT(void *data)
{
  DVDMenuCall(nav, DVD_MENU_Part);
}

void actionResume(void *data)
{
  DVDResume(nav);
}

void actionPauseToggle(void *data)
{
  
  if(isPaused) {
    DVDPauseOff(nav);
    isPaused = 0;
  } else {
    DVDPauseOn(nav);
    isPaused = 1;
  }
  
}

void actionPauseOn(void *data)
{
    DVDPauseOn(nav);
    isPaused = 1;
}

void actionPauseOff(void *data)
{
    DVDPauseOff(nav);
    isPaused = 0;
}

void actionSubpictureToggle(void *data)
{
  DVDResult_t res;
  int spu_nr;
  DVDStream_t spu_this;
  DVDBool_t spu_shown;

  res = DVDGetCurrentSubpicture(nav, &spu_nr, &spu_this, &spu_shown);

  if(res != DVD_E_Ok) {
    return;
  }

  if(spu_shown == DVDTrue) {
    DVDSetSubpictureState(nav, DVDFalse);
  } else {
    DVDSetSubpictureState(nav, DVDTrue);
  }
}

struct timeval pg_timestamp = {0, 0};

void actionNextPG(void *data)
{
  struct timeval curtime;

  gettimeofday(&curtime, NULL);
  pg_timestamp = curtime;

  DVDNextPGSearch(nav);
}

void actionPrevPG(void *data)
{
  struct timeval curtime;

  long diff;
  
  gettimeofday(&curtime, NULL);
  diff = curtime.tv_sec - pg_timestamp.tv_sec;
  pg_timestamp = curtime;  

  if((prevpg_timeout && (diff > prevpg_timeout))) {
    DVDTopPGSearch(nav);
  } else {
    DVDPrevPGSearch(nav);
  }
}

void autosave_bookmark(void)
{
  DVDBookmark_t *bm;
  unsigned char id[16];
  char volid[33];
  int volid_type;
  char *state = NULL;
  int n;
  
  if(bookmarks_autosave) {
    if(DVDGetDiscID(nav, id) != DVD_E_Ok) {
      NOTE("%s", "GetDiscID failed\n");
      return;
    }
    
    if(DVDGetVolumeIdentifiers(nav, 0, &volid_type, volid, NULL) != DVD_E_Ok) {
      DNOTE("%s", "GetVolumeIdentifiers failed\n");
      volid_type = 0;
    }
    
    if(DVDGetState(nav, &state) == DVD_E_Ok) {
      
      if((bm = DVDBookmarkOpen(id, NULL, 1)) == NULL) {
	if(errno != ENOENT) {
	  NOTE("%s", "BookmarkOpen failed: ");
	  perror("");
	}
	free(state);
	return;
      }
    
      n = DVDBookmarkGetNr(bm);
      
      if(n == -1) {
	NOTE("%s", "DVDBookmarkGetNr failed\n");
      } else if(n > 0) {
	for(n--; n >= 0; n--) {
	  char *appinfo;
	  if(DVDBookmarkGet(bm, n, NULL, NULL,
			    "common", &appinfo) != -1) {
	    if(appinfo) {
	      if(!strcmp(appinfo, "autobookmark")) {
		if(DVDBookmarkRemove(bm, n) == -1) {
		  NOTE("%s", "DVDBookmarkRemove failed\n");
		}
	      }
	      free(appinfo);
	    }
	  } else {
	    NOTE("%s", "DVDBookmarkGet failed\n");
	  }
	}
      }
      
      
      
      if(DVDBookmarkAdd(bm, state, NULL, "common", "autobookmark") == -1) {
	DNOTE("%s", "BookmarkAdd failed\n");
	DVDBookmarkClose(bm);
	free(state);
	return;
      }
      free(state);
      
      if(volid_type != 0) {
	char *disccomment = NULL;
	if(DVDBookmarkGetDiscComment(bm, &disccomment) != -1) {
	  if((disccomment == NULL) || (disccomment[0] == '\0')) {
	    if(DVDBookmarkSetDiscComment(bm, volid) == -1) {
	      DNOTE("%s", "SetDiscComment failed\n");
	    }
	  }
	  if(disccomment) {
	    free(disccomment);
	  }
	}
      }
      if(DVDBookmarkSave(bm, 0) == -1) {
	NOTE("%s", "BookmarkSave failed\n");
      }
      DVDBookmarkClose(bm);
    }
  }
}

void actionQuit(void *data)
{
  DVDResult_t res;
  
  autosave_bookmark();

  res = DVDCloseNav(nav);
  if(res != DVD_E_Ok ) {
    DVDPerror("DVDCloseNav", res);
  }
  exit(0);
}

void actionFullScreenToggle(void *data)
{
  fs = !fs;
  if(fs) {
    DVDSetZoomMode(nav, ZoomModeFullScreen);
  } else {
    DVDSetZoomMode(nav, ZoomModeResizeAllowed);
  }
}

void actionForwardScan(void *data)
{
  DVDForwardScan(nav, 1.0);
}


void actionPlay(void *data)
{
  if(isPaused) {
    isPaused = 0;
    DVDPauseOff(nav);
  }
  speed = 1.0;
  DVDForwardScan(nav, speed);
}


void actionFastForward(void *data)
{
  if(isPaused) {
    isPaused = 0;
    DVDPauseOff(nav);
  }

  if((speed >= 1.0) && (speed < 8.0)) {
    speed +=0.5;
  } else if(speed < 1.0) {
    speed = 1.5;
  }
  DVDForwardScan(nav, speed);
}

void actionSlowForward(void *data)
{
  if(isPaused) {
    isPaused = 0;
    DVDPauseOff(nav);
  }

  if(speed > 1.0) {
    speed = 0.5;
  } else if((speed > 0.1) && (speed <= 1.0)) {
    speed /= 2.0;
  }
  DVDForwardScan(nav, speed);
}


void actionFaster(void *data)
{
  if(isPaused) {
    isPaused = 0;
    DVDPauseOff(nav);
  }
  
  if((speed >= 1.0) && (speed < 8.0)) {
    speed += 0.5;
  } else if(speed < 1.0) {
    speed *= 2.0;
  }
  DVDForwardScan(nav, speed);
}

void actionSlower(void *data)
{
  if(isPaused) {
    isPaused = 0;
    DVDPauseOff(nav);
  }

  if(speed > 1.0) {
    speed -= 0.5;
  } else if((speed > 0.1) && (speed <= 1.0)) {
    speed /= 2.0;
  }
  DVDForwardScan(nav, speed);
}


void actionBookmarkAdd(void *data)
{
  DVDBookmark_t *bm;
  unsigned char id[16];
  char *state = NULL;
  char volid[33];
  int volid_type;
  char *disccomment = NULL;
  
  if(DVDGetDiscID(nav, id) != DVD_E_Ok) {
    NOTE("%s", "GetDiscID failed\n");
    return;
  }
  
  if(DVDGetVolumeIdentifiers(nav, 0, &volid_type, volid, NULL) != DVD_E_Ok) {
    DNOTE("%s", "GetVolumeIdentifiers failed\n");
    volid_type = 0;
  }
  
  if(DVDGetState(nav, &state) == DVD_E_Ok) {
    if((bm = DVDBookmarkOpen(id, NULL, 1)) == NULL) {
      if(errno != ENOENT) {
	NOTE("%s", "BookmarkOpen failed: ");
	perror("");
      }
      free(state);
      return;
    }
    if(DVDBookmarkAdd(bm, state, NULL, NULL, NULL) == -1) {
      DNOTE("%s", "BookmarkAdd failed\n");
      DVDBookmarkClose(bm);
      free(state);
      return;
    }
    free(state);
    if(volid_type != 0) {
      if(DVDBookmarkGetDiscComment(bm, &disccomment) != -1) {
	if((disccomment == NULL) || (disccomment[0] == '\0')) {
	  if(DVDBookmarkSetDiscComment(bm, volid) == -1) {
	    DNOTE("%s", "SetDiscComment failed\n");
	  }
	}
	if(disccomment) {
	  free(disccomment);
	}
      }
    }
    if(DVDBookmarkSave(bm, 0) == -1) {
      NOTE("%s", "BookmarkSave failed\n");
    }
    DVDBookmarkClose(bm);
  }
}


void actionBookmarkRemove(void *data)
{
  struct action_number *user = (struct action_number *)data;
  DVDBookmark_t *bm;
  unsigned char id[16];
  int n;
  
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
    if(user != NULL) {
      if(user->valid && (user->nr < n) && (user->nr > 0)) {
	n = user->nr;
      }
      user->valid = 0;
    }

    if(DVDBookmarkRemove(bm, n-1) != -1) {
      if(DVDBookmarkSave(bm, 0) == -1) {
	NOTE("%s", "BookmarkSave failed\n");
      }
    } else {
      NOTE("%s", "DVDBookmarkRemove failed\n");
    }
  }
  
  DVDBookmarkClose(bm);
}

void actionBookmarkRestore(void *data)
{
  struct action_number *user = (struct action_number *)data;
  DVDBookmark_t *bm;
  unsigned char id[16];
  char *state = NULL;
  int n;


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
    if(user != NULL) {
      if(user->valid && (user->nr < n) && (user->nr > 0)) {
	n = user->nr;
      }
      user->valid = 0;
    }
    if(DVDBookmarkGet(bm, n-1, &state, NULL, NULL, NULL) != -1) {
      if(state) {
	if(DVDSetState(nav, state) != DVD_E_Ok) {
	  NOTE("%s", "DVDSetState failed\n");
	}
	free(state);
      }
    } else {
      NOTE("%s", "BookmarkGet failed\n");
    }
  }
  DVDBookmarkClose(bm);
}


void actionDigit(void *data)
{
  struct timeval curtime;
  long diff;
  
  gettimeofday(&curtime, NULL);
  diff = curtime.tv_sec - user_nr.timestamp.tv_sec;
  
  if(!user_nr.valid || (digit_timeout && (diff > digit_timeout))) {
    user_nr.nr = 0;
  }
  
  user_nr.timestamp = curtime;
  user_nr.nr = user_nr.nr * 10 + *(uint8_t *)data;
  user_nr.valid = 1;
}


void actionSkipForward(void *data)
{
  struct action_number *user = (struct action_number *)data;

  if(user != NULL) {
    if((user->nr == 0) || (user->nr < 0)) {
      skip_seconds = default_skip_seconds;
    } else {
      skip_seconds = user->nr;
    }
  } 
  
  DVDTimeSkip(nav, skip_seconds);
}

void actionSkipBackward(void *data)
{
  struct action_number *user = (struct action_number *)data;
  if(user != NULL) {
    if((user->nr == 0) || (user->nr < 0)) {
      skip_seconds = default_skip_seconds;
    } else {
      skip_seconds = user->nr;
    }
  } 
  
  DVDTimeSkip(nav, -skip_seconds);
}


void actionSaveScreenshot(void *data)
{
  DVDSaveScreenshot(nav, ScreenshotModeWithoutSPU, NULL);
}


void actionSaveScreenshotWithSPU(void *data)
{
  DVDSaveScreenshot(nav, ScreenshotModeWithSPU, NULL);
}


void do_number_action(void *vfun)
{
  void (*number_action)(void *) = vfun;
  struct timeval curtime;
  long diff;
  struct action_number *number = NULL;
  
  gettimeofday(&curtime, NULL);
  diff = curtime.tv_sec - user_nr.timestamp.tv_sec;
  
  if(number_timeout && (diff > number_timeout)) {
    user_nr.valid = 0;
  }
  if(user_nr.valid) {
    number = &user_nr;
  }
  number_action(number);
  user_nr.valid = 0;  
}

void do_action(void *fun)
{
  void (*action)(void *) = fun;

  user_nr.valid = 0;  
  action(NULL);
}

typedef struct {
  PointerEventType event_type;
  unsigned int button;
  unsigned int modifier_mask;
  void (*fun)(void *);
} pointer_mapping_t;

static unsigned int pointer_mappings_index = 0;
//static unsigned int nr_pointer_mappings = 0;

static pointer_mapping_t *pointer_mappings;

typedef struct {
  KeySym keysym;
  void (*fun)(void *);
  void *arg;
} ks_map_t;

static unsigned int ks_maps_index = 0;
static unsigned int nr_ks_maps = 0;

static ks_map_t *ks_maps;

static const uint8_t digits[10] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };

typedef struct {
  char *str;
  void (*fun)(void *);
  void *ptr;
} action_mapping_t;

static action_mapping_t actions[] = {
  { "Play", do_action, actionPlay },
  { "PauseToggle", do_action, actionPauseToggle },
  { "Stop", NULL, NULL },
  { "FastForward", do_action, actionFastForward },
  { "SlowForward", do_action, actionSlowForward },
  { "Faster", do_action, actionFaster },
  { "Slower", do_action, actionSlower },
  { "NextPG", do_action, actionNextPG },
  { "PrevPG", do_action, actionPrevPG },
  { "UpperButton", do_action, actionUpperButtonSelect },
  { "LowerButton", do_action, actionLowerButtonSelect },
  { "LeftButton", do_action, actionLeftButtonSelect },
  { "RightButton", do_action, actionRightButtonSelect },
  { "ButtonActivate", do_number_action, actionButtonActivate },
  { "TitleMenu", do_action, actionMenuCallTitle },
  { "RootMenu", do_action, actionMenuCallRoot },
  { "AudioMenu", do_action, actionMenuCallAudio },
  { "AngleMenu", do_action, actionMenuCallAngle },
  { "PTTMenu", do_action, actionMenuCallPTT },
  { "SubtitleMenu", do_action, actionMenuCallSubpicture },
  { "Resume", do_action, actionResume },
  { "FullScreenToggle", do_action, actionFullScreenToggle },
  { "SubtitleToggle", do_action, actionSubpictureToggle },
  { "Quit", do_action, actionQuit },
  { "BookmarkAdd", do_action, actionBookmarkAdd },
  { "BookmarkRemove", do_number_action, actionBookmarkRemove },
  { "BookmarkRestore", do_number_action, actionBookmarkRestore },
  { "DigitZero", actionDigit, (void *)&digits[0] },
  { "DigitOne",  actionDigit, (void *)&digits[1] },
  { "DigitTwo",  actionDigit, (void *)&digits[2] },
  { "DigitThree",actionDigit, (void *)&digits[3] },
  { "DigitFour", actionDigit, (void *)&digits[4] },
  { "DigitFive", actionDigit, (void *)&digits[5] },
  { "DigitSix",  actionDigit, (void *)&digits[6] },
  { "DigitSeven",actionDigit, (void *)&digits[7] },
  { "DigitEight",actionDigit, (void *)&digits[8] },
  { "DigitNine", actionDigit, (void *)&digits[9] },
  { "SkipForward", do_number_action, actionSkipForward },
  { "SkipBackward", do_number_action, actionSkipBackward },
  { "SaveScreenshot", do_action, actionSaveScreenshot },
  { "SaveScreenshotWithSPU", do_action, actionSaveScreenshotWithSPU },
  { NULL, NULL }
};


void do_pointer_action(pointer_event_t *p_ev)
{
  int n;
  
  for(n = 0; n < pointer_mappings_index; n++) {
    if((pointer_mappings[n].event_type == p_ev->type) &&
       (pointer_mappings[n].modifier_mask == p_ev->modifier_mask)) {
      if(pointer_mappings[n].fun != NULL) {
	pointer_mappings[n].fun(p_ev);
      }
      return;
    }
  }
  
  return;
}


void do_keysym_action(KeySym keysym)
{
  int n;

  for(n = 0; n < ks_maps_index; n++) {
    if(ks_maps[n].keysym == keysym) {
      if(ks_maps[n].fun != NULL) {
	ks_maps[n].fun(ks_maps[n].arg);
      }
      return;
    }
  }
  return;
}


void remove_keysym_binding(KeySym keysym)
{
  int n;
  
  for(n = 0; n < ks_maps_index; n++) {
    if(ks_maps[n].keysym == keysym) {
      ks_maps[n].keysym = NoSymbol;
      ks_maps[n].fun = NULL;
      ks_maps[n].arg = NULL;
      return;
    }
  }
}

void add_keysym_binding(KeySym keysym, void(*fun)(void *), void *arg)
{
  int n;
  
  for(n = 0; n < ks_maps_index; n++) {
    if(ks_maps[n].keysym == keysym) {
      ks_maps[n].fun = fun;
      ks_maps[n].arg = arg;
      return;
    }
  }

  if(ks_maps_index >= nr_ks_maps) {
    nr_ks_maps+=32;
    ks_maps = (ks_map_t *)realloc(ks_maps, sizeof(ks_map_t)*nr_ks_maps);
  }
  
  ks_maps[ks_maps_index].keysym = keysym;
  ks_maps[ks_maps_index].fun = fun;
  ks_maps[ks_maps_index].arg = arg;
  
  ks_maps_index++;
  

  return;
}
  
void add_keybinding(char *key, char *action)
{
  KeySym keysym;
  int n = 0;
  
  keysym = XStringToKeysym(key);
  
  if(keysym == NoSymbol) {
    WARNING("add_keybinding(): '%s' not a valid keysym\n", key);
    return;
  }
  
  if(!strcmp("NoAction", action)) {
    remove_keysym_binding(keysym);
    return;
  }
    
  for(n = 0; actions[n].str != NULL; n++) {
    if(!strcmp(actions[n].str, action)) {
      if(actions[n].fun != NULL) {
	add_keysym_binding(keysym, actions[n].fun, actions[n].ptr);
      }
      return;
    }
  }
  
  WARNING("add_keybinding(): No such action: '%s'\n", action);
  
  return;
}

/*
void add_pointerbinding(char *, char *action)
{
  KeySym keysym;
  int n = 0;
  
  keysym = XStringToKeysym(key);
  
  if(keysym == NoSymbol) {
  WARNING("add_keybinding(): '%s' not a valid keysym\n", key);
  return;
  }
  
  if(!strcmp("NoAction", action)) {
    remove_keysym_binding(keysym);
    return;
  }
    
  for(n = 0; actions[n].str != NULL; n++) {
    if(!strcmp(actions[n].str, action)) {
      if(actions[n].fun != NULL) {
	add_keysym_binding(keysym, actions[n].fun);
      }
      return;
    }
  }
  
  WARNING("add_keybinding(): No such action: '%s'\n", action);
  
  return;
}
*/
