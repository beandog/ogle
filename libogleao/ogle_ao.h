#ifndef OGLE_AO_H
#define OGLE_AO_H

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

#include <stdlib.h>
#include <inttypes.h>

typedef enum {
  OGLE_AO_ENCODING_NONE,
  OGLE_AO_ENCODING_LINEAR,
  OGLE_AO_ENCODING_IEC61937
} ogle_ao_encoding_t;

typedef enum {
  OGLE_AO_BYTEORDER_BE = 0,
  OGLE_AO_BYTEORDER_LE = 1,
#if WORDS_BIGENDIAN == 1
  OGLE_AO_BYTEORDER_NE = 0
#else
  OGLE_AO_BYTEORDER_NE = 1
#endif
} ogle_ao_byteorder_t;

typedef enum {
  OGLE_AO_CHTYPE_UNSPECIFIED = 0x0,
  OGLE_AO_CHTYPE_NULL        = 0x1,
  OGLE_AO_CHTYPE_MONO        = 0x2,
  OGLE_AO_CHTYPE_LEFT        = 0x4,
  OGLE_AO_CHTYPE_RIGHT       = 0x8,
  OGLE_AO_CHTYPE_CENTER      = 0x10,
  OGLE_AO_CHTYPE_REARLEFT    = 0x20,
  OGLE_AO_CHTYPE_REARCENTER  = 0x40,
  OGLE_AO_CHTYPE_REARRIGHT   = 0x80,
  OGLE_AO_CHTYPE_REARMONO    = 0x100,  //mono surround
  OGLE_AO_CHTYPE_LFE         = 0x200
} ogle_ao_chtype_t;

/* A sample is a number representing the sound at a specific time.
 * The sample_resolution is the number of bits that encode information
 * in the sample.
 * channels is the number of samples taken at the same time.
 * The sample_frame_size is the space in bytes that is used to store
 * all samples taken at the same time.
 * For normal CD audio (44.1kHz 16bit stereo) this would be:
 * sample_rate = 44100, sample_resolution = 16, channels = 2
 * encoding = OGLE_AO_ENCODING_LINEAR, sample_frame_size = 4
 * A fragment is the smallest size of data that is transferred to
 * the sound card.
 * fragments together with fragment_size defines the sample buffer.
 */
typedef struct {
  int32_t sample_rate;        // samples per second
  int32_t sample_resolution;  // bits per sample
  int32_t channels;           // number of channels (1 = mono, 2 = stereo,...)
  ogle_ao_chtype_t chtypes;   // the combined types of channels
  ogle_ao_chtype_t *chlist;    // gives the position and type of each channel
  ogle_ao_encoding_t encoding; // ( linear, ulaw, ac3, mpeg, ... )
  uint32_t sample_frame_size;  // the size in bytes of one sample frame
  ogle_ao_byteorder_t byteorder;  // the endianess of the samples
  int fragments;               // number of fragments
  int fragment_size;           // size of a fragment
} ogle_ao_audio_info_t;

typedef struct ogle_ao_instance_s ogle_ao_instance_t;

typedef ogle_ao_instance_t *(*ao_open_t)(char *dev);

typedef struct {
    char * name;
    ao_open_t open;
} ao_driver_t;

/* return NULL terminated array of all drivers */
ao_driver_t * ao_drivers (void);


/* Open a given driver, and give it the argument of dev */  
ogle_ao_instance_t *ogle_ao_open(ao_open_t open, char *dev);

/* Call init in the given driver instance */
int ogle_ao_init(ogle_ao_instance_t *instance, 
		 ogle_ao_audio_info_t *audio_info);

/* Queue up nbytes worth of sample data to be played */ 
int ogle_ao_play(ogle_ao_instance_t *instance, void* samples, size_t nbytes);

/* Close the given driver instance */
void ogle_ao_close(ogle_ao_instance_t *instance);

/* Query the number of sample intervals left in the output queue. */
int ogle_ao_odelay(ogle_ao_instance_t *instance,
		   unsigned int *sample_intervals);

/* Discard as much of the queued ouput data as possible (as fast as possible)*/
int ogle_ao_flush(ogle_ao_instance_t *instance);

/* Block until all of the queued output data has been played */
int ogle_ao_drain(ogle_ao_instance_t *instance);

#endif /* OGLE_AO_H */
