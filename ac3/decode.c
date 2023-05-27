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
#include <stdio.h>
#include <inttypes.h>

#include "decode.h"
#include "decode_private.h"

#include "debug_print.h"

#include "decode_a52.h"
#include "decode_lpcm.h"
#include "decode_mpeg.h"
#include "decode_dts.h"

//extern adec_handle_t *init_a52(void);


static
int decode_none(adec_handle_t *handle, uint8_t *start, int len,
		int pts_offset, uint64_t PTS, int scr_nr)
{
  return 0;
}

static
int flush_none(adec_handle_t *handle)
{
  return 0;
}

static
int drain_none(adec_handle_t *handle)
{
  return 0;
}

static
void free_none(adec_handle_t *handle)
{
  return;
}




int adec_decode(adec_handle_t *handle,
		uint8_t *start, int len,
		int pts_offset, uint64_t PTS, int scr_nr)
{
  return (handle->decode)(handle, start, len, pts_offset, PTS, scr_nr);
}

int adec_flush(adec_handle_t *handle)
{
  return (handle->flush)(handle);
}

int adec_drain(adec_handle_t *handle)
{
  return (handle->drain)(handle);
}

void adec_free(adec_handle_t *handle)
{
  (handle->free)(handle);
}

AudioType_t adec_type(adec_handle_t *handle)
{
  return handle->type;
}

adec_handle_t *adec_init(AudioType_t audio_type)
{
  static adec_handle_t none_handle;  
  adec_handle_t *handle = NULL;
  
  switch(audio_type) {
  case AudioType_MPEG:
    handle = init_mpeg();
    break;
  case AudioType_AC3:
    handle = init_a52();
    break;
  case AudioType_DTS:
    handle = init_dts();
    break;
  case AudioType_LPCM:
    handle = init_lpcm();
    break;
  case AudioType_None:
    handle = &none_handle;
    handle->decode = decode_none;
    handle->flush  = flush_none;
    handle->drain  = drain_none;
    handle->free   = free_none;
    break;
  default:
    ERROR("adec_init() illegal audio type: %d\n", audio_type);
    return NULL;
  }
  
  if(handle) {

    handle->type = audio_type;
    
    handle->config = audio_config_init();
  }

  return handle;
}
