#ifndef AUDIO_CONFIG_H
#define AUDIO_CONFIG_H

/* Ogle - A video player
 * Copyright (C) 2002 Björn Englund, Håkan Hjort
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

#include <libogleao/ogle_ao.h>
#include "timemath.h"
#include "audio_types.h"

typedef enum {
  SampleFormat_None,
  SampleFormat_LPCM,
  SampleFormat_Unsigned,
  SampleFormat_Signed,
  SampleFormat_A52float,
  SampleFormat_MadFixed,
  SampleFormat_AC3Frame,
  SampleFormat_DTSFrame,
  SampleFormat_IEC61937
} SampleFormat_t;

typedef struct {
  SampleFormat_t sample_format;
  int nr_channels;
  ChannelType_t *ch_array;
  int interleaved; //the samples for the different channels are interleaved
  int sample_rate; // in Hz
  int sample_resolution; //bits per sample (used)
  int sample_size;       //bytes per sample (including padding)
  int sample_frame_size; //bytes per sample frame (samples for all channels)
  int sample_byte_order; //endianess
} audio_format_t;

typedef enum {
  SyncType_odelay,
  SyncType_clock
} SyncType_t;

typedef struct {
  int delay_resolution;
  int delay_resolution_set;
  int max_sync_diff;
  int prev_delay;
  int samples_added;
  SyncType_t type;
  int resample;
  clocktime_t offset;
} audio_sync_t;

typedef struct {
  audio_format_t src_format;  //the source audio format
  audio_format_t dst_format;  //the destination audio format
  ogle_ao_instance_t *adev_handle;
  ogle_ao_audio_info_t *ainfo; //the audio format of the sound driver
  audio_sync_t sync;
} audio_config_t;


audio_config_t *audio_config_init(void);
int audio_config(audio_config_t *aconf,
		 ChannelType_t channels,
		 int sample_rate, int sample_resolution);
void audio_config_close(audio_config_t *aconf);

#endif /* AUDIO_CONFIG_H */
