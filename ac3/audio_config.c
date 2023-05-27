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

#include <stdio.h>
#include <string.h>

#include "audio_types.h"
#include "parse_config.h"
#include "audio_config.h"

#include "sync.h"
#include "debug_print.h"

audio_config_t *audio_config_init(void)
{
  audio_config_t *conf;
  char *type;
  int ms_offset;
  
  conf = malloc(sizeof(audio_config_t));
  memset(conf, 0, sizeof(audio_config_t));
  
  conf->dst_format.nr_channels = -1;
  conf->dst_format.ch_array = NULL;
  conf->adev_handle = NULL;
  conf->ainfo = NULL;
  
  conf->sync.delay_resolution = 0;
  conf->sync.delay_resolution_set = 0;
  conf->sync.max_sync_diff = 0;
  conf->sync.prev_delay = -1;
  conf->sync.samples_added = 0;

  type = get_sync_type();
  if(!strcmp(type, "clock")) {
    conf->sync.type = SyncType_clock;
  } else {
    conf->sync.type = SyncType_odelay;
  }
  conf->sync.resample = get_sync_resample();
  ms_offset = get_sync_offset();
  if(ms_offset > 999 || ms_offset < -999) {
    ms_offset = 0;
  }
  TIME_S(conf->sync.offset) = 0;
  TIME_SS(conf->sync.offset) = ms_offset * CT_FRACTION/1000;

  return conf;
}


channel_config_t *get_config(ChannelType_t chtypemask_wanted)
{
  channel_config_t *configs;
  int nr_configs;
  channel_config_t *chconf = NULL;
  int n;
  ChannelType_t chtypemask_avail = 0;

  nr_configs = get_channel_configs(&configs);
  
  DNOTE("%s", "Searching <speakers> for:\n");
  if(chtypemask_wanted & ChannelTypeMask_Channels) {
    DNOTE("%s", "");
    for(n = 1; n <= 0x100; n = n << 1) {
      if(n & chtypemask_wanted) {
	DNOTEC(" '%s'", channeltype_str(n));
      }
    }
    DNOTEC("%s", "\n");
    if(chtypemask_wanted & ChannelTypeMask_Streams) {
      DNOTE("%s", "or for:\n");
    }
  }

  if(chtypemask_wanted & ChannelTypeMask_Streams) {
    DNOTE(" '%s'",
	  channeltype_str(chtypemask_wanted & ChannelTypeMask_Streams));
  }
  DNOTEC("%s", "\n");
  
  for(n = 0; n < nr_configs; n++) {
    int m;
    int nr_ch;
    
    chtypemask_avail = 0;
    nr_ch = configs[n].nr_ch;
    DNOTE("%s", "<channel_config>\n");
    for(m = 0; m < nr_ch; m++) {
      chtypemask_avail |= configs[n].chtype[m];
      DNOTE("  <chtype>%s</chtype>\n", channeltype_str(configs[n].chtype[m]));
    }
    DNOTE("%s", "</channel_config>\n");
    /* Want some channel based format, check if we can output that. */
    if(chtypemask_wanted & ChannelTypeMask_Channels) {
      if((chtypemask_wanted & chtypemask_avail) ==
	 (chtypemask_wanted & ChannelTypeMask_Channels)) {
	chconf = &configs[n];
	DNOTE("%s", "Found matching channel_config\n");
	break;
      }
    }
    /* Want some stream based format, check if it matches. */
    if(chtypemask_wanted & ChannelTypeMask_Streams) {
      if((chtypemask_wanted & chtypemask_avail) ==
	 (chtypemask_wanted & ChannelTypeMask_Streams)) {
	chconf = &configs[n];
	DNOTE("%s", "Found matching channel_config\n");
	break;
      }
    }
  }
  if((!chconf) && (nr_configs > 0)) {
    chconf = &configs[nr_configs-1];
    DNOTE("%s", "Didn't find matching channel_config, using the last one\n");
  } else if(nr_configs == 0) {
    DNOTE("%s", "No channel configs found\n");
  }
  
  return chconf;
}

void free_config(channel_config_t *conf)
{
  return;

  if(conf) {
    if(conf->chtype) {
      free(conf->chtype);
    }
    free(conf);
  }

  return;
}


int audio_config(audio_config_t *aconf,
		 ChannelType_t src_chtypemask,
		 int sample_rate, int sample_resolution)
{
  int n;
  int output_channels;
  int frag_size = 4096;
  ogle_ao_encoding_t encoding;

  channel_config_t *config = NULL;
  // returns NULL if no config
  // returns first config covering the requested chtypemask
  // returns last config if no covering config found
  // free_config should be called when config not needed more
  config = get_config(src_chtypemask);
  if(config) {
    if((config->nr_ch > 0) && (config->chtype[0] & ChannelTypeMask_Streams)) {
      if(config->chtype[0] == ChannelType_AC3) {
	aconf->dst_format.sample_resolution = 0; //not used
	aconf->dst_format.nr_channels = 2;
	aconf->dst_format.ch_array = 
	  realloc(aconf->dst_format.ch_array, 
		  sizeof(ChannelType_t) * 2);
	aconf->dst_format.ch_array[0] = ChannelType_AC3;
	aconf->dst_format.ch_array[1] = ChannelType_AC3;
	aconf->dst_format.sample_format = SampleFormat_IEC61937;

      } else if(config->chtype[0] == ChannelType_DTS) {
	aconf->dst_format.sample_resolution = 2;
	aconf->dst_format.nr_channels = 2;
	aconf->dst_format.ch_array = 
	  realloc(aconf->dst_format.ch_array, 
		  sizeof(ChannelType_t) * 2);
	aconf->dst_format.ch_array[0] = ChannelType_DTS;
	aconf->dst_format.ch_array[1] = ChannelType_DTS;
	aconf->dst_format.sample_format = SampleFormat_IEC61937;

      } else {

	FATAL("channel type: %d not implemented\n", config->chtype[0]);
      }
    } else {
      aconf->dst_format.sample_format = SampleFormat_Signed;
      output_channels = config->nr_ch;
      if(aconf->dst_format.nr_channels != output_channels) {
	aconf->dst_format.ch_array = 
	  realloc(aconf->dst_format.ch_array, 
		  sizeof(ChannelType_t) * output_channels);
      }
      aconf->dst_format.sample_resolution = sample_resolution;
      aconf->dst_format.nr_channels = output_channels;
      for(n = 0; n < output_channels; n++) {
	aconf->dst_format.ch_array[n] = config->chtype[n];
      }
    }
  } else {
    aconf->dst_format.sample_format = SampleFormat_Signed;
    aconf->dst_format.sample_resolution = sample_resolution;
    aconf->dst_format.nr_channels = 0;
    if(aconf->dst_format.ch_array) {
      free(aconf->dst_format.ch_array);
      aconf->dst_format.ch_array = NULL;
    }
  }
  
  
  {
    ao_driver_t *drivers;
    
    drivers = ao_drivers();
    if(!aconf->adev_handle) {
      int n;
      int driver_n = 0;
      char *driver_str = get_audio_driver();
      
      if(driver_str != NULL) {
	for(n = 0; drivers[n].name != NULL; n++) {
	  if(!strcmp(drivers[n].name, driver_str)) {
	    driver_n = n;
	    break;
	  }
	}
	if(drivers[n].name == NULL) {
	  NOTE("Audio driver '%s' not found\n", driver_str);
	}
      }

      NOTE("Using audio driver '%s'\n", drivers[driver_n].name);
	     
      if(drivers[driver_n].open != NULL) {
	char *dev_string;
	if(!strcmp("alsa", drivers[driver_n].name)) {
	  dev_string = get_audio_alsa_name();
	} else {
	  dev_string = get_audio_device();
	}
	aconf->adev_handle = ogle_ao_open(drivers[driver_n].open, dev_string);
	if(!aconf->adev_handle) {
	  FATAL("failed opening the %s audio driver at %s\n",
		drivers[driver_n].name, dev_string);
	  exit(1);
	}
      } else {
	FATAL("%s", "ogle_ao_open not taken, no audio ouput driver!\n");
	exit(1);
      }
    }
    // DNOTE("%s", "ogle_ao_open passed\n");
  }
  if(!aconf->ainfo) {
    aconf->ainfo = malloc(sizeof(ogle_ao_audio_info_t));
    aconf->ainfo->chlist = NULL;
  }

  if(aconf->dst_format.sample_format == SampleFormat_IEC61937) {
    aconf->ainfo->sample_rate = sample_rate;
    aconf->ainfo->sample_resolution = sample_resolution;
    aconf->ainfo->byteorder = OGLE_AO_BYTEORDER_NE;
    aconf->ainfo->channels = aconf->dst_format.nr_channels;
    encoding = OGLE_AO_ENCODING_IEC61937;
    aconf->ainfo->fragment_size = frag_size;
    aconf->ainfo->fragments = -1;  // don't limit
  } else {
    
    aconf->ainfo->sample_rate = sample_rate;
    aconf->ainfo->sample_resolution = sample_resolution;
    aconf->ainfo->byteorder = OGLE_AO_BYTEORDER_NE;
    aconf->ainfo->channels = aconf->dst_format.nr_channels;
    encoding = OGLE_AO_ENCODING_LINEAR;
    aconf->ainfo->fragment_size = frag_size;
    aconf->ainfo->fragments = -1;  // don't limit
    if(config) {
      aconf->ainfo->chtypes = OGLE_AO_CHTYPE_UNSPECIFIED;
    } else {
      ogle_ao_chtype_t ao_chmask = 0;
      if(src_chtypemask & ChannelType_Left) {
	ao_chmask |= OGLE_AO_CHTYPE_LEFT;
      }
      if(src_chtypemask & ChannelType_Right) {
	ao_chmask |= OGLE_AO_CHTYPE_RIGHT;
      }
      if(src_chtypemask & ChannelType_Center) {
	ao_chmask |= OGLE_AO_CHTYPE_CENTER;
      }
      if(src_chtypemask & ChannelType_LeftSurround) {
	ao_chmask |= OGLE_AO_CHTYPE_REARLEFT;
      }
      if(src_chtypemask & ChannelType_RightSurround) {
	ao_chmask |= OGLE_AO_CHTYPE_REARRIGHT;
      }
      if(src_chtypemask & ChannelType_LFE) {
	ao_chmask |= OGLE_AO_CHTYPE_LFE;
      }
      if(src_chtypemask & ChannelType_Mono) {
	ao_chmask |= OGLE_AO_CHTYPE_MONO;
      }
      if(src_chtypemask & ChannelType_Surround) {
	ao_chmask |= OGLE_AO_CHTYPE_REARMONO;
      }
      if(src_chtypemask & ChannelType_CenterSurround) {
	ao_chmask |= OGLE_AO_CHTYPE_REARCENTER;
      }
      
      if(ao_chmask == 0) {
	ao_chmask = OGLE_AO_CHTYPE_NULL;
      }
      aconf->ainfo->chtypes = ao_chmask;
    }
  }

  aconf->ainfo->encoding = encoding;


  // check return value
  if(ogle_ao_init(aconf->adev_handle, aconf->ainfo) == -1) {

    FATAL("%s", "ogle_ao_init: ");
    if(aconf->ainfo->sample_rate == -1) {
      FATALC("sample_rate %d failed", sample_rate);
    } else if(aconf->ainfo->sample_resolution == -1 ||
	      aconf->ainfo->encoding == -1 ||
	      aconf->ainfo->byteorder == -1) {
      FATALC("encoding %d, resolution %d, byte order %d failed",
	    encoding, sample_resolution, OGLE_AO_BYTEORDER_NE);
    } else if(aconf->ainfo->channels == -1) {
      FATALC("channels %d failed", aconf->dst_format.nr_channels);
    } 
    FATALC("%s", "\n");
  } else {
    if(aconf->ainfo->sample_rate != sample_rate) {
      ERROR("wanted sample rate %d, got %d\n",
	    sample_rate,
	    aconf->ainfo->sample_rate);
    }
    if(aconf->ainfo->sample_resolution != sample_resolution) {
      ERROR("wanted sample resolution %d, got %d\n",
	    sample_resolution,
	    aconf->ainfo->sample_resolution);	   
    }
    if(aconf->ainfo->byteorder != OGLE_AO_BYTEORDER_NE) {
      ERROR("wanted byte order %d, got %d\n",
	    OGLE_AO_BYTEORDER_NE, aconf->ainfo->byteorder);
    }
    if(aconf->ainfo->channels != aconf->dst_format.nr_channels &&
       aconf->dst_format.nr_channels != -1) {
      NOTE("wanted %d channels, got %d\n",
	   aconf->dst_format.nr_channels,
	   aconf->ainfo->channels); 
      if(config) {
	if((config->nr_ch > 0) && 
	   (config->chtype[0] & ChannelTypeMask_Streams)) {
	  
	} else {
	  output_channels = aconf->ainfo->channels;
	  if(aconf->dst_format.nr_channels != output_channels) {
	    aconf->dst_format.ch_array = 
	      realloc(aconf->dst_format.ch_array, 
		      sizeof(ChannelType_t) * output_channels);
	  }
	  aconf->dst_format.nr_channels = output_channels;
	  for(n = 0; n < output_channels; n++) {
	    if(n < config->nr_ch) {
	      aconf->dst_format.ch_array[n] = config->chtype[n];
	    } else {
	      aconf->dst_format.ch_array[n] = ChannelType_Null;
	    }
	  }
	}
      }
    }
    if(aconf->ainfo->encoding != encoding) {
      ERROR("wanted encoding %d, got %d\n",
	    encoding,
	    aconf->ainfo->encoding);
      switch(aconf->ainfo->encoding) {
      case OGLE_AO_ENCODING_IEC61937:
	aconf->dst_format.sample_format = SampleFormat_IEC61937;
	break;
      case OGLE_AO_ENCODING_LINEAR:
      default:	
	aconf->dst_format.sample_format = SampleFormat_Signed;
	break;
      }
    }
    if(frag_size != -1 && aconf->ainfo->fragment_size != frag_size) {
      NOTE("wanted fragment size %d, got %d\n",
	   frag_size,
	   aconf->ainfo->fragment_size);
    }

    DNOTE("channels %d, encoding %d, resolution %d\n", 
	  aconf->ainfo->channels,
	  aconf->ainfo->encoding,
	  aconf->ainfo->sample_resolution);
    DNOTE("byte order %d, rate %d\n",
	  aconf->ainfo->byteorder,
	  aconf->ainfo->sample_rate);
    DNOTE("fragments %d, size %d\n",
	  aconf->ainfo->fragments,
	  aconf->ainfo->fragment_size);
    if(!config) {
      aconf->dst_format.nr_channels = aconf->ainfo->channels;
      aconf->dst_format.ch_array = 
	realloc(aconf->dst_format.ch_array, 
		sizeof(ChannelType_t)*aconf->dst_format.nr_channels);

      for(n = 0; n < aconf->dst_format.nr_channels; n++) {
	ChannelType_t ct;
	switch(aconf->ainfo->chlist[n]) {
	case OGLE_AO_CHTYPE_LEFT:
	  ct = ChannelType_Left;
	  break;
	case OGLE_AO_CHTYPE_RIGHT:
	  ct = ChannelType_Right;
	  break;
	case OGLE_AO_CHTYPE_CENTER:
	  ct = ChannelType_Center;
	  break;
	case OGLE_AO_CHTYPE_REARLEFT:
	  ct = ChannelType_LeftSurround;
	  break;
	case OGLE_AO_CHTYPE_REARRIGHT:
	  ct = ChannelType_RightSurround;
	  break;
	case OGLE_AO_CHTYPE_LFE:
	  ct = ChannelType_LFE;
	  break;
	case OGLE_AO_CHTYPE_MONO:
	  ct = ChannelType_Mono;
	  break;
	case OGLE_AO_CHTYPE_REARCENTER:
	  ct = ChannelType_CenterSurround;
	  break;
	case OGLE_AO_CHTYPE_REARMONO:
	  ct = ChannelType_Surround;
	  break;
	default:
	  DNOTE("unkown chtype: %d\n", aconf->ainfo->chlist[n]);
	  ct = -1;
	  break;
	}
	aconf->dst_format.ch_array[n] = ct;
      }
    }
    aconf->dst_format.sample_resolution = aconf->ainfo->sample_resolution;
  }

  if(config) {
    free_config(config);
    config = NULL;
  }

  aconf->dst_format.sample_frame_size = aconf->ainfo->sample_frame_size;


  aconf->sync.delay_resolution = aconf->ainfo->sample_rate / 10;
  aconf->sync.delay_resolution_set = 0;
  aconf->sync.max_sync_diff = 2 * aconf->sync.delay_resolution *
    (CT_FRACTION / aconf->ainfo->sample_rate);
  aconf->sync.prev_delay = -1;
  aconf->sync.samples_added = 0;

  return 0;
}


void audio_config_close(audio_config_t *aconf)
{
  
  if(aconf != NULL) {
    if(aconf->adev_handle != NULL) {
      ogle_ao_close(aconf->adev_handle);
      aconf->adev_handle = NULL;
    }
    
    if(aconf->dst_format.ch_array != NULL) {
      free(aconf->dst_format.ch_array);
      aconf->dst_format.ch_array = NULL;
    }
    
    free(aconf);
  }
  
}
