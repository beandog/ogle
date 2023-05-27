/* Ogle - A video player
 * Copyright (C) 2002 Björn Englund
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

#ifdef LIBOGLEAO_NULL

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>

#include "ogle_ao.h"
#include "ogle_ao_private.h"

typedef struct null_instance_s {
  ogle_ao_instance_t ao;
  int sample_rate;
  int samples_written;
  int sample_frame_size;
  int initialized;
} null_instance_t;



static
int null_init(ogle_ao_instance_t *_instance,
	      ogle_ao_audio_info_t *audio_info)
{
  int single_sample_size;
  null_instance_t *instance = (null_instance_t *)_instance;
  
#if 0
  if(instance->initialized) {
    return -1;  // for now we must close and open the device for reinit
  }
#endif

  switch(audio_info->encoding) {
  case OGLE_AO_ENCODING_LINEAR:
    /* ok */
    break;
  default:
    /* not supported */
    return -1;
  }
  if(audio_info->chtypes != OGLE_AO_CHTYPE_UNSPECIFIED) {
    // do this only if we have requested specific channels in chtypes
    // otherwise trust the nr channels
    
    audio_info->chtypes = OGLE_AO_CHTYPE_LEFT | OGLE_AO_CHTYPE_RIGHT;
    audio_info->channels = 2;
  }
  if(audio_info->chlist) {
    free(audio_info->chlist);
  }
  audio_info->chlist = NULL;
  
  if(audio_info->chtypes != OGLE_AO_CHTYPE_UNSPECIFIED) {
    audio_info->channels = 2;
    audio_info->chlist = malloc(audio_info->channels *
				sizeof(ogle_ao_chtype_t));
    
    audio_info->chlist[0] = OGLE_AO_CHTYPE_LEFT;
    audio_info->chlist[1] = OGLE_AO_CHTYPE_RIGHT;
  }

  instance->sample_rate = audio_info->sample_rate;
  
  single_sample_size = (audio_info->sample_resolution + 7) / 8;
  if(single_sample_size > 2) {
    single_sample_size = 4;
  }
  instance->sample_frame_size = 
    audio_info->channels * single_sample_size;

  audio_info->sample_frame_size = instance->sample_frame_size;

  instance->initialized = 1;
  
  return 0;
}

static
int null_play(ogle_ao_instance_t *_instance, void *samples, size_t nbyte)
{
  null_instance_t *instance = (null_instance_t *)_instance;
  
  instance->samples_written += nbyte / instance->sample_frame_size;
  
  return 0;
}

static
int null_odelay(ogle_ao_instance_t *_instance, uint32_t *samples_return)
{
  //null_instance_t *instance = (null_instance_t *)_instance;
  
  /* We'll get strange sync if we do it like this.  We'd have to take
   * a time stamp and then calculate from the current time and sample
   * rate how many we 'should have' played. */
  
  // int samples_played;
  // samples_played = (time_now - time_at_init) * sample_rate;
  // odelay = instance->samples_written - samples_played;
  // *samples_return = 0;

  return -1; // Not supported by null driver
}

static
void null_close(ogle_ao_instance_t *_instance)
{
  //null_instance_t *instance = (null_instance_t *)_instance;
}

static
int null_flush(ogle_ao_instance_t *_instance)
{
  //null_instance_t *instance = (null_instance_t *)_instance;
  
  return 0;
}

static
int null_drain(ogle_ao_instance_t *_instance)
{
  //null_instance_t *instance = (null_instance_t *)_instance;
  
  return 0;
}


static 
ogle_ao_instance_t *null_open(char *dev)
{
    null_instance_t *instance;
    
    instance = malloc(sizeof(null_instance_t));
    if(instance == NULL)
      return NULL;
    
    instance->ao.init   = null_init;
    instance->ao.play   = null_play;
    instance->ao.close  = null_close;
    instance->ao.odelay = null_odelay;
    instance->ao.flush  = null_flush;
    instance->ao.drain  = null_drain;
    
    instance->initialized = 0;
    instance->sample_rate = 0;
    instance->samples_written = 0;
    instance->sample_frame_size = 0;
    
    return (ogle_ao_instance_t *)instance;
}


/* The one directly exported function */
ogle_ao_instance_t *ao_null_open(char *dev)
{
  return null_open(dev);
}

#endif
