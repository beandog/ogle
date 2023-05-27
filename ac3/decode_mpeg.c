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


#include <libogleao/ogle_ao.h> // Remove me
#include <mad.h>

#include "decode.h"
#include "decode_private.h"

#include "debug_print.h"

#include "audio_config.h"
#include "conversion.h"
#include "audio_play.h"

#include "decode_mpeg.h"

// should be at least max size of an mpeg audio frame
#define CODED_BUF_SIZE_MPEG 2048*2 

typedef struct {
  adec_handle_t handle;
  uint64_t PTS;
  int pts_valid;
  int scr_nr;
  uint8_t *coded_buf;
  uint8_t *buf_ptr;
  int sample_rate;
  int bytes_needed;
  int availflags;
  int output_flags;
  int decoding_flags;

  /* libmad */
  struct mad_stream stream;
  struct mad_frame frame;
  struct mad_synth synth;
  mad_timer_t timer;

} adec_mpeg_handle_t;


static int decode_mpeg(adec_mpeg_handle_t *h, uint8_t *start, int len,
		       int pts_offset, uint64_t new_PTS, int scr_nr)
{
  int bytes_left;
  int pts_valid;
  static int prev_scr_nr;
  static uint64_t prev_PTS;
  static int prev_pts_valid = 0;
  uint8_t *packet_start = NULL;
  int first = 1;

  bytes_left = len;
  
  if(pts_offset == -1) {
    pts_valid = 0;
  } else {
    pts_valid = 1;
  }
  
  while(bytes_left > 0) {
    int avail_buf;
    avail_buf = CODED_BUF_SIZE_MPEG - (h->buf_ptr - h->coded_buf);
    if(avail_buf <= 0) {
      FATAL("mpeg coded buf full: %d\n", avail_buf);
    }
    /*
    fprintf(stderr, "avail: %d, len: %d, ptr: %d\n",
	    avail_buf, bytes_left, h->buf_ptr - h->coded_buf);
    */

    if(avail_buf >= bytes_left) { 
      memcpy(h->buf_ptr, start, bytes_left);
      if(first) {
	packet_start = h->buf_ptr;
      }
      h->buf_ptr += bytes_left;
      bytes_left = 0;
    } else {
      memcpy(h->buf_ptr, start, avail_buf);
      if(first) {
	packet_start = h->buf_ptr;
      }
      bytes_left -= avail_buf;
      start += avail_buf;
      h->buf_ptr += avail_buf;
    }
    first = 0;
    
      
    mad_stream_buffer(&h->stream, h->coded_buf,
		      h->buf_ptr - h->coded_buf);
    h->stream.error = 0;
    
    while(1) {
      if(mad_frame_decode(&h->frame, &h->stream)) {
	if(MAD_RECOVERABLE(h->stream.error)) {
	  if(h->stream.error == MAD_ERROR_LOSTSYNC) {
	    NOTE("%s", "mpeg lost sync\n");
	  } else {
	    DNOTE("mpeg recoverable: %d\n", h->stream.error);
	  }
	} else {
	  /*
	  fprintf(stderr, "unrec curframe: %u, nextframe: %u\n",
		  h->stream.this_frame - h->coded_buf,
		  h->stream.next_frame - h->coded_buf);
	  */
	  if(h->stream.error == MAD_ERROR_BUFLEN) {
	    memmove(h->coded_buf, h->stream.next_frame,
		    h->buf_ptr - h->stream.next_frame);
	    h->buf_ptr-= (h->stream.next_frame - h->coded_buf); 
	    packet_start-= (h->stream.next_frame - h->coded_buf);
	    if(pts_valid) {
	      prev_pts_valid = 1;
	      prev_PTS = new_PTS;
	      prev_scr_nr = scr_nr;
	    } else {
	      prev_pts_valid = 0;
	    }
	    break;
	  } else {
	    FATAL("mpeg unrecoverable error: %d\n", h->stream.error);
	  }
	}
      } else {
	int frame_pts_valid;
	uint64_t frame_PTS = 0; /* init to shut up compiler */
	int frame_scr_nr;
	mad_synth_frame(&h->synth, &h->frame);
	
	if(h->frame.header.samplerate != h->sample_rate) {
	  audio_format_t new_format;
	  
	  h->sample_rate = h->frame.header.samplerate;
	  audio_config(h->handle.config, 2, h->sample_rate, 16);
	  
	  new_format.ch_array = malloc(2 * sizeof(ChannelType_t));
	  new_format.ch_array[0] = ChannelType_Left;
	  new_format.ch_array[1] = ChannelType_Right;
	  
	  new_format.sample_rate = h->sample_rate;
	  new_format.sample_resolution = 16;
	  new_format.sample_format = SampleFormat_MadFixed;
	  init_sample_conversion((adec_handle_t *)h, &new_format, 1152);
	  
	  free(new_format.ch_array);
	}
	convert_samples_start((adec_handle_t *)h);
	convert_samples((adec_handle_t *)h, h->synth.pcm.samples,
			h->synth.pcm.length);
	if(h->stream.this_frame >= packet_start) {
	  if(pts_valid) {
	    frame_pts_valid = 1;
	    frame_PTS = new_PTS;
	    frame_scr_nr = scr_nr;
	    //fprintf(stderr, "PTS: %llu\n", new_PTS);
	    pts_valid = 0;
	  } else {
	    frame_pts_valid = 0;
	    frame_scr_nr = scr_nr;
	    //fprintf(stderr, "no PTS:\n");
	  }
	} else {
	  if(prev_pts_valid) {
	    frame_pts_valid = 1;
	    frame_PTS = prev_PTS;
	    frame_scr_nr = prev_scr_nr;
	    // fprintf(stderr, "prev PTS: %llu\n", prev_PTS);
	    prev_pts_valid = 0;
	  } else {
	    frame_pts_valid = 0;
	    frame_scr_nr = scr_nr;
	    //fprintf(stderr, "no prev PTS:\n");
	  }
	}
	play_samples((adec_handle_t *)h, frame_scr_nr, 
		     frame_PTS, frame_pts_valid);
	/*
	  fprintf(stderr, "curframe: %u, nextframe: %u\n",
	  h->stream.this_frame - h->coded_buf,
	  h->stream.next_frame - h->coded_buf);
	*/
	
      }
    }
  }
  
  return 0;
}


static
int flush_mpeg(adec_mpeg_handle_t *handle)
{
  handle->pts_valid = 0;
  handle->buf_ptr = handle->coded_buf;
  handle->bytes_needed = 7;
  
  // Fix this.. not the right way to do things I belive.
  if(handle->handle.config && handle->handle.config->adev_handle)
    ogle_ao_flush(handle->handle.config->adev_handle);
  return 0;
}


static
void free_mpeg(adec_mpeg_handle_t *handle)
{
  mad_synth_finish(&handle->synth);
  mad_frame_finish(&handle->frame);
  mad_stream_finish(&handle->stream);

  audio_config_close(handle->handle.config);
  free(handle->coded_buf);
  free(handle);
  return;
}


adec_handle_t *init_mpeg(void)
{
  adec_mpeg_handle_t *handle;
  
  if(!(handle = (adec_mpeg_handle_t *)malloc(sizeof(adec_mpeg_handle_t)))) {
    return NULL;
  }
  
  memset(&handle->handle, 0, sizeof(struct adec_handle_s));
  // not set: drain
  handle->handle.decode = (audio_decode_t) decode_mpeg;  // function pointers
  handle->handle.flush  = (audio_flush_t)  flush_mpeg;
  handle->handle.free   = (audio_free_t)   free_mpeg;
  handle->handle.output_buf = NULL;
  handle->handle.output_buf_size = 0;
  handle->handle.output_buf_ptr = handle->handle.output_buf;

  handle->PTS = 0;
  handle->pts_valid = 0;
  handle->scr_nr = 0;
  handle->coded_buf = (uint8_t *)malloc(CODED_BUF_SIZE_MPEG);
  handle->buf_ptr = (uint8_t *)handle->coded_buf;
  handle->bytes_needed = 7;
  handle->sample_rate = 0;
  //  handle->decoded_format = NULL;

  {
    
    mad_stream_init(&handle->stream);
    mad_frame_init(&handle->frame);
    mad_synth_init(&handle->synth);
    mad_timer_reset(&handle->timer);

  }
  
  return (adec_handle_t *)handle;
}
