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

#ifdef LIBOGLEAO_OSS

#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#ifdef	HAVE_STROPTS_H
#include <stropts.h>
#endif
#include <stdio.h>

#if defined(__OpenBSD__) || defined(__NetBSD__)
#include <soundcard.h>
#else
#include <sys/soundcard.h>
#endif
/* AFMT_AC3 is really IEC61937 / IEC60958, mpeg/ac3/dts over spdif */
#ifndef AFMT_AC3
#define AFMT_AC3        0x00000400      /* Dolby Digital AC3 */
#endif
#ifndef AFMT_S32_LE
#define AFMT_S32_LE     0x00001000  /* 32/24-bits, in 24bit use the msbs */ 
#endif
#ifndef AFMT_S32_BE
#define AFMT_S32_BE     0x00002000  /* 32/24-bits, in 24bit use the msbs */ 
#endif

#include "ogle_ao.h"
#include "ogle_ao_private.h"

#include <string.h>

typedef struct oss_instance_s {
  ogle_ao_instance_t ao;
  int fd;
  int sample_frame_size;
  int fragment_size;
  int nr_fragments;
  int fmt;
  int channels;
  int speed;
  int initialized;
  char *dev;
} oss_instance_t;

static int log2(int val)
{
  int res = 0;

  while(val != 1) {
    val >>=1;
    ++res;
  }
  return res;
}

static
int test_audio(oss_instance_t *i)
{
  int sample_size;
  int buf_size;
  char *buf;
  int32_t *data32 = NULL;
  int16_t *data16 = NULL;
  int ch;
  int swap = 0;

  switch(i->fmt) {
  case AFMT_S32_BE:
  case AFMT_S16_BE:
    if(OGLE_AO_BYTEORDER_NE != OGLE_AO_BYTEORDER_BE) {
      swap = 1;
    }
    break;
  case AFMT_S32_LE:
  case AFMT_S16_LE:
    if(OGLE_AO_BYTEORDER_NE != OGLE_AO_BYTEORDER_LE) {
      swap = 1;
    }
    break;
  default:
    break;
  }
  switch(i->fmt) {
  case AFMT_S32_BE:
    sample_size = 4;
  case AFMT_S32_LE:
    sample_size = 4;
    break;
  case AFMT_S16_BE:
  case AFMT_S16_LE:
    sample_size = 2;
    break;
  default:
    fprintf(stderr, "oss_audio: Can't test audio format %d\n", i->fmt);
    return -1;
    break;
  }

  buf_size = sample_size*i->channels*i->speed/10;
  buf = malloc(buf_size);
  if(!buf) {
    perror("oss_audio: malloc\n");
    return -1;
  }
  data16 = (int16_t *)buf;
  data32 = (int32_t *)buf;

  for(ch = 0; ch < i->channels; ch++) {
    int f, wr, m;
    for(m = 0; m < 2; m++) {
      wr = 0;
      for(f = 100; f <= 2500; f+=400) {
	int period, step, s, n;
	period = i->speed / f;
	step = 16384 / period;
	memset(buf, 0, buf_size);
	for(n=0, s=0; n < i->speed/10; n+=sample_size*i->channels, s+=step) {
	  if(s >= 16384) {
	    s = 0;
	  }
	  if(sample_size == 2) {
	    data16[n+ch] = swap ? ((s>>8)&0xff) | s<<8: s;
	  } else {
	    data32[n+ch] = swap ? (((s>>8)&0xff) | s<<8)&0xffff : s<<16;
	  }
	}
	wr+= write(i->fd, buf, buf_size);
	if(f < 105) {
	  f-=399;
	}
      }
      fprintf(stderr, "audio_out: ch %d, written %d bytes\n", ch, wr);
    }
  }
  return 0;
}

static
int oss_init(ogle_ao_instance_t *_instance,
		 ogle_ao_audio_info_t *audio_info)
{
  oss_instance_t *instance = (oss_instance_t *)_instance;
  int single_sample_size;
  int number_of_channels, sample_format, original_sample_format, sample_speed;
  int supported_formats;
  uint32_t fragment; 
  uint16_t nr_fragments;
  uint16_t fragment_size;
  audio_buf_info info;
  
  if(instance->initialized) {
    if(getenv("OGLE_OSS_RESET_BUG")) {
      fprintf(stderr, "oss_audio: closing/reopening %s\n", instance->dev);
      close(instance->fd);
      instance->fd = open(instance->dev, O_WRONLY);
      if(instance->fd < 0) {
	perror("open failed");
	return -1;
      }
    } else {
      // SNDCTL_DSP_SYNC resets the audio device so we can set new parameters
      ioctl(instance->fd, SNDCTL_DSP_SYNC, 0);
    }
  } else {
    
    
    // set fragment size if requested
    // can only be done once after open
    
    if(audio_info->fragment_size != -1) {
      if(log2(audio_info->fragment_size) > 0xffff) {
	fragment_size = 0xffff;
      } else {
	fragment_size = log2(audio_info->fragment_size);
      }
      if(audio_info->fragments != -1) {
	if(audio_info->fragments > 0xffff) {
	  nr_fragments = 0xffff;
	} else {
	  nr_fragments = audio_info->fragments;
	}
      } else {
	nr_fragments = 0x7fff;
      }
      
      fragment = (nr_fragments << 16) | fragment_size;
    
      if(ioctl(instance->fd, SNDCTL_DSP_SETFRAGMENT, &fragment) == -1) {
	perror("SNDCTL_DSP_SETFRAGMENT");
	//this is not fatal
      }
    }
  }

  // Query supported formats

  if(ioctl(instance->fd, SNDCTL_DSP_GETFMTS, &supported_formats) == -1) {
    perror("SNDCTL_DSP_GETFMTS");
    //say that all formats are supported if we can't get the formats
    supported_formats = -1;
  }

  // Set sample format, number of channels, and sample speed
  // The order here is important according to the manual

  switch(audio_info->encoding) {
  case OGLE_AO_ENCODING_LINEAR:
    switch(audio_info->sample_resolution) {
    case 24:
      if(audio_info->byteorder == OGLE_AO_BYTEORDER_BE) {
	sample_format = AFMT_S32_BE;
      } else {
	sample_format = AFMT_S32_LE;
      }
      audio_info->sample_resolution = 32;
      if(supported_formats & sample_format) {
	break;
      }
    case 16:
      if(audio_info->byteorder == OGLE_AO_BYTEORDER_BE) {
	sample_format = AFMT_S16_BE;
      } else {
	sample_format = AFMT_S16_LE;
      }
      audio_info->sample_resolution = 16;
      // don't check if this format is supported, try it anyway
      // and hope to get a usable format 
      break;
    default:
      audio_info->sample_resolution = -1;
      return -1;
      break;
    }
    break;
  case OGLE_AO_ENCODING_IEC61937:
    sample_format = AFMT_AC3; // this is iec61937
                              // handles ac3/dts/... over iec60958 (spdif)
    audio_info->sample_resolution = 16;
    break;
  default:
    audio_info->encoding = -1;
    return -1;
    break;
  }
  original_sample_format = sample_format;
  if(ioctl(instance->fd, SNDCTL_DSP_SETFMT, &sample_format) == -1) {
    perror("SNDCTL_DSP_SETFMT");
    return -1;
  }
  
  instance->fmt = sample_format;
  
  // Test if we got the right format
  if (sample_format != original_sample_format) {
    switch(sample_format) {
    case AFMT_S16_BE:
      audio_info->encoding = OGLE_AO_ENCODING_LINEAR;
      audio_info->sample_resolution = 16;
      audio_info->byteorder = OGLE_AO_BYTEORDER_BE;
      break;
    case AFMT_S16_LE:
      audio_info->encoding = OGLE_AO_ENCODING_LINEAR;
      audio_info->sample_resolution = 16;
      audio_info->byteorder = OGLE_AO_BYTEORDER_LE;
      break;      
    case AFMT_S32_BE:
      audio_info->encoding = OGLE_AO_ENCODING_LINEAR;
      audio_info->sample_resolution = 32;
      audio_info->byteorder = OGLE_AO_BYTEORDER_BE;
      break;
    case AFMT_S32_LE:
      audio_info->encoding = OGLE_AO_ENCODING_LINEAR;
      audio_info->sample_resolution = 32;
      audio_info->byteorder = OGLE_AO_BYTEORDER_LE;
      break;      
    default:
      fprintf(stderr, "*** format: %0x\n", sample_format);
      audio_info->encoding = OGLE_AO_ENCODING_NONE;
      audio_info->sample_resolution = 0;
      audio_info->byteorder = 0;
      break;
    }
  }
  
  if(audio_info->chtypes != OGLE_AO_CHTYPE_UNSPECIFIED) {
    // do this only if we have requested specific channels in chtypes
    // otherwise trust the nr channels
#ifdef SNDCTL_DSP_GETCHANNELMASK
    int chmask;
    if(ioctl(instance->fd, SNDCTL_DSP_GETCHANNELMASK, &chmask) == -1) {
      //driver doesn't support this, assume it does 2ch stereo
      perror("SNDCTL_DSP_GETCHANNELMASK");
      audio_info->chtypes = OGLE_AO_CHTYPE_LEFT | OGLE_AO_CHTYPE_RIGHT;
      audio_info->channels = 2;
    } else {
      ogle_ao_chtype_t chtypes = 0;
      int nr_ch = 0;

      audio_info->channels = 0;

      if(chmask & DSP_BIND_FRONT) {
	nr_ch += 2;
	chtypes |= OGLE_AO_CHTYPE_LEFT | OGLE_AO_CHTYPE_RIGHT;
      }
      if((audio_info->chtypes & chtypes) == audio_info->chtypes) {
	//we don't have all needed types
	if(chmask & DSP_BIND_SURR) {
	  nr_ch += 2;
	  chtypes |= OGLE_AO_CHTYPE_REARLEFT | OGLE_AO_CHTYPE_REARRIGHT;
	}
	
	if((audio_info->chtypes & chtypes) != audio_info->chtypes) {
	  //we don't have all needed types
	  if(chmask & DSP_BIND_CENTER_LFE) {
	    nr_ch += 2;
	    chtypes |= OGLE_AO_CHTYPE_CENTER | OGLE_AO_CHTYPE_LFE;
	  }
	}
      }
      audio_info->chtypes = chtypes;
      audio_info->channels = nr_ch;
    }
#else
    audio_info->chtypes = OGLE_AO_CHTYPE_LEFT | OGLE_AO_CHTYPE_RIGHT;
    audio_info->channels = 2;
#endif
  }
  if(audio_info->chlist) {
    free(audio_info->chlist);
    audio_info->chlist = NULL;
  }

  number_of_channels = audio_info->channels;
  if(ioctl(instance->fd, SNDCTL_DSP_CHANNELS, &number_of_channels) == -1) {
    perror("SNDCTL_DSP_CHANNELS");
    audio_info->channels = -1;
    return -1;
  }

  if(audio_info->chtypes != OGLE_AO_CHTYPE_UNSPECIFIED) {
    audio_info->chlist = malloc(number_of_channels * sizeof(ogle_ao_chtype_t));
    if(number_of_channels > 0) {
      audio_info->chlist[0] = OGLE_AO_CHTYPE_LEFT;
      if(number_of_channels > 1) {
	audio_info->chlist[1] = OGLE_AO_CHTYPE_RIGHT;
	if(number_of_channels > 2) {
	  audio_info->chlist[2] = OGLE_AO_CHTYPE_REARLEFT;
	  audio_info->chlist[3] = OGLE_AO_CHTYPE_REARRIGHT;
	  if(number_of_channels > 4) {
	    audio_info->chlist[4] = OGLE_AO_CHTYPE_CENTER;
	    audio_info->chlist[5] = OGLE_AO_CHTYPE_LFE;
	  }
	}
      }
    }
  }
  instance->channels = number_of_channels;

  // report back (maybe we can't do stereo or something...)
  audio_info->channels = number_of_channels;

  sample_speed = audio_info->sample_rate;
  if(ioctl(instance->fd, SNDCTL_DSP_SPEED, &sample_speed) == -1) {
    perror("SNDCTL_DSP_SPEED");
    audio_info->sample_rate = -1;
    return -1;
  }
  instance->speed = sample_speed;
  // report back the actual speed used
  audio_info->sample_rate = sample_speed;

  single_sample_size = (audio_info->sample_resolution + 7) / 8;
  if(single_sample_size > 2) {
    single_sample_size = 4;
  }
  instance->sample_frame_size = single_sample_size*number_of_channels;

  audio_info->sample_frame_size = instance->sample_frame_size;
  
  
  if(ioctl(instance->fd, SNDCTL_DSP_GETOSPACE, &info) == -1) {
    perror("SNDCTL_DSP_GETOSPACE");
    audio_info->fragment_size = -1;
    audio_info->fragments = -1;
    instance->fragment_size = -1;
    instance->nr_fragments = -1;
  } else {
    audio_info->fragment_size = info.fragsize;
    audio_info->fragments = info.fragstotal;
    instance->fragment_size = info.fragsize;
    instance->nr_fragments = info.fragstotal;
  }
  
  instance->initialized = 1;

  if(getenv("OGLE_OSS_TEST")) {
    test_audio(instance);
  }
  
  return 0;
}

/*
 * Only to be called after a SNDCTL_DSP_RESET or SNDCTL_DSP_SYNC
 * and after init
 */
static int oss_reinit(oss_instance_t *instance)
{
  int sample_format;
  int nr_channels;
  int sample_speed;
  audio_buf_info info;
  
  if(!instance->initialized) {
    return -1;
  }

  sample_format = instance->fmt;
  if(ioctl(instance->fd, SNDCTL_DSP_SETFMT, &sample_format) == -1) {
    perror("SNDCTL_DSP_SETFMT");
    return -1;
  }
  if(sample_format != instance->fmt) {
    fprintf(stderr, "oss_audio: couldn't reinit sample_format\n");
    return -1;
  }
  
  nr_channels = instance->channels;
  if(ioctl(instance->fd, SNDCTL_DSP_CHANNELS, &nr_channels) == -1) {
    perror("SNDCTL_DSP_CHANNELS");
    return -1;
  }
  if(nr_channels != instance->channels) {
    fprintf(stderr, "oss_audio: couldn't reinit nr_channels\n");
    return -1;
  }

  sample_speed = instance->speed;
  if(ioctl(instance->fd, SNDCTL_DSP_SPEED, &sample_speed) == -1) {
    perror("SNDCTL_DSP_SPEED");
    return -1;
  }
  if(sample_speed != instance->speed) {
    fprintf(stderr, "oss_audio: couldn't reinit speed\n");
    return -1;
  }
  
  if(ioctl(instance->fd, SNDCTL_DSP_GETOSPACE, &info) == -1) {
    perror("SNDCTL_DSP_GETOSPACE");
  } else {
    if(instance->fragment_size != info.fragsize) {
      fprintf(stderr, "oss_audio: fragment size differs after reinit\n");
    }
    if(instance->nr_fragments != info.fragstotal) {
      fprintf(stderr, "oss_audio: nr_fragments size differs after reinit\n");
    }
  }
  
  return 0;
}


static
int oss_play(ogle_ao_instance_t *_instance, void *samples, size_t nbyte)
{
  oss_instance_t *instance = (oss_instance_t *)_instance;
  int written;
  
  written = write(instance->fd, samples, nbyte);
  if(written == -1) {
    perror("audio write");
    return -1;
  }
  
  return 0;
}

static
int oss_odelay(ogle_ao_instance_t *_instance, uint32_t *samples_return)
{
  oss_instance_t *instance = (oss_instance_t *)_instance;
  int res;
  int odelay;

  res = ioctl(instance->fd, SNDCTL_DSP_GETODELAY, &odelay);
  if(res == -1) {
    perror("SNDCTL_DSP_GETODELAY");
    return -1;
  }

  *samples_return = odelay / instance->sample_frame_size;

  return 0;
}

static
void oss_close(ogle_ao_instance_t *_instance)
{
  oss_instance_t *instance = (oss_instance_t *)_instance;
  if(instance->dev) {
    free(instance->dev);
  }
  close(instance->fd);
}

static
int oss_flush(ogle_ao_instance_t *_instance)
{
  oss_instance_t *instance = (oss_instance_t *)_instance;
  
  ioctl(instance->fd, SNDCTL_DSP_RESET, 0);

  //Some cards need a reinit after DSP_RESET, maybe all do?
  oss_reinit(instance);

  return 0;
}

static
int oss_drain(ogle_ao_instance_t *_instance)
{
  oss_instance_t *instance = (oss_instance_t *)_instance;
  
  ioctl(instance->fd, SNDCTL_DSP_SYNC, 0);
  
  return 0;
}

static 
ogle_ao_instance_t *oss_open(char *dev)
{
    oss_instance_t *instance;
    
    instance = malloc(sizeof(oss_instance_t));
    if(instance == NULL)
      return NULL;
    
    instance->ao.init   = oss_init;
    instance->ao.play   = oss_play;
    instance->ao.close  = oss_close;
    instance->ao.odelay = oss_odelay;
    instance->ao.flush  = oss_flush;
    instance->ao.drain  = oss_drain;
    
    instance->initialized = 0;
    instance->dev = strdup(dev);
    instance->fd = open(dev, O_WRONLY);
    if(instance->fd < 0) {
      free(instance);
      return NULL;
    }
    
    return (ogle_ao_instance_t *)instance;
}

/* The one directly exported function */
ogle_ao_instance_t *ao_oss_open(char *dev)
{
  return oss_open(dev);
}

#endif
