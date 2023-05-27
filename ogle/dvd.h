#ifndef DVD_H_INCLUDED
#define DVD_H_INCLUDED

/* Ogle - A video player
 * Copyright (C) 2000, 2001 Bj�rn Englund, H�kan Hjort
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

#include <inttypes.h>

/**
 * Possible return values from DVD functions.
 */
typedef enum {
  DVD_E_Ok = 0,               /**< Success, No Error */
  DVD_E_Unspecified = 127,    /**< An Error */
  DVD_E_NotImplemented = 128,  /**< The function is not implemented */
  DVD_E_NOMEM,
  DVD_E_RootNotSet,
  DVD_E_FailedToSend          /**< Unable to send the request/event */
} DVDResult_t;


/**
 * DVD Domain
 */
typedef enum {
  DVD_DOMAIN_FirstPlay,  /**< First Play Domain */
  DVD_DOMAIN_VMG,        /**< Video Manager Domain */
  DVD_DOMAIN_VTSMenu,    /**< Video Title Set Menu Domain */
  DVD_DOMAIN_VTSTitle,   /**< Video Title Set Domain */
  DVD_DOMAIN_Stop        /**< Stop Domain */
} DVDDomain_t;

/**
 * DVD Menu
 */
typedef enum {
  DVD_MENU_Title      = 2, /**< TBD */
  DVD_MENU_Root       = 3, /**< TBD */
  DVD_MENU_Subpicture = 4, /**< TBD */
  DVD_MENU_Audio      = 5, /**< TBD */
  DVD_MENU_Angle      = 6, /**< TBD */
  DVD_MENU_Part       = 7  /**< TBD */
} DVDMenuID_t;

/**
 * User operations
 */
typedef enum {
  UOP_FLAG_TitleOrTimePlay            = 0x00000001, 
  UOP_FLAG_ChapterSearchOrPlay        = 0x00000002, 
  UOP_FLAG_TitlePlay                  = 0x00000004, 
  UOP_FLAG_Stop                       = 0x00000008,  
  UOP_FLAG_GoUp                       = 0x00000010,
  UOP_FLAG_TimeOrChapterSearch        = 0x00000020, 
  UOP_FLAG_PrevOrTopPGSearch          = 0x00000040,  
  UOP_FLAG_NextPGSearch               = 0x00000080,   
  UOP_FLAG_ForwardScan                = 0x00000100,  
  UOP_FLAG_BackwardScan               = 0x00000200,
  UOP_FLAG_TitleMenuCall              = 0x00000400,
  UOP_FLAG_RootMenuCall               = 0x00000800,
  UOP_FLAG_SubPicMenuCall             = 0x00001000,
  UOP_FLAG_AudioMenuCall              = 0x00002000,
  UOP_FLAG_AngleMenuCall              = 0x00004000,
  UOP_FLAG_ChapterMenuCall            = 0x00008000,
  UOP_FLAG_Resume                     = 0x00010000,
  UOP_FLAG_ButtonSelectOrActivate     = 0x00020000,
  UOP_FLAG_StillOff                   = 0x00040000,
  UOP_FLAG_PauseOn                    = 0x00080000,
  UOP_FLAG_AudioStreamChange          = 0x00100000,
  UOP_FLAG_SubPicStreamChange         = 0x00200000,
  UOP_FLAG_AngleChange                = 0x00400000,
  UOP_FLAG_KaraokeAudioPresModeChange = 0x00800000,
  UOP_FLAG_VideoPresModeChange        = 0x01000000 
} DVDUOP_t;


/**
 * Parental Level
 */
typedef enum {
  DVD_PARENTAL_LEVEL_1 = 1,
  DVD_PARENTAL_LEVEL_2 = 2,
  DVD_PARENTAL_LEVEL_3 = 3,
  DVD_PARENTAL_LEVEL_4 = 4,
  DVD_PARENTAL_LEVEL_5 = 5,
  DVD_PARENTAL_LEVEL_6 = 6,
  DVD_PARENTAL_LEVEL_7 = 7,
  DVD_PARENTAL_LEVEL_8 = 8,
  DVD_PARENTAL_LEVEL_None = 15
} DVDParentalLevel_t;

/**
 * Player Region
 * Only one bit can be set
 */
typedef uint8_t DVDPlayerRegion_t;


/**
 * Language ID (ISO-639 language code)
 */
typedef uint16_t DVDLangID_t;

/**
 * Country ID (ISO-3166 country code)
 */
typedef uint16_t DVDCountryID_t;

/**
 * Register
 */
typedef uint16_t DVDRegister_t;

typedef enum {
  DVDFalse = 0,
  DVDTrue = 1
} DVDBool_t; 

typedef DVDRegister_t DVDGPRMArray_t[16];
typedef DVDRegister_t DVDSPRMArray_t[24];

typedef int DVDStream_t;

/**
 * Angle number (1-9 or default?)
 */
typedef int DVDAngle_t;

typedef int DVDPTT_t;
typedef int DVDTitle_t;
typedef struct {
  uint8_t Hours;
  uint8_t Minutes;
  uint8_t Seconds;
  uint8_t Frames;
} DVDTimecode_t;

/** 
 * Subpicture stream number (0-31,62,63)
 */
typedef int DVDSubpictureStream_t;  

/** 
 * Audio stream number (0-7, 15(none))
 */
typedef int DVDAudioStream_t;  


/**
 * The audio application mode
 */
typedef enum {
  DVD_AUDIO_APP_MODE_None     = 0, /**< app mode none     */
  DVD_AUDIO_APP_MODE_Karaoke  = 1, /**< app mode karaoke  */
  DVD_AUDIO_APP_MODE_Surround = 2, /**< app mode surround */
  DVD_AUDIO_APP_MODE_Other    = 3  /**< app mode other    */
} DVDAudioAppMode_t;

/**
 * The audio format
 */
typedef enum {
  DVD_AUDIO_FORMAT_AC3       = 0, /**< Dolby AC-3 */
  DVD_AUDIO_FORMAT_MPEG1     = 1, /**< MPEG-1 */
  DVD_AUDIO_FORMAT_MPEG1_DRC = 2, /**< MPEG-1 with dynamic range control */
  DVD_AUDIO_FORMAT_MPEG2     = 3, /**< MPEG-2 */
  DVD_AUDIO_FORMAT_MPEG2_DRC = 4, /**< MPEG-2 with dynamic range control */
  DVD_AUDIO_FORMAT_LPCM      = 5, /**< Linear Pulse Code Modulation */
  DVD_AUDIO_FORMAT_DTS       = 6, /**< Digital Theater Systems */
  DVD_AUDIO_FORMAT_SDDS      = 7, /**< Sony Dynamic Digital Sound */
  DVD_AUDIO_FORMAT_Other     = 8  /**< Other format*/
} DVDAudioFormat_t;

/**
 * Audio language extension
 */
typedef enum {
  DVD_AUDIO_LANG_EXT_NotSpecified       = 0, /**< TBD */
  DVD_AUDIO_LANG_EXT_NormalCaptions     = 1, /**< TBD */
  DVD_AUDIO_LANG_EXT_VisuallyImpaired   = 2, /**< TBD */
  DVD_AUDIO_LANG_EXT_DirectorsComments1 = 3, /**< TBD */
  DVD_AUDIO_LANG_EXT_DirectorsComments2 = 4  /**< TBD */
} DVDAudioLangExt_t;

/**
 * Subpicture language extension
 */
typedef enum {
  DVD_SUBPICTURE_LANG_EXT_NotSpecified  = 0,
  DVD_SUBPICTURE_LANG_EXT_NormalCaptions  = 1,
  DVD_SUBPICTURE_LANG_EXT_BigCaptions  = 2,
  DVD_SUBPICTURE_LANG_EXT_ChildrensCaptions  = 3,
  DVD_SUBPICTURE_LANG_EXT_NormalCC  = 5,
  DVD_SUBPICTURE_LANG_EXT_BigCC  = 6,
  DVD_SUBPICTURE_LANG_EXT_ChildrensCC  = 7,
  DVD_SUBPICTURE_LANG_EXT_Forced  = 9,
  DVD_SUBPICTURE_LANG_EXT_NormalDirectorsComments  = 13,
  DVD_SUBPICTURE_LANG_EXT_BigDirectorsComments  = 14,
  DVD_SUBPICTURE_LANG_EXT_ChildrensDirectorsComments  = 15
} DVDSubpictureLangExt_t;  

/**
 * Karaoke Downmix mode
 */
typedef enum {
  DVD_KARAOKE_DOWNMIX_0to0 = 0x0001,
  DVD_KARAOKE_DOWNMIX_1to0 = 0x0002,
  DVD_KARAOKE_DOWNMIX_2to0 = 0x0004,
  DVD_KARAOKE_DOWNMIX_3to0 = 0x0008,
  DVD_KARAOKE_DOWNMIX_4to0 = 0x0010,
  DVD_KARAOKE_DOWNMIX_Lto0 = 0x0020,
  DVD_KARAOKE_DOWNMIX_Rto0 = 0x0040,
  DVD_KARAOKE_DOWNMIX_0to1 = 0x0100,
  DVD_KARAOKE_DOWNMIX_1to1 = 0x0200,
  DVD_KARAOKE_DOWNMIX_2to1 = 0x0400,
  DVD_KARAOKE_DOWNMIX_3to1 = 0x0800,
  DVD_KARAOKE_DOWNMIX_4to1 = 0x1000,
  DVD_KARAOKE_DOWNMIX_Lto1 = 0x2000,
  DVD_KARAOKE_DOWNMIX_Rto1 = 0x4000
} DVDKaraokeDownmix_t;

typedef int DVDKaraokeDownmixMask_t;

typedef enum {
  DVD_DISPLAY_MODE_ContentDefault = 0,
  DVD_DISPLAY_MODE_16x9 = 1,
  DVD_DISPLAY_MODE_4x3PanScan = 2,
  DVD_DISPLAY_MODE_4x3Letterboxed = 3  
} DVDDisplayMode_t;

typedef int DVDAudioSampleFreq_t;  /**< TBD */
typedef int DVDAudioSampleQuant_t; /**< TBD */
typedef int DVDChannelNumber_t;    /**< TBD */


typedef struct {
  DVDAudioAppMode_t     AppMode;
  DVDAudioFormat_t      AudioFormat;
  DVDLangID_t           Language;
  DVDAudioLangExt_t     LanguageExtension;
  DVDBool_t             HasMultichannelInfo;
  DVDAudioSampleFreq_t  SampleFrequency;
  DVDAudioSampleQuant_t SampleQuantization;
  DVDChannelNumber_t    NumberOfChannels;
} DVDAudioAttributes_t;

typedef enum {
  DVD_SUBPICTURE_TYPE_NotSpecified = 0,
  DVD_SUBPICTURE_TYPE_Language     = 1,
  DVD_SUBPICTURE_TYPE_Other        = 2
} DVDSubpictureType_t;

typedef enum {
  DVD_SUBPICTURE_CODING_RunLength = 0,
  DVD_SUBPICTURE_CODING_Extended  = 1,
  DVD_SUBPICTURE_CODING_Other     = 2
} DVDSubpictureCoding_t;

typedef struct {
  DVDSubpictureType_t    Type;
  DVDSubpictureCoding_t  CodingMode;
  DVDLangID_t            Language;
  DVDSubpictureLangExt_t LanguageExtension;
} DVDSubpictureAttributes_t;

typedef int DVDVideoCompression_t; /**< TBD */

typedef struct {
  DVDBool_t PanscanPermitted;
  DVDBool_t LetterboxPermitted;
  int AspectX;
  int AspectY;
  int FrameRate;
  int FrameHeight;
  DVDVideoCompression_t Compression;
  DVDBool_t Line21Field1InGop;
  DVDBool_t Line21Field2InGop;
  int more_to_come;
} DVDVideoAttributes_t;

typedef struct {
  DVDTitle_t title;     /**< TT number not VTS_TT number */
  DVDPTT_t ptt;
  DVDTimecode_t title_current;
  DVDTimecode_t title_total;
} DVDLocation_t;

typedef struct {
  int nrofvolumes;
  int volume;
  int side;             /**< 0: Side_A, 1: Side_B */
  int nroftitles;
} DVDVolumeInfo_t;

/* hack */

typedef enum {
  AspectModeSrcVM,
  AspectModeSrcMPEG,
  AspectModeSrcUser,
  AspectModeSrcFillWindow
} AspectModeSrc_t;

typedef enum {
  ZoomModeFullScreen,
  ZoomModeNormalScreen,
  ZoomModeSet,
  ZoomModeResizeAllowed,
  ZoomModeResizeDisallowed
} ZoomMode_t;

typedef enum {
  ScreenshotModeWithoutSPU,
  ScreenshotModeWithSPU
} ScreenshotMode_t;

typedef enum {
  INPUT_MASK_None          = 0,
  INPUT_MASK_KeyPress      = (1<<0),
  INPUT_MASK_KeyRelease    = (1<<1),
  INPUT_MASK_ButtonPress   = (1<<2),
  INPUT_MASK_ButtonRelease = (1<<3),
  INPUT_MASK_PointerMotion = (1<<6)
} InputMask_t;

#endif /* DVD_H_INCLUDED */
