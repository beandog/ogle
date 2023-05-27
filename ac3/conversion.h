#ifndef CONVERSION_H
#define CONVERSION_H

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

#include "audio_config.h"
#include "decode.h" // FIXME

void setup_channel_conf(int *ch_conf, int nr_ch, int *input_ch, 
			int *output_ch);
int init_sample_conversion(adec_handle_t *h, audio_format_t *format,
			   int nr_samples);
int convert_samples(adec_handle_t *handle, void *samples, int nr_samples);

void convert_samples_start(adec_handle_t *h);

#endif /* CONVERSION_H */
