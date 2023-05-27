/*
 *  mmx_mlib.c
 *
 *  Intel MMX implementation of motion comp routines.
 *  MMX code written by David I. Lehn <dlehn@vt.edu>.
 *  lib{mmx,xmmx,sse} can be found at http://shay.ecn.purdue.edu/~swar/
 *
 *  Copyright 2000, David I. Lehn <dlehn@vt.edu>. Released under GPL.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>

#include "include/debug_print.h"

#include "mmx.h"
#include "mmx_mlib.h"

static uint64_t ones = 0x0001000100010001ULL;
static uint64_t twos = 0x0002000200020002ULL;

//#define MC_MMX_verify

void
mlib_Init(void)
{
  DNOTE("Using MMX accelerated media functions\n");
  return;
}
/*
static inline uint8_t
clip_to_u8 (int16_t value)
{
  //return value < 0 ? 0 : (value > 255 ? 255 : value);
  return ((uint16_t)value) > 256 ? value < 0 ? 0 : 255 : value;
}
*/
static inline void
mmx_average_2_U8(uint8_t *dst, const uint8_t *src1, const uint8_t *src2)
{
   //
   // *dst = (*src1 + *src2 + 1)/2;
   //

   //pxor_r2r(mm0,mm0);         // load 0 into mm0

   movq_m2r(*src1,mm1);        // load 8 src1 bytes
   movq_r2r(mm1,mm2);          // copy 8 src1 bytes

   movq_m2r(*src2,mm3);        // load 8 src2 bytes
   movq_r2r(mm3,mm4);          // copy 8 src2 bytes

   punpcklbw_r2r(mm0,mm1);     // unpack low src1 bytes
   punpckhbw_r2r(mm0,mm2);     // unpack high src1 bytes

   punpcklbw_r2r(mm0,mm3);     // unpack low src2 bytes
   punpckhbw_r2r(mm0,mm4);     // unpack high src2 bytes

   paddw_m2r(ones, mm1);
   paddw_r2r(mm3,mm1);         // add lows to mm1
   psraw_i2r(1,mm1);           // /2

   paddw_m2r(ones, mm2);
   paddw_r2r(mm4,mm2);         // add highs to mm2
   psraw_i2r(1,mm2);           // /2

   packuswb_r2r(mm2,mm1);      // pack (w/ saturation)
   movq_r2m(mm1,*dst);         // store result in dst
}

static inline void
mmx_interp_average_2_U8(uint8_t *dst, const uint8_t *src1, const uint8_t *src2)
{
   //
   // *dst = (*dst + (*src1 + *src2 + 1)/2 + 1)/2;
   //

   //pxor_r2r(mm0,mm0);             // load 0 into mm0

   movq_m2r(*dst,mm1);            // load 8 dst bytes
   movq_r2r(mm1,mm2);             // copy 8 dst bytes

   movq_m2r(*src1,mm3);           // load 8 src1 bytes
   movq_r2r(mm3,mm4);             // copy 8 src1 bytes

   movq_m2r(*src2,mm5);           // load 8 src2 bytes
   movq_r2r(mm5,mm6);             // copy 8 src2 bytes

   punpcklbw_r2r(mm0,mm1);        // unpack low dst bytes
   punpckhbw_r2r(mm0,mm2);        // unpack high dst bytes

   punpcklbw_r2r(mm0,mm3);        // unpack low src1 bytes
   punpckhbw_r2r(mm0,mm4);        // unpack high src1 bytes

   punpcklbw_r2r(mm0,mm5);        // unpack low src2 bytes
   punpckhbw_r2r(mm0,mm6);        // unpack high src2 bytes

   paddw_m2r(ones, mm3);
   paddw_r2r(mm5,mm3);            // add lows
   paddw_m2r(ones, mm4);
   paddw_r2r(mm6,mm4);            // add highs

   psraw_i2r(1,mm3);              // /2
   psraw_i2r(1,mm4);              // /2

   paddw_m2r(ones, mm1);
   paddw_r2r(mm3,mm1);            // add lows
   paddw_m2r(ones, mm2);
   paddw_r2r(mm4,mm2);            // add highs

   psraw_i2r(1,mm1);              // /2
   psraw_i2r(1,mm2);              // /2

   packuswb_r2r(mm2,mm1);         // pack (w/ saturation)
   movq_r2m(mm1,*dst);            // store result in dst
}

static inline void
mmx_average_4_U8(uint8_t *dst, const uint8_t *src1, const uint8_t *src2,
		 const uint8_t *src3, const uint8_t *src4)
{
   //
   // *dst = (*src1 + *src2 + *src3 + *src4 + 2)/4;
   //

   //pxor_r2r(mm0,mm0);                  // load 0 into mm0

   movq_m2r(*src1,mm1);                // load 8 src1 bytes
   movq_r2r(mm1,mm2);                  // copy 8 src1 bytes

   punpcklbw_r2r(mm0,mm1);             // unpack low src1 bytes
   punpckhbw_r2r(mm0,mm2);             // unpack high src1 bytes

   movq_m2r(*src2,mm3);                // load 8 src2 bytes
   movq_r2r(mm3,mm4);                  // copy 8 src2 bytes

   punpcklbw_r2r(mm0,mm3);             // unpack low src2 bytes
   punpckhbw_r2r(mm0,mm4);             // unpack high src2 bytes

   paddw_r2r(mm3,mm1);                 // add lows
   paddw_r2r(mm4,mm2);                 // add highs

   // now have partials in mm1 and mm2

   movq_m2r(*src3,mm3);                // load 8 src3 bytes
   movq_r2r(mm3,mm4);                  // copy 8 src3 bytes

   punpcklbw_r2r(mm0,mm3);             // unpack low src3 bytes
   punpckhbw_r2r(mm0,mm4);             // unpack high src3 bytes

   paddw_r2r(mm3,mm1);                 // add lows
   paddw_r2r(mm4,mm2);                 // add highs

   movq_m2r(*src4,mm5);                // load 8 src4 bytes
   movq_r2r(mm5,mm6);                  // copy 8 src4 bytes

   punpcklbw_r2r(mm0,mm5);             // unpack low src4 bytes
   punpckhbw_r2r(mm0,mm6);             // unpack high src4 bytes

   paddw_m2r(twos, mm1);
   paddw_r2r(mm5,mm1);                 // add lows
   paddw_m2r(twos, mm2);
   paddw_r2r(mm6,mm2);                 // add highs

   // now have subtotal in mm1 and mm2

   psraw_i2r(2,mm1);                   // /4
   psraw_i2r(2,mm2);                   // /4

   packuswb_r2r(mm2,mm1);              // pack (w/ saturation)
   movq_r2m(mm1,*dst);                 // store result in dst
}

static inline void
mmx_interp_average_4_U8(uint8_t *dst, const uint8_t *src1, const uint8_t *src2,
			const uint8_t *src3, const uint8_t *src4)
{
   //
   // *dst = (*dst + (*src1 + *src2 + *src3 + *src4 + 2)/4 + 1)/2;
   //

   //pxor_r2r(mm0,mm0);                  // load 0 into mm0

   movq_m2r(*src1,mm1);                // load 8 src1 bytes
   movq_r2r(mm1,mm2);                  // copy 8 src1 bytes

   punpcklbw_r2r(mm0,mm1);             // unpack low src1 bytes
   punpckhbw_r2r(mm0,mm2);             // unpack high src1 bytes

   movq_m2r(*src2,mm3);                // load 8 src2 bytes
   movq_r2r(mm3,mm4);                  // copy 8 src2 bytes

   punpcklbw_r2r(mm0,mm3);             // unpack low src2 bytes
   punpckhbw_r2r(mm0,mm4);             // unpack high src2 bytes

   paddw_r2r(mm3,mm1);                 // add lows
   paddw_r2r(mm4,mm2);                 // add highs

   // now have partials in mm1 and mm2

   movq_m2r(*src3,mm3);                // load 8 src3 bytes
   movq_r2r(mm3,mm4);                  // copy 8 src3 bytes

   punpcklbw_r2r(mm0,mm3);             // unpack low src3 bytes
   punpckhbw_r2r(mm0,mm4);             // unpack high src3 bytes

   paddw_r2r(mm3,mm1);                 // add lows
   paddw_r2r(mm4,mm2);                 // add highs

   movq_m2r(*src4,mm5);                // load 8 src4 bytes
   movq_r2r(mm5,mm6);                  // copy 8 src4 bytes

   punpcklbw_r2r(mm0,mm5);             // unpack low src4 bytes
   punpckhbw_r2r(mm0,mm6);             // unpack high src4 bytes

   paddw_m2r(twos, mm1);
   paddw_r2r(mm5,mm1);                 // add lows
   paddw_m2r(twos, mm2);
   paddw_r2r(mm6,mm2);                 // add highs

   psraw_i2r(2,mm1);                   // /4
   psraw_i2r(2,mm2);                   // /4

   // now have subtotal/4 in mm1 and mm2

   movq_m2r(*dst,mm3);                 // load 8 dst bytes
   movq_r2r(mm3,mm4);                  // copy 8 dst bytes

   punpcklbw_r2r(mm0,mm3);             // unpack low dst bytes
   punpckhbw_r2r(mm0,mm4);             // unpack high dst bytes

   paddw_m2r(ones, mm1);
   paddw_r2r(mm3,mm1);                 // add lows
   paddw_m2r(ones, mm2);
   paddw_r2r(mm4,mm2);                 // add highs

   psraw_i2r(1,mm1);                   // /2
   psraw_i2r(1,mm2);                   // /2

   // now have end value in mm1 and mm2

   packuswb_r2r(mm2,mm1);              // pack (w/ saturation)
   movq_r2m(mm1,*dst);                 // store result in dst
}


// VideoCopyRef* - Copy block from reference block to current block
// ---------------------------------------------------------------
static inline void
mlib_VideoCopyRefAve_U8_U8_MxN(
      const uint8_t m,
      const uint8_t n,
      uint8_t *curr_block,
      const uint8_t *ref_block,
      int32_t stride)
{
#define MMX_mmx_VideoCopyRefAve_U8_U8_MxN
#if !defined(HAVE_MMX) || !defined(MMX_mmx_VideoCopyRefAve_U8_U8_MxN)
   int x,y;
   const int jump = stride - m;

   for (y = 0; y < n; y++) {
      for (x = 0; x < m; x++)
         *curr_block++ = (*curr_block + *ref_block++ + 1)/2;
      ref_block += jump;
      curr_block += jump;
   }
#else
   int x,y;
   const int step = 8;
   const int jump = stride - m;

   pxor_r2r(mm0,mm0);             // load 0 into mm0

   for (y = 0; y < n; y++) {
      for (x = 0; x < m/8; x++) {
         mmx_average_2_U8(curr_block, curr_block, ref_block);

         curr_block += step;
         ref_block  += step;
      }
      curr_block += jump;
      ref_block  += jump;
   }
#endif
}

void
mlib_VideoCopyRefAve_U8_U8_16x16(
      uint8_t *curr_block,
      const uint8_t *ref_block,
      int32_t stride)
{
   mlib_VideoCopyRefAve_U8_U8_MxN(
      16, 16,
      curr_block,
      ref_block,
      stride);
}

void
mlib_VideoCopyRefAve_U8_U8_16x8(
      uint8_t *curr_block,
      const uint8_t *ref_block,
      int32_t stride)
{
   mlib_VideoCopyRefAve_U8_U8_MxN(
      16, 8,
      curr_block,
      ref_block,
      stride);
}

void
mlib_VideoCopyRefAve_U8_U8_8x8(
      uint8_t *curr_block,
      const uint8_t *ref_block,
      int32_t stride)
{
   mlib_VideoCopyRefAve_U8_U8_MxN(
      8, 8, 
      curr_block,
      ref_block,
      stride);
}

void
mlib_VideoCopyRefAve_U8_U8_8x4(
      uint8_t *curr_block,
      const uint8_t *ref_block,
      int32_t stride)
{
   mlib_VideoCopyRefAve_U8_U8_MxN(
      8, 4,
      curr_block,
      ref_block,
      stride);
}

#if 0
inline void
mlib_VideoCopyRef_U8_U8(
      uint8_t *curr_block,
      const uint8_t *ref_block,
      int32_t width,
      int32_t height,
      int32_t stride)
{
   int x,y;
   const int jump = stride - width;
   printf("I don't get called!");

   for (y = 0; y < height; y++) {
      for (x = 0; x < width; x++)
         *curr_block++ = *ref_block++;
      ref_block  += jump;
      curr_block += jump;
   }
}
#endif
void
print_U8_U8_MxN(
      const uint8_t m,
      const uint8_t n,
      uint8_t *block0,
      uint8_t *block1,
      int32_t stride)
{
   int x,y;
   int jump = stride - m;
   printf("block %dx%d @ %x<-%x stride=%d\n",n,n,(uint32_t)block0,(uint32_t)block1,stride);
   for (y = 0; y < n; y++) {
      printf("%2d: ",y);
      for (x = 0; x < m; x++) {
         printf("%3d<%3d ",*block0++,*block1++);
      }
      printf("\n");
      block0 += jump;
      block1 += jump;
   }
}

static inline void
mlib_VideoCopyRef_U8_U8_MxN(
      const uint8_t m,
      const uint8_t n,
      uint8_t *curr_block,
      const uint8_t *ref_block,
      int32_t stride)
{
#define MMX_mmx_VideoCopyRef_U8_U8_MxN
#if !defined(HAVE_MMX) || !defined(MMX_mmx_VideoCopyRef_U8_U8_MxN)
   int x,y;
   const int jump = stride - m;

   for (y = 0; y < n; y++) {
      for (x = 0; x < m; x++)
         *curr_block++ = *ref_block++;
      ref_block += jump;
      curr_block += jump;
   }
#else
   int x,y;
   const int step = 8;
   const int jump = stride - m;

   pxor_r2r(mm0,mm0);             // load 0 into mm0

   for (y = 0; y < n; y++) {
      for (x = 0; x < m/8; x++) {
         movq_m2r(*ref_block,mm1);    // load 8 ref bytes
         movq_r2m(mm1,*curr_block);   // store 8 bytes at curr

         curr_block += step;
         ref_block  += step;
      }
      curr_block += jump;
      ref_block  += jump;
   }
#endif
}

void
mlib_VideoCopyRef_U8_U8_16x16(
      uint8_t *curr_block,
      const uint8_t *ref_block,
      int32_t stride)
{
   mlib_VideoCopyRef_U8_U8_MxN(
      16, 16,
      curr_block,
      ref_block,
      stride);
}

void
mlib_VideoCopyRef_U8_U8_16x8(
      uint8_t *curr_block,
      const uint8_t *ref_block,
      int32_t stride)
{
   mlib_VideoCopyRef_U8_U8_MxN(
      16, 8,
      curr_block,
      ref_block,
      stride);
}

void
mlib_VideoCopyRef_U8_U8_8x8(
      uint8_t *curr_block,
      const uint8_t *ref_block,
      int32_t stride)
{
   mlib_VideoCopyRef_U8_U8_MxN(
      8, 8,
      curr_block,
      ref_block,
      stride);
}

void
mlib_VideoCopyRef_U8_U8_8x4(
      uint8_t *curr_block,
      const uint8_t *ref_block,
      int32_t stride)
{
   mlib_VideoCopyRef_U8_U8_MxN(
      8, 4,
      curr_block,
      ref_block,
      stride);
}

// VideoInterp*X - Half pixel interpolation in the x direction
// ------------------------------------------------------------------
static inline void 
mlib_VideoInterpAveX_U8_U8_MxN(
      const uint8_t m,
      const uint8_t n,
      uint8_t *curr_block,
      const uint8_t *ref_block,
      int32_t frame_stride,
      int32_t field_stride) 
{
#define MMX_mmx_VideoInterpAveX_U8_U8_MxN
#if !defined(HAVE_MMX) || !defined(MMX_mmx_VideoInterpAveX_U8_U8_MxN)
   int x,y;
   const int jump = frame_stride - m;

   for (y = 0; y < n; y++) {
      for (x = 0; x < m; x++)
         *curr_block++ = (*curr_block + (*ref_block++ + *(ref_block + 1) + 1)/2 + 1)/2;
      ref_block += jump;
      curr_block += jump;
   }
#else
   int x,y;
   const int step = 8;
   const int jump = frame_stride - m;

   pxor_r2r(mm0,mm0);             // load 0 into mm0

   for (y = 0; y < n; y++) {
      for (x = 0; x < m/8; x++) {
         mmx_interp_average_2_U8(curr_block, ref_block, ref_block + 1);

         curr_block += step;
         ref_block  += step;
      }
      curr_block += jump;
      ref_block  += jump;
   }
#endif
}

void 
mlib_VideoInterpAveX_U8_U8_16x16(
      uint8_t *curr_block,
      const uint8_t *ref_block,
      int32_t frame_stride,
      int32_t field_stride) 
{
   mlib_VideoInterpAveX_U8_U8_MxN(
      16, 16,
      curr_block,
      ref_block,
      frame_stride,
      field_stride);
}

void 
mlib_VideoInterpAveX_U8_U8_16x8(
      uint8_t *curr_block,
      const uint8_t *ref_block,
      int32_t frame_stride,
      int32_t field_stride) 
{
   mlib_VideoInterpAveX_U8_U8_MxN(
      16, 8,
      curr_block,
      ref_block,
      frame_stride,
      field_stride);
}

void 
mlib_VideoInterpAveX_U8_U8_8x8(
      uint8_t *curr_block,
      const uint8_t *ref_block,
      int32_t frame_stride,
      int32_t field_stride) 
{
   mlib_VideoInterpAveX_U8_U8_MxN(
      8, 8,
      curr_block,
      ref_block,
      frame_stride,
      field_stride);
}

void 
mlib_VideoInterpAveX_U8_U8_8x4(
      uint8_t *curr_block,
      const uint8_t *ref_block,
      int32_t frame_stride,
      int32_t field_stride) 
{
   mlib_VideoInterpAveX_U8_U8_MxN(
      8, 4,
      curr_block,
      ref_block,
      frame_stride,
      field_stride);
}

static inline void
mlib_VideoInterpX_U8_U8_MxN(
      const uint8_t m,
      const uint8_t n,
      uint8_t *curr_block,
      const uint8_t *ref_block,
      int32_t frame_stride,
      int32_t field_stride) 
{
#define MMX_mmx_VideoInterpX_U8_U8_MxN
#if !defined(HAVE_MMX) || !defined(MMX_mmx_VideoInterpX_U8_U8_MxN)
   int x,y;
   const int jump = frame_stride - m;

   for (y = 0; y < n; y++) {
      for (x = 0; x < m; x++)
         *curr_block++ = (*ref_block++ + *(ref_block + 1) + 1)/2;
      ref_block += jump;
      curr_block += jump;
   }
#else
   int x,y;
   const int step = 8;
   const int jump = frame_stride - m;

   pxor_r2r(mm0,mm0);             // load 0 into mm0

   for (y = 0; y < n; y++) {
      for (x = 0; x < m/8; x++) {
         mmx_average_2_U8(curr_block, ref_block, ref_block + 1);

         curr_block += step;
         ref_block  += step;
      }
      curr_block += jump;
      ref_block  += jump;
   }
#endif
}

void
mlib_VideoInterpX_U8_U8_16x16(
      uint8_t *curr_block,
      const uint8_t *ref_block,
      int32_t frame_stride,
      int32_t field_stride) 
{
   mlib_VideoInterpX_U8_U8_MxN(
      16, 16,
      curr_block,
      ref_block,
      frame_stride,
      field_stride);
}

void
mlib_VideoInterpX_U8_U8_16x8(
      uint8_t *curr_block,
      const uint8_t *ref_block,
      int32_t frame_stride,
      int32_t field_stride) 
{
   mlib_VideoInterpX_U8_U8_MxN(
      16, 8,
      curr_block,
      ref_block,
      frame_stride,
      field_stride);
}

void
mlib_VideoInterpX_U8_U8_8x8(
      uint8_t *curr_block,
      const uint8_t *ref_block,
      int32_t frame_stride,
      int32_t field_stride) 
{
   mlib_VideoInterpX_U8_U8_MxN(
      8, 8,
      curr_block,
      ref_block,
      frame_stride,
      field_stride);
}

void
mlib_VideoInterpX_U8_U8_8x4(
      uint8_t *curr_block,
      const uint8_t *ref_block,
      int32_t frame_stride,
      int32_t field_stride) 
{
   mlib_VideoInterpX_U8_U8_MxN(
      8, 4,
      curr_block,
      ref_block,
      frame_stride,
      field_stride);
}

// VideoInterp*XY - half pixel interpolation in both x and y directions
// --------------------------------------------------------------------
static inline void 
mlib_VideoInterpAveXY_U8_U8_MxN(
      const uint8_t m,
      const uint8_t n,
      uint8_t *curr_block,
      const uint8_t *ref_block,
      int32_t frame_stride,
      int32_t field_stride) 
{
#define MMX_mmx_VideoInterpAveXY_U8_U8_MxN
#if !defined(HAVE_MMX) || !defined(MMX_mmx_VideoInterpAveXY_U8_U8_MxN)
   int x,y;
   const int jump = frame_stride - m;
   const uint8_t *ref_block_next = ref_block + field_stride;

   for (y = 0; y < n; y++) {
      for (x = 0; x < m; x++)
         *curr_block++ = (*curr_block + (*ref_block++ + *(ref_block + 1) + *ref_block_next++ + *(ref_block_next + 1) + 2)/4 + 1)/2;
      curr_block     += jump;
      ref_block      += jump;
      ref_block_next += jump;
   }
#else
   int x,y;
   const int step = 8;
   const int jump = frame_stride - m;
   const uint8_t *ref_block_next = ref_block + field_stride;

   pxor_r2r(mm0,mm0);             // load 0 into mm0

   for (y = 0; y < n; y++) {
      for (x = 0; x < m/8; x++) {
         mmx_interp_average_4_U8(curr_block, ref_block, ref_block + 1, ref_block_next, ref_block_next + 1);

         curr_block     += step;
         ref_block      += step;
         ref_block_next += step;
      }
      curr_block     += jump;
      ref_block      += jump;
      ref_block_next += jump;
   }
#endif
}

void 
mlib_VideoInterpAveXY_U8_U8_16x16(
      uint8_t *curr_block,
      const uint8_t *ref_block,
      int32_t frame_stride,
      int32_t field_stride) 
{
   mlib_VideoInterpAveXY_U8_U8_MxN(
      16, 16,
      curr_block,
      ref_block,
      frame_stride,
      field_stride);
}

void 
mlib_VideoInterpAveXY_U8_U8_16x8(
      uint8_t *curr_block,
      const uint8_t *ref_block,
      int32_t frame_stride,
      int32_t field_stride) 
{
   mlib_VideoInterpAveXY_U8_U8_MxN(
      16, 8,
      curr_block,
      ref_block,
      frame_stride,
      field_stride);
}

void 
mlib_VideoInterpAveXY_U8_U8_8x8(
      uint8_t *curr_block,
      const uint8_t *ref_block,
      int32_t frame_stride,
      int32_t field_stride) 
{
   mlib_VideoInterpAveXY_U8_U8_MxN(
      8, 8,
      curr_block,
      ref_block,
      frame_stride,
      field_stride);
}

void 
mlib_VideoInterpAveXY_U8_U8_8x4(
      uint8_t *curr_block,
      const uint8_t *ref_block,
      int32_t frame_stride,
      int32_t field_stride) 
{
   mlib_VideoInterpAveXY_U8_U8_MxN(
      8, 4,
      curr_block,
      ref_block,
      frame_stride,
      field_stride);
}

static inline void 
mlib_VideoInterpXY_U8_U8_MxN(
      const uint8_t m,
      const uint8_t n,
      uint8_t *curr_block,
      const uint8_t *ref_block,
      int32_t frame_stride,
      int32_t field_stride) 
{
#define MMX_mmx_VideoInterpXY_U8_U8_MxN
#if !defined(HAVE_MMX) || !defined(MMX_mmx_VideoInterpXY_U8_U8_MxN)
   int x,y;
   const int jump = frame_stride - m;
   const uint8_t *ref_block_next = ref_block + field_stride;

   for (y = 0; y < n; y++) {
      for (x = 0; x < m; x++)
         *curr_block++ = (*ref_block++ + *(ref_block + 1) + *ref_block_next++ + *(ref_block_next + 1) + 2)/4;
      curr_block += jump;
      ref_block += jump;
      ref_block_next += jump;
   }
#else
   int x,y;
   const int step = 8;
   const int jump = frame_stride - m;
   const uint8_t *ref_block_next = ref_block + field_stride;

   pxor_r2r(mm0,mm0);             // load 0 into mm0

   for (y = 0; y < n; y++) {
      for (x = 0; x < m/8; x++) {
         mmx_average_4_U8(curr_block, ref_block, ref_block + 1, ref_block_next, ref_block_next + 1);

         curr_block     += step;
         ref_block      += step;
         ref_block_next += step;
      }
      curr_block     += jump;
      ref_block      += jump;
      ref_block_next += jump;
   }
#endif
}

void 
mlib_VideoInterpXY_U8_U8_16x16(
      uint8_t *curr_block,
      const uint8_t *ref_block,
      int32_t frame_stride,
      int32_t field_stride) 
{
   mlib_VideoInterpXY_U8_U8_MxN(
      16, 16,
      curr_block,
      ref_block,
      frame_stride,
      field_stride);
}

void 
mlib_VideoInterpXY_U8_U8_16x8(
      uint8_t *curr_block,
      const uint8_t *ref_block,
      int32_t frame_stride,
      int32_t field_stride) 
{
   mlib_VideoInterpXY_U8_U8_MxN(
      16, 8,
      curr_block,
      ref_block,
      frame_stride,
      field_stride);
}

void 
mlib_VideoInterpXY_U8_U8_8x8(
      uint8_t *curr_block,
      const uint8_t *ref_block,
      int32_t frame_stride,
      int32_t field_stride) 
{
   mlib_VideoInterpXY_U8_U8_MxN(
      8, 8,
      curr_block,
      ref_block,
      frame_stride,
      field_stride);
}

void 
mlib_VideoInterpXY_U8_U8_8x4(
      uint8_t *curr_block,
      const uint8_t *ref_block,
      int32_t frame_stride,
      int32_t field_stride) 
{
   mlib_VideoInterpXY_U8_U8_MxN(
      8, 4,
      curr_block,
      ref_block,
      frame_stride,
      field_stride);
}

// VideoInterp*Y - half pixel interpolation in the y direction
// -----------------------------------------------------------
static inline void 
mlib_VideoInterpAveY_U8_U8_MxN(
      const uint8_t m,
      const uint8_t n,
      uint8_t *curr_block,
      const uint8_t *ref_block,
      int32_t frame_stride,
      int32_t field_stride) 
{
#define MMX_mmx_VideoInterpAveY_U8_U8_MxN
#if !defined(HAVE_MMX) || !defined(MMX_mmx_VideoInterpAveY_U8_U8_MxN)
   int x,y;
   const int jump = frame_stride - m;
   const uint8_t *ref_block_next = ref_block + field_stride;

   for (y = 0; y < n; y++) {
      for (x = 0; x < m; x++)
         *curr_block++ = (*curr_block + (*ref_block++ + *ref_block_next++ + 1)/2 + 1)/2;
      curr_block     += jump;
      ref_block      += jump;
      ref_block_next += jump;
   }
#else
   int x,y;
   const int step = 8;
   const int jump = frame_stride - m;
   const uint8_t *ref_block_next = ref_block + field_stride;

   pxor_r2r(mm0,mm0);             // load 0 into mm0

   for (y = 0; y < n; y++) {
      for (x = 0; x < m/8; x++) {
         mmx_interp_average_2_U8(curr_block, ref_block, ref_block_next);

         curr_block     += step;
         ref_block      += step;
         ref_block_next += step;
      }
      curr_block     += jump;
      ref_block      += jump;
      ref_block_next += jump;
   }
#endif
}

void 
mlib_VideoInterpAveY_U8_U8_16x16(
      uint8_t *curr_block,
      const uint8_t *ref_block,
      int32_t frame_stride,
      int32_t field_stride) 
{
   mlib_VideoInterpAveY_U8_U8_MxN(
      16, 16,
      curr_block,
      ref_block,
      frame_stride,
      field_stride);
}

void 
mlib_VideoInterpAveY_U8_U8_16x8(
      uint8_t *curr_block,
      const uint8_t *ref_block,
      int32_t frame_stride,
      int32_t field_stride) 
{
   mlib_VideoInterpAveY_U8_U8_MxN(
      16, 8,
      curr_block,
      ref_block,
      frame_stride,
      field_stride);
}

void 
mlib_VideoInterpAveY_U8_U8_8x8(
      uint8_t *curr_block,
      const uint8_t *ref_block,
      int32_t frame_stride,
      int32_t field_stride) 
{
   mlib_VideoInterpAveY_U8_U8_MxN(
      8, 8,
      curr_block,
      ref_block,
      frame_stride,
      field_stride);
}

void 
mlib_VideoInterpAveY_U8_U8_8x4(
      uint8_t *curr_block,
      const uint8_t *ref_block,
      int32_t frame_stride,
      int32_t field_stride) 
{
   mlib_VideoInterpAveY_U8_U8_MxN(
      8, 4,
      curr_block,
      ref_block,
      frame_stride,
      field_stride);
}

static inline void
mlib_VideoInterpY_U8_U8_MxN(
      const uint8_t m,
      const uint8_t n,
      uint8_t *curr_block,
      const uint8_t *ref_block,
      int32_t frame_stride,
      int32_t field_stride) 
{
#define MMX_mmx_VideoInterpY_U8_U8_MxN
#if !defined(HAVE_MMX) || !defined(MMX_mmx_VideoInterpY_U8_U8_MxN)
   int x,y;
   const int jump = frame_stride - m;
   const uint8_t *ref_block_next = ref_block + field_stride;

   for (y = 0; y < n; y++) {
      for (x = 0; x < m; x++)
         *curr_block++ = (*ref_block++ + *ref_block_next++ + 1)/2;
      curr_block     += jump;
      ref_block      += jump;
      ref_block_next += jump;
   }
#else
   int x,y;
   const int step = 8;
   const int jump = frame_stride - m;
   const uint8_t *ref_block_next = ref_block + field_stride;

   pxor_r2r(mm0,mm0);             // load 0 into mm0

   for (y = 0; y < n; y++) {
      for (x = 0; x < m/8; x++) {
         mmx_average_2_U8(curr_block, ref_block, ref_block_next);

         curr_block     += step;
         ref_block      += step;
         ref_block_next += step;
      }
      curr_block     += jump;
      ref_block      += jump;
      ref_block_next += jump;
   }
#endif
}

void
mlib_VideoInterpY_U8_U8_16x16(
      uint8_t *curr_block,
      const uint8_t *ref_block,
      int32_t frame_stride,
      int32_t field_stride) 
{
   mlib_VideoInterpY_U8_U8_MxN(
      16, 16,
      curr_block,
      ref_block,
      frame_stride,
      field_stride);
}

void
mlib_VideoInterpY_U8_U8_16x8(
      uint8_t *curr_block,
      const uint8_t *ref_block,
      int32_t frame_stride,
      int32_t field_stride) 
{
   mlib_VideoInterpY_U8_U8_MxN(
      16, 8,
      curr_block,
      ref_block,
      frame_stride,
      field_stride);
}

void
mlib_VideoInterpY_U8_U8_8x8(
      uint8_t *curr_block,
      const uint8_t *ref_block,
      int32_t frame_stride,
      int32_t field_stride) 
{
   mlib_VideoInterpY_U8_U8_MxN(
      8, 8,
      curr_block,
      ref_block,
      frame_stride,
      field_stride);
}

void
mlib_VideoInterpY_U8_U8_8x4(
      uint8_t *curr_block,
      const uint8_t *ref_block,
      int32_t frame_stride,
      int32_t field_stride) 
{
   mlib_VideoInterpY_U8_U8_MxN(
      8, 4,
      curr_block,
      ref_block,
      frame_stride,
      field_stride);
}
