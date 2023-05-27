#ifndef DECODE_PRIVATE_H
#define DECODE_PRIVATE_H

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

#include "decode.h"

typedef int (*audio_decode_t)(adec_handle_t *handle, uint8_t *start, int len,
			      int pts_offset, uint64_t PTS, int scr_nr);

typedef int (*audio_flush_t)(adec_handle_t *handle);

typedef int (*audio_drain_t)(adec_handle_t *handle);

typedef void (*audio_free_t)(adec_handle_t *handle);

struct adec_handle_s {
  AudioType_t type;
  audio_decode_t decode;
  audio_flush_t  flush;
  audio_drain_t  drain;
  audio_free_t   free;
  audio_config_t *config;
  uint8_t *output_buf;
  int output_buf_size;
  uint8_t *output_buf_ptr; //output_buf_ptr - output_buf = bytes to play
};

#endif /* DECODE_PRIVATE_H */

