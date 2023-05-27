#ifndef OGLE_AO_PRIVATE_H
#define OGLE_AO_PRIVATE_H

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

struct ogle_ao_instance_s {
  int (* init)(ogle_ao_instance_t *instance, ogle_ao_audio_info_t *audio_info);
  int (* play)(ogle_ao_instance_t *instance, void *samples, size_t nbytes);
  int (* odelay)(ogle_ao_instance_t *instance, uint32_t *samples_return);
  int (* flush)(ogle_ao_instance_t *instance);
  int (* drain)(ogle_ao_instance_t *instance);
  void (* close)(ogle_ao_instance_t *instance);
};

#endif /* OGLE_AO_PRIVATE_H */
