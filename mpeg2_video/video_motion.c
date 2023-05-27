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

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <inttypes.h>
#include <signal.h>

#include "debug_print.h"
#include "video_stream.h"
#include "video_types.h"
#include "video_tables.h" // only MACROBLOCK_MOTION_FORWARD/BACKWARD

#ifdef HAVE_MLIB
#include <mlib_types.h>
#include <mlib_status.h>
#include <mlib_sys.h>
#include <mlib_video.h>
#include <mlib_algebra.h>
#define mlib_VideoIDCTAdd_U8_S16(CURR_BLOCK, COEFFS, STRIDE) \
  { \
     mlib_VideoIDCT8x8_S16_S16(COEFFS, COEFFS); \
     mlib_VideoAddBlock_U8_S16(CURR_BLOCK, COEFFS, STRIDE); \
  }
#else
#ifdef HAVE_MMX
#include "mmx.h"
#include "mmx_mlib.h"
#else
#include "c_mlib.h"
#endif
#endif

#include "common.h"

extern yuv_image_t *dst_image;
extern yuv_image_t *fwd_ref_image;
extern yuv_image_t *bwd_ref_image;

#ifdef HAVE_MLIB
typedef mlib_status (*mc_function_t)(mlib_u8 *curr_block, mlib_u8 *ref_block, 
				     mlib_s32 frm_stride, mlib_s32 fld_stride);
#else
typedef void (*mc_function_t)(uint8_t *curr_block, const uint8_t *ref_block, 
			      int32_t frm_stride, int32_t fld_stride);
#endif

/* [replace/average] [size] [half-pell-mode] */
static const mc_function_t motion[2][4][4] = {
  { 
    { 
      (mc_function_t)mlib_VideoCopyRef_U8_U8_16x16,
      mlib_VideoInterpY_U8_U8_16x16,
      mlib_VideoInterpX_U8_U8_16x16,
      mlib_VideoInterpXY_U8_U8_16x16
    },
    {  	
      (mc_function_t)mlib_VideoCopyRef_U8_U8_16x8,
      mlib_VideoInterpY_U8_U8_16x8,
      mlib_VideoInterpX_U8_U8_16x8,
      mlib_VideoInterpXY_U8_U8_16x8
    },
    {
      (mc_function_t)mlib_VideoCopyRef_U8_U8_8x8,
      mlib_VideoInterpY_U8_U8_8x8,
      mlib_VideoInterpX_U8_U8_8x8,
      mlib_VideoInterpXY_U8_U8_8x8
    },
    {
      (mc_function_t)mlib_VideoCopyRef_U8_U8_8x4,
      mlib_VideoInterpY_U8_U8_8x4,
      mlib_VideoInterpX_U8_U8_8x4,
      mlib_VideoInterpXY_U8_U8_8x4
    }
  },
  {
    { 
      (mc_function_t)mlib_VideoCopyRefAve_U8_U8_16x16,
      mlib_VideoInterpAveY_U8_U8_16x16,
      mlib_VideoInterpAveX_U8_U8_16x16,
      mlib_VideoInterpAveXY_U8_U8_16x16
    },
    {  	
      (mc_function_t)mlib_VideoCopyRefAve_U8_U8_16x8,
      mlib_VideoInterpAveY_U8_U8_16x8,
      mlib_VideoInterpAveX_U8_U8_16x8,
      mlib_VideoInterpAveXY_U8_U8_16x8
    },
    {
      (mc_function_t)mlib_VideoCopyRefAve_U8_U8_8x8,
      mlib_VideoInterpAveY_U8_U8_8x8,
      mlib_VideoInterpAveX_U8_U8_8x8,
      mlib_VideoInterpAveXY_U8_U8_8x8
    },
    {
      (mc_function_t)mlib_VideoCopyRefAve_U8_U8_8x4,
      mlib_VideoInterpAveY_U8_U8_8x4,
      mlib_VideoInterpAveX_U8_U8_8x4,
      mlib_VideoInterpAveXY_U8_U8_8x4
    }
  }
};

/* This should be cleand up. */
void motion_comp()
{
  const unsigned int padded_width = seq.mb_width * 16;

  DPRINTF(5, "dct_type: %d\n", mb.modes.dct_type);
  
  if(mb.prediction_type == PRED_TYPE_DUAL_PRIME) {
    fprintf(stderr, "**** DP remove this when implemented\n");
    //exit(1);
  }
  if(mb.prediction_type == PRED_TYPE_16x8_MC) {
    fprintf(stderr, "**** 16x8 MC remove this? check if working first.\n");
    //exit(1);
  }

  
  
  if(mb.modes.macroblock_type & MACROBLOCK_MOTION_FORWARD) {
    uint8_t *dst_y, *dst_u;//, *dst_v;
    uint8_t *block_pred_y, *block_pred_u;//, *pred_v;
    
    DPRINTF(5, "forward_motion_comp\n");
    DPRINTF(5, "x: %d, y: %d\n", seq.mb_column, seq.mb_row);

    /* Image/Field select */
    /* FIXME: Should test for 'is second coded field' not bottom_field. */
    if(pic.coding_ext.picture_structure == PIC_STRUCT_BOTTOM_FIELD &&
       pic.header.picture_coding_type == PIC_CODING_TYPE_P &&
       ((mb.motion_vertical_field_select[0][0] == 0 /* TOP_FIELD */ &&
	 pic.coding_ext.picture_structure == PIC_STRUCT_BOTTOM_FIELD) ||
	(mb.motion_vertical_field_select[0][0] == 1 /* BOTTOM_FIELD */ &&
	 pic.coding_ext.picture_structure == PIC_STRUCT_TOP_FIELD))) {
      block_pred_y = dst_image->y;
      block_pred_u = dst_image->u;
      //pred_v = dst_image->v;
    } else {
      block_pred_y = fwd_ref_image->y;
      block_pred_u = fwd_ref_image->u;
      //pred_v = fwd_ref_image->v;
    }
    dst_y = dst_image->y;
    dst_u = dst_image->u;
    //dst_v = dst_image->v;
    
    /* Motion vector Origo (block->index) */
    /* Prediction destination (block->index) */
    if(pic.coding_ext.picture_structure == PIC_STRUCT_FRAME_PICTURE) {
      unsigned int x = seq.mb_column;
      unsigned int y = seq.mb_row;
      block_pred_y += x * 16 + y * 16 * padded_width;
      block_pred_u += x * 8 + y * 8 * padded_width/2;
      //pred_v += x * 8 + y * 8 * padded_width/2;
      dst_y += x * 16 + y * 16 * padded_width;
      dst_u += x * 8 + y * 8 * padded_width/2;
      //dst_v += x * 8 + y * 8 * padded_width/2;
    } else {
      unsigned int x = seq.mb_column;
      unsigned int y = seq.mb_row;
      block_pred_y += x * 16 + y * 16 * padded_width*2;
      block_pred_u += x * 8 + y * 8 * padded_width;	
      //pred_v += x * 8 + y * 8 * padded_width;	
      dst_y += x * 16 + y * 16 * padded_width*2;
      dst_u += x * 8 + y * 8 * padded_width;
      //dst_v += x * 8 + y * 8 * padded_width;
      
      /* Prediction destination (field) */
      if(pic.coding_ext.picture_structure == PIC_STRUCT_BOTTOM_FIELD) {
	dst_y += padded_width;
	dst_u += padded_width/2;
	//dst_v += padded_width/2;
      }
    }
    
       
    {
      uint8_t *pred_y, *pred_u;//, *pred_v;
      int pred_stride, stride;
      int half_flags_y, half_flags_uv;
      int half_height;
      int dd;
    
      /* Motion vector offset */
      pred_stride = padded_width;
      if(mb.mv_format == MV_FORMAT_FIELD)
	pred_stride = padded_width * 2;
      
      pred_y = block_pred_y + 
	(mb.vector[0][0][0] >> 1) + 
	(mb.vector[0][0][1] >> 1) * pred_stride;
      pred_u = block_pred_u + 
	((mb.vector[0][0][0]/2) >> 1) + 
	((mb.vector[0][0][1]/2) >> 1) * pred_stride/2;
      //pred_v +=  ((mb.vector[0][0][0]/2) >> 1) + 
      //((mb.vector[0][0][1]/2) >> 1) * pred_stride/2;
      
      half_flags_y = 
	((mb.vector[0][0][0] << 1) | (mb.vector[0][0][1] & 1)) & 3;
      half_flags_uv = 
	(((mb.vector[0][0][0]/2) << 1) | ((mb.vector[0][0][1]/2) & 1)) & 3;
	
      /* Field select */
      // This is wong for Dual_Prime...
      if(mb.mv_format == MV_FORMAT_FIELD) {
	if(mb.motion_vertical_field_select[0][0]) {
	  pred_y += padded_width;
	  pred_u += padded_width/2;
	  //pred_v += padded_width/2;
	}
      }
      
      if(mb.prediction_type == PRED_TYPE_FIELD_BASED)
	stride = padded_width * 2;
      else
	stride = padded_width;
      
      dd = fwd_ref_image->v - fwd_ref_image->u;
      half_height = (mb.motion_vector_count == 2) ? 1 :0; // ??

      motion[0][half_height][half_flags_y](dst_y, pred_y, stride, stride);
      motion[0][half_height+2][half_flags_uv](dst_u, pred_u, 
					      stride/2, stride/2);
      motion[0][half_height+2][half_flags_uv](dst_u+dd, pred_u+dd,
					      stride/2, stride/2);
    }
    // Only field_pred in frame_pictures have m.v.c. == 2 except for 
    // field_pictures with 16x8 MC
    if(mb.motion_vector_count == 2) {
      uint8_t *pred_y, *pred_u;//, *pred_v;
      int pred_stride, stride;
      int half_flags_y, half_flags_uv;
      int dd;
    
      /* Motion vector offset */
      pred_stride = padded_width * 2; 
      
      pred_y = block_pred_y + 
	(mb.vector[1][0][0] >> 1) + 
	(mb.vector[1][0][1] >> 1) * pred_stride;
      pred_u = block_pred_u + 
	((mb.vector[1][0][0]/2) >> 1) + 
	((mb.vector[1][0][1]/2) >> 1) * pred_stride/2;
      //pred_v +=  ((mb.vector[1][0][0]/2) >> 1) + 
      //((mb.vector[1][0][1]/2) >> 1) * pred_stride/2;
      
      half_flags_y = 
	((mb.vector[1][0][0] << 1) | (mb.vector[1][0][1] & 1)) & 3;
      half_flags_uv = 
	(((mb.vector[1][0][0]/2) << 1) | ((mb.vector[1][0][1]/2) & 1)) & 3;
      
      /* Field select */
      // motion_vertical_field_select is always coded if we have m.v.c. == 2
      if(mb.motion_vertical_field_select[1][0]) {
	pred_y += padded_width;
	pred_u += padded_width/2;
	//pred_v += padded_width/2;
      }
      
      /* Prediction destination (field) */
      if(mb.prediction_type == PRED_TYPE_16x8_MC) {
	// Is this right for 16x8 MC... 
	dst_y += 8 * 2 * padded_width;
	dst_u += 8 * 2 * padded_width/2;
	//dst_v += 8 * 2 * padded_width/2;
      } else {
	dst_y += padded_width;
	dst_u += padded_width/2;
	//dst_v += padded_width/2;
      }
      
      // if we have m.v.c. == 2 then either field_pred in frame or a field_pic
      stride = padded_width*2;
      
      dd = fwd_ref_image->v - fwd_ref_image->u;
     
      motion[0][1][half_flags_y](dst_y, pred_y, stride, stride);
      motion[0][3][half_flags_uv](dst_u, pred_u, stride/2, stride/2);
      motion[0][3][half_flags_uv](dst_u+dd, pred_u+dd, stride/2, stride/2);
    }
  }
  
  if(mb.modes.macroblock_type & MACROBLOCK_MOTION_BACKWARD) {
    uint8_t *dst_y, *dst_u;//, *dst_v;
    uint8_t *block_pred_y, *block_pred_u;//, *pred_v;
    
    DPRINTF(5, "backward_motion_comp\n");
    DPRINTF(5, "x: %d, y: %d\n", seq.mb_column, seq.mb_row);
    
    /* Image/Field select */
    block_pred_y = bwd_ref_image->y;
    block_pred_u = bwd_ref_image->u;
    //pred_v = bwd_ref_image->v;
    dst_y = dst_image->y;
    dst_u = dst_image->u;
    //dst_v = dst_ref_image->v;
    
    /* Motion vector Origo (block->index) */
    /* Prediction destination (block->index) */
    if(pic.coding_ext.picture_structure == PIC_STRUCT_FRAME_PICTURE) {
      unsigned int x = seq.mb_column;
      unsigned int y = seq.mb_row;
      block_pred_y += x * 16 + y * 16 * padded_width;
      block_pred_u += x * 8 + y * 8 * padded_width/2;
      //pred_v += x * 8 + y * 8 * padded_width/2;
      dst_y += x * 16 + y * 16 * padded_width;
      dst_u += x * 8 + y * 8 * padded_width/2;
      //dst_v += x * 8 + y * 8 * padded_width/2;
    } else {
      unsigned int x = seq.mb_column;
      unsigned int y = seq.mb_row;
      block_pred_y += x * 16 + y * 16 * padded_width*2;
      block_pred_u += x * 8 + y * 8 * padded_width;	
      //pred_v += x * 8 + y * 8 * padded_width;	
      dst_y += x * 16 + y * 16 * padded_width*2;
      dst_u += x * 8 + y * 8 * padded_width;
      //dst_v += x * 8 + y * 8 * padded_width;
      
      /* Prediction destination (field) */
      if(pic.coding_ext.picture_structure == PIC_STRUCT_BOTTOM_FIELD) {
	dst_y += padded_width;
	dst_u += padded_width/2;
	//dst_v += padded_width/2;
      }
    }
      
    {
      uint8_t *pred_y, *pred_u;//, *pred_v;
      int half_flags_y;
      int half_flags_uv;
      int pred_stride, stride;
      int half_height;
      int d;
      
      /* Motion vector offset */
      if(mb.mv_format == MV_FORMAT_FIELD)
	pred_stride = padded_width * 2;
      else
	pred_stride = padded_width;
      
      pred_y = block_pred_y + 
	(mb.vector[0][1][0] >> 1) + 
	(mb.vector[0][1][1] >> 1) * pred_stride;
      pred_u = block_pred_u + 
	((mb.vector[0][1][0]/2) >> 1) + 
	((mb.vector[0][1][1]/2) >> 1) * pred_stride/2;
      //pred_v +=  ((mb.vector[0][1][0]/2) >> 1) + 
      //((mb.vector[0][1][1]/2) >> 1) * pred_stride/2;
      
      half_flags_y = 
	((mb.vector[0][1][0] << 1) | (mb.vector[0][1][1] & 1)) & 3;
      half_flags_uv = 
	(((mb.vector[0][1][0]/2) << 1) | ((mb.vector[0][1][1]/2) & 1)) & 3;
	
      /* Field select */
      // This is wrong for Dual_Prime... 
      if(mb.mv_format == MV_FORMAT_FIELD) {
	if(mb.motion_vertical_field_select[0][1]) {
	  pred_y += padded_width;
	  pred_u += padded_width/2;
	  //pred_v += padded_width/2;
	}
      }
      
      if(mb.prediction_type == PRED_TYPE_FIELD_BASED)
	stride = padded_width * 2;
      else
	stride = padded_width;
      
      d = bwd_ref_image->v - bwd_ref_image->u;
      half_height = (mb.motion_vector_count == 2) ? 1 :0; // ??
      /*
      fprintf(stderr, "motion[%d][%d][%d](%d, %d, %d, %d);",
	      !!(mb.modes.macroblock_type & MACROBLOCK_MOTION_FORWARD),
	      half_height,
	      half_flags_y,
	      dst_y, pred_y, stride, stride);
      */

      motion[!!(mb.modes.macroblock_type & MACROBLOCK_MOTION_FORWARD)]
	[half_height][half_flags_y](dst_y, pred_y, stride, stride);
      motion[!!(mb.modes.macroblock_type & MACROBLOCK_MOTION_FORWARD)]
	[half_height+2][half_flags_uv](dst_u, pred_u, stride/2, stride/2);
      motion[!!(mb.modes.macroblock_type & MACROBLOCK_MOTION_FORWARD)]
	[half_height+2][half_flags_uv](dst_u+d, pred_u+d, stride/2, stride/2);
    }
    // Only field_pred in frame_pictures have m.v.c. == 2 except for 
    // field_pictures with 16x8 MC
    if(mb.motion_vector_count == 2) {
      uint8_t *pred_y, *pred_u;//, *pred_v;
      int pred_stride, stride;
      int half_flags_y, half_flags_uv;
      int dd;
    
      /* Motion vector offset */
      pred_stride = padded_width * 2; 
      
      pred_y = block_pred_y + 
	(mb.vector[1][1][0] >> 1) + 
	(mb.vector[1][1][1] >> 1) * pred_stride;
      pred_u = block_pred_u + 
	((mb.vector[1][1][0]/2) >> 1) + 
	((mb.vector[1][1][1]/2) >> 1) * pred_stride/2;
      //pred_v +=  ((mb.vector[1][1][0]/2) >> 1) + 
      //((mb.vector[1][1][1]/2) >> 1) * pred_stride/2;
      
      half_flags_y = 
	((mb.vector[1][1][0] << 1) | (mb.vector[1][1][1] & 1)) & 3;
      half_flags_uv = 
	(((mb.vector[1][1][0]/2) << 1) | ((mb.vector[1][1][1]/2) & 1)) & 3;
      
      /* Field select */
      // motion_vertical_field_select is always coded if we have m.v.c. == 2
      if(mb.motion_vertical_field_select[1][1]) {
	pred_y += padded_width;
	pred_u += padded_width/2;
	//pred_v += padded_width/2;
      }
      
      /* Prediction destination (field) */
      if(mb.prediction_type == PRED_TYPE_16x8_MC) {
	// Is this right for 16x8 MC... 
	dst_y += 8 * 2 * padded_width;
	dst_u += 8 * 2 * padded_width/2;
	//dst_v += 8 * 2 * padded_width/2;
      } else {
	dst_y += padded_width;
	dst_u += padded_width/2;
	//dst_v += padded_width/2;
      }
      
      // if we have m.v.c. == 2 then either field_pred in frame or a field_pic
      stride = padded_width*2;
      
      dd = bwd_ref_image->v - bwd_ref_image->u;
     
      motion[!!(mb.modes.macroblock_type & MACROBLOCK_MOTION_FORWARD)]
	[1][half_flags_y](dst_y, pred_y, stride, stride);
      motion[!!(mb.modes.macroblock_type & MACROBLOCK_MOTION_FORWARD)]
	[3][half_flags_uv](dst_u, pred_u, stride/2, stride/2);
      motion[!!(mb.modes.macroblock_type & MACROBLOCK_MOTION_FORWARD)]
	[3][half_flags_uv](dst_u+dd, pred_u+dd, stride/2, stride/2);
    }    
  }
}


void motion_comp_add_coeff(unsigned int i)
{
  int padded_width,x,y;
  int stride;
  int d;
  uint8_t *dst;

  padded_width = seq.mb_width * 16; //seq.horizontal_size;

  x = seq.mb_column;
  y = seq.mb_row;
  if (pic.coding_ext.picture_structure == PIC_STRUCT_FRAME_PICTURE) {
    if (mb.modes.dct_type) {
      d = 1;
      stride = padded_width * 2;
    } else {
      d = 8;
      stride = padded_width;
    }
    if(i < 4) {
      dst = &dst_image->y[x * 16 + y * 16 * padded_width];
      dst = (i & 1) ? dst + 8 : dst;
      dst = (i >= 2) ? dst + padded_width * d : dst;
    }
    else {
      stride = padded_width / 2; // OBS
      if( i == 4 )
	dst = &dst_image->u[x * 8 + y * 8 * stride];
      else // i == 5
	dst = &dst_image->v[x * 8 + y * 8 * stride];
    }
  } else {
    if(i < 4) {
      stride = padded_width * 2;
      dst = &dst_image->y[x * 16 + y * 16 * stride];
      dst = (i & 1) ? dst + 8 : dst;
      dst = (i >= 2) ? dst + stride * 8 : dst;
    }
    else {
      stride = padded_width; // OBS
      if(i == 4)
	dst = &dst_image->u[x * 8 + y * 8 * stride];
      else // i == 5
	dst = &dst_image->v[x * 8 + y * 8 * stride];
    }
    if (pic.coding_ext.picture_structure == PIC_STRUCT_BOTTOM_FIELD)
      dst += stride/2;
  }
  //mlib_VideoIDCT8x8_S16_S16((int16_t *)mb.QFS, (int16_t *)mb.QFS);
  //mlib_VideoAddBlock_U8_S16(dst, mb.QFS, stride);
  mlib_VideoIDCTAdd_U8_S16(dst, mb.QFS, stride);
}
