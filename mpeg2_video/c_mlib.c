/* Ogle - A video player
 * Copyright (C) 2000, 2001 Martin Norbäck
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

#include <inttypes.h>
#include "c_mlib.h"

static inline void
mlib_VideoCopyRefAve_U8_U8 (uint8_t *curr_block,
			    const uint8_t *ref_block,
                            const int width, const int height,
			    int32_t stride);
static inline void
mlib_VideoInterpAveX_U8_U8(uint8_t *curr_block, 
                           const uint8_t *ref_block,
                           const int width, int height,
                           int32_t frame_stride,   
                           int32_t field_stride);
static inline void
mlib_VideoInterpAveY_U8_U8(uint8_t *curr_block, 
                           const uint8_t *ref_block,
                           const int width, const int height,
                           int32_t frame_stride,   
                           int32_t field_stride);
static inline void
mlib_VideoInterpAveXY_U8_U8(uint8_t *curr_block, 
                            const uint8_t *ref_block, 
                            const int width, const int height,
                            int32_t frame_stride,   
                            int32_t field_stride);
static inline void
mlib_VideoInterpX_U8_U8(uint8_t *curr_block, 
			const uint8_t *ref_block,
			const int width, const int height,
			int32_t frame_stride,   
			int32_t field_stride);
static inline void
mlib_VideoInterpY_U8_U8(uint8_t *curr_block, 
			const uint8_t *ref_block,
			const int width, const int height,
			int32_t frame_stride,   
			int32_t field_stride);
static inline void
mlib_VideoInterpXY_U8_U8(uint8_t *curr_block, 
			 const uint8_t *ref_block, 
			 const int width, const int height,
			 int32_t frame_stride,   
			 int32_t field_stride);


static inline unsigned int
clip_to_u8 (int value)
{
  //return value < 0 ? 0 : (value > 255 ? 255 : value);
  //return ((uint16_t)value) > 256 ? value < 0 ? 0 : 255 : value;
  return ((unsigned)value) > 256 ? ( 255 & ~(value >> 31) ) : value;
}

void
mlib_Init(void)
{
}

inline void
mlib_VideoCopyRef_U8_U8 (uint8_t *curr_block,
                         const uint8_t *ref_block,
                         const int32_t width,
                         const int32_t height,
                         int32_t stride)
{
  int x, y;
  
  for (y = 0; y < height; y++) {
    for (x = 0; x < width; x++)
      curr_block[x] = ref_block[x];
    ref_block += stride;
    curr_block += stride;
  }
}

static inline void
mlib_VideoCopyRefAve_U8_U8 (uint8_t *curr_block,
			    const uint8_t *ref_block,
			    const int width, const int height,
			    int32_t stride)
{
  int x, y;
  
  for (y = 0; y < height; y++) {
    for (x = 0; x < width; x++)
      curr_block[x] = (curr_block[x] + ref_block[x] + 1U) / 2U;
    ref_block += stride;
    curr_block += stride;
  }
}

static inline void
mlib_VideoInterpAveX_U8_U8(uint8_t *curr_block, 
                           const uint8_t *ref_block,
                           const int width, const int height,
                           int32_t frame_stride,   
                           int32_t field_stride) 
{
  int x, y;

  for (y = 0; y < height; y++) {
    for (x = 0; x < width; x++)
      curr_block[x] = (curr_block[x] +
		       ((ref_block[x] + ref_block[x+1] + 1U) / 2U) + 1U) / 2U;
    ref_block += frame_stride;
    curr_block += frame_stride;
  }
}

static inline void
mlib_VideoInterpAveY_U8_U8(uint8_t *curr_block, 
                           const uint8_t *ref_block,
                           const int width, const int height,
                           int32_t frame_stride,   
                           int32_t field_stride) 
{
  int x, y;
  const uint8_t *ref_block_next = ref_block + field_stride;

  for (y = 0; y < height; y++) {
    for (x = 0; x < width; x++)
      curr_block[x] = (curr_block[x] + 
		       ((ref_block[x] + ref_block_next[x] + 1U) / 2U) + 1U) / 2U;
    curr_block     += frame_stride;
    ref_block      += frame_stride;
    ref_block_next += frame_stride;
  }
}

static inline void
mlib_VideoInterpAveXY_U8_U8(uint8_t *curr_block, 
                            const uint8_t *ref_block, 
                            const int width, const int height,
                            int32_t frame_stride,   
                            int32_t field_stride) 
{
  int x, y;
  const uint8_t *ref_block_next = ref_block + field_stride;

  for (y = 0; y < height; y++) {
    for (x = 0; x < width; x++)
      curr_block[x] = (curr_block[x] + 
		       ((ref_block[x] + ref_block_next[x] +
			 ref_block[x+1] + ref_block_next[x+1] + 2U) / 4U) + 1U) / 2U;
    curr_block     += frame_stride;
    ref_block      += frame_stride;
    ref_block_next += frame_stride;
  }
}

static inline void
mlib_VideoInterpX_U8_U8(uint8_t *curr_block, 
			const uint8_t *ref_block,
			const int width, const int height,
			int32_t frame_stride,   
			int32_t field_stride) 
{
  int x, y;

  for (y = 0; y < height; y++) {
    for (x = 0; x < width; x++)
      curr_block[x] = (ref_block[x] + ref_block[x+1] + 1U) / 2U;
    ref_block += frame_stride;
    curr_block += frame_stride;
  }
}

static inline void
mlib_VideoInterpY_U8_U8(uint8_t *curr_block, 
			const uint8_t *ref_block,
			const int width, const int height,
			int32_t frame_stride,   
			int32_t field_stride) 
{
  int x, y;
  const uint8_t *ref_block_next = ref_block + field_stride;

  for (y = 0; y < height; y++) {
    for (x = 0; x < width; x++)
      curr_block[x] = (ref_block[x] + ref_block_next[x] + 1U) / 2U;
    curr_block     += frame_stride;
    ref_block      += frame_stride;
    ref_block_next += frame_stride;
  }
}

static inline void
mlib_VideoInterpXY_U8_U8(uint8_t *curr_block, 
			 const uint8_t *ref_block, 
			 const int width, const int height,
			 int32_t frame_stride,   
			 int32_t field_stride) 
{
  int x, y;
  const uint8_t *ref_block_next = ref_block + field_stride;

  for (y = 0; y < height; y++) {
    for (x = 0; x < width; x++)
      curr_block[x] = (ref_block[x] + ref_block_next[x] +
		       ref_block[x+1] + ref_block_next[x+1] + 2U) / 4U;
    curr_block     += frame_stride;
    ref_block      += frame_stride;
    ref_block_next += frame_stride;
  }
}

void
mlib_VideoCopyRefAve_U8_U8_16x16(uint8_t *curr_block,
                                 const uint8_t *ref_block,
                                 int32_t stride)
{
  mlib_VideoCopyRefAve_U8_U8 (curr_block, ref_block, 16, 16, stride);
}

void 
mlib_VideoCopyRefAve_U8_U8_16x8(uint8_t *curr_block,
				const uint8_t *ref_block,
				int32_t stride)
{
  mlib_VideoCopyRefAve_U8_U8 (curr_block, ref_block, 16, 8, stride);
}

void 
mlib_VideoCopyRefAve_U8_U8_8x8(uint8_t *curr_block,
			       const uint8_t *ref_block,
			       int32_t stride)
{
  mlib_VideoCopyRefAve_U8_U8 (curr_block, ref_block, 8, 8, stride);
}

void 
mlib_VideoCopyRefAve_U8_U8_8x4(uint8_t *curr_block,
			       const uint8_t *ref_block,
			       int32_t stride)
{
  mlib_VideoCopyRefAve_U8_U8 (curr_block, ref_block, 8, 4, stride);
}

void
mlib_VideoCopyRef_U8_U8_16x16(uint8_t *curr_block,
			      const uint8_t *ref_block,
			      int32_t stride)
{
  mlib_VideoCopyRef_U8_U8 (curr_block, ref_block, 16, 16, stride);
}

void 
mlib_VideoCopyRef_U8_U8_16x8(uint8_t *curr_block,
			     const uint8_t *ref_block,
			     int32_t stride)
{
  mlib_VideoCopyRef_U8_U8 (curr_block, ref_block, 16, 8, stride);
}

void 
mlib_VideoCopyRef_U8_U8_8x8(uint8_t *curr_block,
			    const uint8_t *ref_block,
			    int32_t stride)
{
  mlib_VideoCopyRef_U8_U8 (curr_block, ref_block, 8, 8, stride);
}

void 
mlib_VideoCopyRef_U8_U8_8x4(uint8_t *curr_block,
			    const uint8_t *ref_block,
			    int32_t stride)
{
  mlib_VideoCopyRef_U8_U8 (curr_block, ref_block, 8, 4, stride);
}

void
mlib_VideoInterpAveX_U8_U8_16x16(uint8_t *curr_block,
                                 const uint8_t *ref_block,
                                 int32_t frame_stride,
                                 int32_t field_stride)
{
  mlib_VideoInterpAveX_U8_U8 (curr_block, ref_block, 16, 16, frame_stride, field_stride);
}

void 
mlib_VideoInterpAveX_U8_U8_16x8(uint8_t *curr_block,
				const uint8_t *ref_block,
				int32_t frame_stride,
				int32_t field_stride)
{
  mlib_VideoInterpAveX_U8_U8 (curr_block, ref_block, 16, 8, frame_stride, field_stride);
}

void 
mlib_VideoInterpAveX_U8_U8_8x8(uint8_t *curr_block,
			       const uint8_t *ref_block,
			       int32_t frame_stride,
			       int32_t field_stride)
{
  mlib_VideoInterpAveX_U8_U8 (curr_block, ref_block, 8, 8, frame_stride, field_stride);
}

void 
mlib_VideoInterpAveX_U8_U8_8x4(uint8_t *curr_block,
			       const uint8_t *ref_block,
			       int32_t frame_stride,
			       int32_t field_stride)
{
  mlib_VideoInterpAveX_U8_U8 (curr_block, ref_block, 8, 4, frame_stride, field_stride);
}

void
mlib_VideoInterpAveY_U8_U8_16x16(uint8_t *curr_block,
                                 const uint8_t *ref_block,
                                 int32_t frame_stride,
                                 int32_t field_stride)
{
  mlib_VideoInterpAveY_U8_U8 (curr_block, ref_block, 16, 16, frame_stride, field_stride);
}

void 
mlib_VideoInterpAveY_U8_U8_16x8(uint8_t *curr_block,
				const uint8_t *ref_block,
				int32_t frame_stride,
				int32_t field_stride)
{
  mlib_VideoInterpAveY_U8_U8 (curr_block, ref_block, 16, 8, frame_stride, field_stride);
}

void 
mlib_VideoInterpAveY_U8_U8_8x8(uint8_t *curr_block,
			       const uint8_t *ref_block,
			       int32_t frame_stride,
			       int32_t field_stride)
{
  mlib_VideoInterpAveY_U8_U8 (curr_block, ref_block, 8, 8, frame_stride, field_stride);
}

void 
mlib_VideoInterpAveY_U8_U8_8x4(uint8_t *curr_block,
			       const uint8_t *ref_block,
			       int32_t frame_stride,
			       int32_t field_stride)
{
  mlib_VideoInterpAveY_U8_U8 (curr_block, ref_block, 8, 4, frame_stride, field_stride);
}

void
mlib_VideoInterpAveXY_U8_U8_16x16(uint8_t *curr_block,
				  const uint8_t *ref_block,
				  int32_t frame_stride,
				  int32_t field_stride)
{
  mlib_VideoInterpAveXY_U8_U8 (curr_block, ref_block, 16, 16, frame_stride, field_stride);
}

void 
mlib_VideoInterpAveXY_U8_U8_16x8(uint8_t *curr_block,
                                 const uint8_t *ref_block,
                                 int32_t frame_stride,
                                 int32_t field_stride)
{
  mlib_VideoInterpAveXY_U8_U8 (curr_block, ref_block, 16, 8, frame_stride, field_stride);
}

void 
mlib_VideoInterpAveXY_U8_U8_8x8(uint8_t *curr_block,
				const uint8_t *ref_block,
				int32_t frame_stride,
				int32_t field_stride)
{
  mlib_VideoInterpAveXY_U8_U8 (curr_block, ref_block, 8, 8, frame_stride, field_stride);
}

void 
mlib_VideoInterpAveXY_U8_U8_8x4(uint8_t *curr_block,
				const uint8_t *ref_block,
				int32_t frame_stride,
				int32_t field_stride)
{
  mlib_VideoInterpAveXY_U8_U8 (curr_block, ref_block, 8, 4, frame_stride, field_stride);
}

void
mlib_VideoInterpX_U8_U8_16x16(uint8_t *curr_block,
			      const uint8_t *ref_block,
			      int32_t frame_stride,
			      int32_t field_stride)
{
  mlib_VideoInterpX_U8_U8 (curr_block, ref_block, 16, 16, frame_stride, field_stride);
}

void 
mlib_VideoInterpX_U8_U8_16x8(uint8_t *curr_block,
			     const uint8_t *ref_block,
			     int32_t frame_stride,
			     int32_t field_stride)
{
  mlib_VideoInterpX_U8_U8 (curr_block, ref_block, 16, 8, frame_stride, field_stride);
}

void 
mlib_VideoInterpX_U8_U8_8x8(uint8_t *curr_block,
			    const uint8_t *ref_block,
			    int32_t frame_stride,
			    int32_t field_stride)
{
  mlib_VideoInterpX_U8_U8 (curr_block, ref_block, 8, 8, frame_stride, field_stride);
}

void 
mlib_VideoInterpX_U8_U8_8x4(uint8_t *curr_block,
			    const uint8_t *ref_block,
			    int32_t frame_stride,
			    int32_t field_stride)
{
  mlib_VideoInterpX_U8_U8 (curr_block, ref_block, 8, 4, frame_stride, field_stride);
}

void
mlib_VideoInterpY_U8_U8_16x16(uint8_t *curr_block,
			      const uint8_t *ref_block,
			      int32_t frame_stride,
			      int32_t field_stride)
{
  mlib_VideoInterpY_U8_U8 (curr_block, ref_block, 16, 16, frame_stride, field_stride);
}

void 
mlib_VideoInterpY_U8_U8_16x8(uint8_t *curr_block,
			     const uint8_t *ref_block,
			     int32_t frame_stride,
			     int32_t field_stride)
{
  mlib_VideoInterpY_U8_U8 (curr_block, ref_block, 16, 8, frame_stride, field_stride);
}

void 
mlib_VideoInterpY_U8_U8_8x8(uint8_t *curr_block,
			    const uint8_t *ref_block,
			    int32_t frame_stride,
			    int32_t field_stride)
{
  mlib_VideoInterpY_U8_U8 (curr_block, ref_block, 8, 8, frame_stride, field_stride);
}

void 
mlib_VideoInterpY_U8_U8_8x4(uint8_t *curr_block,
			    const uint8_t *ref_block,
			    int32_t frame_stride,
			    int32_t field_stride)
{
  mlib_VideoInterpY_U8_U8 (curr_block, ref_block, 8, 4, frame_stride, field_stride);
}

void
mlib_VideoInterpXY_U8_U8_16x16(uint8_t *curr_block,
			       const uint8_t *ref_block,
			       int32_t frame_stride,
			       int32_t field_stride)
{
  mlib_VideoInterpXY_U8_U8 (curr_block, ref_block, 16, 16, frame_stride, field_stride);
}

void 
mlib_VideoInterpXY_U8_U8_16x8(uint8_t *curr_block,
			      const uint8_t *ref_block,
			      int32_t frame_stride,
			      int32_t field_stride)
{
  mlib_VideoInterpXY_U8_U8 (curr_block, ref_block, 16, 8, frame_stride, field_stride);
}

void 
mlib_VideoInterpXY_U8_U8_8x8(uint8_t *curr_block,
			     const uint8_t *ref_block,
			     int32_t frame_stride,
			     int32_t field_stride)
{
  mlib_VideoInterpXY_U8_U8 (curr_block, ref_block, 8, 8, frame_stride, field_stride);
}

void 
mlib_VideoInterpXY_U8_U8_8x4(uint8_t *curr_block,
			     const uint8_t *ref_block,
			     int32_t frame_stride,
			     int32_t field_stride)
{
  mlib_VideoInterpXY_U8_U8 (curr_block, ref_block, 8, 4, frame_stride, field_stride);
}





#define W1 2841 /* 2048*sqrt(2)*cos(1*pi/16) */
#define W2 2676 /* 2048*sqrt(2)*cos(2*pi/16) */
#define W3 2408 /* 2048*sqrt(2)*cos(3*pi/16) */
#define W5 1609 /* 2048*sqrt(2)*cos(5*pi/16) */
#define W6 1108 /* 2048*sqrt(2)*cos(6*pi/16) */
#define W7 565  /* 2048*sqrt(2)*cos(7*pi/16) */



/* row (horizontal) IDCT
 *
 *           7                       pi         1
 * dst[k] = sum c[l] * src[l] * cos( -- * ( k + - ) * l )
 *          l=0                      8          2
 *
 * where: c[0]    = 128
 *        c[1..7] = 128*sqrt(2)
 */

static inline void 
idct_row(int16_t *blk, const int16_t *coeffs)
{
  int x0, x1, x2, x3, x4, x5, x6, x7, x8;

  x1 = coeffs[4]<<11; 
  x2 = coeffs[6]; 
  x3 = coeffs[2];
  x4 = coeffs[1]; 
  x5 = coeffs[7]; 
  x6 = coeffs[5]; 
  x7 = coeffs[3];

#if 0
  /* shortcut */
  if (!(x1 | x2 | x3 | x4 | x5 | x6 | x7 ))
  {
    blk[0]=blk[1]=blk[2]=blk[3]=blk[4]=blk[5]=blk[6]=blk[7]=coeffs[0]<<3;
    return;
  }
#endif

  x0 = (coeffs[0]<<11) + 128; /* for proper rounding in the fourth stage */

  /* first stage */
  x8 = W7*(x4+x5);
  x4 = x8 + (W1-W7)*x4;
  x5 = x8 - (W1+W7)*x5;
  x8 = W3*(x6+x7);
  x6 = x8 - (W3-W5)*x6;
  x7 = x8 - (W3+W5)*x7;
  
  /* second stage */
  x8 = x0 + x1;
  x0 -= x1;
  x1 = W6*(x3+x2);
  x2 = x1 - (W2+W6)*x2;
  x3 = x1 + (W2-W6)*x3;
  x1 = x4 + x6;
  x4 -= x6;
  x6 = x5 + x7;
  x5 -= x7;
  
  /* third stage */
  x7 = x8 + x3;
  x8 -= x3;
  x3 = x0 + x2;
  x0 -= x2;
  x2 = (181*(x4+x5)+128)>>8;
  x4 = (181*(x4-x5)+128)>>8;
  
  /* fourth stage */
  blk[0] = (x7+x1)>>8;
  blk[1] = (x3+x2)>>8;
  blk[2] = (x0+x4)>>8;
  blk[3] = (x8+x6)>>8;
  blk[4] = (x8-x6)>>8;
  blk[5] = (x0-x4)>>8;
  blk[6] = (x3-x2)>>8;
  blk[7] = (x7-x1)>>8;
}


/* column (vertical) IDCT
 *
 *             7                         pi         1
 * dst[8*k] = sum c[l] * src[8*l] * cos( -- * ( k + - ) * l )
 *            l=0                        8          2
 *
 * where: c[0]    = 1/1024
 *        c[1..7] = (1/1024)*sqrt(2)
 */

/* FIXME something odd is going on with inlining this 
 * procedure. Things break if it isn't inlined */
static inline void 
idct_col(int16_t *blk, const int16_t *coeffs)
{
  int x0, x1, x2, x3, x4, x5, x6, x7, x8;

  /* shortcut */
  x1 = (coeffs[8*4]<<8); 
  x2 = coeffs[8*6]; 
  x3 = coeffs[8*2];
  x4 = coeffs[8*1];
  x5 = coeffs[8*7]; 
  x6 = coeffs[8*5];
  x7 = coeffs[8*3];

#if 0
  if (!(x1  | x2 | x3 | x4 | x5 | x6 | x7 ))
  {
    blk[8*0] = blk[8*1] = blk[8*2] = blk[8*3] 
      = blk[8*4] = blk[8*5] = blk[8*6] = blk[8*7] 
      = (coeffs[8*0]+32)>>6;
    return;
  }
#endif

  x0 = (coeffs[8*0]<<8) + 8192;

  /* first stage */
  x8 = W7*(x4+x5) + 4;
  x4 = (x8+(W1-W7)*x4)>>3;
  x5 = (x8-(W1+W7)*x5)>>3;
  x8 = W3*(x6+x7) + 4;
  x6 = (x8-(W3-W5)*x6)>>3;
  x7 = (x8-(W3+W5)*x7)>>3;
  
  /* second stage */
  x8 = x0 + x1;
  x0 -= x1;
  x1 = W6*(x3+x2) + 4;
  x2 = (x1-(W2+W6)*x2)>>3;
  x3 = (x1+(W2-W6)*x3)>>3;
  x1 = x4 + x6;
  x4 -= x6;
  x6 = x5 + x7;
  x5 -= x7;
  
  /* third stage */
  x7 = x8 + x3;
  x8 -= x3;
  x3 = x0 + x2;
  x0 -= x2;
  x2 = (181*(x4+x5)+128)>>8;
  x4 = (181*(x4-x5)+128)>>8;
  
  /* fourth stage */
  blk[8*0] = (x7+x1)>>14;
  blk[8*1] = (x3+x2)>>14;
  blk[8*2] = (x0+x4)>>14;
  blk[8*3] = (x8+x6)>>14;
  blk[8*4] = (x8-x6)>>14;
  blk[8*5] = (x0-x4)>>14;
  blk[8*6] = (x3-x2)>>14;
  blk[8*7] = (x7-x1)>>14;
}

static inline void 
idct_col_u8(uint8_t *blk, const int16_t *coeffs, int stride)
{
  int x0, x1, x2, x3, x4, x5, x6, x7, x8;

  /* shortcut */
  x1 = (coeffs[8*4]<<8); 
  x2 = coeffs[8*6]; 
  x3 = coeffs[8*2];
  x4 = coeffs[8*1];
  x5 = coeffs[8*7]; 
  x6 = coeffs[8*5];
  x7 = coeffs[8*3];

#if 0
  if (!(x1  | x2 | x3 | x4 | x5 | x6 | x7 ))
  {
    blk[stride*0] = blk[stride*1] = blk[stride*2] = blk[stride*3]
      = blk[stride*4] = blk[stride*5] = blk[stride*6] = blk[stride*7]
      = clip_to_u8(coeffs[stride*0]+32)>>6;
    return;
  }
#endif

  x0 = (coeffs[8*0]<<8) + 8192;

  /* first stage */
  x8 = W7*(x4+x5) + 4;
  x4 = (x8+(W1-W7)*x4)>>3;
  x5 = (x8-(W1+W7)*x5)>>3;
  x8 = W3*(x6+x7) + 4;
  x6 = (x8-(W3-W5)*x6)>>3;
  x7 = (x8-(W3+W5)*x7)>>3;
  
  /* second stage */
  x8 = x0 + x1;
  x0 -= x1;
  x1 = W6*(x3+x2) + 4;
  x2 = (x1-(W2+W6)*x2)>>3;
  x3 = (x1+(W2-W6)*x3)>>3;
  x1 = x4 + x6;
  x4 -= x6;
  x6 = x5 + x7;
  x5 -= x7;
  
  /* third stage */
  x7 = x8 + x3;
  x8 -= x3;
  x3 = x0 + x2;
  x0 -= x2;
  x2 = (181*(x4+x5)+128)>>8;
  x4 = (181*(x4-x5)+128)>>8;
  
  /* fourth stage */
  blk[stride*0] = clip_to_u8((x7+x1)>>14);
  blk[stride*1] = clip_to_u8((x3+x2)>>14);
  blk[stride*2] = clip_to_u8((x0+x4)>>14);
  blk[stride*3] = clip_to_u8((x8+x6)>>14);
  blk[stride*4] = clip_to_u8((x8-x6)>>14);
  blk[stride*5] = clip_to_u8((x0-x4)>>14);
  blk[stride*6] = clip_to_u8((x3-x2)>>14);
  blk[stride*7] = clip_to_u8((x7-x1)>>14);
}


void
mlib_VideoIDCTAdd_U8_S16 (uint8_t *curr_block,
			  const int16_t *coeffs, 
			  int32_t stride) 
{
  int16_t temp[64];
  int16_t *block = &temp[0];
  int x,y;
  int i;
  
  for (i=0; i<8; i++)
    idct_row(block + 8*i, coeffs + 8*i);
  for (i=0; i<8; i++)
    idct_col(block + i, block + i);
  
  for (y = 0; y < 8; y++) {
    for (x = 0; x < 8; x++)
      curr_block[x] = clip_to_u8(curr_block[x] + *block++);
    curr_block += stride;
  }
}  


void
mlib_VideoIDCT8x8_U8_S16(uint8_t *block,
			 const int16_t *coeffs,
			 int32_t stride)
{
  int16_t temp[64];
  int i;
  
  for (i=0; i<8; i++)
    idct_row(temp + 8*i, coeffs + 8*i);
  for (i=0; i<8; i++)
    idct_col_u8(block + i, temp + i, stride);
}


