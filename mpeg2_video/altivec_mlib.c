/* Ogle - A video player
 * Copyright (C) 2001, Charles M. Hannum <root@ihack.net>
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
/*
 * AltiVec support written by Charles M. Hannum <root@ihack.net>, except
 * for the core IDCT routine published by Motorola.
 *
 * Notes:
 * 1) All AltiVec loads and stores are aligned.  Conveniently, the output
 *    area for the IDCT and motion comp functions is always aligned.
 *    However, the reference area is not; therefore, we check its
 *    alignment and use lvsl/vperm as necessary to align the reference
 *    data as it's loaded.
 * 2) Unfortunately, AltiVec doesn't do 8-byte loads and stores.  This
 *    means that the fastest paths are only applicable to the Y channel.
 *    For 8-byte operations on the U and V channels, there are two cases.
 *    When the alignment of the input and output are the same, we can do
 *    16-byte loads and just do two 4-byte stores.  For the unmatched
 *    alignment case, we have to do a rotation of the loaded data first.
 * 3) The `i[0-7]' variables look silly, but they prevent GCC from
 *    generating gratuitous multiplies, and allow the loaded constants
 *    to be recycled several times in the IDCT routine.
 * 4) The use of "b" constraints is *very* important.  Using r0 in any
 *    of the AltiVec load/store instructions is equivalent to a constant
 *    0.
 */

#include <inttypes.h>

#if 0
#define	ASSERT(x)	if (!(x)) abort()
#else
#define	ASSERT(x)
#endif

void
mlib_Init(void)
{
	asm("mtspr	0x100,%0" : : "b" (-1));
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
			curr_block[x] =
			    (curr_block[x] + 
			     ((ref_block[x] + ref_block_next[x] +
			       ref_block[x+1] + ref_block_next[x+1] + 2) >> 2) + 1) >> 1;
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
			curr_block[x] =
			    (ref_block[x] + ref_block_next[x] +
			     ref_block[x+1] + ref_block_next[x+1] + 2) >> 2;
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
	ASSERT(((int)curr_block & 15) == 0);

	if (((int)ref_block & 15) != 0) {
		int i0 = 0, i1 = 16;
		asm(""
			"lvsl 3,%0,%1\n"
                    "" : : "b" (ref_block), "b" (i0));
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"lvx 2,%0,%2\n"
			"vperm 0,0,1,3\n"
			"vavgub 0,0,2\n"
			"stvx 0,%0,%2\n"
                    "" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
                    "lvx 0,%1,%2\n"
                    "lvx 1,%1,%3\n"
                    "lvx 2,%0,%2\n"
                    "vperm 0,0,1,3\n"
                    "vavgub 0,0,2\n"
                    "stvx 0,%0,%2\n"
                    "" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n" 
			"lvx 2,%0,%2\n"
			"vperm 0,0,1,3\n"
			"vavgub 0,0,2\n"
			"stvx 0,%0,%2\n"
                    "" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"lvx 2,%0,%2\n"
			"vperm 0,0,1,3\n"
			"vavgub 0,0,2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"lvx 2,%0,%2\n"
			"vperm 0,0,1,3\n"
			"vavgub 0,0,2\n"
			"stvx 0,%0,%2 \n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"lvx 2,%0,%2\n"
			"vperm 0,0,1,3\n"
			"vavgub 0,0,2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"lvx 2,%0,%2\n"
			"vperm 0,0,1,3\n"
			"vavgub 0,0,2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"lvx 2,%0,%2\n"
			"vperm 0,0,1,3\n"
			"vavgub 0,0,2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"lvx 2,%0,%2\n"
			"vperm 0,0,1,3\n"
			"vavgub 0,0,2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"lvx 2,%0,%2\n"
			"vperm 0,0,1,3\n"
			"vavgub 0,0,2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"lvx 2,%0,%2\n"
			"vperm 0,0,1,3\n"
			"vavgub 0,0,2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"lvx 2,%0,%2\n"
			"vperm 0,0,1,3\n"
			"vavgub 0,0,2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"lvx 2,%0,%2\n"
			"vperm 0,0,1,3\n"
			"vavgub 0,0,2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"lvx 2,%0,%2\n"
			"vperm 0,0,1,3\n"
			"vavgub 0,0,2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"lvx 2,%0,%2\n"
			"vperm 0,0,1,3\n"
			"vavgub 0,0,2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"lvx 2,%0,%2\n"
			"vperm 0,0,1,3\n"
			"vavgub 0,0,2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
	} else {
		int i0 = 0;
		asm(""
			"lvx 0,%0,%2\n"
			"lvx 1,%1,%2\n"
			"vavgub 0,0,1\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx 0,%0,%2\n"
			"lvx 1,%1,%2\n"
			"vavgub 0,0,1\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx 0,%0,%2\n"
			"lvx 1,%1,%2\n"
			"vavgub 0,0,1\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx 0,%0,%2\n"
			"lvx 1,%1,%2\n"
			"vavgub 0,0,1\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx 0,%0,%2\n"
			"lvx 1,%1,%2\n"
			"vavgub 0,0,1\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx 0,%0,%2\n"
			"lvx 1,%1,%2\n"
			"vavgub 0,0,1\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx 0,%0,%2\n"
			"lvx 1,%1,%2\n"
			"vavgub 0,0,1\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx 0,%0,%2\n"
			"lvx 1,%1,%2\n"
			"vavgub 0,0,1\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx 0,%0,%2\n"
			"lvx 1,%1,%2\n"
			"vavgub 0,0,1\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx 0,%0,%2\n"
			"lvx 1,%1,%2\n"
			"vavgub 0,0,1\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx 0,%0,%2\n"
			"lvx 1,%1,%2\n"
			"vavgub 0,0,1\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx 0,%0,%2\n"
			"lvx 1,%1,%2\n"
			"vavgub 0,0,1\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx 0,%0,%2\n"
			"lvx 1,%1,%2\n"
			"vavgub 0,0,1\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx 0,%0,%2\n"
			"lvx 1,%1,%2\n"
			"vavgub 0,0,1\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx 0,%0,%2\n"
			"lvx 1,%1,%2\n"
			"vavgub 0,0,1\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx 0,%0,%2\n"
			"lvx 1,%1,%2\n"
			"vavgub 0,0,1\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
	}
}

void 
mlib_VideoCopyRefAve_U8_U8_16x8(uint8_t *curr_block,
				const uint8_t *ref_block,
				int32_t stride)
{
	ASSERT(((int)curr_block & 15) == 0);

	if (((int)ref_block & 15) != 0) {
		int i0 = 0, i1 = 16;
		asm(""
			"lvsl 3,%0,%1\n"
		"" : : "b" (ref_block), "b" (i0));
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"lvx 2,%0,%2\n"
			"vperm 0,0,1,3\n"
			"vavgub 0,0,2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"lvx 2,%0,%2\n"
			"vperm 0,0,1,3\n"
			"vavgub 0,0,2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"lvx 2,%0,%2\n"
			"vperm 0,0,1,3\n"
			"vavgub 0,0,2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"lvx 2,%0,%2\n"
			"vperm 0,0,1,3\n"
			"vavgub 0,0,2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"lvx 2,%0,%2\n"
			"vperm 0,0,1,3\n"
			"vavgub 0,0,2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"lvx 2,%0,%2\n"
			"vperm 0,0,1,3\n"
			"vavgub 0,0,2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"lvx 2,%0,%2\n"
			"vperm 0,0,1,3\n"
			"vavgub 0,0,2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"lvx 2,%0,%2\n"
			"vperm 0,0,1,3\n"
			"vavgub 0,0,2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
	} else {
		int i0 = 0;
		asm(""
			"lvx 0,%0,%2\n"
			"lvx 1,%1,%2\n"
			"vavgub 0,0,1\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx 0,%0,%2\n"
			"lvx 1,%1,%2\n"
			"vavgub 0,0,1\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx 0,%0,%2\n"
			"lvx 1,%1,%2\n"
			"vavgub 0,0,1\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx 0,%0,%2\n"
			"lvx 1,%1,%2\n"
			"vavgub 0,0,1\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx 0,%0,%2\n"
			"lvx 1,%1,%2\n"
			"vavgub 0,0,1\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx 0,%0,%2\n"
			"lvx 1,%1,%2\n"
			"vavgub 0,0,1\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx 0,%0,%2\n"
			"lvx 1,%1,%2\n"
			"vavgub 0,0,1\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx 0,%0,%2\n"
			"lvx 1,%1,%2\n"
			"vavgub 0,0,1\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
	}
}

void 
mlib_VideoCopyRefAve_U8_U8_8x8(uint8_t *curr_block,
			       const uint8_t *ref_block,
			       int32_t stride)
{
	ASSERT(((int)curr_block & 7) == 0);

	if ((((int)ref_block ^ (int)curr_block) & 15) != 0) {
		const int i0 = 0, i1 = 16, i2 = 4;
		asm(""
			"lvsl 3,%1,%2\n"
			"lvsl 4,%1,%3\n"
			"lvsr 5,%0,%2\n"
			"lvsr 6,%0,%3\n"
			"vperm 3,3,3,5\n"
			"vperm 4,4,4,6\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i0 + stride));
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"lvx 2,%0,%2\n"
			"vperm 0,0,1,3\n"
			"vavgub 0,0,2\n"
			"stvewx 0,%0,%2\n"
			"stvewx 0,%0,%4\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i1), "b" (i2));
		curr_block += stride, ref_block += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"lvx 2,%0,%2\n"
			"vperm 0,0,1,4\n"
			"vavgub 0,0,2\n"
			"stvewx 0,%0,%2\n"
			"stvewx 0,%0,%4\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i1), "b" (i2));
		curr_block += stride, ref_block += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"lvx 2,%0,%2\n"
			"vperm 0,0,1,3\n"
			"vavgub 0,0,2\n"
			"stvewx 0,%0,%2\n"
			"stvewx 0,%0,%4\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i1), "b" (i2));
		curr_block += stride, ref_block += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"lvx 2,%0,%2\n"
			"vperm 0,0,1,4\n"
			"vavgub 0,0,2\n"
			"stvewx 0,%0,%2\n"
			"stvewx 0,%0,%4\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i1), "b" (i2));
		curr_block += stride, ref_block += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"lvx 2,%0,%2\n"
			"vperm 0,0,1,3\n"
			"vavgub 0,0,2\n"
			"stvewx 0,%0,%2\n"
			"stvewx 0,%0,%4\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i1), "b" (i2));
		curr_block += stride, ref_block += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"lvx 2,%0,%2\n"
			"vperm 0,0,1,4\n"
			"vavgub 0,0,2\n"
			"stvewx 0,%0,%2\n"
			"stvewx 0,%0,%4\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i1), "b" (i2));
		curr_block += stride, ref_block += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"lvx 2,%0,%2\n"
			"vperm 0,0,1,3\n"
			"vavgub 0,0,2\n"
			"stvewx 0,%0,%2\n"
			"stvewx 0,%0,%4\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i1), "b" (i2));
		curr_block += stride, ref_block += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"lvx 2,%0,%2\n"
			"vperm 0,0,1,4\n"
			"vavgub 0,0,2\n"
			"stvewx 0,%0,%2\n"
			"stvewx 0,%0,%4\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i1), "b" (i2));
	} else {
		int i0 = 0, i1 = 4;
		asm(""
			"lvx 0,%0,%2\n"
			"lvx 1,%1,%2\n"
			"vavgub 0,0,1\n"
			"stvewx 0,%0,%2\n"
			"stvewx 0,%0,%3\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx 0,%0,%2\n"
			"lvx 1,%1,%2\n"
			"vavgub 0,0,1\n"
			"stvewx 0,%0,%2\n"
			"stvewx 0,%0,%3\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx 0,%0,%2\n"
			"lvx 1,%1,%2\n"
			"vavgub 0,0,1\n"
			"stvewx 0,%0,%2\n"
			"stvewx 0,%0,%3\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx 0,%0,%2\n"
			"lvx 1,%1,%2\n"
			"vavgub 0,0,1\n"
			"stvewx 0,%0,%2\n"
			"stvewx 0,%0,%3\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx 0,%0,%2\n"
			"lvx 1,%1,%2\n"
			"vavgub 0,0,1\n"
			"stvewx 0,%0,%2\n"
			"stvewx 0,%0,%3\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx 0,%0,%2\n"
			"lvx 1,%1,%2\n"
			"vavgub 0,0,1\n"
			"stvewx 0,%0,%2\n"
			"stvewx 0,%0,%3\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx 0,%0,%2\n"
			"lvx 1,%1,%2\n"
			"vavgub 0,0,1\n"
			"stvewx 0,%0,%2\n"
			"stvewx 0,%0,%3\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx 0,%0,%2\n"
			"lvx 1,%1,%2\n"
			"vavgub 0,0,1\n"
			"stvewx 0,%0,%2\n"
			"stvewx 0,%0,%3\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
	}
}

void 
mlib_VideoCopyRefAve_U8_U8_8x4(uint8_t *curr_block,
			       const uint8_t *ref_block,
			       int32_t stride)
{
	ASSERT(((int)curr_block & 7) == 0);

	if ((((int)ref_block ^ (int)curr_block) & 15) != 0) {
		const int i0 = 0, i1 = 16, i2 = 4;
		asm(""
			"lvsl 3,%1,%2\n"
			"lvsl 4,%1,%3\n"
			"lvsr 5,%0,%2\n"
			"lvsr 6,%0,%3\n"
			"vperm 3,3,3,5\n"
			"vperm 4,4,4,6\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i0 + stride));
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"lvx 2,%0,%2\n"
			"vperm 0,0,1,3\n"
			"vavgub 0,0,2\n"
			"stvewx 0,%0,%2\n"
			"stvewx 0,%0,%4\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i1), "b" (i2));
		curr_block += stride, ref_block += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"lvx 2,%0,%2\n"
			"vperm 0,0,1,4\n"
			"vavgub 0,0,2\n"
			"stvewx 0,%0,%2\n"
			"stvewx 0,%0,%4\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i1), "b" (i2));
		curr_block += stride, ref_block += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"lvx 2,%0,%2\n"
			"vperm 0,0,1,3\n"
			"vavgub 0,0,2\n"
			"stvewx 0,%0,%2\n"
			"stvewx 0,%0,%4\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i1), "b" (i2));
		curr_block += stride, ref_block += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"lvx 2,%0,%2\n"
			"vperm 0,0,1,4\n"
			"vavgub 0,0,2\n"
			"stvewx 0,%0,%2\n"
			"stvewx 0,%0,%4\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i1), "b" (i2));
	} else {
		int i0 = 0, i1 = 4;
		asm(""
			"lvx 0,%0,%2\n"
			"lvx 1,%1,%2\n"
			"vavgub 0,0,1\n"
			"stvewx 0,%0,%2\n"
			"stvewx 0,%0,%3\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx 0,%0,%2\n"
			"lvx 1,%1,%2\n"
			"vavgub 0,0,1\n"
			"stvewx 0,%0,%2\n"
			"stvewx 0,%0,%3\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx 0,%0,%2\n"
			"lvx 1,%1,%2\n"
			"vavgub 0,0,1\n"
			"stvewx 0,%0,%2\n"
			"stvewx 0,%0,%3\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx 0,%0,%2\n"
			"lvx 1,%1,%2\n"
			"vavgub 0,0,1\n"
			"stvewx 0,%0,%2\n"
			"stvewx 0,%0,%3\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
	}
}

void
mlib_VideoCopyRef_U8_U8_16x16_multiple(uint8_t *curr_block,
				       const uint8_t *ref_block,
				       int32_t stride,
				       int32_t count)
{
	ASSERT(((int)curr_block & 15) == 0);
	ASSERT(((int)ref_block & 15) == 0);
	
	while (count--) {
		int i0 = 0;
		asm(""
			"lvx 0,%1,%2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));

		curr_block += 16, ref_block += 16;
	}
}

void
mlib_VideoCopyRef_U8_U8_16x16(uint8_t *curr_block,
			      const uint8_t *ref_block,
			      int32_t stride)
{
	ASSERT(((int)curr_block & 15) == 0);
	
	if (((int)ref_block & 15) != 0) {
		int i0 = 0, i1 = 16;
		asm(""
			"lvsl 2,%0,%1\n"
		"" : : "b" (ref_block), "b" (i0));
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"vperm 0,0,1,2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"vperm 0,0,1,2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"vperm 0,0,1,2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"vperm 0,0,1,2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"vperm 0,0,1,2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"vperm 0,0,1,2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"vperm 0,0,1,2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"vperm 0,0,1,2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"vperm 0,0,1,2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"vperm 0,0,1,2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"vperm 0,0,1,2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"vperm 0,0,1,2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"vperm 0,0,1,2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"vperm 0,0,1,2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"vperm 0,0,1,2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"vperm 0,0,1,2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
	} else {
		int i0 = 0;
		asm(""
			"lvx 0,%1,%2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
	}
}

void 
mlib_VideoCopyRef_U8_U8_16x8(uint8_t *curr_block,
			     const uint8_t *ref_block,
			     int32_t stride)
{
	ASSERT(((int)curr_block & 15) == 0);

	if (((int)ref_block & 15) != 0) {
		int i0 = 0, i1 = 16;
		asm(""
			"lvsl 2,%0,%1\n"
		"" : : "b" (ref_block), "b" (i0));
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"vperm 0,0,1,2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"vperm 0,0,1,2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"vperm 0,0,1,2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"vperm 0,0,1,2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"vperm 0,0,1,2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"vperm 0,0,1,2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"vperm 0,0,1,2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"vperm 0,0,1,2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
	} else {
		int i0 = 0;
		asm(""
			"lvx 0,%1,%2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
		i0 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"stvx 0,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0));
	}
}

void 
mlib_VideoCopyRef_U8_U8_8x8_multiple(uint8_t *curr_block,
				     uint8_t *ref_block,
				     int32_t stride,
				     int32_t count)
{
	ASSERT(((int)curr_block & 7) == 0);

	while (count--) {
		int i0 = 0, i1 = 4;
		asm(""
			"lvx 0,%1,%2\n"
			"stvewx 0,%0,%2\n"
			"stvewx 0,%0,%3\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"stvewx 0,%0,%2\n"
			"stvewx 0,%0,%3\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"stvewx 0,%0,%2\n"
			"stvewx 0,%0,%3\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"stvewx 0,%0,%2\n"
			"stvewx 0,%0,%3\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"stvewx 0,%0,%2\n"
			"stvewx 0,%0,%3\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"stvewx 0,%0,%2\n"
			"stvewx 0,%0,%3\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"stvewx 0,%0,%2\n"
			"stvewx 0,%0,%3\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"stvewx 0,%0,%2\n"
			"stvewx 0,%0,%3\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));

		curr_block += 8, ref_block += 8;
	}
}

void 
mlib_VideoCopyRef_U8_U8_8x8(uint8_t *curr_block,
			    const uint8_t *ref_block,
			    int32_t stride)
{
	ASSERT(((int)curr_block & 7) == 0);

	if ((((int)ref_block ^ (int)curr_block) & 15) != 0) {
		const int i0 = 0, i1 = 16, i2 = 4;
		asm(""
			"lvsl 2,%1,%2\n"
			"lvsl 3,%1,%3\n"
			"lvsr 4,%0,%2\n"
			"lvsr 5,%0,%3\n"
			"vperm 2,2,2,4\n"
			"vperm 3,3,3,5\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i0 + stride));
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"vperm 0,0,1,2\n"
			"stvewx 0,%0,%2\n"
			"stvewx 0,%0,%4\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i1), "b" (i2));
		curr_block += stride, ref_block += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"vperm 0,0,1,3\n"
			"stvewx 0,%0,%2\n"
			"stvewx 0,%0,%4\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i1), "b" (i2));
		curr_block += stride, ref_block += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"vperm 0,0,1,2\n"
			"stvewx 0,%0,%2\n"
			"stvewx 0,%0,%4\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i1), "b" (i2));
		curr_block += stride, ref_block += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"vperm 0,0,1,3\n"
			"stvewx 0,%0,%2\n"
			"stvewx 0,%0,%4\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i1), "b" (i2));
		curr_block += stride, ref_block += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"vperm 0,0,1,2\n"
			"stvewx 0,%0,%2\n"
			"stvewx 0,%0,%4\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i1), "b" (i2));
		curr_block += stride, ref_block += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"vperm 0,0,1,3\n"
			"stvewx 0,%0,%2\n"
			"stvewx 0,%0,%4\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i1), "b" (i2));
		curr_block += stride, ref_block += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"vperm 0,0,1,2\n"
			"stvewx 0,%0,%2\n"
			"stvewx 0,%0,%4\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i1), "b" (i2));
		curr_block += stride, ref_block += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"vperm 0,0,1,3\n"
			"stvewx 0,%0,%2\n"
			"stvewx 0,%0,%4\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i1), "b" (i2));
	} else {
		int i0 = 0, i1 = 4;
		asm(""
			"lvx 0,%1,%2\n"
			"stvewx 0,%0,%2\n"
			"stvewx 0,%0,%3\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"stvewx 0,%0,%2\n"
			"stvewx 0,%0,%3\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"stvewx 0,%0,%2\n"
			"stvewx 0,%0,%3\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"stvewx 0,%0,%2\n"
			"stvewx 0,%0,%3\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"stvewx 0,%0,%2\n"
			"stvewx 0,%0,%3\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"stvewx 0,%0,%2\n"
			"stvewx 0,%0,%3\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"stvewx 0,%0,%2\n"
			"stvewx 0,%0,%3\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"stvewx 0,%0,%2\n"
			"stvewx 0,%0,%3\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
	}
}

void 
mlib_VideoCopyRef_U8_U8_8x4(uint8_t *curr_block,
			    const uint8_t *ref_block,
			    int32_t stride)
{
	ASSERT(((int)curr_block & 7) == 0);

	if ((((int)ref_block ^ (int)curr_block) & 15) != 0) {
		const int i0 = 0, i1 = 16, i2 = 4;
		asm(""
			"lvsl 2,%1,%2\n"
			"lvsl 3,%1,%3\n"
			"lvsr 4,%0,%2\n"
			"lvsr 5,%0,%3\n"
			"vperm 2,2,2,4\n"
			"vperm 3,3,3,5\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i0 + stride));
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"vperm 0,0,1,2\n"
			"stvewx 0,%0,%2\n"
			"stvewx 0,%0,%4\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i1), "b" (i2));
		curr_block += stride, ref_block += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"vperm 0,0,1,3\n"
			"stvewx 0,%0,%2\n"
			"stvewx 0,%0,%4\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i1), "b" (i2));
		curr_block += stride, ref_block += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"vperm 0,0,1,2\n"
			"stvewx 0,%0,%2\n"
			"stvewx 0,%0,%4\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i1), "b" (i2));
		curr_block += stride, ref_block += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"vperm 0,0,1,3\n"
			"stvewx 0,%0,%2\n"
			"stvewx 0,%0,%4\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i1), "b" (i2));
	} else {
		int i0 = 0, i1 = 4;
		asm(""
			"lvx 0,%1,%2\n"
			"stvewx 0,%0,%2\n"
			"stvewx 0,%0,%3\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"stvewx 0,%0,%2\n"
			"stvewx 0,%0,%3\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"stvewx 0,%0,%2\n"
			"stvewx 0,%0,%3\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
		i0 += stride, i1 += stride;
		asm(""
			"lvx 0,%1,%2\n"
			"stvewx 0,%0,%2\n"
			"stvewx 0,%0,%3\n"
		"" : : "b" (curr_block), "b" (ref_block), "b" (i0), "b" (i1));
	}
}

void
mlib_VideoInterpAveX_U8_U8_16x16(uint8_t *curr_block,
                                 const uint8_t *ref_block,
                                 int32_t frame_stride,
                                 int32_t field_stride)
{
	int i;
	int i0 = 0, i1 = 16;

	ASSERT(((int)curr_block & 15) == 0);

	asm(""
		"vspltisb 0,1\n"
		"lvsl 2,%0,%1\n"
		"vaddubs 3,2,0\n"
	"" : : "b" (ref_block), "b" (i0));
	for (i = 0; i < 16; i++) {
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"vperm 4,0,1,2\n"
			"vperm 5,0,1,3\n"
			"lvx 6,%0,%2\n"
			"vavgub 4,4,5\n"
			"vavgub 4,4,6\n"
			"stvx 4,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i1));
		i0 += frame_stride, i1 += frame_stride;
	}
}

void 
mlib_VideoInterpAveX_U8_U8_16x8(uint8_t *curr_block,
				const uint8_t *ref_block,
				int32_t frame_stride,
				int32_t field_stride)
{
	int i;
	int i0 = 0, i1 = 16;

	ASSERT(((int)curr_block & 15) == 0);

	asm(""
		"vspltisb 0,1\n"
		"lvsl 2,%0,%1\n"
		"vaddubs 3,2,0\n"
	"" : : "b" (ref_block), "b" (i0), "b" (i1));
	for (i = 0; i < 8; i++) {
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"vperm 4,0,1,2\n"
			"vperm 5,0,1,3\n"
			"lvx 6,%0,%2\n"
			"vavgub 4,4,5\n"
			"vavgub 4,4,6\n"
			"stvx 4,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i1));
		i0 += frame_stride, i1 += frame_stride;
	}
}

void 
mlib_VideoInterpAveX_U8_U8_8x8(uint8_t *curr_block,
			       const uint8_t *ref_block,
			       int32_t frame_stride,
			       int32_t field_stride)
{
	int i;
	const int i0 = 0, i1 = 16, i2 = 4;

	ASSERT(((int)curr_block & 7) == 0);

	asm(""
		"vspltisb 0,1\n"
		"lvsl 2,%1,%2\n"
		"lvsr 3,%0,%2\n"
		"lvsl 4,%1,%3\n"
		"lvsr 5,%0,%3\n"
		"vperm 2,2,2,3\n"
		"vperm 4,4,4,5\n"
		"vaddubs 3,2,0\n"
		"vaddubs 5,4,0\n"
	"" : : "b" (curr_block), "b" (ref_block),
	      "b" (i0), "b" (i0 + frame_stride));
	for (i = 0; i < 4; i++) {
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"vperm 6,0,1,2\n"
			"vperm 7,0,1,3\n"
			"lvx 8,%0,%2\n"
			"vavgub 6,6,7\n"
			"vavgub 6,6,8\n"
			"stvewx 6,%0,%2\n"
			"stvewx 6,%0,%4\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i1), "b" (i2));
		curr_block += frame_stride, ref_block += frame_stride;
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"vperm 6,0,1,4\n"
			"vperm 7,0,1,5\n"
			"lvx 8,%0,%2\n"
			"vavgub 6,6,7\n"
			"vavgub 6,6,8\n"
			"stvewx 6,%0,%2\n"
			"stvewx 6,%0,%4\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i1), "b" (i2));
		curr_block += frame_stride, ref_block += frame_stride;
	}
}

void 
mlib_VideoInterpAveX_U8_U8_8x4(uint8_t *curr_block,
			       const uint8_t *ref_block,
			       int32_t frame_stride,
			       int32_t field_stride)
{
	int i;
	const int i0 = 0, i1 = 16, i2 = 4;

	ASSERT(((int)curr_block & 7) == 0);

	asm(""
		"vspltisb 0,1\n"
		"lvsl 2,%1,%2\n"
		"lvsr 3,%0,%2\n"
		"lvsl 4,%1,%3\n"
		"lvsr 5,%0,%3\n"
		"vperm 2,2,2,3\n"
		"vperm 4,4,4,5\n"
		"vaddubs 3,2,0\n"
		"vaddubs 5,4,0\n"
	"" : : "b" (curr_block), "b" (ref_block),
	      "b" (i0), "b" (i0 + frame_stride));
	for (i = 0; i < 2; i++) {
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"vperm 6,0,1,2\n"
			"vperm 7,0,1,3\n"
			"lvx 8,%0,%2\n"
			"vavgub 6,6,7\n"
			"vavgub 6,6,8\n"
			"stvewx 6,%0,%2\n"
			"stvewx 6,%0,%4\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i1), "b" (i2));
		curr_block += frame_stride, ref_block += frame_stride;
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"vperm 6,0,1,4\n"
			"vperm 7,0,1,5\n"
			"lvx 8,%0,%2\n"
			"vavgub 6,6,7\n"
			"vavgub 6,6,8\n"
			"stvewx 6,%0,%2\n"
			"stvewx 6,%0,%4\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i1), "b" (i2));
		curr_block += frame_stride, ref_block += frame_stride;
	}
}

void
mlib_VideoInterpAveY_U8_U8_16x16(uint8_t *curr_block,
                                 const uint8_t *ref_block,
                                 int32_t frame_stride,
                                 int32_t field_stride)
{
	int i;

	ASSERT(((int)curr_block & 15) == 0);

	if (((int)ref_block & 15) != 0) {
		int i0 = 0, i1 = 16;
		asm(""
			"lvsl 4,%0,%1\n"
		"" : : "b" (ref_block), "b" (i0));
		for (i = 0; i < 16; i++) {
			asm(""
				"lvx 0,%1,%3\n"
				"lvx 1,%2,%3\n"
				"lvx 2,%1,%4\n"
				"lvx 3,%2,%4\n"
				"vperm 0,0,2,4\n"
				"vperm 1,1,3,4\n"
				"lvx 2,%0,%3\n"
				"vavgub 0,0,1\n"
				"vavgub 0,0,2\n"
				"stvx 0,%0,%3\n"
			"" : : "b" (curr_block), "b" (ref_block),
			      "b" (ref_block + field_stride),
			      "b" (i0), "b" (i1));
			i0 += frame_stride, i1 += frame_stride;
		}
	} else {
		int i0 = 0;
		for (i = 0; i < 16; i++) {
			asm(""
				"lvx 0,%1,%3\n"
				"lvx 1,%2,%3\n"
				"lvx 2,%0,%3\n"
				"vavgub 0,0,1\n"
				"vavgub 0,0,2\n"
				"stvx 0,%0,%3\n"
			"" : : "b" (curr_block), "b" (ref_block),
			      "b" (ref_block + field_stride),
			      "b" (i0));
			i0 += frame_stride;
		}
	}
}

void 
mlib_VideoInterpAveY_U8_U8_16x8(uint8_t *curr_block,
				const uint8_t *ref_block,
				int32_t frame_stride,
				int32_t field_stride)
{
	int i;

	ASSERT(((int)curr_block & 15) == 0);

	if (((int)ref_block & 15) != 0) {
		int i0 = 0, i1 = 16;
		asm(""
			"lvsl 4,%0,%1\n"
		"" : : "b" (ref_block), "b" (i0));
		for (i = 0; i < 8; i++) {
			asm(""
				"lvx 0,%1,%3\n"
				"lvx 1,%2,%3\n"
				"lvx 2,%1,%4\n"
				"lvx 3,%2,%4\n"
				"vperm 0,0,2,4\n"
				"vperm 1,1,3,4\n"
				"lvx 2,%0,%3\n"
				"vavgub 0,0,1\n"
				"vavgub 0,0,2\n"
				"stvx 0,%0,%3\n"
			"" : : "b" (curr_block), "b" (ref_block),
			      "b" (ref_block + field_stride),
			      "b" (i0), "b" (i1));
			i0 += frame_stride, i1 += frame_stride;
		}
	} else {
		int i0 = 0;
		for (i = 0; i < 8; i++) {
			asm(""
				"lvx 0,%1,%3\n"
				"lvx 1,%2,%3\n"
				"lvx 2,%0,%3\n"
				"vavgub 0,0,1\n"
				"vavgub 0,0,2\n"
				"stvx 0,%0,%3\n"
			"" : : "b" (curr_block), "b" (ref_block),
			      "b" (ref_block + field_stride),
			      "b" (i0));
			i0 += frame_stride;
		}
	}
}

void 
mlib_VideoInterpAveY_U8_U8_8x8(uint8_t *curr_block,
			       const uint8_t *ref_block,
			       int32_t frame_stride,
			       int32_t field_stride)
{
	int i;

	ASSERT(((int)curr_block & 7) == 0);

	if (((((int)ref_block ^ (int)curr_block) | field_stride) & 15) != 0) {
		const int i0 = 0, i1 = 16, i2 = 4;
		asm(""
			"lvsl 4,%1,%3\n"
			"lvsl 5,%1,%4\n"
			"lvsl 6,%2,%3\n"
			"lvsl 7,%2,%4\n"
			"lvsr 8,%0,%3\n"
			"lvsr 9,%0,%4\n"
			"vperm 4,4,4,8\n"
			"vperm 5,5,5,9\n"
			"vperm 6,6,6,8\n"
			"vperm 7,7,7,9\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (ref_block + field_stride),
		      "b" (i0), "b" (i0 + frame_stride));
		for (i = 0; i < 4; i++) {
			asm(""
				"lvx 0,%1,%3\n"
				"lvx 1,%1,%4\n"
				"lvx 2,%2,%3\n"
				"lvx 3,%2,%4\n"
				"vperm 8,0,1,4\n"
				"vperm 9,2,3,6\n"
				"lvx 10,%0,%3\n"
				"vavgub 8,8,9\n"
				"vavgub 8,8,10\n"
				"stvewx 8,%0,%3\n"
				"stvewx 8,%0,%5\n"
			"" : : "b" (curr_block), "b" (ref_block),
			      "b" (ref_block + field_stride),
			      "b" (i0), "b" (i1), "b" (i2));
			curr_block += frame_stride, ref_block += frame_stride;
			asm(""
				"lvx 0,%1,%3\n"
				"lvx 1,%1,%4\n"
				"lvx 2,%2,%3\n"
				"lvx 3,%2,%4\n"
				"vperm 8,0,1,5\n"
				"vperm 9,2,3,7\n"
				"lvx 10,%0,%3\n"
				"vavgub 8,8,9\n"
				"vavgub 8,8,10\n"
				"stvewx 8,%0,%3\n"
				"stvewx 8,%0,%5\n"
			"" : : "b" (curr_block), "b" (ref_block),
			      "b" (ref_block + field_stride),
			      "b" (i0), "b" (i1), "b" (i2));
			curr_block += frame_stride, ref_block += frame_stride;
		}
	} else {
		int i0 = 0, i1 = 4;
		for (i = 0; i < 8; i++) {
			asm(""
				"lvx 0,%1,%3\n"
				"lvx 1,%2,%3\n"
				"lvx 2,%0,%3\n"
				"vavgub 0,0,1\n"
				"vavgub 0,0,2\n"
				"stvewx 0,%0,%3\n"
				"stvewx 0,%0,%4\n"
			"" : : "b" (curr_block), "b" (ref_block),
			      "b" (ref_block + field_stride),
			      "b" (i0), "b" (i1));
			i0 += frame_stride, i1 += frame_stride;
		}
	}
}

void 
mlib_VideoInterpAveY_U8_U8_8x4(uint8_t *curr_block,
			       const uint8_t *ref_block,
			       int32_t frame_stride,
			       int32_t field_stride)
{
	int i;

	ASSERT(((int)curr_block & 7) == 0);

	if (((((int)ref_block ^ (int)curr_block) | field_stride) & 15) != 0) {
		const int i0 = 0, i1 = 16, i2 = 4;
		asm(""
			"lvsl 4,%1,%3\n"
			"lvsl 5,%1,%4\n"
			"lvsl 6,%2,%3\n"
			"lvsl 7,%2,%4\n"
			"lvsr 8,%0,%3\n"
			"lvsr 9,%0,%4\n"
			"vperm 4,4,4,8\n"
			"vperm 5,5,5,9\n"
			"vperm 6,6,6,8\n"
			"vperm 7,7,7,9\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (ref_block + field_stride),
		      "b" (i0), "b" (i0 + frame_stride));
		for (i = 0; i < 2; i++) {
			asm(""
				"lvx 0,%1,%3\n"
				"lvx 1,%1,%4\n"
				"lvx 2,%2,%3\n"
				"lvx 3,%2,%4\n"
				"vperm 8,0,1,4\n"
				"vperm 9,2,3,6\n"
				"lvx 10,%0,%3\n"
				"vavgub 8,8,9\n"
				"vavgub 8,8,10\n"
				"stvewx 8,%0,%3\n"
				"stvewx 8,%0,%5\n"
			"" : : "b" (curr_block), "b" (ref_block),
			      "b" (ref_block + field_stride),
			      "b" (i0), "b" (i1), "b" (i2));
			curr_block += frame_stride, ref_block += frame_stride;
			asm(""
				"lvx 0,%1,%3\n"
				"lvx 1,%1,%4\n"
				"lvx 2,%2,%3\n"
				"lvx 3,%2,%4\n"
				"vperm 8,0,1,5\n"
				"vperm 9,2,3,7\n"
				"lvx 10,%0,%3\n"
				"vavgub 8,8,9\n"
				"vavgub 8,8,10\n"
				"stvewx 8,%0,%3\n"
				"stvewx 8,%0,%5\n"
			"" : : "b" (curr_block), "b" (ref_block),
			      "b" (ref_block + field_stride),
			      "b" (i0), "b" (i1), "b" (i2));
			curr_block += frame_stride, ref_block += frame_stride;
		}
	} else {
		int i0 = 0, i1 = 4;
		for (i = 0; i < 4; i++) {
			asm(""
				"lvx 0,%1,%3\n"
				"lvx 1,%2,%3\n"
				"lvx 2,%0,%3\n"
				"vavgub 0,0,1\n"
				"vavgub 0,0,2\n"
				"stvewx 0,%0,%3\n"
				"stvewx 0,%0,%4\n"
			"" : : "b" (curr_block), "b" (ref_block),
			      "b" (ref_block + field_stride),
			      "b" (i0), "b" (i1));
			i0 += frame_stride, i1 += frame_stride;
		}
	}
}

void
mlib_VideoInterpAveXY_U8_U8_16x16(uint8_t *curr_block,
				  const uint8_t *ref_block,
				  int32_t frame_stride,
				  int32_t field_stride)
{
	int i;
	int i0 = 0, i1 = 16;

	ASSERT(((int)curr_block & 15) == 0);

	asm(""
		"vspltisb 0,1\n"
		"lvsl 4,%0,%1\n"
		"vaddubs 5,4,0\n"
	"" : : "b" (ref_block), "b" (i0));
	for (i = 0; i < 16; i++) {
		asm(""
			"lvx 0,%1,%3\n"
			"lvx 1,%2,%3\n"
			"lvx 2,%1,%4\n"
			"lvx 3,%2,%4\n"
			"vperm 6,0,2,4\n"
			"vperm 7,0,2,5\n"
			"vperm 8,1,3,4\n"
			"vperm 9,1,3,5\n"
			"vavgub 6,6,7\n"
			"vavgub 8,8,9\n"
			"lvx 10,%0,%3\n"
			"vavgub 6,6,8\n"
			"vavgub 6,6,10\n"
			"stvx 6,%0,%3\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (ref_block + field_stride),
		      "b" (i0), "b" (i1));
		i0 += frame_stride, i1 += frame_stride;
	}
}

void 
mlib_VideoInterpAveXY_U8_U8_16x8(uint8_t *curr_block,
                                 const uint8_t *ref_block,
                                 int32_t frame_stride,
                                 int32_t field_stride)
{
	int i;
	int i0 = 0, i1 = 16;

	ASSERT(((int)curr_block & 15) == 0);

	asm(""
		"vspltisb 0,1\n"
		"lvsl 4,%0,%1\n"
		"vaddubs 5,4,0\n"
	"" : : "b" (ref_block), "b" (i0));
	for (i = 0; i < 8; i++) {
		asm(""
			"lvx 0,%1,%3\n"
			"lvx 1,%2,%3\n"
			"lvx 2,%1,%4\n"
			"lvx 3,%2,%4\n"
			"vperm 6,0,2,4\n"
			"vperm 7,0,2,5\n"
			"vperm 8,1,3,4\n"
			"vperm 9,1,3,5\n"
			"vavgub 6,6,7\n"
			"vavgub 8,8,9\n"
			"lvx 10,%0,%3\n"
			"vavgub 6,6,8\n"
			"vavgub 6,6,10\n"
			"stvx 6,%0,%3\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (ref_block + field_stride),
		      "b" (i0), "b" (i1));
		i0 += frame_stride, i1 += frame_stride;
	}
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
	int i;
	int i0 = 0, i1 = 16;

	ASSERT(((int)curr_block & 15) == 0);

	asm(""
		"vspltisb 0,1\n"
		"lvsl 2,%0,%1\n"
		"vaddubs 3,2,0\n"
	"" : : "b" (ref_block), "b" (i0), "b" (i1));
	for (i = 0; i < 16; i++) {
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"vperm 4,0,1,2\n"
			"vperm 5,0,1,3\n"
			"vavgub 4,4,5\n"
			"stvx 4,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i1));
		i0 += frame_stride, i1 += frame_stride;
	}
}

void 
mlib_VideoInterpX_U8_U8_16x8(uint8_t *curr_block,
			     const uint8_t *ref_block,
			     int32_t frame_stride,
			     int32_t field_stride)
{
	int i;
	int i0 = 0, i1 = 16;

	ASSERT(((int)curr_block & 15) == 0);

	asm(""
		"vspltisb 0,1\n"
		"lvsl 2,%0,%1\n"
		"vaddubs 3,2,0\n"
	"" : : "b" (ref_block), "b" (i0), "b" (i1));
	for (i = 0; i < 8; i++) {
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"vperm 4,0,1,2\n"
			"vperm 5,0,1,3\n"
			"vavgub 4,4,5\n"
			"stvx 4,%0,%2\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i1));
		i0 += frame_stride, i1 += frame_stride;
	}
}

void 
mlib_VideoInterpX_U8_U8_8x8(uint8_t *curr_block,
			    const uint8_t *ref_block,
			    int32_t frame_stride,
			    int32_t field_stride)
{
	int i;
	const int i0 = 0, i1 = 16, i2 = 4;

	ASSERT(((int)curr_block & 7) == 0);

	asm(""
		"vspltisb 0,1\n"
		"lvsl 2,%1,%2\n"
		"lvsr 3,%0,%2\n"
		"lvsl 4,%1,%3\n"
		"lvsr 5,%0,%3\n"
		"vperm 2,2,2,3\n"
		"vperm 4,4,4,5\n"
		"vaddubs 3,2,0\n"
		"vaddubs 5,4,0\n"
	"" : : "b" (curr_block), "b" (ref_block),
	      "b" (i0), "b" (i0 + frame_stride));
	for (i = 0; i < 4; i++) {
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"vperm 6,0,1,2\n"
			"vperm 7,0,1,3\n"
			"vavgub 6,6,7\n"
			"stvewx 6,%0,%2\n"
			"stvewx 6,%0,%4\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i1), "b" (i2));
		curr_block += frame_stride, ref_block += frame_stride;
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"vperm 6,0,1,4\n"
			"vperm 7,0,1,5\n"
			"vavgub 6,6,7\n"
			"stvewx 6,%0,%2\n"
			"stvewx 6,%0,%4\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i1), "b" (i2));
		curr_block += frame_stride, ref_block += frame_stride;
	}
}

void 
mlib_VideoInterpX_U8_U8_8x4(uint8_t *curr_block,
			    const uint8_t *ref_block,
			    int32_t frame_stride,
			    int32_t field_stride)
{
	int i;
	const int i0 = 0, i1 = 16, i2 = 4;

	ASSERT(((int)curr_block & 7) == 0);

	asm(""
		"vspltisb 0,1\n"
		"lvsl 2,%1,%2\n"
		"lvsr 3,%0,%2\n"
		"lvsl 4,%1,%3\n"
		"lvsr 5,%0,%3\n"
		"vperm 2,2,2,3\n"
		"vperm 4,4,4,5\n"
		"vaddubs 3,2,0\n"
		"vaddubs 5,4,0\n"
	"" : : "b" (curr_block), "b" (ref_block),
	      "b" (i0), "b" (i0 + frame_stride));
	for (i = 0; i < 2; i++) {
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"vperm 6,0,1,2\n"
			"vperm 7,0,1,3\n"
			"vavgub 6,6,7\n"
			"stvewx 6,%0,%2\n"
			"stvewx 6,%0,%4\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i1), "b" (i2));
		curr_block += frame_stride, ref_block += frame_stride;
		asm(""
			"lvx 0,%1,%2\n"
			"lvx 1,%1,%3\n"
			"vperm 6,0,1,4\n"
			"vperm 7,0,1,5\n"
			"vavgub 6,6,7\n"
			"stvewx 6,%0,%2\n"
			"stvewx 6,%0,%4\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (i0), "b" (i1), "b" (i2));
		curr_block += frame_stride, ref_block += frame_stride;
	}
}

void
mlib_VideoInterpY_U8_U8_16x16(uint8_t *curr_block,
			      const uint8_t *ref_block,
			      int32_t frame_stride,
			      int32_t field_stride)
{
	int i;

	ASSERT(((int)curr_block & 15) == 0);

	if (((int)ref_block & 15) != 0) {
		int i0 = 0, i1 = 16;
		asm(""
			"lvsl 4,%0,%1\n"
		"" : : "b" (ref_block), "b" (i0));
		for (i = 0; i < 16; i++) {
			asm(""
				"lvx 0,%1,%3\n"
				"lvx 1,%1,%4\n"
				"lvx 2,%2,%3\n"
				"lvx 3,%2,%4\n"
				"vperm 5,0,1,4\n"
				"vperm 6,2,3,4\n"
				"vavgub 5,5,6\n"
				"stvx 5,%0,%3\n"
			"" : : "b" (curr_block), "b" (ref_block),
			      "b" (ref_block + field_stride),
			      "b" (i0), "b" (i1));
			i0 += frame_stride, i1 += frame_stride;
		}
	} else {
		int i0 = 0;
		for (i = 0; i < 16; i++) {
			asm(""
				"lvx 0,%1,%3\n"
				"lvx 1,%2,%3\n"
				"vavgub 0,0,1\n"
				"stvx 0,%0,%3\n"
			"" : : "b" (curr_block), "b" (ref_block),
			      "b" (ref_block + field_stride),
			      "b" (i0));
			i0 += frame_stride;
		}
	}
}

void 
mlib_VideoInterpY_U8_U8_16x8(uint8_t *curr_block,
			     const uint8_t *ref_block,
			     int32_t frame_stride,
			     int32_t field_stride)
{
	int i;

	ASSERT(((int)curr_block & 15) == 0);

	if (((int)ref_block & 15) != 0) {
		int i0 = 0, i1 = 16;
		asm(""
			"lvsl 4,%0,%1\n"
		"" : : "b" (ref_block), "b" (i0));
		for (i = 0; i < 8; i++) {
			asm(""
				"lvx 0,%1,%3\n"
				"lvx 1,%1,%4\n"
				"lvx 2,%2,%3\n"
				"lvx 3,%2,%4\n"
				"vperm 5,0,1,4\n"
				"vperm 6,2,3,4\n"
				"vavgub 5,5,6\n"
				"stvx 5,%0,%3\n"
			"" : : "b" (curr_block), "b" (ref_block),
			      "b" (ref_block + field_stride),
			      "b" (i0), "b" (i1));
			i0 += frame_stride, i1 += frame_stride;
		}
	} else {
		int i0 = 0;
		for (i = 0; i < 8; i++) {
			asm(""
				"lvx 0,%1,%3\n"
				"lvx 1,%2,%3\n"
				"vavgub 0,0,1\n"
				"stvx 0,%0,%3\n"
			"" : : "b" (curr_block), "b" (ref_block),
			      "b" (ref_block + field_stride),
			      "b" (i0));
			i0 += frame_stride;
		}
	}
}

void 
mlib_VideoInterpY_U8_U8_8x8(uint8_t *curr_block,
			    const uint8_t *ref_block,
			    int32_t frame_stride,
			    int32_t field_stride)
{
	int i;

	ASSERT(((int)curr_block & 7) == 0);

	if (((((int)ref_block ^ (int)curr_block) | field_stride) & 15) != 0) {
		const int i0 = 0, i1 = 16, i2 = 4;
		asm(""
			"lvsl 4,%1,%3\n"
			"lvsl 5,%1,%4\n"
			"lvsl 6,%2,%3\n"
			"lvsl 7,%2,%4\n"
			"lvsr 8,%0,%3\n"
			"lvsr 9,%0,%4\n"
			"vperm 4,4,4,8\n"
			"vperm 5,5,5,9\n"
			"vperm 6,6,6,8\n"
			"vperm 7,7,7,9\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (ref_block + field_stride),
		      "b" (i0), "b" (i0 + frame_stride));
		for (i = 0; i < 4; i++) {
			asm(""
				"lvx 0,%1,%3\n"
				"lvx 1,%1,%4\n"
				"lvx 2,%2,%3\n"
				"lvx 3,%2,%4\n"
				"vperm 8,0,1,4\n"
				"vperm 9,2,3,6\n"
				"vavgub 8,8,9\n"
				"stvewx 8,%0,%3\n"
				"stvewx 8,%0,%5\n"
			"" : : "b" (curr_block), "b" (ref_block),
			      "b" (ref_block + field_stride),
			      "b" (i0), "b" (i1), "b" (i2));
			curr_block += frame_stride, ref_block += frame_stride;
			asm(""
				"lvx 0,%1,%3\n"
				"lvx 1,%1,%4\n"
				"lvx 2,%2,%3\n"
				"lvx 3,%2,%4\n"
				"vperm 8,0,1,5\n"
				"vperm 9,2,3,7\n"
				"vavgub 8,8,9\n"
				"stvewx 8,%0,%3\n"
				"stvewx 8,%0,%5\n"
			"" : : "b" (curr_block), "b" (ref_block),
			      "b" (ref_block + field_stride),
			      "b" (i0), "b" (i1), "b" (i2));
			curr_block += frame_stride, ref_block += frame_stride;
		}
	} else {
		int i0 = 0, i1 = 4;
		for (i = 0; i < 8; i++) {
			asm(""
				"lvx 0,%1,%3\n"
				"lvx 1,%2,%3\n"
				"vavgub 0,0,1\n"
				"stvewx 0,%0,%3\n"
				"stvewx 0,%0,%4\n"
			"" : : "b" (curr_block), "b" (ref_block),
			      "b" (ref_block + field_stride),
			      "b" (i0), "b" (i1));
			i0 += frame_stride, i1 += frame_stride;
		}
	}
}

void 
mlib_VideoInterpY_U8_U8_8x4(uint8_t *curr_block,
			    const uint8_t *ref_block,
			    int32_t frame_stride,
			    int32_t field_stride)
{
	int i;

	ASSERT(((int)curr_block & 7) == 0);

	if (((((int)ref_block ^ (int)curr_block) | field_stride) & 15) != 0) {
		const int i0 = 0, i1 = 16, i2 = 4;
		asm(""
			"lvsl 4,%1,%3\n"
			"lvsl 5,%1,%4\n"
			"lvsl 6,%2,%3\n"
			"lvsl 7,%2,%4\n"
			"lvsr 8,%0,%3\n"
			"lvsr 9,%0,%4\n"
			"vperm 4,4,4,8\n"
			"vperm 5,5,5,9\n"
			"vperm 6,6,6,8\n"
			"vperm 7,7,7,9\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (ref_block + field_stride),
		      "b" (i0), "b" (i0 + frame_stride));
		for (i = 0; i < 2; i++) {
			asm(""
				"lvx 0,%1,%3\n"
				"lvx 1,%1,%4\n"
				"lvx 2,%2,%3\n"
				"lvx 3,%2,%4\n"
				"vperm 8,0,1,4\n"
				"vperm 9,2,3,6\n"
				"vavgub 8,8,9\n"
				"stvewx 8,%0,%3\n"
				"stvewx 8,%0,%5\n"
			"" : : "b" (curr_block), "b" (ref_block),
			      "b" (ref_block + field_stride),
			      "b" (i0), "b" (i1), "b" (i2));
			curr_block += frame_stride, ref_block += frame_stride;
			asm(""
				"lvx 0,%1,%3\n"
				"lvx 1,%1,%4\n"
				"lvx 2,%2,%3\n"
				"lvx 3,%2,%4\n"
				"vperm 8,0,1,5\n"
				"vperm 9,2,3,7\n"
				"vavgub 8,8,9\n"
				"stvewx 8,%0,%3\n"
				"stvewx 8,%0,%5\n"
			"" : : "b" (curr_block), "b" (ref_block),
			      "b" (ref_block + field_stride),
			      "b" (i0), "b" (i1), "b" (i2));
			curr_block += frame_stride, ref_block += frame_stride;
		}
	} else {
		int i0 = 0, i1 = 4;
		for (i = 0; i < 4; i++) {
			asm(""
				"lvx 0,%1,%3\n"
				"lvx 1,%2,%3\n"
				"vavgub 0,0,1\n"
				"stvewx 0,%0,%3\n"
				"stvewx 0,%0,%4\n"
			"" : : "b" (curr_block), "b" (ref_block),
			      "b" (ref_block + field_stride),
			      "b" (i0), "b" (i1));
			i0 += frame_stride, i1 += frame_stride;
		}
	}
}

void
mlib_VideoInterpXY_U8_U8_16x16(uint8_t *curr_block,
			       const uint8_t *ref_block,
			       int32_t frame_stride,
			       int32_t field_stride)
{
	int i;
	int i0 = 0, i1 = 16;

	ASSERT(((int)curr_block & 15) == 0);

	asm(""
		"vspltisb 0,1\n"
		"lvsl 4,%0,%1\n"
		"vaddubs 5,4,0\n"
	"" : : "b" (ref_block), "b" (i0));
	for (i = 0; i < 16; i++) {
		asm(""
			"lvx 0,%1,%3\n"
			"lvx 1,%2,%3\n"
			"lvx 2,%1,%4\n"
			"lvx 3,%2,%4\n"
			"vperm 6,0,2,4\n"
			"vperm 7,0,2,5\n"
			"vperm 8,1,3,4\n"
			"vperm 9,1,3,5\n"
			"vavgub 6,6,7\n"
			"vavgub 8,8,9\n"
			"vavgub 6,6,8\n"
			"stvx 6,%0,%3\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (ref_block + field_stride),
		      "b" (i0), "b" (i1));
		i0 += frame_stride, i1 += frame_stride;
	}
}

void 
mlib_VideoInterpXY_U8_U8_16x8(uint8_t *curr_block,
			      const uint8_t *ref_block,
			      int32_t frame_stride,
			      int32_t field_stride)
{
	int i;
	int i0 = 0, i1 = 16;

	ASSERT(((int)curr_block & 15) == 0);

	asm(""
		"vspltisb 0,1\n"
		"lvsl 4,%0,%1\n"
		"vaddubs 5,4,0\n"
	"" : : "b" (ref_block), "b" (i0));
	for (i = 0; i < 8; i++) {
		asm(""
			"lvx 0,%1,%3\n"
			"lvx 1,%2,%3\n"
			"lvx 2,%1,%4\n"
			"lvx 3,%2,%4\n"
			"vperm 6,0,2,4\n"
			"vperm 7,0,2,5\n"
			"vperm 8,1,3,4\n"
			"vperm 9,1,3,5\n"
			"vavgub 6,6,7\n"
			"vavgub 8,8,9\n"
			"vavgub 6,6,8\n"
			"stvx 6,%0,%3\n"
		"" : : "b" (curr_block), "b" (ref_block),
		      "b" (ref_block + field_stride),
		      "b" (i0), "b" (i1));
		i0 += frame_stride, i1 += frame_stride;
	}
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

void mlib_ClearCoeffs(int16_t *coeffs)
{
	asm(""
		"vspltish 0,0\n"
		"stvx 0,%0,%1\n"
		"addi %1,%1,32\n"
		"stvx 0,%0,%2\n"
		"addi %2,%2,32\n"
		"stvx 0,%0,%1\n"
		"addi %1,%1,32\n"
		"stvx 0,%0,%2\n"
		"addi %2,%2,32\n"
		"stvx 0,%0,%1\n"
		"addi %1,%1,32\n"
		"stvx 0,%0,%2\n"
		"addi %2,%2,32\n"
		"stvx 0,%0,%1\n"
		"stvx 0,%0,%2\n"
	"" : : "b" (coeffs), "b" (0), "b" (16));
}


/***************************************************************
 *
 * Copyright:   (c) Copyright Motorola Inc. 1998
 *
 * Date:        April 17, 1998
 *
 * Function:    IDCT
 *
 * Description: Scaled Chen (III) algorithm for IDCT
 *              Arithmetic is 16-bit fixed point.
 *
 * Inputs:      input - Pointer to input data (short), which
 *                      must be between -2048 to +2047.
 *                      It is assumed that the allocated array
 *                      has been 128-bit aligned and contains
 *                      8x8 short elements.
 *
 * Outputs:     output - Pointer to output area for the transfored
 *                       data. The output values are between -255
 *                       and 255 . It is assumed that a 128-bit
 *                       aligned 8x8 array of short has been
 *                       pre-allocated.
 *
 * Return:      None
 *
 ***************************************************************/

static const int16_t SpecialConstants[8] __attribute__ ((aligned (16))) = {
	23170, 13573, 6518, 21895, -23170, -21895, 32, 0 };

static const int16_t PreScale[64] __attribute__ ((aligned (16))) = {
	16384, 22725, 21407, 19266, 16384, 19266, 21407, 22725, 
	22725, 31521, 29692, 26722, 22725, 26722, 29692, 31521, 
	21407, 29692, 27969, 25172, 21407, 25172, 27969, 29692, 
	19266, 26722, 25172, 22654, 19266, 22654, 25172, 26722, 
};

void mlib_VideoIDCTAdd_U8_S16(uint8_t *output, const int16_t *input, int32_t stride) 
{
	ASSERT(((int)output & 7) == 0);

	/* Load constants, input data, and prescale factors.  Do prescaling. */
	asm(""
		"vspltish        31,0\n"
		"lvx		24,0,%1\n"
		"vspltish	23,4\n"
\
		"addi		5,0,0\n"
		"vsplth		29,24,4\n"
		"lvx		0,%0,5\n"
		"addi		6,0,16\n"
		"vsplth		28,24,3\n"
		"lvx		0+16,%2,5\n"
		"addi		7,0,32\n"
		"vsplth		27,24,2\n"
		"lvx		1,%0,6\n"
		"addi		8,0,48\n"
		"vsplth		26,24,1\n"
		"lvx		1+16,%2,6\n"
		"addi		5,0,64\n"
		"vsplth		25,24,0\n"
		"lvx		2,%0,7\n"
		"addi		6,0,80\n"
		"vslh		0,0,23\n"
		"lvx		2+16,%2,7\n"
		"addi		7,0,96\n"
		"vslh		1,1,23\n"
		"lvx		3,%0,8\n"
		"vslh		2,2,23\n"
		"lvx		3+16,%2,8\n"
		"addi		8,0,112\n"
		"vslh		3,3,23\n"
		"lvx		4,%0,5\n"
		"vsplth		30,24,5\n"
		"lvx		5,%0,6\n"
		"vsplth		24,24,6\n"
		"lvx		6,%0,7\n"
		"vslh		4,4,23\n"
		"lvx		7,%0,8\n"
		"vslh		5,5,23\n"
		"vmhraddshs	0,0,0+16,31\n"
		"vslh		6,6,23\n"
		"vmhraddshs	4,4,0+16,31\n"
		"vslh		7,7,23\n"
	"" : : "b" (input), "b" (SpecialConstants), "b" (PreScale)
	  : "cc", "r5", "r6", "r7", "r8", "memory");

	asm(""
		"vmhraddshs	1,1,1+16,31\n"
		"vmhraddshs	5,5,3+16,31\n"
		"vmhraddshs	2,2,2+16,31\n"
		"vmhraddshs	6,6,2+16,31\n"
		"vmhraddshs	3,3,3+16,31\n"
		"vmhraddshs	7,7,1+16,31\n"
\
\
		"vmhraddshs      11,27,7,1\n"
		"vmhraddshs      19,27,1,31\n"
		"vmhraddshs      12,26,6,2\n"
		"vmhraddshs      13,30,3,5\n"
		"vmhraddshs      17,28,5,3\n"
		"vsubshs		18,19,7\n"
	"");

	/* Second stage. */
	asm(""
		"vmhraddshs      19,26,2,31\n"
		"vaddshs		15,0,4\n"
		"vsubshs		10,0,4\n"
		"vsubshs		14,19,6\n"
		"vaddshs		16,18,13\n"
		"vsubshs		13,18,13\n"
		"vsubshs		18,11,17\n"
		"vaddshs		11,11,17\n"
	"");

	/* Third stage. */
	asm(""
		"vaddshs		17,15,12\n"
		"vsubshs		12,15,12\n"
		"vaddshs		15,10,14\n"
		"vsubshs		10,10,14\n"
		"vsubshs		14,18,13\n"
		"vaddshs		13,18,13\n"
	"");

	/* Fourth stage. */
	asm(""
		"vmhraddshs      2,25,14,10\n"
		"vsubshs		4,12,16\n"
		"vmhraddshs      1,25,13,15\n"
		"vaddshs		0,17,11\n"
		"vmhraddshs      5,29,14,10\n"
		"vmrghh  0+8,0,4\n"
		"vaddshs		3,12,16\n"
		"vmrglh  1+8,0,4\n"
		"vmhraddshs      6,29,13,15\n"
		"vmrghh  2+8,1,5\n"
		"vsubshs		7,17,11\n"
	"");

	/* Transpose the matrix again. */
	asm(""
		"vmrglh  3+8,1,5\n"
		"vmrghh  4+8,2,6\n"
		"vmrglh  5+8,2,6\n"
		"vmrghh  6+8,3,7\n"
		"vmrglh  7+8,3,7\n"
\
		"vmrghh  0+16,0+8,4+8\n"
		"vmrglh  1+16,0+8,4+8\n"
		"vmrghh  2+16,1+8,5+8\n"
		"vmrglh  3+16,1+8,5+8\n"
		"vmrghh  4+16,2+8,6+8\n"
		"vmrglh  5+16,2+8,6+8\n"
		"vmrghh  6+16,3+8,7+8\n"
		"vmrglh  7+16,3+8,7+8\n"
\
		"vmrglh  1,0+16,4+16\n"
		"vmrglh  7,3+16,7+16\n"
		"vmrglh  3,1+16,5+16\n"
		"vmrghh  2,1+16,5+16\n"
		"vmhraddshs      11,27,7,1\n"
		"vmrghh  6,3+16,7+16\n"
		"vmhraddshs      19,27,1,31\n"
		"vmrglh  5,2+16,6+16\n"
		"vmhraddshs      12,26,6,2\n"
		"vmrghh  0,0+16,4+16\n"
		"vmhraddshs      13,30,3,5\n"
		"vmrghh  4,2+16,6+16\n"
		"vmhraddshs      17,28,5,3\n"
		"vsubshs		18,19,7\n"
	"");

	/* Add a rounding bias for the final shift.  v0 is added into every
	   vector, so the bias propagates from here. */
	asm(""
		"vaddshs	0,0,24\n"
	"");

	/* Second stage. */
	asm(""
		"vmhraddshs      19,26,2,31\n"
		"vaddshs		15,0,4\n"
		"vsubshs		10,0,4\n"
		"vsubshs		14,19,6\n"
		"vaddshs		16,18,13\n"
		"vsubshs		13,18,13\n"
		"vsubshs		18,11,17\n"
		"vaddshs		11,11,17\n"
	"");

	/* Third stage. */
	asm(""
		"vaddshs		17,15,12\n"
		"vsubshs		12,15,12\n"
		"vaddshs		15,10,14\n"
		"vsubshs		10,10,14\n"
		"vsubshs		14,18,13\n"
		"vaddshs		13,18,13\n"
	"");

	/* Fourth stage. */
	asm(""
		"vmhraddshs	2,25,14,10\n"
		"vsubshs		4,12,16\n"
		"vmhraddshs	1,25,13,15\n"
		"vaddshs		0,17,11\n"
		"vmhraddshs	5,29,14,10\n"
		"vaddshs		3,12,16\n"
		"vspltish	23,6\n"
		"vmhraddshs	6,29,13,15\n"
		"vsubshs		7,17,11\n"
	"");

	/* Load and permutations for the reference data we're adding to. */
	asm(""
		"lvsl		17,%0,%1\n"
		"vspltisb	19,-1\n"
		"lvsl		18,%0,%2\n"
		"vmrghb		17,19,17\n"
		"vmrghb		18,19,18\n"
	"" : : "b" (output), "b" (0), "b" (0+stride));

	/* Copy out each 8 values, adding to the existing frame. */
	asm(""
		"vsrah		0,0,23\n"
		"lvx		0+8,%0,%1\n"
		"vsrah		1,1,23\n"
		"lvx		1+8,%0,%3\n"
		"vperm		0+8,0+8,31,17\n"
		"vperm		1+8,1+8,31,18\n"
		"vaddshs		0,0,0+8\n"
		"vaddshs		1,1,1+8\n"
		"vmaxsh		0,0,31\n"
		"vmaxsh		1,1,31\n"
		"vpkuhus		0,0,0\n"
		"stvewx		0,%0,%1\n"
		"vpkuhus		1,1,1\n"
		"stvewx		0,%0,%2\n"
		"stvewx		1,%0,%3\n"
		"stvewx		1,%0,%4\n"
	"" : : "b" (output), "b" (0), "b" (4), "b" (0+stride), "b" (4+stride));
	output += stride<<1;
	asm(""
		"vsrah		2,2,23\n"
		"lvx		2+8,%0,%1\n"
		"vsrah		3,3,23\n"
		"lvx		3+8,%0,%3\n"
		"vperm		2+8,2+8,31,17\n"
		"vperm		3+8,3+8,31,18\n"
		"vaddshs		2,2,2+8\n"
		"vaddshs		3,3,3+8\n"
		"vmaxsh		2,2,31\n"
		"vmaxsh		3,3,31\n"
		"vpkuhus		2,2,2\n"
		"stvewx		2,%0,%1\n"
		"vpkuhus		3,3,3\n"
		"stvewx		2,%0,%2\n"
		"stvewx		3,%0,%3\n"
		"stvewx		3,%0,%4\n"
	"" : : "b" (output), "b" (0), "b" (4), "b" (0+stride), "b" (4+stride));
	output += stride<<1;
	asm(""
		"vsrah		4,4,23\n"
		"lvx		4+8,%0,%1\n"
		"vsrah		5,5,23\n"
		"lvx		5+8,%0,%3\n"
		"vperm		4+8,4+8,31,17\n"
		"vperm		5+8,5+8,31,18\n"
		"vaddshs		4,4,4+8\n"
		"vaddshs		5,5,5+8\n"
		"vmaxsh		4,4,31\n"
		"vmaxsh		5,5,31\n"
		"vpkuhus		4,4,4\n"
		"stvewx		4,%0,%1\n"
		"vpkuhus		5,5,5\n"
		"stvewx		4,%0,%2\n"
		"stvewx		5,%0,%3\n"
		"stvewx		5,%0,%4\n"
	"" : : "b" (output), "b" (0), "b" (4), "b" (0+stride), "b" (4+stride));
	output += stride<<1;
	asm(""
		"vsrah		6,6,23\n"
		"lvx		6+8,%0,%1\n"
		"vsrah		7,7,23\n"
		"lvx		7+8,%0,%3\n"
		"vperm		6+8,6+8,31,17\n"
		"vperm		7+8,7+8,31,18\n"
		"vaddshs		6,6,6+8\n"
		"vaddshs		7,7,7+8\n"
		"vmaxsh		6,6,31\n"
		"vmaxsh		7,7,31\n"
		"vpkuhus		6,6,6\n"
		"stvewx		6,%0,%1\n"
		"vpkuhus		7,7,7\n"
		"stvewx		6,%0,%2\n"
		"stvewx		7,%0,%3\n"
		"stvewx		7,%0,%4\n"
	"" : : "b" (output), "b" (0), "b" (4), "b" (0+stride), "b" (4+stride));
}  

void mlib_VideoIDCT8x8_U8_S16(uint8_t *output, const int16_t *input, int32_t stride)
{
	ASSERT(((int)output & 7) == 0);

	/* Load constants, input data, and prescale factors.  Do prescaling. */
	asm(""
		"vspltish        31,0\n"
		"lvx		24,0,%1\n"
		"vspltish	23,4\n"
\
		"addi		5,0,0\n"
		"vsplth		29,24,4\n"
		"lvx		0,%0,5\n"
		"addi		6,0,16\n"
		"vsplth		28,24,3\n"
		"lvx		0+16,%2,5\n"
		"addi		7,0,32\n"
		"vsplth		27,24,2\n"
		"lvx		1,%0,6\n"
		"addi		8,0,48\n"
		"vsplth		26,24,1\n"
		"lvx		1+16,%2,6\n"
		"addi		5,0,64\n"
		"vsplth		25,24,0\n"
		"lvx		2,%0,7\n"
		"addi		6,0,80\n"
		"vslh		0,0,23\n"
		"lvx		2+16,%2,7\n"
		"addi		7,0,96\n"
		"vslh		1,1,23\n"
		"lvx		3,%0,8\n"
		"vslh		2,2,23\n"
		"lvx		3+16,%2,8\n"
		"addi		8,0,112\n"
		"vslh		3,3,23\n"
		"lvx		4,%0,5\n"
		"vsplth		30,24,5\n"
		"lvx		5,%0,6\n"
		"vsplth		24,24,6\n"
		"lvx		6,%0,7\n"
		"vslh		4,4,23\n"
		"lvx		7,%0,8\n"
		"vslh		5,5,23\n"
		"vmhraddshs	0,0,0+16,31\n"
		"vslh		6,6,23\n"
		"vmhraddshs	4,4,0+16,31\n"
		"vslh		7,7,23\n"
	"" : : "b" (input), "b" (SpecialConstants), "b" (PreScale)
	  : "cc", "r5", "r6", "r7", "r8", "memory");

	asm(""
		"vmhraddshs	1,1,1+16,31\n"
		"vmhraddshs	5,5,3+16,31\n"
		"vmhraddshs	2,2,2+16,31\n"
		"vmhraddshs	6,6,2+16,31\n"
		"vmhraddshs	3,3,3+16,31\n"
		"vmhraddshs	7,7,1+16,31\n"
\
		"vmhraddshs      11,27,7,1\n"
		"vmhraddshs      19,27,1,31\n"
		"vmhraddshs      12,26,6,2\n"
		"vmhraddshs      13,30,3,5\n"
		"vmhraddshs      17,28,5,3\n"
		"vsubshs		18,19,7\n"
	"");

	/* Second stage. */
	asm(""
		"vmhraddshs      19,26,2,31\n"
		"vaddshs		15,0,4\n"
		"vsubshs		10,0,4\n"
		"vsubshs		14,19,6\n"
		"vaddshs		16,18,13\n"
		"vsubshs		13,18,13\n"
		"vsubshs		18,11,17\n"
		"vaddshs		11,11,17\n"
	"");

	/* Third stage. */
	asm(""
		"vaddshs		17,15,12\n"
		"vsubshs		12,15,12\n"
		"vaddshs		15,10,14\n"
		"vsubshs		10,10,14\n"
		"vsubshs		14,18,13\n"
		"vaddshs		13,18,13\n"
	"");

	/* Fourth stage. */
	asm(""
		"vmhraddshs      2,25,14,10\n"
		"vsubshs		4,12,16\n"
		"vmhraddshs      1,25,13,15\n"
		"vaddshs		0,17,11\n"
		"vmhraddshs      5,29,14,10\n"
		"vmrghh  0+8,0,4\n"
		"vaddshs		3,12,16\n"
		"vmrglh  1+8,0,4\n"
		"vmhraddshs      6,29,13,15\n"
		"vmrghh  2+8,1,5\n"
		"vsubshs		7,17,11\n"
	"");

	/* Transpose the matrix again. */
	asm(""
		"vmrglh  3+8,1,5\n"
		"vmrghh  4+8,2,6\n"
		"vmrglh  5+8,2,6\n"
		"vmrghh  6+8,3,7\n"
		"vmrglh  7+8,3,7\n"
\
		"vmrghh  0+16,0+8,4+8\n"
		"vmrglh  1+16,0+8,4+8\n"
		"vmrghh  2+16,1+8,5+8\n"
		"vmrglh  3+16,1+8,5+8\n"
		"vmrghh  4+16,2+8,6+8\n"
		"vmrglh  5+16,2+8,6+8\n"
		"vmrghh  6+16,3+8,7+8\n"
		"vmrglh  7+16,3+8,7+8\n"
\
		"vmrglh  1,0+16,4+16\n"
		"vmrglh  7,3+16,7+16\n"
		"vmrglh  3,1+16,5+16\n"
		"vmrghh  2,1+16,5+16\n"
		"vmhraddshs      11,27,7,1\n"
		"vmrghh  6,3+16,7+16\n"
		"vmhraddshs      19,27,1,31\n"
		"vmrglh  5,2+16,6+16\n"
		"vmhraddshs      12,26,6,2\n"
		"vmrghh  0,0+16,4+16\n"
		"vmhraddshs      13,30,3,5\n"
		"vmrghh  4,2+16,6+16\n"
		"vmhraddshs      17,28,5,3\n"
		"vsubshs		18,19,7\n"
	"");

	/* Add a rounding bias for the final shift.  v0 is added into every
	   vector, so the bias propagates from here. */
	asm(""
		"vaddshs	0,0,24\n"
	"");

	/* Second stage. */
	asm(""
		"vmhraddshs      19,26,2,31\n"
		"vaddshs		15,0,4\n"
		"vsubshs		10,0,4\n"
		"vsubshs		14,19,6\n"
		"vaddshs		16,18,13\n"
		"vsubshs		13,18,13\n"
		"vsubshs		18,11,17\n"
		"vaddshs		11,11,17\n"
	"");

	/* Third stage. */
	asm(""
		"vaddshs		17,15,12\n"
		"vsubshs		12,15,12\n"
		"vaddshs		15,10,14\n"
		"vsubshs		10,10,14\n"
		"vsubshs		14,18,13\n"
		"vaddshs		13,18,13\n"
	"");

	/* Fourth stage. */
	asm(""
		"vmhraddshs	2,25,14,10\n"
		"vsubshs		4,12,16\n"
		"vmhraddshs	1,25,13,15\n"
		"vaddshs		0,17,11\n"
		"vmhraddshs	5,29,14,10\n"
		"vaddshs		3,12,16\n"
		"vspltish	23,6\n"
		"vmhraddshs	6,29,13,15\n"
		"vsubshs		7,17,11\n"
	"");

	/* Copy out each 8 values. */
	asm(""
		"vmaxsh		0,0,31\n"
		"vmaxsh		1,1,31\n"
		"vsrah		0,0,23\n"
		"vsrah		1,1,23\n"
		"vpkuhus		0,0,0\n"
		"vmaxsh		2,2,31\n"
		"stvewx		0,%0,%1\n"
		"vpkuhus		1,1,1\n"
		"vmaxsh		3,3,31\n"
		"stvewx		0,%0,%2\n"
		"stvewx		1,%0,%3\n"
		"stvewx		1,%0,%4\n"
	"" : : "b" (output), "b" (0), "b" (4), "b" (0+stride), "b" (4+stride));
	output += stride<<1;
	asm(""
		"vsrah		2,2,23\n"
		"vsrah		3,3,23\n"
		"vpkuhus		2,2,2\n"
		"vmaxsh		4,4,31\n"
		"stvewx		2,%0,%1\n"
		"vpkuhus		3,3,3\n"
		"vmaxsh		5,5,31\n"
		"stvewx		2,%0,%2\n"
		"stvewx		3,%0,%3\n"
		"stvewx		3,%0,%4\n"
	"" : : "b" (output), "b" (0), "b" (4), "b" (0+stride), "b" (4+stride));
	output += stride<<1;
	asm(""
		"vsrah		4,4,23\n"
		"vsrah		5,5,23\n"
		"vpkuhus		4,4,4\n"
		"vmaxsh		6,6,31\n"
		"stvewx		4,%0,%1\n"
		"vpkuhus		5,5,5\n"
		"vmaxsh		7,7,31\n"
		"stvewx		4,%0,%2\n"
		"stvewx		5,%0,%3\n"
		"stvewx		5,%0,%4\n"
	"" : : "b" (output), "b" (0), "b" (4), "b" (0+stride), "b" (4+stride));
	output += stride<<1;
	asm(""
		"vsrah		6,6,23\n"
		"vsrah		7,7,23\n"
		"vpkuhus		6,6,6\n"
		"stvewx		6,%0,%1\n"
		"vpkuhus		7,7,7\n"
		"stvewx		6,%0,%2\n"
		"stvewx		7,%0,%3\n"
		"stvewx		7,%0,%4\n"
	"" : : "b" (output), "b" (0), "b" (4), "b" (0+stride), "b" (4+stride));
}
