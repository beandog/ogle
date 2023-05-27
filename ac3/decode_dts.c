/* Ogle - A video player
 * Copyright (C) 2001, 2002 Björn Englund, Håkan Hjort
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>


#include "parse_config.h"
#include "decode.h"
#include "decode_private.h"

#include "audio_config.h"
#include "conversion.h"
#include "audio_play.h"
#include "audio_types.h"
#include "debug_print.h"

typedef struct {
  adec_handle_t handle;
  uint64_t PTS;
  int pts_valid;
  int scr_nr;
  uint8_t *buf_ptr;
  SampleFormat_t output_format;
  int amode;
  int sfreq;
  int rate;
  
} adec_dts_handle_t;



static 
int decode_dts(adec_dts_handle_t *handle, uint8_t *start, int len,
	       int pts_offset, uint64_t new_PTS, int scr_nr)
{
  static uint8_t *indata_ptr;
  int bytes_left;

  int ftype;
  int surp;
  int unknown_bit;
  int nblks;
  int fsize;
  int amode;
  int sfreq;
  int rate;
  
  indata_ptr = start;
  bytes_left = len;

  if(pts_offset == 0 || pts_offset == 1) {
    handle->PTS = new_PTS;
    handle->pts_valid = 1;
    handle->scr_nr = scr_nr;
  } else {
    FATAL("DTS: pts_offset: %d, REPORT BUG\n", pts_offset);
    exit(1);
  }
  
  
  while(bytes_left > 0) {
    if(bytes_left < 10) {
      FATAL("%s", "DTS less than 10 bytes, REPORT BUG\n");
      exit(1);
    }
    if(((indata_ptr[0]<<24)|(indata_ptr[1]<<16)|
	(indata_ptr[2]<<8)|(indata_ptr[3])) != 0x7ffe8001) {
      FATAL("%s", "DTS sync wrong, REPORT BUG\n");
      exit(1);
    }

    ftype = indata_ptr[4] >> 7;

    surp = (indata_ptr[4] >> 2) & 0x1f;
    surp = (surp + 1) % 32;

    unknown_bit = (indata_ptr[4] >> 1) & 0x01;

    nblks = (indata_ptr[4] & 0x01) << 6 | (indata_ptr[5] >> 2);
    nblks = nblks + 1;

    fsize = (indata_ptr[5] & 0x03) << 12 |
      (indata_ptr[6] << 4) | (indata_ptr[7] >> 4);
    fsize = fsize + 1;

    amode = (indata_ptr[7] & 0x0f) << 2 | (indata_ptr[8] >> 6);
    sfreq = (indata_ptr[8] >> 2) & 0x0f;
    rate = (indata_ptr[8] & 0x03) << 3 | ((indata_ptr[9] >> 5) & 0x07);
    
    if(amode != handle->amode ||
       sfreq != handle->sfreq ||
       rate != handle->rate) {
      char *tmpstr;
      handle->amode = amode;
      handle->sfreq = sfreq;
      handle->rate = rate;
      DNOTE("%s", "DTS: channel arrangement: ");
      switch(amode) {
      case 0:
	tmpstr = "1-ch A";
	break;
      case 1:
	tmpstr = "2-ch A, B (dual mono)";
	break;
      case 2:
	tmpstr = "2-ch A, B (stereo)";
	break;
      case 3:
	tmpstr = "2-ch (L + R), (L - R) (sum-difference)";
	break;
      case 4:
	tmpstr = "2-ch Lt, Rt (total)";
	break;
      case 5:
	tmpstr = "3-ch L, R, C";
	break;
      case 6:
	tmpstr = "3-ch L, R, S";
	break;
      case 7:
	tmpstr = "4-ch L, R, C, S";
	break;
      case 8:
	tmpstr = "4-ch L, R, SL, SR";
	break;
      case 9:
	tmpstr = "5-ch L, R, C, SL, SR";
	break;
      case 10:
	tmpstr = "6-ch L, R, CL, CR, SL, SR";
	break;
      case 11:
	tmpstr = "6-ch Lf, Rf, Cf, Cr, Lr, Rr";
	break;
      case 12:
	tmpstr = "7-ch L, CL, C, CR, R, SL, SR";
	break;
      case 13:
	tmpstr = "8-ch L, CL, CR, R, SL1, SL2, SR1, SR2";
	break;
      case 14:
	tmpstr = "8-ch L, CL, C, CR, R, SL, S, SR";
	break;
      default:
	tmpstr = (amode <= 48) ? "User defined" : "Invalid";
	break;
      }
      fprintf(stderr, "%s\n", tmpstr);
      
      DNOTE("%s", "DTS: sample rate: ");
      switch(sfreq) {
      case 1:
	tmpstr = "8";
	break;
      case 2:
	tmpstr = "16";
	break;
      case 3:
	tmpstr = "32";
	break;
      case 4:
	tmpstr = "64";
	break;
      case 5:
	tmpstr = "128";
	break;
      case 6:
	tmpstr = "11.025";
	break;
      case 7:
	tmpstr = "22.05";
	break;
      case 8:
	tmpstr = "44.1";
	break;
      case 9:
	tmpstr = "88.2";
	break;
      case 10:
	tmpstr = "176.4";
	break;
      case 11:
	tmpstr = "12";
	break;
      case 12:
	tmpstr = "24";
	break;
      case 13:
	tmpstr = "48";
	break;
      case 14:
	tmpstr = "96";
	break;
      case 15:
	tmpstr = "192";
	break;
      default:
	tmpstr = "Invalid";
	break;
      }
      fprintf(stderr, "%s kHz\n", tmpstr);

      DNOTE("%s", "DTS: bit rate: ");
      switch(rate) {
      case 0:
	tmpstr = "32";
	break;
      case 1:
	tmpstr = "56";
	break;
      case 2:
	tmpstr = "64";
	break;
      case 3:
	tmpstr = "96";
	break;
      case 4:
	tmpstr = "112";
	break;
      case 5:
	tmpstr = "128";
	break;
      case 6:
	tmpstr = "192";
	break;
      case 7:
	tmpstr = "224";
	break;
      case 8:
	tmpstr = "256";
	break;
      case 9:
	tmpstr = "320";
	break;
      case 10:
	tmpstr = "384";
	break;
      case 11:
	tmpstr = "448";
	break;
      case 12:
	tmpstr = "512";
	break;
      case 13:
	tmpstr = "576";
	break;
      case 14:
	tmpstr = "640";
	break;
      case 15:
	tmpstr = "768";
	break;
      case 16:
	tmpstr = "896";
	break;
      case 17:
	tmpstr = "1024";
	break;
      case 18:
	tmpstr = "1152";
	break;
      case 19:
	tmpstr = "1280";
	break;
      case 20:
	tmpstr = "1344";
	break;
      case 21:
	tmpstr = "1408";
	break;
      case 22:
	tmpstr = "1411.2";
	break;
      case 23:
	tmpstr = "1472";
	break;
      case 24:
	tmpstr = "1536";
	break;
      case 25:
	tmpstr = "1920";
	break;
      case 26:
	tmpstr = "2048";
	break;
      case 27:
	tmpstr = "3072";
	break;
      case 28:
	tmpstr = "3840";
	break;
      case 29:
	tmpstr = "4096";
	break;
      case 30:
	tmpstr = "Variable";
	break;
      case 31:
	tmpstr = "Lossless";
	break; 
      }
      if(rate < 30) {
	fprintf(stderr, "%s kbps\n", tmpstr);
      } else {
	fprintf(stderr, "%s\n", tmpstr);
      }
      
      {
	ChannelType_t chtypemask;
	SampleFormat_t format;
	audio_format_t new_format;
	
	chtypemask = ChannelType_DTS;
	audio_config(handle->handle.config, chtypemask,
		     48000, 16);
	format = handle->handle.config->dst_format.sample_format;
	switch(format) {
	case SampleFormat_IEC61937:
	  if(handle->handle.config->dst_format.ch_array[0] ==
	     ChannelType_DTS) {
	    handle->output_format = SampleFormat_DTSFrame;
	  } else {
	    WARNING("DTS: unsupported channel type %d\n",
		    handle->handle.config->dst_format.ch_array[0]); 
	    handle->output_format = SampleFormat_None;
	  }
	  break;
	default:
	  WARNING("DTS: unsupported sample format %d\n", format); 
	  handle->output_format = SampleFormat_None;
	  break;
	}

	if(handle->output_format != SampleFormat_None) {
	  new_format.sample_rate = 48000;
	  new_format.sample_resolution = 16;
	  new_format.sample_format = handle->output_format;
	  init_sample_conversion((adec_handle_t *)handle, &new_format, nblks*32);
	}
      }
    }

    if(ftype != 1) {
      FATAL("%s", "DTS: Termination frames not handled, REPORT BUG\n");
      exit(1);
    }
    
    if(sfreq != 13) {
      FATAL("%s", "DTS: Only 48kHz supported, REPORT BUG\n");
      exit(1);
    }
    
    if((fsize > 8192) || (fsize < 96)) {
      FATAL("DTS: fsize: %d invalid, REPORT BUG\n", fsize);
      exit(1);
    }
    
    if(nblks != 8 &&
       nblks != 16 &&
       nblks != 32 &&
       nblks != 64 &&
       nblks != 128 &&
       ftype == 1) {
      FATAL("DTS: nblks %d not valid for normal frame, REPORT BUG\n", nblks);
      exit(1);
    }

    if(bytes_left < fsize) {
      FATAL("%s", "DTS: not enough data for frame, REPORT BUG\n");
      exit(1);
    }

    

    if(handle->output_format != SampleFormat_None) {
      convert_samples_start((adec_handle_t *)handle);
      
      convert_samples((adec_handle_t *)handle, indata_ptr, 
		      nblks*32);
      
      play_samples((adec_handle_t *)handle, handle->scr_nr, 
		   handle->PTS, handle->pts_valid);
      handle->pts_valid = 0;
    }

    

    indata_ptr += fsize;
    bytes_left -= fsize;
  }



  return 0;
}


static
int flush_dts(adec_dts_handle_t *handle)
{
  handle->pts_valid = 0;
  
  // Fix this.. not the right way to do things I belive.
  if(handle->handle.config && handle->handle.config->adev_handle)
    ogle_ao_flush(handle->handle.config->adev_handle);
  return 0;
}


static
void free_dts(adec_dts_handle_t *handle)
{
  audio_config_close(handle->handle.config);
  free(handle);
  return;
}


adec_handle_t *init_dts(void)
{
  adec_dts_handle_t *handle;
  
  if(!(handle = (adec_dts_handle_t *)malloc(sizeof(adec_dts_handle_t)))) {
    return NULL;
  }
  
  memset(&handle->handle, 0, sizeof(struct adec_handle_s));
  // not set: drain  
  handle->handle.decode = (audio_decode_t) decode_dts;  // function pointers
  handle->handle.flush  = (audio_flush_t)  flush_dts;
  handle->handle.free   = (audio_free_t)   free_dts;
  handle->handle.output_buf = NULL;
  handle->handle.output_buf_size = 0;
  handle->handle.output_buf_ptr = handle->handle.output_buf;

  handle->PTS = 0;
  handle->pts_valid = 0;
  handle->scr_nr = 0;

  handle->amode = -1;
  handle->sfreq = -1;
  handle->rate = -1;


  return (adec_handle_t *)handle;
}
