#ifndef SPU_MIXER_H_INCLUDED
#define SPU_MIXER_H_INCLUDED

/* Ogle - A video player
 * Copyright (C) 2000, 2001 Björn Englund, Håkan Hjort
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


//ugly hack
void mix_subpicture_init(int pixel_stride,int mode);
void mix_subpicture_rgb(char *data, int width, int height);
int mix_subpicture_yuv(yuv_image_t *img, yuv_image_t *reserv);

int init_spu(void);

void flush_subpicture(int scr_nr);

#endif /* SPU_MIXER_H_INCLUDED */

