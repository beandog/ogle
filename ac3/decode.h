#ifndef DECODE_H
#define DECODE_H

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

#include <stdio.h>
#include <inttypes.h>

#include "audio_config.h"

typedef enum {
  AudioType_None,
  AudioType_LPCM,
  AudioType_MPEG,
  AudioType_AC3,
  AudioType_DTS,
  AudioType_SDDS
} AudioType_t;


typedef struct adec_handle_s adec_handle_t;

adec_handle_t *adec_init(AudioType_t audio_type);

AudioType_t adec_type(adec_handle_t *handle);

int adec_decode(adec_handle_t *handle, uint8_t *start, int len,
		int pts_offset, uint64_t PTS, int scr_nr);

int adec_flush(adec_handle_t *handle);

int adec_drain(adec_handle_t *handle);

void adec_free(adec_handle_t *handle);

#endif /* DECODE_H */
