#ifndef SCREENSHOT_H_INCLUDED
#define SCREENSHOT_H_INCLUDED

/* Ogle - A video player
 * Copyright (C) 2000, 2001 Vilhelm Bergman
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

void screenshot_rgb_jpg(unsigned char *data,
			unsigned int width, unsigned int height,
			int sar_frac_n, int sar_frac_d);
void screenshot_yuv_jpg(yuv_image_t *yuv_data, XImage *ximg,
			int sar_frac_n, int sar_frac_d);

int screenshot_set_formatstr(char *str);

#endif /* SCREENSHOT_H_INCLUDED */
