/* Ogle - A video player
 * Copyright (C) 2002 Björn Englund
 * Copyright (C) 2002 Torgeir Veimo
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

#ifdef LIBOGLEAO_ALSA

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>

//this needs to be set before including asoundlib.h
#define ALSA_PCM_NEW_HW_PARAMS_API 
#include <alsa/asoundlib.h>

#include "../include/debug_print.h"

#include "ogle_ao.h"
#include "ogle_ao_private.h"

typedef struct alsa_instance_s {
  ogle_ao_instance_t ao;
  int sample_rate;
  int samples_written;
  int sample_frame_size;
  int channels;
  int initialized;
  snd_pcm_t *alsa_pcm;
  snd_pcm_hw_params_t *hwparams;
  snd_pcm_sw_params_t *swparams;
  snd_pcm_format_t format;
  snd_pcm_uframes_t buffer_size;
  snd_pcm_uframes_t period_size;
} alsa_instance_t;

// logging
static snd_output_t *logs = NULL;

/* ring buffer length in us */
#define BUFFER_TIME 500000
/* period time in us */ 
#define PERIOD_TIME 10000
/* minimum samples before play */
#define MINIMUM_SAMPLES 5000

static int set_hwparams(alsa_instance_t *i)
{
  int err, dir;
  unsigned int tmp_uint;

  if((err = snd_pcm_hw_params_any(i->alsa_pcm, i->hwparams)) < 0) {
    ERROR("No configurations available: %s\n", snd_strerror(err));
    return err;
  }
  /* set the interleaved read/write format */
  if((err = snd_pcm_hw_params_set_access(i->alsa_pcm, i->hwparams, 
					 SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
    ERROR("Access type not available for playback: %s\n", snd_strerror(err));
    return err;
  }
  /* set the sample format */
  if((err = snd_pcm_hw_params_set_format(i->alsa_pcm, i->hwparams,
					 i->format)) < 0) {
    ERROR("Sample format not available for playback: %s\n", snd_strerror(err));
    return err;
  }
  /* get min/max channels available */
  if((err = snd_pcm_hw_params_get_channels_min(i->hwparams, &tmp_uint)) < 0) {
    WARNING("Unable to get min channels: %s\n", snd_strerror(err));
  } else {
    DNOTE("Min channels: %u\n", tmp_uint);
  }
  if((err = snd_pcm_hw_params_get_channels_max(i->hwparams, &tmp_uint)) < 0) {
    WARNING("Unable to get max channels: %s\n", snd_strerror(err));
  } else {
    DNOTE("Max channels: %u\n", tmp_uint);
  }

  /* set the count of channels */
  tmp_uint = i->channels;
  if((err = snd_pcm_hw_params_set_channels_near(i->alsa_pcm, i->hwparams,
						&tmp_uint)) < 0) {
    ERROR("Channels count (%u) not available for playbacks: %s\n",
	  i->channels, snd_strerror(err));
    i->channels = -1;
    return err;
  } else {
    i->channels = tmp_uint;
    DNOTE("Channels: %u\n", i->channels);
  }

  /* set the stream rate */
  dir = 0;
  tmp_uint = i->sample_rate;
  if((err = snd_pcm_hw_params_set_rate_near(i->alsa_pcm, i->hwparams,
					    &tmp_uint, &dir)) < 0) {
    ERROR("Rate %u Hz not available for playback: %s\n",
	  i->sample_rate, snd_strerror(err));
    i->sample_rate = -1;
    return err;
  } else {
    i->sample_rate = tmp_uint;
  }
  /* set buffer time */
  tmp_uint = BUFFER_TIME;
  dir = 0;
  if((err = snd_pcm_hw_params_set_buffer_time_near(i->alsa_pcm, i->hwparams,
						   &tmp_uint, &dir)) < 0) {
    WARNING("Unable to set buffer time %i: %s\n",
	    BUFFER_TIME, snd_strerror(err));
  } else {
    DNOTE("Wanted buffer time %d, got %d\n",
	  BUFFER_TIME, tmp_uint);
  }
  if((err = snd_pcm_hw_params_get_buffer_size(i->hwparams,
					      &(i->buffer_size))) < 0) {
    WARNING("Unable to get buffer size: %s\n", snd_strerror(err));    
  } else {
    DNOTE("Buffer size %u frames\n", (unsigned int)i->buffer_size);
  }
  /* set period time */
  tmp_uint = PERIOD_TIME;
  dir = 0;
  if((err = snd_pcm_hw_params_set_period_time_near(i->alsa_pcm, i->hwparams,
						   &tmp_uint, &dir)) < 0) {
    WARNING("Unable to set period time %u for playback: %s\n",
	    PERIOD_TIME, snd_strerror(err));
  } else {
    DNOTE("Period time: %u us\n", tmp_uint);
  }
  dir = 0;
  if((err = snd_pcm_hw_params_get_period_size(i->hwparams, &(i->period_size),
					      &dir)) < 0) {
    WARNING("Unable to get period size: %s\n", snd_strerror(err)); 
  } else {
    DNOTE("Period size: %u frames\n", (unsigned int)i->period_size);
  }
  /* write the parameters to device */
  if((err = snd_pcm_hw_params(i->alsa_pcm, i->hwparams)) < 0) {
    ERROR("Unable to set hw params for playback: %s\n", snd_strerror(err));
    return err;
  }

  return 0;
}

static int set_swparams(alsa_instance_t *i)
{
  int err;
  
  /* get current swparams */
  if((err = snd_pcm_sw_params_current(i->alsa_pcm, i->swparams)) < 0) {
    ERROR("Unable to determine current swparams for playback: %s\n",
	  snd_strerror(err));
  }
  /* allow transfer when at least period_size samples can be processed */
  if((err = snd_pcm_sw_params_set_avail_min(i->alsa_pcm, i->swparams,
					    MINIMUM_SAMPLES)) < 0) {
    ERROR("Unable to set avail min for playback: %s\n", snd_strerror(err));
  }
  /* align all transfers to 1 samples */
  if((err = snd_pcm_sw_params_set_xfer_align(i->alsa_pcm, 
					     i->swparams, 1)) < 0) {
    ERROR("Unable to set transfer align for playback: %s\n",
	  snd_strerror(err));
  }
  /* set swparams */	
  if((err = snd_pcm_sw_params(i->alsa_pcm, i->swparams)) < 0) {	
    ERROR("Unable to set sw params for playback: %s\n", snd_strerror(err));
    return err;
  }
  return 0;
}



/*
 * Set the device to IEC60958 (spdif) in IEC61937 mode.
 */
static int ctl_set_iec60958(alsa_instance_t *i)
{
  snd_pcm_t *handle = i->alsa_pcm;
  snd_pcm_info_t *info;
  static snd_aes_iec958_t spdif;
  int err;
  
  snd_pcm_info_alloca(&info);
  
  if ((err = snd_pcm_info(handle, info)) < 0) {
    ERROR("info: %s\n", snd_strerror(err));
    return err;
  }
  DNOTE("device: %d, subdevice: %d\n", 
	snd_pcm_info_get_device(info),
	snd_pcm_info_get_subdevice(info));                              
  {
    snd_ctl_elem_value_t *ctl;
    snd_ctl_t *ctl_handle;
    char ctl_name[12];
    int ctl_card;
    
    spdif.status[0] = IEC958_AES0_NONAUDIO | IEC958_AES0_CON_EMPHASIS_NONE;
    spdif.status[1] = IEC958_AES1_CON_ORIGINAL | IEC958_AES1_CON_PCM_CODER;
    spdif.status[2] = 0;
    spdif.status[3] = IEC958_AES3_CON_FS_48000;
    
    snd_ctl_elem_value_alloca(&ctl);
    snd_ctl_elem_value_set_interface(ctl, SND_CTL_ELEM_IFACE_PCM);
    snd_ctl_elem_value_set_device(ctl, snd_pcm_info_get_device(info));
    snd_ctl_elem_value_set_subdevice(ctl, snd_pcm_info_get_subdevice(info));
    snd_ctl_elem_value_set_name(ctl, 
				SND_CTL_NAME_IEC958("", PLAYBACK, PCM_STREAM));
    snd_ctl_elem_value_set_iec958(ctl, &spdif);
    
    ctl_card = snd_pcm_info_get_card(info);
    if(ctl_card < 0) {
      ERROR("Unable to setup the IEC958 (S/PDIF) interface - PCM has no assigned card\n");
      goto __diga_end;
    }
    
    sprintf(ctl_name, "hw:%d", ctl_card);

    if((err = snd_ctl_open(&ctl_handle, ctl_name, 0)) < 0) {
      ERROR("Unable to open the control interface '%s': %s\n", 
	    ctl_name, snd_strerror(err));
      goto __diga_end;
    }
    
    if((err = snd_ctl_elem_write(ctl_handle, ctl)) < 0) {
      ERROR("Unable to update the IEC958 control: %s\n",
	    snd_strerror(err));
      goto __diga_end;
      //Shouldn't ctl_handle be closed here?
    }
    
    snd_ctl_close(ctl_handle);
  
  __diga_end:
  }
  
  return 0;
}




static
int alsa_init(ogle_ao_instance_t *_instance,
	      ogle_ao_audio_info_t *audio_info)
{
  alsa_instance_t *i = (alsa_instance_t *)_instance;
  int single_sample_size;
  int err;
	
  
  if(i->initialized) {
    DNOTE("%s", "alsa reinit\n");
   
    if((err = snd_pcm_drain(i->alsa_pcm)) < 0) {
      ERROR("drain failed: %s\n", snd_strerror(err));
    }
    
    if((err = snd_pcm_prepare(i->alsa_pcm)) < 0) {
      ERROR("prepare failed: %s\n",
	    snd_strerror(err));
    }    
  }

  // these are allocated with alloca and automatically freed when
  // we return
  snd_pcm_hw_params_alloca(&(i->hwparams));
  snd_pcm_sw_params_alloca(&(i->swparams));
  
  i->sample_rate = audio_info->sample_rate;
  
  switch(audio_info->encoding) {
  case OGLE_AO_ENCODING_LINEAR:
    switch(audio_info->sample_resolution) {
    case 16:
      if(audio_info->byteorder == OGLE_AO_BYTEORDER_BE) {
	i->format = SND_PCM_FORMAT_S16_BE;
      } else {
	i->format = SND_PCM_FORMAT_S16_LE;
      }
      /* ok */
      break;
    default:
      /* not supported */
      return -1;
      break;
    }
    break;
  case OGLE_AO_ENCODING_IEC61937:
    // Setup the IEC60958 (S/PDIF) interface for NONAUDIO
    ctl_set_iec60958(i);
    i->format = SND_PCM_FORMAT_S16_LE;
    break;
  default:
    /* not supported */
    return -1;
    break;
  }
  
  /* Check that we actually got the requested nuber of channels,
   * the frequency and precision that we asked for?  */
  
  
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
    audio_info->chlist = malloc(audio_info->channels * sizeof(ogle_ao_chtype_t));
    
    audio_info->chlist[0] = OGLE_AO_CHTYPE_LEFT;
    audio_info->chlist[1] = OGLE_AO_CHTYPE_RIGHT;
  }
  
  i->channels = audio_info->channels;
  
  
  if((err = set_hwparams(i)) < 0) {
    ERROR("Setting of hwparams failed: %s\n", snd_strerror(err));
    audio_info->sample_rate = i->sample_rate;
    audio_info->channels = i->channels;  
    return -1;
  }
  
  if((err = set_swparams(i)) < 0) {
    ERROR("Setting of swparams failed: %s\n", snd_strerror(err));
    return -1;
  }
  
  audio_info->sample_rate = i->sample_rate;
  audio_info->channels = i->channels;  

  /* Stored as 8bit, 16bit, or 32bit */
  single_sample_size = (audio_info->sample_resolution + 7) / 8;
  if(single_sample_size > 2) {
    single_sample_size = 4;
  }
  i->sample_frame_size = single_sample_size*audio_info->channels;

  i->initialized = 1;
  
  audio_info->sample_frame_size = i->sample_frame_size;

  DNOTE("Stream parameters are %u Hz, %s, %u channels, sample size: %d\n", 
	i->sample_rate, snd_pcm_format_name(i->format), 
	i->channels, i->sample_frame_size);

  return 0;
}

static int xrun_recovery(snd_pcm_t *handle, int err)
{
  NOTE("xrun_recovery: %s\n", snd_strerror(err));
  if(err == -EPIPE) {	/* underrun */
    if((err = snd_pcm_prepare(handle)) < 0) {
      ERROR("Can't recovery from underrun, prepare failed: %s\n",
	    snd_strerror(err));
    }
    return 0;
  } else if(err == -ESTRPIPE) {
    while((err = snd_pcm_resume(handle)) == -EAGAIN)
      sleep(1);	/* wait until suspend flag is released */
    if(err < 0) {
      if((err = snd_pcm_prepare(handle)) < 0) {
	ERROR("Can't recovery from suspend, prepare failed: %s\n",
	      snd_strerror(err));
      }
    }
    return 0;
  } else {
    if((err = snd_pcm_prepare(handle)) < 0) {
      ERROR("Can't recover from %s\n", snd_strerror(err));
    }
    return 0;
    
  }
  return err;
}

static
int alsa_play(ogle_ao_instance_t *_instance, void *samples, size_t nbytes)
{
  alsa_instance_t *i = (alsa_instance_t *)_instance;
  long nsamples;
  long written;
  void *ptr;
  
  nsamples = nbytes / i->sample_frame_size;
  ptr = samples;
  while (nsamples > 0) {
    written = snd_pcm_writei(i->alsa_pcm, ptr, nsamples);
    if(written == -EAGAIN) {
      //printf("buffer overrun, retrying..\n");
      snd_pcm_wait(i->alsa_pcm, 1000);
      continue;
    }
    if(written < 0) {
      //printf("buffer underrun, retrying..\n");
      if(xrun_recovery(i->alsa_pcm, written) < 0) {
	ERROR("Write error: %s\n", snd_strerror(written));
	return -1;
      }
      break;	/* skip one period */
    }
    ptr += written * i->channels;
    nsamples -= written;
  }
  
  i->samples_written += nbytes / i->sample_frame_size;
  
  return 0;
}

static
int  alsa_odelay(ogle_ao_instance_t *_instance, uint32_t *samples_return)
{
  alsa_instance_t *i = (alsa_instance_t *)_instance;
  snd_pcm_sframes_t avail;
  int err;
  
  if(snd_pcm_state(i->alsa_pcm) != SND_PCM_STATE_RUNNING) {
    *samples_return = 0;
    return 0;
  }
  if ((err = snd_pcm_delay(i->alsa_pcm, &avail)) < 0) {
    ERROR("odelay error: %s\n", snd_strerror(err));
    avail = 0;
  }

  // If underrun, then we have 0 'delay'.
  if (avail < 0) {
    avail = 0;
  }
  // must this be called after snd_pcm_delay?
  // confused from the also documentation
  snd_pcm_avail_update(i->alsa_pcm);
  
  *samples_return = (uint32_t)avail;
  
  return 0;
}

static
void alsa_close(ogle_ao_instance_t *_instance)
{
  char *name;
  alsa_instance_t *i = (alsa_instance_t *)_instance;
  
  name = snd_pcm_name(i->alsa_pcm);
  DNOTE("Closing alsa pcm device: %s\n", name ? name : "");

  snd_pcm_close(i->alsa_pcm);
}

static
int alsa_flush(ogle_ao_instance_t *_instance)
{
  alsa_instance_t *i = (alsa_instance_t *)_instance;
  int err;
  
  if((err = snd_pcm_drop(i->alsa_pcm)) < 0) {
    ERROR("drop failed: %s\n", snd_strerror(err));
  }
  
  return 0;
}

static
int alsa_drain(ogle_ao_instance_t *_instance)
{
  alsa_instance_t *i = (alsa_instance_t *)_instance;
  int err;
  
  if((err = snd_pcm_drain(i->alsa_pcm)) < 0) {
    ERROR("drain failed: %s\n", snd_strerror(err));
  }
  
  return 0;
}

static
ogle_ao_instance_t *alsa_open(char *device)
{
  alsa_instance_t *i;
  int err = 0;
  
  i = malloc(sizeof(alsa_instance_t));
  
  if(i == NULL)
    return NULL;
  
  i->ao.init   = alsa_init;
  i->ao.play   = alsa_play;
  i->ao.close  = alsa_close;
  i->ao.odelay = alsa_odelay;
  i->ao.flush  = alsa_flush;
  i->ao.drain  = alsa_drain;
  
  i->initialized = 0;
  i->sample_rate = 0;
  i->samples_written = 0;
  i->sample_frame_size = 0;
  
  if((err = snd_output_stdio_attach(&logs, stderr, 0)) < 0) {
    ERROR("Output log failed: %s\n", snd_strerror(err));
    return NULL;
  }
  
  DNOTE("Opening alsa pcm device: %s\n", device);
  
  if((err = snd_pcm_open(&(i->alsa_pcm), device, 
			 SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK)) < 0) {
    ERROR("Opening alsa pcm device '%s': %s\n", device, snd_strerror(err));
    return NULL;
  }    
  
  return (ogle_ao_instance_t *)i;
}

/* The one directly exported function */
ogle_ao_instance_t *ao_alsa_open(char *dev)
{
  return alsa_open(dev);
}

#endif
