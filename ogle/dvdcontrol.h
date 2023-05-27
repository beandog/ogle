#ifndef DVDCONTROL_H_INCLUDED
#define DVDCONTROL_H_INCLUDED

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

#include <ogle/dvd.h>
#include <ogle/msgevents.h> /* Only for the hack with DVDNextEvent !! */
#include <ogle/dvdbookmarks.h>

#include <sys/param.h>

typedef struct DVDNav_s DVDNav_t;

DVDResult_t DVDOpenNav(DVDNav_t **nav, int msgqid);
DVDResult_t DVDCloseNav(DVDNav_t *nav);

void DVDPerror(const char *str, DVDResult_t ErrCode);

/* info commands */

DVDResult_t DVDGetAllGPRMs(DVDNav_t *nav, DVDGPRMArray_t *const Registers);
DVDResult_t DVDGetAllSPRMs(DVDNav_t *nav, DVDSPRMArray_t *const Registers);

DVDResult_t DVDGetCurrentUOPS(DVDNav_t *nav, DVDUOP_t *const uop);
DVDResult_t DVDGetAudioAttributes(DVDNav_t *nav, DVDAudioStream_t StreamNr,
				  DVDAudioAttributes_t *const Attr);
DVDResult_t DVDGetAudioLanguage(DVDNav_t *nav, DVDAudioStream_t StreamNr,
				DVDLangID_t *Language);
DVDResult_t DVDGetCurrentAudio(DVDNav_t *nav, int *const StreamsAvailable,
			       DVDAudioStream_t *const CurrentStream);
DVDResult_t DVDIsAudioStreamEnabled(DVDNav_t *nav, DVDAudioStream_t StreamNr,
				    DVDBool_t *const Enabled);
DVDResult_t DVDGetDefaultAudioLanguage(DVDNav_t *nav, 
				       DVDLangID_t *const Language,
				       DVDAudioLangExt_t *const AudioExtension);
DVDResult_t DVDGetCurrentAngle(DVDNav_t *nav, int *const AnglesAvailable,
			       DVDAngle_t *const CurrentAngle);
DVDResult_t DVDGetCurrentVideoAttributes(DVDNav_t *nav, 
					 DVDVideoAttributes_t *const Attr);

DVDResult_t DVDGetCurrentDomain(DVDNav_t *nav, DVDDomain_t *const Domain);
DVDResult_t DVDGetCurrentLocation(DVDNav_t *nav, 
				  DVDLocation_t *const Location);

DVDResult_t DVDGetDVDVolumeInfo(DVDNav_t *nav, DVDVolumeInfo_t *const VolumeInfo);

DVDResult_t DVDGetTitles(DVDNav_t *nav, int *const TitlesAvailable);

DVDResult_t DVDGetNumberOfPTTs(DVDNav_t *nav, DVDTitle_t Title,
			       int *const PartsAvailable);

DVDResult_t DVDGetCurrentSubpicture(DVDNav_t *nav,
				    int *const StreamsAvailable,
				    DVDSubpictureStream_t *const CurrentStream,
				    DVDBool_t *const Enabled);
DVDResult_t DVDIsSubpictureStreamEnabled(DVDNav_t *nav,
					 DVDSubpictureStream_t StreamNr,
					 DVDBool_t *const Enabled);
DVDResult_t DVDGetSubpictureAttributes(DVDNav_t *nav,
				       DVDSubpictureStream_t StreamNr,
				       DVDSubpictureAttributes_t *const Attr);
DVDResult_t DVDGetSubpictureLanguage(DVDNav_t *nav,
				     DVDSubpictureStream_t StreamNr,
				     DVDLangID_t *const Language);
DVDResult_t DVDGetDefaultSubpictureLanguage(DVDNav_t *nav,
					    DVDLangID_t *const Language,
					    DVDSubpictureLangExt_t *const SubpictureExtension);

DVDResult_t DVDGetState(DVDNav_t *nav, char **state);

DVDResult_t DVDGetDiscID(DVDNav_t *nav, unsigned char *dvdid);

DVDResult_t DVDGetVolumeIdentifiers(DVDNav_t *nav,
				    int type, int *return_type,
				    char *volid,
				    unsigned char *volsetid);
/* end info commands */

/* hack */
#if (defined(BSD) && (BSD >= 199306))
DVDResult_t DVDNextEventNonBlocking(DVDNav_t *nav, MsgEvent_t *ev);
#endif
DVDResult_t DVDNextEvent(DVDNav_t *nav, MsgEvent_t *ev);
DVDResult_t DVDRequestInput(DVDNav_t *nav, InputMask_t mask);

/* control commands */


DVDResult_t DVDSetAspectModeSrc(DVDNav_t *nav, AspectModeSrc_t mode_src);

DVDResult_t DVDSetSrcAspect(DVDNav_t *nav, AspectModeSrc_t mode_sender,
			    uint16_t aspect_frac_n, uint16_t aspect_frac_d);

DVDResult_t DVDSetZoomMode(DVDNav_t *nav, ZoomMode_t zoom_mode);

DVDResult_t DVDSaveScreenshot(DVDNav_t *nav, ScreenshotMode_t mode,
			      char *formatstr);



DVDResult_t DVDSetDVDRoot(DVDNav_t *nav, char *Path);

DVDResult_t DVDLeftButtonSelect(DVDNav_t *nav);
DVDResult_t DVDRightButtonSelect(DVDNav_t *nav);
DVDResult_t DVDUpperButtonSelect(DVDNav_t *nav);
DVDResult_t DVDLowerButtonSelect(DVDNav_t *nav);

DVDResult_t DVDButtonActivate(DVDNav_t *nav);
DVDResult_t DVDButtonSelect(DVDNav_t *nav, int Button);
DVDResult_t DVDButtonSelectAndActivate(DVDNav_t *nav, int Button);

DVDResult_t DVDMouseSelect(DVDNav_t *nav, int x, int y);
DVDResult_t DVDMouseActivate(DVDNav_t *nav, int x, int y);

DVDResult_t DVDMenuCall(DVDNav_t *nav, DVDMenuID_t MenuId);
DVDResult_t DVDResume(DVDNav_t *nav);
DVDResult_t DVDGoUp(DVDNav_t *nav);


DVDResult_t DVDForwardScan(DVDNav_t *nav, double Speed);
DVDResult_t DVDBackwardScan(DVDNav_t *nav, double Speed);

DVDResult_t DVDNextPGSearch(DVDNav_t *nav);
DVDResult_t DVDPrevPGSearch(DVDNav_t *nav);
DVDResult_t DVDTopPGSearch(DVDNav_t *nav);

DVDResult_t DVDPTTSearch(DVDNav_t *nav, DVDPTT_t PTT);
DVDResult_t DVDPTTPlay(DVDNav_t *nav, DVDTitle_t Title, DVDPTT_t PTT);

DVDResult_t DVDTitlePlay(DVDNav_t *nav, DVDTitle_t Title);

DVDResult_t DVDTimeSearch(DVDNav_t *nav, DVDTimecode_t time);
DVDResult_t DVDTimePlay(DVDNav_t *nav, DVDTitle_t Title, DVDTimecode_t time);

DVDResult_t DVDTimeSkip(DVDNav_t *nav, int32_t seconds);

DVDResult_t DVDPauseOn(DVDNav_t *nav);
DVDResult_t DVDPauseOff(DVDNav_t *nav);
DVDResult_t DVDStop(DVDNav_t *nav);
DVDResult_t DVDStillOff(DVDNav_t *nav);


DVDResult_t DVDDefaultMenuLanguageSelect(DVDNav_t *nav, DVDLangID_t Lang);


DVDResult_t DVDAudioStreamChange(DVDNav_t *nav, DVDAudioStream_t StreamNr);
DVDResult_t DVDDefaultAudioLanguageSelect(DVDNav_t *nav, DVDLangID_t Lang);
DVDResult_t DVDKaraokeAudioPresentationMode(DVDNav_t *nav, DVDKaraokeDownmixMask_t DownmixMode);

DVDResult_t DVDAngleChange(DVDNav_t *nav, DVDAngle_t AngleNr);

DVDResult_t DVDVideoPresentationModeChange(DVDNav_t *nav,
					   DVDDisplayMode_t mode);

DVDResult_t DVDSubpictureStreamChange(DVDNav_t *nav,
				      DVDSubpictureStream_t SubpictureNr);
DVDResult_t DVDSetSubpictureState(DVDNav_t *nav, DVDBool_t Display);
DVDResult_t DVDDefaultSubpictureLanguageSelect(DVDNav_t *nav,
					       DVDLangID_t Lang);

DVDResult_t DVDParentalCountrySelect(DVDNav_t *nav, DVDCountryID_t country);
DVDResult_t DVDParentalLevelSelect(DVDNav_t *nav, DVDParentalLevel_t level);

DVDResult_t DVDPlayerRegionSelect(DVDNav_t *nav, DVDPlayerRegion_t region);

DVDResult_t DVDSetState(DVDNav_t *nav, char *state);
/* end control commands */

#endif /* DVDCONTROL_H_INCLUDED */
