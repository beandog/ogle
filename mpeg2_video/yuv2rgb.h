#ifndef YUV2RGB_H_INCLUDED
#define YUV2RGB_H_INCLUDED

/* 
 * yuv2rgb.h, Software YUV to RGB coverter
 *
 *  Copyright (C) 1999, Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
 *  All Rights Reserved. 
 * 
 *  Functions broken out from display_x11.c and several new modes
 *  added by Håkan Hjort <d95hjort@dtek.chalmers.se>
 * 
 *  15 & 16 bpp support by Franck Sicard <Franck.Sicard@solsoft.fr>
 *
 *  This file is part of mpeg2_video, a free MPEG-2 video decoder
 *	
 *  mpeg2_video is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *   
 *  mpeg2_video is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *   
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. 
 *
 */

#define MODE_RGB  0x1
#define MODE_BGR  0x2
#define MODE_RGB_ALIEN 0x5
#define MODE_BGR_ALIEN 0x6
typedef void (*yuv2rgb_fun)(uint8_t* image,
			    const uint8_t* py,
			    const uint8_t* pu,
			    const uint8_t* pv,
			    const uint32_t h_size,
			    const uint32_t v_size,
			    const uint32_t rgb_stride,
			    const uint32_t y_stride,
			    const uint32_t uv_stride);

extern yuv2rgb_fun yuv2rgb;

void yuv2rgb_init(uint32_t bpp, uint32_t mode);

#endif /* VIDEO_TYPES_H_INCLUDED */
