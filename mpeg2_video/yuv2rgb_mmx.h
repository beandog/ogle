#ifndef YUV2RGB_MMX_H_INCLUDED
#define YUV2RGB_MMX_H_INCLUDED

/* 
 *  yuv2rgb_mmx.h, YUV to RGB coverter using MMX.
 *
 *
 *  Copyright (C) 2000, Franck Sicard <Franck.sicard@aaumlv.eu.org>
 *  All Rights Reserved. 
 *
 *  This file is part of mpeg2_video, a free MPEG-2 video stream decoder.
 *  The 16 bit mmx convervion code taken from nist package (anonymous
 *  writer).
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
 *  the Free Software Foundation, 
 * 
 */

yuv2rgb_fun yuv2rgb_mmx_init(int bpp, int mode);

#endif /* YUV2RGB_MMX_H_INCLUDED */
