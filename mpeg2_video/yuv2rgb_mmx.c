/* 
 *  yuv2rgb_mmx.c, YUV to RGB coverter using MMX.
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
 */

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>


#include "yuv2rgb.h"

#ifdef HAVE_MMX
//static unsigned long  MMX_10w[]         = {0x00100010, 0x00100010};
static unsigned long  MMX_80w[]         = {0x00800080, 0x00800080};

static unsigned long  MMX_00FFw[]       = {0x00ff00ff, 0x00ff00ff};

static unsigned short MMX_Ublucoeff[]   = {0x0081, 0x0081, 0x0081, 0x0081};
static unsigned short MMX_Vredcoeff[]   = {0x0066, 0x0066, 0x0066, 0x0066};

static unsigned short MMX_Ugrncoeff[]   = {0xffe8, 0xffe8, 0xffe8, 0xffe8};
static unsigned short MMX_Vgrncoeff[]   = {0xffcd, 0xffcd, 0xffcd, 0xffcd};

static unsigned short MMX_Ycoeff[]      = {0x004a, 0x004a, 0x004a, 0x004a};
static unsigned short MMX_redmask[]     = {0xf800, 0xf800, 0xf800, 0xf800};

static unsigned short MMX_grnmask[]     = {0x07e0, 0x07e0, 0x07e0, 0x07e0};
// static unsigned short MMX_blumask[]  = {0x001f, 0x001f, 0x001f, 0x001f};

// get rif of 'defined but unused' warnings
// gcc doesn't recognise that they _are_ used inside __asm__
// make dummy use of them
void __yuv2rgb_mmx_preventwarnings_dontusethis(void)
{
  printf("%p %p %p %p %p %p %p %p %p\n",MMX_80w,
	 MMX_00FFw,
	 MMX_Ublucoeff,
	 MMX_Vredcoeff,
	 MMX_Ugrncoeff,
	 MMX_Vgrncoeff,
	 MMX_Ycoeff,
	 MMX_redmask,
	 MMX_grnmask);
};

#if defined(__OpenBSD__) && !defined(__ELF__)
#define _	"_"
#else
#define _
#endif
#endif


void  YUV2RGB420_MMX_16(uint8_t *out,
                        const uint8_t* lum, const uint8_t* cb,const uint8_t*cr,
                        const unsigned int cols, const unsigned int rows,
                        const unsigned int screen_width,
                        const unsigned int colsY,const unsigned int colsUV) {
  unsigned short *row1;
  int x;
  unsigned char *y;
  int col1;
  int mod = screen_width/2-cols;
  
  row1 = (unsigned short *)out;
  col1 = cols + mod;
  mod += cols + mod;
  mod *= 2;
  y = (uint8_t *)lum + cols*rows;
  x = 0;
#ifdef HAVE_MMX
  __asm__ __volatile__(
         ".align 8\n"
         "1:\n"
         "movd           (%1),                   %%mm0\n"        // 4 Cb         0  0  0  0 u3 u2 u1 u0
         "pxor           %%mm7,                  %%mm7\n"
         "movd           (%0),                   %%mm1\n" // 4 Cr                0  0  0  0 v3 v2 v1 v0
         "punpcklbw      %%mm7,                  %%mm0\n" // 4 W cb   0 u3  0 u2  0 u1  0 u0
         "punpcklbw      %%mm7,                  %%mm1\n" // 4 W cr   0 v3  0 v2  0 v1  0 v0
         "psubw          "_"MMX_80w,                %%mm0\n"
         "psubw          "_"MMX_80w,                %%mm1\n"
         "movq           %%mm0,                  %%mm2\n"        // Cb                   0 u3  0 u2  0 u1  0 u0
         "movq           %%mm1,                  %%mm3\n" // Cr
         "pmullw         "_"MMX_Ugrncoeff,          %%mm2\n" // Cb2green 0 R3  0 R2  0 R1  0 R0
         "movq           (%2),                   %%mm6\n"        // L1      l7 L6 L5 L4 L3 L2 L1 L0
         "pmullw         "_"MMX_Ublucoeff,          %%mm0\n" // Cb2blue
         "pand           "_"MMX_00FFw,              %%mm6\n" // L1      00 L6 00 L4 00 L2 00 L0
         "pmullw         "_"MMX_Vgrncoeff,          %%mm3\n" // Cr2green
         "movq           (%2),                   %%mm7\n" // L2
         "pmullw         "_"MMX_Vredcoeff,          %%mm1\n" // Cr2red
//                      "psubw          MMX_10w,                %%mm6\n"
         "psrlw          $8,                     %%mm7\n"        // L2           00 L7 00 L5 00 L3 00 L1
         "pmullw         "_"MMX_Ycoeff,             %%mm6\n" // lum1
//                      "psubw          MMX_10w,                %%mm7\n" // L2
         "paddw          %%mm3,                  %%mm2\n" // Cb2green + Cr2green == green
         "pmullw         "_"MMX_Ycoeff,             %%mm7\n"  // lum2

         "movq           %%mm6,                  %%mm4\n"  // lum1
         "paddw          %%mm0,                  %%mm6\n"  // lum1 +blue 00 B6 00 B4 00 B2 00 B0
         "movq           %%mm4,                  %%mm5\n"  // lum1
         "paddw          %%mm1,                  %%mm4\n"  // lum1 +red  00 R6 00 R4 00 R2 00 R0
         "paddw          %%mm2,                  %%mm5\n"  // lum1 +green 00 G6 00 G4 00 G2 00 G0
         "psraw          $6,                     %%mm4\n"  // R1 0 .. 64
         "movq           %%mm7,                  %%mm3\n"  // lum2                       00 L7 00 L5 00 L3 00 L1
         "psraw          $6,                     %%mm5\n"  // G1  - .. +
         "paddw          %%mm0,                  %%mm7\n"  // Lum2 +blue 00 B7 00 B5 00 B3 00 B1
         "psraw          $6,                     %%mm6\n"  // B1         0 .. 64
         "packuswb       %%mm4,                  %%mm4\n"  // R1 R1
         "packuswb       %%mm5,                  %%mm5\n"  // G1 G1
         "packuswb       %%mm6,                  %%mm6\n"  // B1 B1
         "punpcklbw      %%mm4,                  %%mm4\n"
         "punpcklbw      %%mm5,                  %%mm5\n"

         "pand           "_"MMX_redmask,            %%mm4\n"
         "psllw          $3,                     %%mm5\n"  // GREEN       1
         "punpcklbw      %%mm6,                  %%mm6\n"
         "pand           "_"MMX_grnmask,            %%mm5\n"
         "pand           "_"MMX_redmask,            %%mm6\n"
         "por            %%mm5,                  %%mm4\n" //
         "psrlw          $11,                    %%mm6\n"                // BLUE        1
         "movq           %%mm3,                  %%mm5\n" // lum2
         "paddw          %%mm1,                  %%mm3\n"        // lum2 +red      00 R7 00 R5 00 R3 00 R1
         "paddw          %%mm2,                  %%mm5\n" // lum2 +green 00 G7 00 G5 00 G3 00 G1
         "psraw          $6,                     %%mm3\n" // R2
         "por            %%mm6,                  %%mm4\n" // MM4
         "psraw          $6,                     %%mm5\n" // G2
         "movq           (%2,%3),                %%mm6\n"  // L3
         "psraw          $6,                     %%mm7\n"
         "packuswb       %%mm3,                  %%mm3\n"
         "packuswb       %%mm5,                  %%mm5\n"
         "packuswb       %%mm7,                  %%mm7\n"
         "pand           "_"MMX_00FFw,              %%mm6\n"  // L3
         "punpcklbw      %%mm3,                  %%mm3\n"
         //                              "psubw          MMX_10w,                        %%mm6\n"  // L3
         "punpcklbw      %%mm5,                  %%mm5\n"
         "pmullw         "_"MMX_Ycoeff,             %%mm6\n"  // lum3
         "punpcklbw      %%mm7,                  %%mm7\n"
         "psllw          $3,                     %%mm5\n"  // GREEN 2
         "pand           "_"MMX_redmask,            %%mm7\n"
         "pand           "_"MMX_redmask,            %%mm3\n"
         "psrlw          $11,                    %%mm7\n"  // BLUE  2
         "pand           "_"MMX_grnmask,            %%mm5\n"
         "por            %%mm7,                  %%mm3\n"
         "movq           (%2,%3),                %%mm7\n"  // L4
         "por            %%mm5,                  %%mm3\n"     //
         "psrlw          $8,                     %%mm7\n"    // L4
         "movq           %%mm4,                  %%mm5\n"
         //                              "psubw          MMX_10w,                        %%mm7\n"                // L4
         "punpcklwd      %%mm3,                  %%mm4\n"
         "pmullw         "_"MMX_Ycoeff,             %%mm7\n"    // lum4
         "punpckhwd      %%mm3,                  %%mm5\n"

         "movq           %%mm4,                  (%4)\n"
         "movq           %%mm5,                  8(%4)\n"

         "movq           %%mm6,                  %%mm4\n"        // Lum3
         "paddw          %%mm0,                  %%mm6\n"                // Lum3 +blue

         "movq           %%mm4,                  %%mm5\n"                        // Lum3
         "paddw          %%mm1,                  %%mm4\n"       // Lum3 +red
         "paddw          %%mm2,                  %%mm5\n"                        // Lum3 +green
         "psraw          $6,                     %%mm4\n"
         "movq           %%mm7,                  %%mm3\n"                        // Lum4
         "psraw          $6,                     %%mm5\n"
         "paddw          %%mm0,                  %%mm7\n"                   // Lum4 +blue
         "psraw          $6,                     %%mm6\n"                        // Lum3 +blue
         "movq           %%mm3,                  %%mm0\n"  // Lum4
         "packuswb       %%mm4,                  %%mm4\n"
         "paddw          %%mm1,                  %%mm3\n"  // Lum4 +red
         "packuswb       %%mm5,                  %%mm5\n"
         "paddw          %%mm2,                  %%mm0\n"         // Lum4 +green
         "packuswb       %%mm6,                  %%mm6\n"
         "punpcklbw      %%mm4,                  %%mm4\n"
         "punpcklbw      %%mm5,                  %%mm5\n"
         "punpcklbw      %%mm6,                  %%mm6\n"
         "psllw          $3,                     %%mm5\n" // GREEN 3
         "pand           "_"MMX_redmask,            %%mm4\n"
         "psraw          $6,                     %%mm3\n" // psr 6
         "psraw          $6,                     %%mm0\n"
         "pand           "_"MMX_redmask,            %%mm6\n" // BLUE
         "pand           "_"MMX_grnmask,            %%mm5\n"
         "psrlw          $11,                    %%mm6\n"  // BLUE  3
         "por            %%mm5,                  %%mm4\n"
         "psraw          $6,                     %%mm7\n"
         "por            %%mm6,                  %%mm4\n"
         "packuswb       %%mm3,                  %%mm3\n"
         "packuswb       %%mm0,                  %%mm0\n"
         "packuswb       %%mm7,                  %%mm7\n"
         "punpcklbw      %%mm3,                  %%mm3\n"
         "punpcklbw      %%mm0,                  %%mm0\n"
         "punpcklbw      %%mm7,                  %%mm7\n"
         "pand           "_"MMX_redmask,            %%mm3\n"
         "pand           "_"MMX_redmask,            %%mm7\n" // BLUE
         "psllw          $3,                     %%mm0\n" // GREEN 4
         "psrlw          $11,                    %%mm7\n"
         "pand           "_"MMX_grnmask,            %%mm0\n"
         "por            %%mm7,                  %%mm3\n"
         "addl           $8,                     %6\n"
         "por            %%mm0,                  %%mm3\n"

         "movq           %%mm4,                  %%mm5\n"

         "punpcklwd      %%mm3,                  %%mm4\n"
         "punpckhwd      %%mm3,                  %%mm5\n"

         "movq           %%mm4,                  (%4,%5,2)\n"
         "movq           %%mm5,                  8(%4,%5,2)\n"

         "addl           $8,                     %2\n"
         "addl           $4,                     %0\n"
         "addl           $4,                     %1\n"
         "cmpl           %3,                     %6\n"
         "leal           16(%4),                 %4\n"
         "jl             1b\n"
         "addl           %3,                     %2\n"                   /* lum += cols */
         "addl           %7,                     %4\n"                   /* row1 += mod */
         "movl           $0,                     %6\n"
         "cmpl           %8,                     %2\n"
         "jl             1b\n"
         :
         : "r" (cr), "r" (cb), "r" (lum), "r" (cols), "r" (row1) ,"r" (col1), "m" (x), "m" (mod)
         , "m" (y)
         );
      __asm__ __volatile__(
         "emms\n"
         );
#endif
   }

yuv2rgb_fun yuv2rgb_mmx_init(int bpp, int mode) {
  
  if (bpp== 16 && mode == MODE_RGB) {
    fprintf(stdout,"using MMX for yuv2rgb conversion\n"); 
    return YUV2RGB420_MMX_16;
  }
  return NULL;
}
