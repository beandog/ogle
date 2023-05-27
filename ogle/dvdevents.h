#ifndef DVDEVENTS_H_INCLUDED
#define DVDEVENTS_H_INCLUDED

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
#include <limits.h>
#include <ogle/dvd.h>

typedef enum {
  DVDCtrlButtonMask = 0x01000000,
  DVDCtrlMouseMask  = 0x02000000
} DVDCtrlEventMask_t;

typedef enum {
  DVDCtrlLeftButtonSelect        = 0x01000001,
  DVDCtrlRightButtonSelect       = 0x01000002,
  DVDCtrlUpperButtonSelect       = 0x01000003,
  DVDCtrlLowerButtonSelect       = 0x01000004,

  DVDCtrlButtonActivate          = 0x01000005,
  DVDCtrlButtonSelect            = 0x01000006,
  DVDCtrlButtonSelectAndActivate = 0x01000007,

  DVDCtrlMouseSelect             = 0x02000001,
  DVDCtrlMouseActivate           = 0x02000002,

  DVDCtrlMenuCall = 1,
  DVDCtrlResume,
  DVDCtrlGoUp,

  DVDCtrlForwardScan,
  DVDCtrlBackwardScan,

  DVDCtrlNextPGSearch,
  DVDCtrlPrevPGSearch,
  DVDCtrlTopPGSearch,

  DVDCtrlPTTSearch,
  DVDCtrlPTTPlay, /* 10 */
  
  

  DVDCtrlTitlePlay,

  DVDCtrlTimeSearch,
  DVDCtrlTimePlay,

  DVDCtrlPauseOn,
  DVDCtrlPauseOff,

  DVDCtrlStop,

  DVDCtrlDefaultMenuLanguageSelect,
  DVDCtrlDefaultAudioLanguageSelect,
  DVDCtrlDefaultSubpictureLanguageSelect,

  DVDCtrlParentalCountrySelect, /* 20 */
  DVDCtrlParentalLevelSelect,

  DVDCtrlAudioStreamChange,
  DVDCtrlSubpictureStreamChange,
  DVDCtrlSetSubpictureState,
  DVDCtrlAngleChange,


  DVDCtrlGetCurrentAudio,
  DVDCtrlCurrentAudio,

  DVDCtrlIsAudioStreamEnabled,
  DVDCtrlAudioStreamEnabled,
  
  DVDCtrlGetAudioAttributes,
  DVDCtrlAudioAttributes,
 
  DVDCtrlGetCurrentSubpicture,
  DVDCtrlCurrentSubpicture,

  DVDCtrlIsSubpictureStreamEnabled,
  DVDCtrlSubpictureStreamEnabled,
  
  DVDCtrlGetSubpictureAttributes,
  DVDCtrlSubpictureAttributes,

  DVDCtrlGetCurrentAngle,
  DVDCtrlCurrentAngle,

  DVDCtrlPlayerRegionSelect,
  
  DVDCtrlGetCurrentUOPS,
  DVDCtrlCurrentUOPS,
  
  DVDCtrlGetDVDVolumeInfo,
  DVDCtrlDVDVolumeInfo,

  DVDCtrlGetTitles,
  DVDCtrlTitles,
  
  DVDCtrlGetNumberOfPTTs,
  DVDCtrlNumberOfPTTs,
  
  DVDCtrlGetCurrentDomain,
  DVDCtrlCurrentDomain,
  
  DVDCtrlGetCurrentLocation,
  DVDCtrlCurrentLocation,
  
  DVDCtrlGetState,

  DVDCtrlGetDiscID,
  DVDCtrlDiscID,
  
  DVDCtrlGetVolIds,
  
  DVDCtrlTimeSkip,

  DVDCtrlRetVal

} DVDCtrlEventType_t;

typedef enum {
  DVDCtrlLongSetDVDRoot,
  DVDCtrlLongState,
  DVDCtrlLongSetState,
  DVDCtrlLongVolIds
} DVDCtrlLongEventType_t;

typedef struct {
  DVDCtrlLongEventType_t type;
  int32_t serial;
  char path[PATH_MAX];
} DVDCtrlLongDVDRootEvent_t;

typedef struct {
  DVDCtrlLongEventType_t type;
  int32_t serial;
  char xmlstr[1024];
} DVDCtrlLongStateEvent_t;

typedef struct {
  DVDCtrlLongEventType_t type;
  int32_t serial;
  int voltype;
  char volid[33];
  unsigned char volsetid[128];
} DVDCtrlLongVolIdsEvent_t;

typedef struct {
  DVDCtrlLongEventType_t type;
  int32_t serial;
} DVDCtrlLongAnyEvent_t;


typedef struct {
  DVDCtrlEventType_t type;
  int32_t serial;
  int nr;
} DVDCtrlButtonEvent_t;


typedef struct {
  DVDCtrlEventType_t type;
  int32_t serial;
  int x;
  int y;
} DVDCtrlMouseEvent_t;

typedef struct {
  DVDCtrlEventType_t type;
  int32_t serial;
  DVDMenuID_t menuid;
} DVDCtrlMenuCallEvent_t;


typedef struct {
  DVDCtrlEventType_t type;
  int32_t serial;
  double speed;
} DVDCtrlScanEvent_t;


typedef struct {
  DVDCtrlEventType_t type;
  int32_t serial;
  DVDPTT_t ptt;
} DVDCtrlPTTSearchEvent_t;

typedef struct {
  DVDCtrlEventType_t type;
  int32_t serial;
  DVDTitle_t title;
  DVDPTT_t ptt;
} DVDCtrlPTTPlayEvent_t;

typedef struct {
  DVDCtrlEventType_t type;
  int32_t serial;
  DVDTitle_t title;
} DVDCtrlTitlePlayEvent_t;

typedef struct {
  DVDCtrlEventType_t type;
  int32_t serial;
  DVDTimecode_t time;
} DVDCtrlTimeSearchEvent_t;

typedef struct {
  DVDCtrlEventType_t type;
  int32_t serial;
  DVDTitle_t title;
  DVDTimecode_t time;
} DVDCtrlTimePlayEvent_t;


typedef struct {
  DVDCtrlEventType_t type;
  int32_t serial;
  DVDUOP_t uops;
} DVDCtrlCurrentUOPS_t;

typedef struct {
  DVDCtrlEventType_t type;
  int32_t serial;
  DVDLangID_t langid;
} DVDCtrlLanguageEvent_t;


typedef struct {
  DVDCtrlEventType_t type;
  int32_t serial;
  DVDCountryID_t countryid;
} DVDCtrlCountryEvent_t;


typedef struct {
  DVDCtrlEventType_t type;
  int32_t serial;
  DVDParentalLevel_t level;
} DVDCtrlParentalLevelEvent_t;


typedef struct {
  DVDCtrlEventType_t type;
  int32_t serial;
  DVDPlayerRegion_t region;
} DVDCtrlPlayerRegionEvent_t;


typedef struct {
  DVDCtrlEventType_t type;
  int32_t serial;
  DVDAudioStream_t streamnr;
} DVDCtrlAudioStreamChangeEvent_t;

typedef struct {
  DVDCtrlEventType_t type;
  int32_t serial;
  int nrofstreams;
  DVDAudioStream_t currentstream;
} DVDCtrlCurrentAudioEvent_t;

typedef struct {
  DVDCtrlEventType_t type;
  int32_t serial;
  DVDAudioStream_t streamnr;
  DVDBool_t enabled;
} DVDCtrlAudioStreamEnabledEvent_t;

typedef struct {
  DVDCtrlEventType_t type;
  int32_t serial;
  DVDAudioStream_t streamnr;
  DVDAudioAttributes_t attr;
} DVDCtrlAudioAttributesEvent_t;

typedef struct {
  DVDCtrlEventType_t type;
  int32_t serial;
  DVDDomain_t domain;
} DVDCtrlCurrentDomainEvent_t;

typedef struct {
  DVDCtrlEventType_t type;
  int32_t serial;
  DVDLocation_t location;
} DVDCtrlCurrentLocationEvent_t;

typedef struct {
  DVDCtrlEventType_t type;
  int32_t serial;
  DVDVolumeInfo_t volumeinfo;
} DVDCtrlVolumeInfoEvent_t;

typedef struct {
  DVDCtrlEventType_t type;
  int32_t serial;
  int titles;
} DVDCtrlTitlesEvent_t;

typedef struct {
  DVDCtrlEventType_t type;
  int32_t serial;
  DVDTitle_t title;
  int ptts;
} DVDCtrlNumberOfPTTsEvent_t;

typedef struct {
  DVDCtrlEventType_t type;
  int32_t serial;
  DVDSubpictureStream_t streamnr;
} DVDCtrlSubpictureStreamChangeEvent_t;

typedef struct {
  DVDCtrlEventType_t type;
  int32_t serial;
  DVDBool_t display;
} DVDCtrlSubpictureStateEvent_t;

typedef struct {
  DVDCtrlEventType_t type;
  int32_t serial;
  int nrofstreams;
  DVDSubpictureStream_t currentstream;
  DVDBool_t display;
} DVDCtrlCurrentSubpictureEvent_t;

typedef struct {
  DVDCtrlEventType_t type;
  int32_t serial;
  DVDSubpictureStream_t streamnr;
  DVDBool_t enabled;
} DVDCtrlSubpictureStreamEnabledEvent_t;

typedef struct {
  DVDCtrlEventType_t type;
  int32_t serial;
  DVDSubpictureStream_t streamnr;
  DVDSubpictureAttributes_t attr;
} DVDCtrlSubpictureAttributesEvent_t;

typedef struct {
  DVDCtrlEventType_t type;
  int32_t serial;
  DVDAngle_t anglenr;
} DVDCtrlAngleChangeEvent_t;

typedef struct {
  DVDCtrlEventType_t type;
  int32_t serial;
  int anglesavailable;
  DVDAngle_t anglenr;
} DVDCtrlCurrentAngleEvent_t;

typedef struct {
  DVDCtrlEventType_t type;
  int32_t serial;
  unsigned char id[16];
} DVDCtrlDiscIDEvent_t;

typedef struct {
  DVDCtrlLongEventType_t type;
  int32_t serial;
  int voltype;
} DVDCtrlGetVolIdsEvent_t;

typedef struct {
  DVDCtrlEventType_t type;
  int32_t serial;
  int32_t seconds;
} DVDCtrlTimeSkipEvent_t;

typedef struct {
  DVDCtrlEventType_t type;
  int32_t serial;
  DVDResult_t val;
} DVDCtrlRetValEvent_t;

typedef struct {
  DVDCtrlEventType_t type;
  int32_t serial;
} DVDCtrlAnyEvent_t;

typedef union {
  DVDCtrlEventType_t type;
  DVDCtrlAnyEvent_t any;
  DVDCtrlButtonEvent_t button;

  DVDCtrlMouseEvent_t mouse;

  DVDCtrlMenuCallEvent_t menucall;

  DVDCtrlScanEvent_t scan;

  DVDCtrlPTTSearchEvent_t pttsearch;
  DVDCtrlPTTPlayEvent_t pttplay;
  DVDCtrlTitlePlayEvent_t titleplay;
  DVDCtrlTimeSearchEvent_t timesearch;
  DVDCtrlTimePlayEvent_t timeplay;

  DVDCtrlLanguageEvent_t defaultmenulanguageselect;
  DVDCtrlLanguageEvent_t defaultaudiolanguageselect;
  DVDCtrlLanguageEvent_t defaultsubpicturelanguageselect;

  DVDCtrlCountryEvent_t parentalcountryselect;
  DVDCtrlParentalLevelEvent_t parentallevelselect;

  DVDCtrlPlayerRegionEvent_t playerregionselect;

  DVDCtrlAudioStreamChangeEvent_t audiostreamchange;
  DVDCtrlSubpictureStreamChangeEvent_t subpicturestreamchange;

  DVDCtrlSubpictureStateEvent_t subpicturestate;

  DVDCtrlAngleChangeEvent_t anglechange;
  
  DVDCtrlTimeSkipEvent_t timeskip;

  /* infocmd */
  DVDCtrlCurrentUOPS_t currentuops;

  DVDCtrlCurrentAudioEvent_t currentaudio;
  DVDCtrlAudioStreamEnabledEvent_t audiostreamenabled;
  DVDCtrlAudioAttributesEvent_t audioattributes;

  DVDCtrlVolumeInfoEvent_t volumeinfo;
  DVDCtrlTitlesEvent_t titles;
  DVDCtrlNumberOfPTTsEvent_t parts;

  DVDCtrlCurrentSubpictureEvent_t currentsubpicture;
  DVDCtrlSubpictureStreamEnabledEvent_t subpicturestreamenabled;
  DVDCtrlSubpictureAttributesEvent_t subpictureattributes;
  DVDCtrlCurrentAngleEvent_t currentangle;
  
  DVDCtrlCurrentDomainEvent_t domain;
  DVDCtrlCurrentLocationEvent_t location;

  DVDCtrlDiscIDEvent_t discid;

  DVDCtrlGetVolIdsEvent_t volids;
  /* end infocmd */

  DVDCtrlRetValEvent_t retval;
} DVDCtrlEvent_t;

typedef union {
  DVDCtrlLongEventType_t type;
  DVDCtrlLongAnyEvent_t any;
  DVDCtrlLongDVDRootEvent_t dvdroot;
  DVDCtrlLongStateEvent_t state;
  DVDCtrlLongVolIdsEvent_t volids;
} DVDCtrlLongEvent_t;


#endif /* DVDEVENTS_H_INCLUDED */
