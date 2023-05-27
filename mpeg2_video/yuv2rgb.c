/* 
 * yuv2rgb.c, Software YUV to RGB coverter
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

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include "yuv2rgb.h"
#include "yuv2rgb_mmx.h"
#include "yuv2rgb_mlib.h"
#include "yuv2rgb_mmx.h"

static yuv2rgb_fun yuv2rgb_c_init(uint32_t bpp, uint32_t mode);

yuv2rgb_fun yuv2rgb;

void yuv2rgb_init(uint32_t bpp, uint32_t mode) 
{
#ifdef HAVE_MMX
  if(1)
    yuv2rgb = yuv2rgb_mmx_init(bpp, mode);
  else
#endif
#ifdef HAVE_MLIB
  if(1)
    yuv2rgb = yuv2rgb_mlib_init(bpp, mode);
  else
#endif
    ;
  
  if( yuv2rgb == NULL ) {
    fprintf( stderr, "No accelerated colorspace coversion found\n" );
    yuv2rgb = yuv2rgb_c_init(bpp, mode);
  }
}





// Clamp to [0,255]
static uint8_t clip_tbl[1024]; /* clipping table */
static uint8_t *clip;

static uint16_t* lookUpTable = NULL;


static void YUV2ARGB420_32(uint8_t* image, const uint8_t* py, 
			   const uint8_t* pu, const uint8_t* pv, 
			   const uint32_t h_size, const uint32_t v_size, 
			   const uint32_t rgb_stride, const uint32_t y_stride,
			   const uint32_t uv_stride) 
{
  int Y,U,V;
  int  g_common,b_common,r_common;
  unsigned int x,y;
  
  uint32_t *dst_line_1;
  uint32_t *dst_line_2;
  const uint8_t* py_line_1;
  const uint8_t* py_line_2;
  
  /* matrix coefficients */
  const int cy  = 76310;  /* 1.1644 << 16 */
  const int crv = 104635; /* 1.5966 << 16 */
  const int cbu = 132278; /* 2.0184 << 16 */
  const int cgu = 25690;  /* 0.3920 << 16 */
  const int cgv = 53294;  /* 0.8132 << 16 */
	
  dst_line_1 = (uint32_t *)(image);
  dst_line_2 = (uint32_t *)(image + rgb_stride);
  
  py_line_1 = py;
  py_line_2 = py + y_stride;
  
  for (y = 0; y < v_size / 2; y++) 
    {
      for (x = 0; x < h_size / 2; x++) 
	{
	  uint32_t pixel1,pixel2,pixel3,pixel4;

	  //Common to all four pixels
	  U = (*pu++) - 128;
	  V = (*pv++) - 128;

	  r_common = crv * V + 32768;
	  g_common = cgu * U + cgv * V - 32768;
	  b_common = cbu * U + 32768;

	  //Pixel I
	  Y = cy * ((*py_line_1++) - 16);
	  pixel1 = 
	    clip[(Y+r_common)>>16]<<16 |
	    clip[(Y-g_common)>>16]<<8 |
	    clip[(Y+b_common)>>16];
	  *dst_line_1++ = pixel1;
		  
	  //Pixel II
	  Y = cy * ((*py_line_1++) - 16);
	  pixel2 = 
	    clip[(Y+r_common)>>16]<<16 |
	    clip[(Y-g_common)>>16]<<8 |
	    clip[(Y+b_common)>>16];
	  *dst_line_1++ = pixel2;

	  //Pixel III
	  Y = cy * ((*py_line_2++) - 16);
	  pixel3 = 
	    clip[(Y+r_common)>>16]<<16 |
	    clip[(Y-g_common)>>16]<<8 |
	    clip[(Y+b_common)>>16];
	  *dst_line_2++ = pixel3;

	  //Pixel IV
	  Y = cy * ((*py_line_2++) - 16);
	  pixel4 = 
	    clip[(Y+r_common)>>16]<<16 |
	    clip[(Y-g_common)>>16]<<8|
	    clip[(Y+b_common)>>16];
	  *dst_line_2++ = pixel4;
	}

      py_line_1 += y_stride;
      py_line_2 += y_stride;
      pu += uv_stride - h_size/2;
      pv += uv_stride - h_size/2;
      dst_line_1 += rgb_stride/4;
      dst_line_2 += rgb_stride/4;
    }
}

static void YUV2ABGR420_32(uint8_t* image, const uint8_t* py, 
			   const uint8_t* pu, const uint8_t* pv, 
			   const uint32_t h_size, const uint32_t v_size, 
			   const uint32_t rgb_stride, const uint32_t y_stride, 
			   const uint32_t uv_stride)
{
  int Y,U,V;
  int g_common,b_common,r_common;
  unsigned int x,y;
  
  uint32_t *dst_line_1;
  uint32_t *dst_line_2;
  const uint8_t* py_line_1;
  const uint8_t* py_line_2;
  
  /* matrix coefficients */
  const int cy  = 76310;  /* 1.1644 << 16 */
  const int crv = 104635; /* 1.5966 << 16 */
  const int cbu = 132278; /* 2.0184 << 16 */
  const int cgu = 25690;  /* 0.3920 << 16 */
  const int cgv = 53294;  /* 0.8132 << 16 */
	
  dst_line_1 = (uint32_t *)(image);
  dst_line_2 = (uint32_t *)(image + rgb_stride);
  
  py_line_1 = py;
  py_line_2 = py + y_stride;
  
  for (y = 0; y < v_size / 2; y++) 
    {
      for (x = 0; x < h_size / 2; x++) 
	{
	  uint32_t pixel1,pixel2,pixel3,pixel4;

	  //Common to all four pixels
	  U = (*pu++) - 128;
	  V = (*pv++) - 128;

	  r_common = crv * V + 32768;
	  g_common = cgu * U + cgv * V - 32768;
	  b_common = cbu * U + 32768;

	  //Pixel I
	  Y = cy * ((*py_line_1++) - 16);
	  pixel1 = 
	    clip[(Y+b_common)>>16]<<16 |
	    clip[(Y-g_common)>>16]<<8 |
	    clip[(Y+r_common)>>16];
	  *dst_line_1++ = pixel1;
		  
	  //Pixel II
	  Y = cy * ((*py_line_1++) - 16);
	  pixel2 = 
	    clip[(Y+b_common)>>16]<<16 |
	    clip[(Y-g_common)>>16]<<8 |
	    clip[(Y+r_common)>>16];
	  *dst_line_1++ = pixel2;

	  //Pixel III
	  Y = cy * ((*py_line_2++) - 16);
	  pixel3 = 
	    clip[(Y+b_common)>>16]<<16 |
	    clip[(Y-g_common)>>16]<<8 |
	    clip[(Y+r_common)>>16];
	  *dst_line_2++ = pixel3;

	  //Pixel IV
	  Y = cy * ((*py_line_2++) - 16);
	  pixel4 = 
	    clip[(Y+b_common)>>16]<<16 |
	    clip[(Y-g_common)>>16]<<8|
	    clip[(Y+r_common)>>16];
	  *dst_line_2++ = pixel4;
	}

      py_line_1 += y_stride;
      py_line_2 += y_stride;
      pu += uv_stride - h_size/2;
      pv += uv_stride - h_size/2;
      dst_line_1 += rgb_stride/4;
      dst_line_2 += rgb_stride/4;
    }
}


static void YUV2RGBA420_32(uint8_t* image, const uint8_t* py, 
			   const uint8_t* pu, const uint8_t* pv, 
			   const uint32_t h_size, const uint32_t v_size, 
			   const uint32_t rgb_stride, const uint32_t y_stride,
			   const uint32_t uv_stride) 
{
  int Y,U,V;
  int  g_common,b_common,r_common;
  unsigned int x,y;
  
  uint32_t *dst_line_1;
  uint32_t *dst_line_2;
  const uint8_t* py_line_1;
  const uint8_t* py_line_2;
  
  /* matrix coefficients */
  const int cy  = 76310;  /* 1.1644 << 16 */
  const int crv = 104635; /* 1.5966 << 16 */
  const int cbu = 132278; /* 2.0184 << 16 */
  const int cgu = 25690;  /* 0.3920 << 16 */
  const int cgv = 53294;  /* 0.8132 << 16 */
	
  dst_line_1 = (uint32_t *)(image);
  dst_line_2 = (uint32_t *)(image + rgb_stride);
  
  py_line_1 = py;
  py_line_2 = py + y_stride;
  
  for (y = 0; y < v_size / 2; y++) 
    {
      for (x = 0; x < h_size / 2; x++) 
	{
	  uint32_t pixel1,pixel2,pixel3,pixel4;

	  //Common to all four pixels
	  U = (*pu++) - 128;
	  V = (*pv++) - 128;

	  r_common = crv * V + 32768;
	  g_common = cgu * U + cgv * V - 32768;
	  b_common = cbu * U + 32768;

	  //Pixel I
	  Y = cy * ((*py_line_1++) - 16);
	  pixel1 = 
	    clip[(Y+r_common)>>16]<<24 |
	    clip[(Y-g_common)>>16]<<16 |
	    clip[(Y+b_common)>>16]<<8;
	  *dst_line_1++ = pixel1;
		  
	  //Pixel II
	  Y = cy * ((*py_line_1++) - 16);
	  pixel2 = 
	    clip[(Y+r_common)>>16]<<24 |
	    clip[(Y-g_common)>>16]<<16 |
	    clip[(Y+b_common)>>16]<<8;
	  *dst_line_1++ = pixel2;

	  //Pixel III
	  Y = cy * ((*py_line_2++) - 16);
	  pixel3 = 
	    clip[(Y+r_common)>>16]<<24 |
	    clip[(Y-g_common)>>16]<<16 |
	    clip[(Y+b_common)>>16]<<8;
	  *dst_line_2++ = pixel3;

	  //Pixel IV
	  Y = cy * ((*py_line_2++) - 16);
	  pixel4 = 
	    clip[(Y+r_common)>>16]<<24 |
	    clip[(Y-g_common)>>16]<<16|
	    clip[(Y+b_common)>>16]<<8;
	  *dst_line_2++ = pixel4;
	}

      py_line_1 += y_stride;
      py_line_2 += y_stride;
      pu += uv_stride - h_size/2;
      pv += uv_stride - h_size/2;
      dst_line_1 += rgb_stride/4;
      dst_line_2 += rgb_stride/4;
    }
}

static void YUV2BGRA420_32(uint8_t* image, const uint8_t* py, 
			   const uint8_t* pu, const uint8_t* pv, 
			   const uint32_t h_size, const uint32_t v_size, 
			   const uint32_t rgb_stride, const uint32_t y_stride, 
			   const uint32_t uv_stride)
{
  int Y,U,V;
  int g_common,b_common,r_common;
  unsigned int x,y;
  
  uint32_t *dst_line_1;
  uint32_t *dst_line_2;
  const uint8_t* py_line_1;
  const uint8_t* py_line_2;
  
  /* matrix coefficients */
  const int cy  = 76310;  /* 1.1644 << 16 */
  const int crv = 104635; /* 1.5966 << 16 */
  const int cbu = 132278; /* 2.0184 << 16 */
  const int cgu = 25690;  /* 0.3920 << 16 */
  const int cgv = 53294;  /* 0.8132 << 16 */
	
  dst_line_1 = (uint32_t *)(image);
  dst_line_2 = (uint32_t *)(image + rgb_stride);
  
  py_line_1 = py;
  py_line_2 = py + y_stride;
  
  for (y = 0; y < v_size / 2; y++) 
    {
      for (x = 0; x < h_size / 2; x++) 
	{
	  uint32_t pixel1,pixel2,pixel3,pixel4;

	  //Common to all four pixels
	  U = (*pu++) - 128;
	  V = (*pv++) - 128;

	  r_common = crv * V + 32768;
	  g_common = cgu * U + cgv * V - 32768;
	  b_common = cbu * U + 32768;

	  //Pixel I
	  Y = cy * ((*py_line_1++) - 16);
	  pixel1 = 
	    clip[(Y+b_common)>>16]<<24 |
	    clip[(Y-g_common)>>16]<<16 |
	    clip[(Y+r_common)>>16]<<8;
	  *dst_line_1++ = pixel1;
		  
	  //Pixel II
	  Y = cy * ((*py_line_1++) - 16);
	  pixel2 = 
	    clip[(Y+b_common)>>16]<<24 |
	    clip[(Y-g_common)>>16]<<16 |
	    clip[(Y+r_common)>>16]<<8;
	  *dst_line_1++ = pixel2;

	  //Pixel III
	  Y = cy * ((*py_line_2++) - 16);
	  pixel3 = 
	    clip[(Y+b_common)>>16]<<24 |
	    clip[(Y-g_common)>>16]<<16 |
	    clip[(Y+r_common)>>16]<<8;
	  *dst_line_2++ = pixel3;

	  //Pixel IV
	  Y = cy * ((*py_line_2++) - 16);
	  pixel4 = 
	    clip[(Y+b_common)>>16]<<24 |
	    clip[(Y-g_common)>>16]<<16|
	    clip[(Y+r_common)>>16]<<8;
	  *dst_line_2++ = pixel4;
	}

      py_line_1 += y_stride;
      py_line_2 += y_stride;
      pu += uv_stride - h_size/2;
      pv += uv_stride - h_size/2;
      dst_line_1 += rgb_stride/4;
      dst_line_2 += rgb_stride/4;
    }
}


static void YUV2RGB420_24(uint8_t* image, const uint8_t* py, 
			  const uint8_t* pu, const uint8_t* pv, 
			  const uint32_t h_size, const uint32_t v_size, 
			  const uint32_t rgb_stride, const uint32_t y_stride, 
			  const uint32_t uv_stride)
{
  int Y,U,V;
  int g_common,b_common,r_common;
  unsigned int x,y;
  
  uint8_t *dst_line_1;
  uint8_t *dst_line_2;
  const uint8_t* py_line_1;
  const uint8_t* py_line_2;
  
  /* matrix coefficients */
  const int cy  = 76310;  /* 1.1644 << 16 */
  const int crv = 104635; /* 1.5966 << 16 */
  const int cbu = 132278; /* 2.0184 << 16 */
  const int cgu = 25690;  /* 0.3920 << 16 */
  const int cgv = 53294;  /* 0.8132 << 16 */
	
  dst_line_1 = image;
  dst_line_2 = image + rgb_stride;
  
  py_line_1 = py;
  py_line_2 = py + y_stride;
  
  for (y = 0; y < v_size / 2; y++) 
    {
      for (x = 0; x < h_size / 2; x++) 
	{
	  //Common to all four pixels
	  U = (*pu++) - 128;
	  V = (*pv++) - 128;

	  r_common = crv * V + 32768;
	  g_common = cgu * U + cgv * V - 32768;
	  b_common = cbu * U + 32768;

	  //Pixel I
	  Y = cy * ((*py_line_1++) - 16);
	  *dst_line_1++ = clip[(Y+r_common)>>16];
	  *dst_line_1++ = clip[(Y-g_common)>>16];
	  *dst_line_1++ = clip[(Y+b_common)>>16];
		  
	  //Pixel II
	  Y = cy * ((*py_line_1++) - 16);
	  *dst_line_1++ = clip[(Y+r_common)>>16];
	  *dst_line_1++ = clip[(Y-g_common)>>16];
	  *dst_line_1++ = clip[(Y+b_common)>>16];

	  //Pixel III
	  Y = cy * ((*py_line_2++) - 16);
	  *dst_line_2++ = clip[(Y+r_common)>>16];
	  *dst_line_2++ = clip[(Y-g_common)>>16];
	  *dst_line_2++ = clip[(Y+b_common)>>16];

	  //Pixel IV
	  Y = cy * ((*py_line_2++) - 16);
	  *dst_line_2++ = clip[(Y+r_common)>>16];
	  *dst_line_2++ = clip[(Y-g_common)>>16];
	  *dst_line_2++ = clip[(Y+b_common)>>16];
	}

      py_line_1 += y_stride;
      py_line_2 += y_stride;
      pu += uv_stride - h_size/2;
      pv += uv_stride - h_size/2;
      dst_line_1 += rgb_stride;
      dst_line_2 += rgb_stride;
    }
}

static void YUV2BGR420_24(uint8_t* image, const uint8_t* py, 
			  const uint8_t* pu, const uint8_t* pv, 
			  const uint32_t h_size, const uint32_t v_size, 
			  const uint32_t rgb_stride, const uint32_t y_stride, 
			  const uint32_t uv_stride)
{
  int Y,U,V;
  int g_common,b_common,r_common;
  int x,y;
  
  uint8_t *dst_line_1;
  uint8_t *dst_line_2;
  const uint8_t* py_line_1;
  const uint8_t* py_line_2;
  
  
  /* matrix coefficients */
  const int cy  = 76310;  /* 1.1644 << 16 */
  const int crv = 104635; /* 1.5966 << 16 */
  const int cbu = 132278; /* 2.0184 << 16 */
  const int cgu = 25690;  /* 0.3920 << 16 */
  const int cgv = 53294;  /* 0.8132 << 16 */
	
  dst_line_1 = image;
  dst_line_2 = image + rgb_stride;
  
  py_line_1 = py;
  py_line_2 = py + y_stride;
  
  for (y = 0; y < v_size / 2; y++) 
    {
      for (x = 0; x < h_size / 2; x++) 
	{
	  //Common to all four pixels
	  U = (*pu++) - 128;
	  V = (*pv++) - 128;

	  r_common = crv * V + 32768;
	  g_common = cgu * U + cgv * V - 32768;
	  b_common = cbu * U + 32768;

	  //Pixel I
	  Y = cy * ((*py_line_1++) - 16);
	  *dst_line_1++ = clip[(Y+b_common)>>16];
	  *dst_line_1++ = clip[(Y-g_common)>>16];
	  *dst_line_1++ = clip[(Y+r_common)>>16];
		  
	  //Pixel II
	  Y = cy * ((*py_line_1++) - 16);
	  *dst_line_1++ = clip[(Y+b_common)>>16];
	  *dst_line_1++ = clip[(Y-g_common)>>16];
	  *dst_line_1++ = clip[(Y+r_common)>>16];

	  //Pixel III
	  Y = cy * ((*py_line_2++) - 16);
	  *dst_line_2++ = clip[(Y+b_common)>>16];
	  *dst_line_2++ = clip[(Y-g_common)>>16];
	  *dst_line_2++ = clip[(Y+r_common)>>16];

	  //Pixel IV
	  Y = cy * ((*py_line_2++) - 16);
	  *dst_line_2++ = clip[(Y+b_common)>>16];
	  *dst_line_2++ = clip[(Y-g_common)>>16];
	  *dst_line_2++ = clip[(Y+r_common)>>16];
	}

      py_line_1 += y_stride;
      py_line_2 += y_stride;
      pu += uv_stride - h_size/2;
      pv += uv_stride - h_size/2;
      dst_line_1 += rgb_stride;
      dst_line_2 += rgb_stride;
    }
}

/* do 16 and 15 bpp output */
static void YUV2RGB420_16(uint8_t* image, const uint8_t* py, 
			  const uint8_t* pu, const uint8_t* pv, 
			  const uint32_t h_size, const uint32_t v_size, 
			  const uint32_t rgb_stride, const uint32_t y_stride, 
			  const uint32_t uv_stride) 
{
  unsigned int U,V;
  unsigned int pixel_idx;
  unsigned int x,y;
  
  uint16_t* dst_line_1;
  uint16_t* dst_line_2;
  const uint8_t* py_line_1;
  const uint8_t* py_line_2;
  
  dst_line_1 = (uint16_t*)(image);
  dst_line_2 = (uint16_t*)(image + rgb_stride);
  
  py_line_1 = py;
  py_line_2 = py + y_stride;
  
  for (y = 0; y < v_size / 2; y++) 
    {
      for (x = 0; x < h_size / 2; x++) 
	{
	  //Common to all four pixels
	  U = (*pu++)>>2;
	  V = (*pv++)>>2;
	  pixel_idx = U<<6 | V<<12;
	  
	  //Pixel I
	  *dst_line_1++ = lookUpTable[(*py_line_1++)>>2 | pixel_idx];
	  
	  //Pixel II
	  *dst_line_1++ = lookUpTable[(*py_line_1++)>>2 | pixel_idx];
	  
	  //Pixel III
	  *dst_line_2++ = lookUpTable[(*py_line_2++)>>2 | pixel_idx];
	  
	  //Pixel IV
	  *dst_line_2++ = lookUpTable[(*py_line_2++)>>2 | pixel_idx];
	}
      py_line_1 += y_stride;
      py_line_2 += y_stride;
      pu += uv_stride - h_size/2;
      pv += uv_stride - h_size/2;
      dst_line_1 += rgb_stride/2;
      dst_line_2 += rgb_stride/2;
    }
}

/* CreateCLUT have already taken care of 15/16bit and RGB BGR issues. */
#define YUV2BGR420_16 YUV2RGB420_16


/* We don't have any 8bit support yet. */
static void YUV2RGB420_8(uint8_t* image, const uint8_t* py, const uint8_t* pu,
		  const uint8_t* pv, const uint32_t h_size,
		  const uint32_t v_size, const uint32_t rgb_stride,
		  const uint32_t y_stride, const uint32_t uv_stride)
{
  fprintf( stderr, "No support for 8bit displays.\n" );
  exit(1);
}


/* Not sure if this is a win using the LUT. Can someone try
   the direct calculation (like in the 32bpp) and compare? */
static void createCLUT(uint32_t bpp, uint32_t mode) 
{
  int i, j, k;
  uint8_t r, g, b;
  
  /* matrix coefficients */
  const int cy  = 76310;  /* 1.1644 << 16 */
  const int crv = 104635; /* 1.5966 << 16 */
  const int cbu = 132278; /* 2.0184 << 16 */
  const int cgu = 25690;  /* 0.3920 << 16 */
  const int cgv = 53294;  /* 0.8132 << 16 */
  
  if (lookUpTable == NULL) {
    lookUpTable = malloc((1<<18)*sizeof(uint16_t));
    
    for (i = 0; i<(1<<6); ++i) { /* Y */
      int Y = i<<2;
      
      for(j = 0; j < (1<<6); ++j) { /* U */
	int U = j<<2;
	
	for(k = 0; k < (1<<6); k++) { /* V */
	  int V = k<<2;
	  
	  r = clip[((Y-16)*cy + crv*(V-128))/65536] >> 3;
	  g = clip[((Y-16)*cy - cgu*(U-128) - cgv*(V-128))/65536] 
	    >> (bpp==16?2:3);
	  b = clip[((Y-16)*cy + cbu*(U-128))/65536] >> 3;
	  if( mode == MODE_RGB )
	    lookUpTable[i|(j<<6)|(k<<12)] = (r<<(bpp==16?11:10)) | (g<<5) | b;
	  else
	    lookUpTable[i|(j<<6)|(k<<12)] = (b<<(bpp==16?11:10)) | (g<<5) | r;
	}
      }
    }
  }  
}

static yuv2rgb_fun yuv2rgb_c_init(uint32_t bpp, uint32_t mode) 
{  
  int i;
  
  clip = clip_tbl + 384;
  for (i= -384; i< 640; i++)
    clip[i] = (i < 0) ? 0 : ((i > 255) ? 255 : i);
  
  if( bpp == 8 ) {
    return YUV2RGB420_8;
  }
  
  if( bpp == 15 || bpp == 16 ) {
    createCLUT( bpp, mode );
    if( mode == MODE_RGB || mode == MODE_BGR_ALIEN )
      return YUV2RGB420_16;
    else if( mode == MODE_BGR || mode == MODE_RGB_ALIEN)
      return YUV2BGR420_16;
  }
  
  if( bpp == 24 ) {
    if( mode == MODE_RGB || mode == MODE_BGR_ALIEN)
      return YUV2RGB420_24;
    else if( mode == MODE_BGR || mode == MODE_RGB_ALIEN)
      return YUV2BGR420_24;
  }
  
  if( bpp == 32 ) {
    if( mode == MODE_RGB )
      return YUV2ARGB420_32;
    else if( mode == MODE_BGR )
      return YUV2ABGR420_32;
    else if( mode == MODE_RGB_ALIEN )
      return YUV2BGRA420_32;
    else if( mode == MODE_BGR_ALIEN )
      return YUV2RGBA420_32;
  }
  
  fprintf( stderr, "%ibpp not supported by yuv2rgb\n", bpp );
  exit(1);
}

