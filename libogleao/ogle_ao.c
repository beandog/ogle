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

#include "ogle_ao.h"
#include "ogle_ao_private.h"

#include "null_audio.h"
#include "alsa_audio.h"
#include "obsd_audio.h"
#include "oss_audio.h"
#include "solaris_audio.h"

static ao_driver_t audio_out_drivers[] = {
#ifdef LIBOGLEAO_OBSD
    {"obsd", ao_obsd_open},
#endif
#ifdef LIBOGLEAO_ALSA
    {"alsa", ao_alsa_open},
#endif
#ifdef LIBOGLEAO_OSS
    {"oss", ao_oss_open},
#endif
#ifdef LIBOGLEAO_SOLARIS
    {"solaris", ao_solaris_open},
#endif
#ifdef LIBOGLEAO_NULL
    {"null", ao_null_open},
#endif
    {NULL, NULL}
};


ao_driver_t *ao_drivers (void)
{
  return audio_out_drivers;
}

ogle_ao_instance_t *ogle_ao_open(ao_open_t open, char *dev)
{
  return open(dev);
}

int ogle_ao_init(ogle_ao_instance_t *instance,
		 ogle_ao_audio_info_t *audio_info)
{
  return instance->init(instance, audio_info);
}

int ogle_ao_play(ogle_ao_instance_t *instance, void *samples,
		 size_t nbytes)
{
  return instance->play(instance, samples, nbytes);
}

void ogle_ao_close(ogle_ao_instance_t *instance)
{
  if(instance->close)
    instance->close(instance);
}

int ogle_ao_odelay(ogle_ao_instance_t *instance,
		   unsigned int *sample_intervals)
{
  return instance->odelay(instance, sample_intervals);
}

int ogle_ao_flush(ogle_ao_instance_t *instance)
{
  return instance->flush(instance);
}

int ogle_ao_drain(ogle_ao_instance_t *instance)
{
  return instance->drain(instance);
}

