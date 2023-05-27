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

/*
 * This is core functions that are needed for MPEG-1 decoding. 
 *
 * It relies on the common functions to parse everything but
 * the slices level and down.
 *
 * The code is a modified and simplified version of the MPEG-2 code.
 * MPEG-1 can not be interlaced and there are also other extensions 
 * that are only pressent in MPEG-2. Some functions that could have 
 * been shared between MPEG-1 and 2 are not because they have been 
 * simplifid according to what MPEG-1 supports, it should be possible
 * instead just call the MPEG-2 functions. 
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <inttypes.h>
#include <signal.h>

#include "debug_print.h"
#include "video_stream.h"
#include "video_types.h"

#ifdef HAVE_MLIB
#include <mlib_types.h>
#include <mlib_status.h>
#include <mlib_sys.h>
#include <mlib_video.h>
#include <mlib_algebra.h>
#else
#ifdef HAVE_MMX
#include "mmx.h"
#include "mmx_mlib.h"
#else
#include "c_mlib.h"
#endif
#endif

#include "common.h"

#include "c_getbits.h"
#include "video_tables.h"



extern yuv_image_t *dst_image;

extern void next_start_code(void);
extern void exit_program(int exitcode) ATTRIBUTE_NORETURN;
#if PRAGMA_NORETURN
#pragma does_not_return (exit_program) 
#endif
extern void motion_comp(void);
extern void motion_comp_add_coeff(unsigned int i);
extern int get_vlc(const vlc_table_t *table, char *func);



/*
  Functions defined within this file:
  
  void mpeg1_slice(void);
  void block_intra(unsigned int);
  void block_non_intra(unsigned int);
  void macroblock(void);
  void macroblock_modes(void);         // Could be shared with MPEG2
  void coded_block_pattern(void);      // Could be shared with MPEG2
  void reset_dc_dct_pred(void);        // Could be shared with MPEG2
  void reset_PMV();                    // Could be shared with MPEG2
  void reset_vectors();                // Could be shared with MPEG2
  void motion_vectors(unsigned int s); // Could be shared with MPEG2
  void motion_vector(int r, int s);    // Is identical to MPEG2
*/






static
void reset_dc_dct_pred(void)
{
  DPRINTF(3, "Resetting dc_dct_pred\n");
  
  mb.dc_dct_pred[0] = 128;
  mb.dc_dct_pred[1] = 128;
  mb.dc_dct_pred[2] = 128;
}

static
void reset_PMV(void)
{
  DPRINTF(3, "Resetting PMV\n");
  
  pic.PMV[0][0][0] = 0;
  pic.PMV[0][0][1] = 0;
  pic.PMV[0][1][0] = 0;
  pic.PMV[0][1][1] = 0;
}

static
void reset_vectors(void)
{
  DPRINTF(3, "Resetting motion vectors\n");
  
  mb.vector[0][0][0] = 0;
  mb.vector[0][0][1] = 0;
  mb.vector[0][1][0] = 0;
  mb.vector[0][1][1] = 0;
}




/* 6.2.6 Block */
static
int block_intra(unsigned int i)
{
  unsigned int dct_dc_size;
  int dct_diff = 0;
  
  unsigned int n;
  
  DPRINTF(3, "pattern_code(%d) set\n", i);
  
  { /* Reset all coefficients to 0. */
    int m;
    for(m=0; m<16; m++)
      *(((uint64_t *)mb.QFS) + m) = 0;
  }
  
  /* DC - component */
  if(i < 4) {
    dct_dc_size = get_vlc(table_b12, "dct_dc_size_luminance (b12)");
    DPRINTF(4, "luma_size: %d\n", dct_dc_size);
  } else {
    dct_dc_size = get_vlc(table_b13, "dct_dc_size_chrominance (b13)");
    DPRINTF(4, "chroma_size: %d\n", dct_dc_size);
  } 
    
  if(dct_dc_size != 0) {
    int dct_dc_differential = GETBITS(dct_dc_size, "dct_dc_differential");
    int half_range = 1 << (dct_dc_size - 1);
    
    if(dct_dc_differential >= half_range)
      dct_diff = dct_dc_differential;
    else
      dct_diff = (dct_dc_differential + 1) - (2 * half_range);
  }
      
  { // qfs is always between 0 and 2^(8+dct_dc_size)-1, i.e unsigned.
    unsigned int qfs;
    int cc;
      
    /* Table 7-1. Definition of cc, colour component index */ 
    cc = (i>>2) + ((i>>2) & i);
      
    qfs = mb.dc_dct_pred[cc] + dct_diff;
    mb.dc_dct_pred[cc] = qfs;
      
    /* inverse quantisation */
    {
      // mb.intra_dc_mult is 8 in MPEG-1
      unsigned int f = 8 * qfs;

      if(f > 2047)
	f = 2047;

      mb.QFS[0] = f;
      n = 1;
    }
  }
  
  
  /* AC - components */
  
  while( 1 ) {
    /* Manually inlined and optimized get_dct(..) */
    unsigned int code;
    const DCTtab *tab;
      
    code = nextbits(16);
    if(code >= 16384)
      tab = &DCTtabnext[(code >> 12) - 4];
    else if(code >= 1024)
      tab = &DCTtab0[(code >> 8) - 4];
    else if(code >= 512)
      tab = &DCTtab1[(code >> 6) - 8];
    else if(code >= 256)
      tab = &DCTtab2[(code >> 4) - 16];
    else if(code >= 128)
      tab = &DCTtab3[(code >> 3) - 16];
    else if(code >= 64)
      tab = &DCTtab4[(code >> 2) - 16];
    else if(code >= 32)
      tab = &DCTtab5[(code >> 1) - 16];
    else if(code >= 16)
      tab = &DCTtab6[code - 16];
    else {
      fprintf(stderr,
	      "(vlc) invalid huffman code 0x%x in vlc_get_block_coeff() (intra)\n",
	      code);
      //exit_program(1);
      return -1;
    }

#if DEBUG
    if(tab->run < 64 /* VLC_END_OF_BLOCK */) {
      DPRINTF(4, "coeff run: %d, level: %d\n", tab->run, tab->level);
    }
#endif

    if (tab->run == 64 /* VLC_END_OF_BLOCK */) {
      dropbits(2); // tab->len, end of block always = 2 bits
      break;
    } 
    else {
      unsigned int run, val, sgn, i, f;
	  
      if(tab->run == 65 /* VLC_ESCAPE */) {
	// dropbits(tab->len); // tab->len, escape always = 6 bits	
	// run = GETBITS(6, "(get_dct escape - run )");
	// val = GETBITS(8, "(get_dct escape - level )");
	uint32_t tmp = GETBITS(6 + 6 + 8, "(get_dct escape - run & level)" );
	val = abs((int8_t)(tmp & 0xff));
	run = (tmp >> 8) & 0x3f;
	sgn = tmp & 0x80;
	
	if((tmp & 0x7f) == 0) { // ???
	  val = GETBITS(8, "(get_dct escape - extended level)");
	  val = sgn ? (0x100 - val) : val;
#if 0
	  if(val < 128) {
	    fprintf(stderr, "invalid extended dct escape MPEG-1\n");
	    return -1;
	  }
#endif
	}
      }
      else {
	// dropbits(tab->len); 
	run = tab->run;
	val = tab->level; 
	sgn = 0x1 & GETBITS(tab->len+1, "(get_dct sign )"); //sign bit
      }
    
      n += run;
    
      /* inverse quantisation */
      i = inverse_scan[0][n];
      f = (2 * val
	   * mb.quantiser_scale
	   * seq.header.intra_inverse_quantiser_matrix[i])/16;
      
      if(f > 2047)
	f = 2047;
      
      mb.QFS[i] = sgn ? (-f | 0x1) : -(-f | 0x1); /* Oddification */
      n++;
    }
  }
  DPRINTF(4, "nr of coeffs: %d\n", n);
  return 0;
}





/* 6.2.6 Block */
static
int block_non_intra(unsigned int b)
{
  unsigned int n = 0;
    
  DPRINTF(3, "pattern_code(%d) set\n", b);
  
  { /* Reset all coefficients to 0. */
    int m;
    for(m=0; m<16; m++)
      *(((uint64_t *)mb.QFS) + m) = 0;
  }
  
  while(1) {
    /* Manually inlined and optimized get_dct(..) */
    unsigned int code;
    const DCTtab *tab;
    
    code = nextbits(16);
    
    if (code>=16384)
      if(n == 0)
	tab = &DCTtabfirst[(code>>12)-4];
      else
	tab = &DCTtabnext[(code>>12)-4];
    else if(code>=1024)
      tab = &DCTtab0[(code>>8)-4];
    else if(code>=512)
      tab = &DCTtab1[(code>>6)-8];
    else if(code>=256)
      tab = &DCTtab2[(code>>4)-16];
    else if(code>=128)
      tab = &DCTtab3[(code>>3)-16];
    else if(code>=64)
      tab = &DCTtab4[(code>>2)-16];
    else if(code>=32)
      tab = &DCTtab5[(code>>1)-16];
    else if(code>=16)
      tab = &DCTtab6[code-16];
    else {
      fprintf(stderr, "(vlc) invalid huffman code 0x%x in " 
	      "vlc_get_block_coeff() (non intra)\n", code);
      return -1;
    }
    
#ifdef DEBUG
    if(tab->run < 64 /* VLC_END_OF_BLOCK */) {
      DPRINTF(4, "coeff run: %d, level: %d\n", tab->run, tab->level);
    }
#endif
   
    if(tab->run == 64 /* VLC_END_OF_BLOCK */) {
      dropbits( 2 ); // tab->len, end of block always = 2 bits
      break;
    }
    else {
      unsigned int run, val, sgn, i, f;
      
      if(tab->run == 65 /* VLC_ESCAPE */) {
	// dropbits(tab->len); // tab->len, escape always = 6 bits	
	// run = GETBITS(6, "(get_dct escape - run )");
	// val = GETBITS(8, "(get_dct escape - level )");
	uint32_t tmp = GETBITS(6 + 6 + 8, "(get_dct escape - run & level)" );
	val = abs((int8_t)(tmp & 0xff));
	run = (tmp >> 8) & 0x3f;
	sgn = tmp & 0x80;

	if((tmp & 0x7f) == 0) { // ???
	  val = GETBITS(8, "(get_dct escape - extended level)");
	  val = sgn ? (0x100 - val) : val;
#if 0
	  if(val < 128) {
	    fprintf(stderr, "invalid extended dct escape MPEG-1\n");
	    return -1;
	  }
#endif
	}
      }
      else {
	// dropbits(tab->len); 
	run = tab->run;
	val = tab->level; 
	sgn = 0x1 & GETBITS(tab->len+1, "(get_dct sign )"); //sign bit
      }
      
      n += run;
      
      /* inverse scan & quantisation */
      i = inverse_scan[0][n];
      f = ( (2*val + 1)
	    * mb.quantiser_scale
	    * seq.header.non_intra_inverse_quantiser_matrix[i])/16;
      
      if(f > 2047)
	f = 2047;
      
      mb.QFS[i] = sgn ? (-f | 0x1) : -(-f | 0x1); /* Oddification */
      
      n++;
    }
  }
  DPRINTF(4, "nr of coeffs: %d\n", n);
  return 0;
}




/* 6.2.5.3 Coded block pattern */
static
int coded_block_pattern(void)
{
  DPRINTF(3, "coded_block_pattern\n");

  mb.cbp = get_vlc(table_b9, "cbp (b9)");
  
  if(mb.cbp == 0) {
    fprintf(stderr, "** cbp = 0, shall not be used with 4:2:0 chrominance\n");
    //exit_program(-1);
    return -1;
  }
  DPRINTF(4, "cpb = %u\n", mb.cbp);
  
  return 0;;
}


/* 6.2.5.2.1 Motion vector */
static
void motion_vector(int r, int s)
{
  int t;
  
  DPRINTF(2, "motion_vector(%d, %d)\n", r, s);

  for(t = 0; t < 2; t++) {   
    int delta;
    int prediction;
    int vector;
    
    int r_size = pic.coding_ext.f_code[s][t] - 1;
    
    { // Read and compute the motion vector delta
      int motion_code = get_vlc(table_b10, "motion_code[r][s][0] (b10)");
      
      if((pic.coding_ext.f_code[s][t] != 1) && (motion_code != 0)) {
	int motion_residual = GETBITS(r_size, "motion_residual[r][s][0]");
	
	delta = ((abs(motion_code) - 1) << r_size) + motion_residual + 1;
	if(motion_code < 0)
	  delta = - delta;
      }
      else {
	delta = motion_code;
      }      
    }
    
    if(mb.dmv == 1)
      mb.dmvector[t] = get_vlc(table_b11, "dmvector[0] (b11)");
  
    // Get the predictor
    prediction = pic.PMV[r][s][t];
    if((t==1) && (mb.mv_format == MV_FORMAT_FIELD) && 
       (pic.coding_ext.picture_structure == PIC_STRUCT_FRAME_PICTURE))
      prediction = prediction >> 1;         /* DIV */
    
    { // Compute the resulting motion vector
      int f = 1 << r_size;
      int high = (16 * f) - 1;
      int low = ((-16) * f);
      int range = (32 * f);
      
      vector = prediction + delta;
      if(vector < low)
	vector = vector + range;
      if(vector > high)
	vector = vector - range;
    }
    
    // Update predictors
    if((t==1) && (mb.mv_format == MV_FORMAT_FIELD) && 
       (pic.coding_ext.picture_structure == PIC_STRUCT_FRAME_PICTURE))
      pic.PMV[r][s][t] = vector * 2;
    else
      pic.PMV[r][s][t] = vector;
    
    // Scale the vector so that it is always measured in half pels
    if(pic.header.full_pel_vector[s])
      mb.vector[r][s][t] = vector << 1;
    else
      mb.vector[r][s][t] = vector;
  }
}


/* 6.2.5.2 Motion vectors */
static inline
void motion_vectors(unsigned int s)
{
  DPRINTF(3, "motion_vectors(%u)\n", s);
  
  /* This does not differ from MPEG-2 if:
     pic.coding_ext.picture_structure == PIC_STRUCT_FRAME_PICTURE &&
     pic.coding_ext.frame_pred_frame_dct == 1
     but these are all constant in MPEG1 so we set the following once
     at the start if the video sequence. 
  
     mb.prediction_type = PRED_TYPE_FRAME_BASED;
     mb.motion_vector_count = 1;
     mb.mv_format = MV_FORMAT_FRAME;
     mb.dmv = 0;
  */
  motion_vector(0, s);
}


/* 6.2.5.1 Macroblock modes */
static
int macroblock_modes(void)
{
  DPRINTF(3, "macroblock_modes\n");

  if(pic.header.picture_coding_type == PIC_CODING_TYPE_I) {
    mb.modes.macroblock_type = get_vlc(table_b2, "macroblock_type (b2)");

  } else if(pic.header.picture_coding_type == PIC_CODING_TYPE_P) {
    mb.modes.macroblock_type = get_vlc(table_b3, "macroblock_type (b3)");

  } else if(pic.header.picture_coding_type == PIC_CODING_TYPE_B) {
    mb.modes.macroblock_type = get_vlc(table_b4, "macroblock_type (b4)");
  } else {
    fprintf(stderr, "*** Unsupported picture type %02x\n", 
	    pic.header.picture_coding_type);
    return -1;
    //exit_program(-1);
  }

  if(mb.modes.macroblock_type == VLC_FAIL) {
    return -1;
  }
  
  mb.modes.macroblock_quant = mb.modes.macroblock_type & MACROBLOCK_QUANT;
  mb.modes.macroblock_motion_forward =
    mb.modes.macroblock_type & MACROBLOCK_MOTION_FORWARD;
  mb.modes.macroblock_motion_backward =
    mb.modes.macroblock_type & MACROBLOCK_MOTION_BACKWARD;
  mb.modes.macroblock_pattern = mb.modes.macroblock_type & MACROBLOCK_PATTERN;
  mb.modes.macroblock_intra = mb.modes.macroblock_type & MACROBLOCK_INTRA;

#if 0 // MPEG2
  mb.modes.spatial_temporal_weight_code_flag =
    mb.modes.macroblock_type & SPATIAL_TEMPORAL_WEIGHT_CODE_FLAG;
  
  DPRINTF(5, "spatial_temporal_weight_code_flag: %01x\n", mb.modes.spatial_temporal_weight_code_flag);

  if((mb.modes.spatial_temporal_weight_code_flag == 1) &&
     ( 1 /*spatial_temporal_weight_code_table_index != 0*/)) {
    mb.modes.spatial_temporal_weight_code = GETBITS(2, "spatial_temporal_weight_code");
  }

  if(mb.modes.macroblock_motion_forward ||
     mb.modes.macroblock_motion_backward) {
    if(pic.coding_ext.picture_structure == PIC_STRUCT_FRAME_PICTURE) {
      if(pic.coding_ext.frame_pred_frame_dct == 0) {
	mb.modes.frame_motion_type = GETBITS(2, "frame_motion_type");
      } else {
	/* frame_motion_type omitted from the bitstream */
	mb.modes.frame_motion_type = 0x2;
      }
    } else {
      mb.modes.field_motion_type = GETBITS(2, "field_motion_type");
    }
  }

  /* if(decode_dct_type) */
  if((pic.coding_ext.picture_structure == PIC_STRUCT_FRAME_PICTURE) &&
     (pic.coding_ext.frame_pred_frame_dct == 0) &&
     (mb.modes.macroblock_intra || mb.modes.macroblock_pattern)) {
    
    mb.modes.dct_type = GETBITS(1, "dct_type");
  } else {
    /* Table 6-19. Value of dct_type if dct_type is not in the bitstream.*/
    if(pic.coding_ext.frame_pred_frame_dct == 1) {
      mb.modes.dct_type = 0;
    }
    /* else dct_type is unused, either field picture or mb not coded */
  }
#endif
  
  return 0;
}


/* 6.2.5 Macroblock */
static
int macroblock(int new_slice)
{
  uint16_t inc_add = 0;
  
  DPRINTF(3, "macroblock()\n");

  // MPEG-1 start   (this does not occur in an MPEG-2 stream)
  //                 (not a valid macroblock_escape code)
  while(nextbits(11) == 0x00F) {
    GETBITS(11, "macroblock_stuffing");
  }
  // MPEG-1 end

  while(nextbits(11) == 0x008) {
    GETBITS(11, "macroblock_escape");
    inc_add += 33;
  }
  mb.macroblock_address_increment
    = get_vlc(table_b1, "macroblock_address_increment");
  
  if(mb.macroblock_address_increment == VLC_FAIL) {
    return -1;
  }
  
  mb.macroblock_address_increment += inc_add;  
  seq.macroblock_address += mb.macroblock_address_increment;

  seq.mb_column = seq.macroblock_address % seq.mb_width;
  seq.mb_row = seq.macroblock_address / seq.mb_width;

  if(new_slice) /* There are never any skipped blocks at start of a slice */
    mb.macroblock_address_increment = 1;

  DPRINTF(2, " Macroblock: %d, row: %d, col: %d\n",
	  seq.macroblock_address,
	  seq.mb_row,
	  seq.mb_column);
  
#ifdef DEBUG
  if(mb.macroblock_address_increment > 1) {
    DPRINTF(3, "Skipped %d macroblocks ",
	    mb.macroblock_address_increment);
  }
#endif
  
  
  /* Process all block that are skipped. (Do motion compensation) */
  if(mb.macroblock_address_increment > 1) {    
    int i;    
    
    /* There is plenty of room to optimize this */
    switch(pic.header.picture_coding_type) {
    
    case PIC_CODING_TYPE_I:
      fprintf(stderr, "*** skipped blocks in I-picture\n");
      return -1; // Error parsing bitstream
    
    case PIC_CODING_TYPE_P:
      DPRINTF(3,"in P-picture\n");
      /* Assume prediction is forward with a zero vector */
      mb.modes.macroblock_motion_forward = 1; // Remove Me
      mb.modes.macroblock_motion_backward = 0; // Remove Me
      mb.modes.macroblock_type |= MACROBLOCK_MOTION_FORWARD;
      mb.modes.macroblock_type &= ~MACROBLOCK_MOTION_BACKWARD;	
      reset_vectors(); // Instead of explicit set to zero.
      break;
    
    case PIC_CODING_TYPE_B:
      DPRINTF(3,"in B-frame\n");
      /* Use previous macroblock modes and vectors. */
      break;
    }
    
    /* We only get here for P or B picutes. */
    i = mb.macroblock_address_increment;
    while( --i > 0 ) {
      seq.mb_column = (seq.macroblock_address - i) % seq.mb_width;
      seq.mb_row    = (seq.macroblock_address - i) / seq.mb_width;
      /* Skipped blocks never have any DCT-coefficients,
	 so there is no call to motion_comp_add_coeff */
      motion_comp();
    }
    seq.mb_column = seq.macroblock_address % seq.mb_width;
    seq.mb_row    = seq.macroblock_address / seq.mb_width;
  }

  
  /* What kind of macroblock is this. */
  if(macroblock_modes() == -1) {
    return -1; // Error parsing bitstream
  }
  
  if(mb.modes.macroblock_quant) {
    mb.quantiser_scale = GETBITS(5, "quantiser_scale_code");
  }
  
  if(mb.modes.macroblock_intra == 0) {
    reset_dc_dct_pred();
    DPRINTF(3, "non_intra macroblock\n");
  }
  
  
  /* Decoding of motion vectors. */
  
  // Reset predictors: when a macroblock is skipped in a P-picture. */
  if(mb.macroblock_address_increment > 1) {
    reset_dc_dct_pred(); // Could be moved but fitts fine here
    if(pic.header.picture_coding_type == PIC_CODING_TYPE_P)
      reset_PMV();
  }
  
  if(mb.modes.macroblock_motion_forward)
    motion_vectors(0);
  if(mb.modes.macroblock_motion_backward)
    motion_vectors(1);
  
  // Update predictors.
  if((mb.modes.macroblock_motion_forward == 0) &&
     (mb.modes.macroblock_motion_backward == 0)) {  // Is this correct?
    reset_PMV();
  }
 
  switch (pic.header.picture_coding_type) {
  case PIC_CODING_TYPE_I: /* I-picture */
    break;
  case PIC_CODING_TYPE_P: /* P-picture */
    if(mb.modes.macroblock_intra) {
      reset_PMV();
      reset_vectors();
    }
    if((!mb.modes.macroblock_intra) && (!mb.modes.macroblock_motion_forward)) {
      reset_PMV();
      /* Asume prediction is forward with a zero vector */
      mb.modes.macroblock_motion_forward = 1; // Remove Me
      mb.modes.macroblock_motion_backward = 0; // Remove Me
      mb.modes.macroblock_type |= MACROBLOCK_MOTION_FORWARD;
      mb.modes.macroblock_type &= ~MACROBLOCK_MOTION_BACKWARD;	
      reset_vectors(); // Instead of explicit set to zero.
    }
    break;
  case PIC_CODING_TYPE_B: /* B-picture */
    if(mb.modes.macroblock_intra) {
      reset_PMV();
      reset_vectors();
    }
    break;
  }
  
  
  /* Pel reconstruction 
     - motion compensation (if any)
     - decode coefficents
     - inverse quantisation 
     - 'oddify' the coefficents
     - inverse transform
     - combine the data with motion compensated pels */
  
  if(mb.modes.macroblock_intra) {
    unsigned int i;
    
    /* Intra blocks always have pattern coded for all sub blocks. 
       The IDCT function writes directly to the output buffers. */
    
    for(i = 0; i < 6; i++) {  
      DPRINTF(4, "cbpindex: %d assumed\n", i);
      if(block_intra(i) == -1)
	return -1; // Error parsing bitstream
      
      /* Shortcut: write the IDCT data directly into the picture buffer */
      {
	const int x = seq.mb_column;
	const int y = seq.mb_row;
	const int width = seq.mb_width * 16; //seq.horizontal_size;
	int d, stride;
	uint8_t *dst;
	
	if (mb.modes.dct_type) {
	  d = 1;
	  stride = width * 2;
	} else {
	  d = 8;
	  stride = width;
	}
	
	if(i < 4) {
	  dst = &dst_image->y[x * 16 + y * width * 16];
	  dst = (i & 1) ? dst + 8 : dst;
	  dst = (i >= 2) ? dst + width * d : dst;
	}
	else {
	  stride = width / 2; // HACK alert !!!
	  if(i == 4)
	    dst = &dst_image->u[x * 8 + y * width/2 * 8];
	  else // i == 5
	    dst = &dst_image->v[x * 8 + y * width/2 * 8];
	}
	
	mlib_VideoIDCT8x8_U8_S16(dst, (int16_t *)mb.QFS, stride);
      }
    }
  } 
  else { /* Non-intra block */
    
    motion_comp();
    if(mb.modes.macroblock_pattern) {
      int i;

      if(coded_block_pattern() == -1)
	return -1; // Error parsing bitstream
      
      for(i = 0; i < 6; i++) {
	if(mb.cbp & (1<<(5-i))) {
	  DPRINTF(4, "cbpindex: %d set\n", i);
	  if(block_non_intra(i) == -1)
	    return -1; // Error parsing bitstream
	  
	  //mlib_VideoIDCT8x8_S16_S16((int16_t *)mb.QFS, (int16_t *)mb.QFS);
	  motion_comp_add_coeff(i); // do idtc and add in here
	}
      }
    }
  }
  
  return 0;
}


/* 6.2.4 Slice */
int mpeg1_slice(void)
{
  uint32_t slice_start_code;
  int new_slice = 1;
  
  DPRINTF(3, "slice\n");
  reset_dc_dct_pred();
  reset_PMV();
  reset_vectors();

  DPRINTF(3, "start of slice\n");
  slice_start_code = GETBITS(32, "slice_start_code");
  slice_data.slice_vertical_position = slice_start_code & 0xff;
  
  // Do we need to update seq.mb_col ???
  seq.mb_row = slice_data.slice_vertical_position - 1;
  seq.macroblock_address = (seq.mb_row * seq.mb_width) - 1;

  mb.quantiser_scale = GETBITS(5, "quantiser_scale_code");

  if(nextbits(1) == 1) {
    slice_data.intra_slice_flag = GETBITS(1, "intra_slice_flag");
    slice_data.intra_slice = GETBITS(1, "intra_slice");
    slice_data.reserved_bits = GETBITS(7, "reserved_bits");
    while(nextbits(1) == 1) {
      slice_data.extra_bit_slice = GETBITS(1, "extra_bit_slice");
      slice_data.extra_information_slice 
	= GETBITS(8, "extra_information_slice");
    }
  }
  slice_data.extra_bit_slice = GETBITS(1, "extra_bit_slice");

  do {
    if( macroblock(new_slice) ) {
      //break;
      return -1;
    }
    new_slice = 0;
  } while(nextbits(23) != 0);

  next_start_code();
  return 0;
}

