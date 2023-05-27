#ifndef C_MLIB_H_INCLUDED
#define C_MLIB_H_INCLUDED

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

void
mlib_Init(void);

void
mlib_VideoCopyRef_U8_U8 (uint8_t *curr_block,
                         const uint8_t *ref_block,
                         int32_t width, int32_t height,
                         int32_t stride);
void
mlib_VideoCopyRefAve_U8_U8_16x16(uint8_t *curr_block,
                                 const uint8_t *ref_block,
                                 int32_t stride);
void 
mlib_VideoCopyRefAve_U8_U8_16x8(uint8_t *curr_block,
				const uint8_t *ref_block,
				int32_t stride);
void 
mlib_VideoCopyRefAve_U8_U8_8x8(uint8_t *curr_block,
			       const uint8_t *ref_block,
			       int32_t stride);
void 
mlib_VideoCopyRefAve_U8_U8_8x4(uint8_t *curr_block,
			       const uint8_t *ref_block,
			       int32_t stride);
void
mlib_VideoCopyRef_U8_U8_16x16(uint8_t *curr_block,
			      const uint8_t *ref_block,
			      int32_t stride);
void 
mlib_VideoCopyRef_U8_U8_16x16_multiple(uint8_t *curr_block,
				       const uint8_t *ref_block,
				       int32_t stride,
				       int32_t count);
void 
mlib_VideoCopyRef_U8_U8_16x8(uint8_t *curr_block,
			     const uint8_t *ref_block,
			     int32_t stride);
void
mlib_VideoCopyRef_U8_U8_8x8(uint8_t *curr_block,
			    const uint8_t *ref_block,
			    int32_t stride);
void
mlib_VideoCopyRef_U8_U8_8x8_multiple(uint8_t *curr_block,
				     const uint8_t *ref_block,
				     int32_t stride,
				     int32_t count);
void 
mlib_VideoCopyRef_U8_U8_8x4(uint8_t *curr_block,
			    const uint8_t *ref_block,
			    int32_t stride);
void
mlib_VideoInterpAveX_U8_U8_16x16(uint8_t *curr_block,
                                 const uint8_t *ref_block,
                                 int32_t frame_stride,
                                 int32_t field_stride);
void 
mlib_VideoInterpAveX_U8_U8_16x8(uint8_t *curr_block,
				const uint8_t *ref_block,
				int32_t frame_stride,
				int32_t field_stride);
void 
mlib_VideoInterpAveX_U8_U8_8x8(uint8_t *curr_block,
			       const uint8_t *ref_block,
			       int32_t frame_stride,
			       int32_t field_stride);
void 
mlib_VideoInterpAveX_U8_U8_8x4(uint8_t *curr_block,
			       const uint8_t *ref_block,
			       int32_t frame_stride,
			       int32_t field_stride);
void
mlib_VideoInterpAveY_U8_U8_16x16(uint8_t *curr_block,
                                 const uint8_t *ref_block,
                                 int32_t frame_stride,
                                 int32_t field_stride);
void 
mlib_VideoInterpAveY_U8_U8_16x8(uint8_t *curr_block,
				const uint8_t *ref_block,
				int32_t frame_stride,
				int32_t field_stride);
void 
mlib_VideoInterpAveY_U8_U8_8x8(uint8_t *curr_block,
			       const uint8_t *ref_block,
			       int32_t frame_stride,
			       int32_t field_stride);
void 
mlib_VideoInterpAveY_U8_U8_8x4(uint8_t *curr_block,
			       const uint8_t *ref_block,
			       int32_t frame_stride,
			       int32_t field_stride);
void
mlib_VideoInterpAveXY_U8_U8_16x16(uint8_t *curr_block,
				  const uint8_t *ref_block,
				  int32_t frame_stride,
				  int32_t field_stride);
void 
mlib_VideoInterpAveXY_U8_U8_16x8(uint8_t *curr_block,
                                 const uint8_t *ref_block,
                                 int32_t frame_stride,
                                 int32_t field_stride);
void 
mlib_VideoInterpAveXY_U8_U8_8x8(uint8_t *curr_block,
				const uint8_t *ref_block,
				int32_t frame_stride,
				int32_t field_stride);
void 
mlib_VideoInterpAveXY_U8_U8_8x4(uint8_t *curr_block,
				const uint8_t *ref_block,
				int32_t frame_stride,
				int32_t field_stride);
void
mlib_VideoInterpX_U8_U8_16x16(uint8_t *curr_block,
			      const uint8_t *ref_block,
			      int32_t frame_stride,
			      int32_t field_stride);
void 
mlib_VideoInterpX_U8_U8_16x8(uint8_t *curr_block,
			     const uint8_t *ref_block,
			     int32_t frame_stride,
			     int32_t field_stride);
void 
mlib_VideoInterpX_U8_U8_8x8(uint8_t *curr_block,
			    const uint8_t *ref_block,
			    int32_t frame_stride,
			    int32_t field_stride);
void 
mlib_VideoInterpX_U8_U8_8x4(uint8_t *curr_block,
			    const uint8_t *ref_block,
			    int32_t frame_stride,
			    int32_t field_stride);
void
mlib_VideoInterpY_U8_U8_16x16(uint8_t *curr_block,
			      const uint8_t *ref_block,
			      int32_t frame_stride,
			      int32_t field_stride);
void 
mlib_VideoInterpY_U8_U8_16x8(uint8_t *curr_block,
			     const uint8_t *ref_block,
			     int32_t frame_stride,
			     int32_t field_stride);
void 
mlib_VideoInterpY_U8_U8_8x8(uint8_t *curr_block,
			    const uint8_t *ref_block,
			    int32_t frame_stride,
			    int32_t field_stride);
void 
mlib_VideoInterpY_U8_U8_8x4(uint8_t *curr_block,
			    const uint8_t *ref_block,
			    int32_t frame_stride,
			    int32_t field_stride);
void
mlib_VideoInterpXY_U8_U8_16x16(uint8_t *curr_block,
			       const uint8_t *ref_block,
			       int32_t frame_stride,
			       int32_t field_stride);
void 
mlib_VideoInterpXY_U8_U8_16x8(uint8_t *curr_block,
			      const uint8_t *ref_block,
			      int32_t frame_stride,
			      int32_t field_stride);
void 
mlib_VideoInterpXY_U8_U8_8x8(uint8_t *curr_block,
			     const uint8_t *ref_block,
			     int32_t frame_stride,
			     int32_t field_stride);
void 
mlib_VideoInterpXY_U8_U8_8x4(uint8_t *curr_block,
			     const uint8_t *ref_block,
			     int32_t frame_stride,
			     int32_t field_stride);
void
mlib_VideoIDCTAdd_U8_S16(uint8_t *block,
			 const int16_t *coeffs,
			 int32_t stride);
void
mlib_VideoIDCT8x8_U8_S16(uint8_t *block,
			 const int16_t *coeffs,
			 int32_t stride);

#endif /* C_MLIB_H_INCLUDED */
