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


#define NEW_TABLES 0
#if NEW_TABLES
#include "new_table.h"
#endif

/* #include <libcpc.h> */



extern yuv_image_t *dst_image;
extern yuv_image_t *fwd_ref_image;


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
  
  void mpeg2_slice(void);
  void block_intra(unsigned int);
  void block_non_intra(unsigned int);
  void macroblock(void);
  int macroblock_modes(void);
  void coded_block_pattern(void);
  void reset_dc_dct_pred(void);
  void reset_PMV();
  void reset_vectors();
  void motion_vectors(unsigned int s);
  void motion_vector(int r, int s);
*/







static
void reset_dc_dct_pred(void)
{
  DPRINTFI(3, "Resetting dc_dct_pred\n");
  
  /* Table 7-2. Relation between intra_dc_precision and the predictor
     reset value */
  /*
  switch(pic.coding_ext.intra_dc_precision) {
  case 0:
    mb.dc_dct_pred[0] = 128;
    mb.dc_dct_pred[1] = 128;
    mb.dc_dct_pred[2] = 128;
    break;
  case 1:
    mb.dc_dct_pred[0] = 256;
    mb.dc_dct_pred[1] = 256;
    mb.dc_dct_pred[2] = 256;
    break;
  case 2:
    mb.dc_dct_pred[0] = 512;
    mb.dc_dct_pred[1] = 512;
    mb.dc_dct_pred[2] = 512;
    break;
  case 3:
    mb.dc_dct_pred[0] = 1024;
    mb.dc_dct_pred[1] = 1024;
    mb.dc_dct_pred[2] = 1024;
    break;
  default:
    fprintf(stderr, "*** reset_dc_dct_pred(), invalid intra_dc_precision\n");
    exit_program(1);
    break;
  }
  */
  mb.dc_dct_pred[0] = 128 << pic.coding_ext.intra_dc_precision;
  mb.dc_dct_pred[1] = 128 << pic.coding_ext.intra_dc_precision;
  mb.dc_dct_pred[2] = 128 << pic.coding_ext.intra_dc_precision;
}

static
void reset_PMV(void)
{
  DPRINTFI(3, "Resetting PMV\n");

  pic.PMV[0][0][0] = 0;
  pic.PMV[0][0][1] = 0;
  pic.PMV[0][1][0] = 0;
  pic.PMV[0][1][1] = 0;
  pic.PMV[1][0][0] = 0;
  pic.PMV[1][0][1] = 0;
  pic.PMV[1][1][0] = 0;
  pic.PMV[1][1][1] = 0;
}

static
void reset_vectors(void)
{
  DPRINTFI(3, "Resetting motion vectors\n");
  
  mb.vector[0][0][0] = 0;
  mb.vector[0][0][1] = 0;
  mb.vector[1][0][0] = 0;
  mb.vector[1][0][1] = 0;
  mb.vector[0][1][0] = 0;
  mb.vector[0][1][1] = 0;
  mb.vector[1][1][0] = 0;
  mb.vector[1][1][1] = 0;
}



#if NEW_TABLES
/* 6.2.6 Block */
static
void block_intra(unsigned int i)
{
  const base_table_t *top_table;
  
  unsigned int n;
  int inverse_quantisation_sum;
  
 
  DPRINTFI(3, "pattern_code(%d) set\n", i);
  
  //cpc_count_usr_events(1);
  
  { /* Reset all coefficients to 0. */
    int m;
    for(m=0; m<16; m++)
      *(((uint64_t *)mb.QFS) + m) = 0;
  }
    
    
  /* DC - component */
  {
    unsigned int dct_dc_size;
    int dct_diff;
    
    if(i < 4) {
      dct_dc_size = get_vlc(table_b12, "dct_dc_size_luminance (b12)");
      DPRINTF(4, "luma_size: %d\n", dct_dc_size);
    } else {
      dct_dc_size = get_vlc(table_b13, "dct_dc_size_chrominance (b13)");
      DPRINTF(4, "chroma_size: %d\n", dct_dc_size);
    } 
    
    if(dct_dc_size != 0) {
      int half_range = 1<<(dct_dc_size-1);
      int dct_dc_differential = GETBITS(dct_dc_size, "dct_dc_differential");
      
      DPRINTF(4, "diff_val: %d, ", dct_dc_differential);
      
      if(dct_dc_differential >= half_range) {
	dct_diff = dct_dc_differential;
      } else {
	dct_diff = (dct_dc_differential+1)-(2*half_range);
      }
      DPRINTF(4, "%d\n", dct_diff);  
      
    } else {
      dct_diff = 0;
    }
      
    {
      // qfs is always between 0 and 2^(8+dct_dc_size)-1, i.e unsigned.
      unsigned int qfs;
      int cc;
      
      /* Table 7-1. Definition of cc, colour component index */ 
      cc = (i>>2) + ((i>>2) & i);
      
      qfs = mb.dc_dct_pred[cc] + dct_diff;
      mb.dc_dct_pred[cc] = qfs;
      DPRINTF(4, "QFS[0]: %d\n", qfs);
      
      /* inverse quantisation */
      {
	// mb.intra_dc_mult is 1, 2 , 4 or 8, i.e unsigned.
	unsigned int f = mb.intra_dc_mult * qfs;
#if 0
	if(f > 2047) {
	  fprintf(stderr, "Clipp (block_intra first)\n");
	  f = 2047;
	} 
#endif
	mb.QFS[0] = f;
	inverse_quantisation_sum = f + 1;
      }
      n = 1;
    }
  }
  
  
  /* AC - components */
  
  top_table = top_b14;
  if(pic.coding_ext.intra_vlc_format) {
    top_table = top_b15;
  }
  

  
  while(1) {
    unsigned int bits;
    unsigned int i, f, val, sgn;
    //get_dct_intra(&runlevel,"dct_dc_subsequent");
    
    // We need the first and the following 8 bits
    bits = nextbits(16);
    
    if((bits >> 10) == 1) {
    //if(top == entry_for_escape) { // entry_for_escape {24, 8 }
    //if(top->offset == 24) {
      val = getbits(6+6+12);
      n += (val >> 12) & 0x3f;
      val &= 0xfff;
      sgn = (val >= 2048); // sgn = val >> 11;
      if(val >= 2048)
	val = 4096 - val;
    } else {
      index_table_t const *const sub
	= &sub_table[TOP_OFFSET(top_table, bits >> 8) + 
		    ((0xff & bits) >> TOP_IBITS(top_table, bits >> 8))];
      val = VLC_LEVEL(*sub);
      n += VLC_RUN(*sub);
      // Make sure the code for EOB is compensated for this, it is.
      sgn = getbits(VLC_CODE_BITS(*sub)+1+1) & 1;
      
      // End of block, can't happen for esq. but we might need error check?
      if(n >= 64)
	break;
    }
    
    /* inverse quantisation */
    i = inverse_scan[pic.coding_ext.alternate_scan][n++];
    f = (val 
	 * mb.quantiser_scale
	 * seq.header.intra_inverse_quantiser_matrix[i])/16;
    
#if 0
    if((f - sgn) > 2047)
      f = 2047 + sgn;
#endif
    
    inverse_quantisation_sum += f; // The last bit is the same in f and -f.
    
    // mb.QFS[i] = sgn ? -f : f;
    mb.QFS[i] = (f ^ -sgn) + sgn;
  }
  
  //cpc_count_usr_events(0);

  mb.QFS[63] ^= inverse_quantisation_sum & 1;
  
  DPRINTF(4, "nr of coeffs: %d\n", n);
}



/* 6.2.6 Block */
static
void block_non_intra(unsigned int b)
{
  index_table_t const *sub;
  index_table_t const sub_firt_dc = VLC_RL( 1, 0, 1 );
  unsigned int bits;
  
  unsigned int n = 0;
  int inverse_quantisation_sum = 1;
  
  DPRINTF(3, "pattern_code(%d) set\n", b);
  
  //cpc_count_usr_events(1);

  { /* Reset all coefficients to 0. */
    int m;
    for(m=0; m<16; m++)
      *(((uint64_t *)mb.QFS) + m) = 0;
  }
  
  // We need the first and the following 8 bits
  bits = nextbits(16);
  
  sub = &sub_firt_dc; 
  if(bits >> 15) { // if(top->offset < 2) {
    goto sub_entry;
  } 
  
  while(1) {
    unsigned int i, f, val, sgn;
    
    if((bits>>10) == 1) {
    //if(top->offset == 24) { // offset_for_escape { 24, 8 }
      val = getbits(6+6+12);
      n += (val >> 12) & 0x3f;
      val &= 0xfff;
      sgn = (val >= 2048); // sgn = val >> 11;
      if(val >= 2048)
	val = 4096 - val;
    } else {
      sub = &sub_table[TOP_OFFSET(top_b14, bits >> 8) +
		      ((0xff & bits) >> TOP_IBITS(top_b14, bits >> 8))];
    sub_entry:
      val = VLC_LEVEL(*sub);
      n += VLC_RUN(*sub);
      // Make sure the code for EOB and DCT_DC_FIRST is compensated, it is.
      sgn = getbits(VLC_CODE_BITS(*sub) + 1 + 1) & 1;
      
      // End of block, can't happen for esq. but we might need error check?
      if(n >= 64)
	break;
    }
    
    /* inverse quantisation */
    i = inverse_scan[pic.coding_ext.alternate_scan][n++];
    f = ((val * 2 + 1)
	 * mb.quantiser_scale
	 * seq.header.non_intra_inverse_quantiser_matrix[i])/32;
#if 0
    if((f - sgn) > 2047)
      f = 2047 + sgn;
#endif
    
    inverse_quantisation_sum += f; // The last bit is the same in f and -f.
    
    // mb.QFS[i] = sgn ? -f : f;
    mb.QFS[i] = (f ^ -sgn) + sgn;
    
    /* Start of next code */
    
    // We need the first and the following 8 bits
    bits = nextbits(16);
  }
  
  //cpc_count_usr_events(0);
  
  mb.QFS[63] ^= inverse_quantisation_sum & 1;
  
  DPRINTF(4, "nr of coeffs: %d\n", n);
}


#else /* NEW_TABLES */


/* 6.2.6 Block */
static
void block_intra(unsigned int i)
{
  unsigned int n;
  int inverse_quantisation_sum;
  
  unsigned int left;
  unsigned int offset;
  uint32_t word;
  
  DPRINTFI(3, "pattern_code(%d) set\n", i);
  
  //cpc_count_usr_events(1);
  
  { /* Reset all coefficients to 0. */
    int m;
    for(m=0; m<16; m++)
      *(((uint64_t *)mb.QFS) + m) = 0;
  }
    
  /* DC - component */
  {
    unsigned int dct_dc_size;
    int dct_diff;
    
    if(i < 4) {
      dct_dc_size = get_vlc(table_b12, "dct_dc_size_luminance (b12)");
      DPRINTF(4, "luma_size: %d\n", dct_dc_size);
    } else {
      dct_dc_size = get_vlc(table_b13, "dct_dc_size_chrominance (b13)");
      DPRINTF(4, "chroma_size: %d\n", dct_dc_size);
    } 
    
    if(dct_dc_size != 0) {
      int half_range = 1<<(dct_dc_size-1);
      int dct_dc_differential = GETBITS(dct_dc_size, "dct_dc_differential");
      
      DPRINTF(4, "diff_val: %d, ", dct_dc_differential);
      
      if(dct_dc_differential >= half_range) {
	dct_diff = dct_dc_differential;
      } else {
	dct_diff = (dct_dc_differential+1)-(2*half_range);
      }
      DPRINTF(4, "%d\n", dct_diff);  
      
    } else {
      dct_diff = 0;
    }
      
    {
      // qfs is always between 0 and 2^(8+dct_dc_size)-1, i.e unsigned.
      unsigned int qfs;
      int cc;
      
      /* Table 7-1. Definition of cc, colour component index */ 
      cc = (i>>2) + ((i>>2) & i);
      
      qfs = mb.dc_dct_pred[cc] + dct_diff;
      mb.dc_dct_pred[cc] = qfs;
      DPRINTF(4, "QFS[0]: %d\n", qfs);
      
      /* inverse quantisation */
      {
	// mb.intra_dc_mult is 1, 2 , 4 or 8, i.e unsigned.
	unsigned int f = mb.intra_dc_mult * qfs;
#if 0
	if(f > 2047) {
	  fprintf(stderr, "Clipp (block_intra first)\n");
	  f = 2047;
	} 
#endif
	mb.QFS[0] = f;
	inverse_quantisation_sum = f + 1;
      }
      n = 1;
    }
  }
  
  
  /* AC - components */
  
  left = bits_left;
  offset = offs;
  word = nextbits(24);


  while( 1 ) {
    //get_dct_intra(&runlevel,"dct_dc_subsequent");
    
    const DCTtab *tab;
    /*    const unsigned int code = nextbits(16); */
    const unsigned int code = (word >> (24-16));
    
    if(code>=16384) {
      if (pic.coding_ext.intra_vlc_format)
	tab = &DCTtab0a[(code >> 8) - 4];   // 15
      else
	tab = &DCTtabnext[(code >> 12) - 4];  // 14
      /* EOB here for both cases */
    }
    else if(code>=1024) {
      if (pic.coding_ext.intra_vlc_format)
	tab = &DCTtab0a[(code >> 8) - 4];   // 15
      else
	tab = &DCTtab0[(code >> 8) - 4];   // 14
      /* Escape here for both cases */
    }
    else if(code>=512)
      if (pic.coding_ext.intra_vlc_format)
	tab = &DCTtab1a[(code >> 6) - 8];  // 15
      else
	tab = &DCTtab1[(code >> 6) - 8];  // 14
    else if(code>=256)
      tab = &DCTtab2[(code >> 4) - 16];
    else if(code>=128)
      tab = &DCTtab3[(code >> 3) - 16];
    else if(code>=64)
      tab = &DCTtab4[(code >> 2) - 16];
    else if(code>=32)
      tab = &DCTtab5[(code >> 1) - 16];
    else if(code>=16)
      tab = &DCTtab6[code - 16];
    else {
      fprintf(stderr,
	      "(vlc) invalid huffman code 0x%x in vlc_get_block_coeff()\n",
	      code);
      //exit_program(1);
      tab = &DCTtabnext[4]; // end_of_block 
    }

    
    if(tab->run == 64 /*VLC_END_OF_BLOCK*/) { // end_of_block 
      /*dropbits(tab->len); // pic.coding_ext.intra_vlc_format ? 4 : 2 bits */
      left -= tab->len;
      break;
    } 
    else {
      unsigned int i, f, run, val, sgn;
      
      if(tab->run == 65) { /* escape */
	//    dropbits(tab->len); // always 6 bits.
	//    run = GETBITS(6, "(get_dct escape - run )");
	//    val = GETBITS(12, "(get_dct escape - level )");
	/* val = GETBITS(6 + 6 + 12, "(get_dct escape - run & level )"); */
	val = (word >> (24-(6+12+6))); left -= (6+12+6);
	run = (val >> 12) & 0x3f;
	val = val & 0xfff;

#if 0
	if ((val & 2047) == 0) {
	  fprintf(stderr,"invalid escape in vlc_get_block_coeff()\n");
	  exit_program(1);
	}
#endif
	sgn = (val >= 2048);
	if(val >= 2048)                // !!!! ?sgn? 
	  val = 4096 - val;
#ifdef DEBUG
	DPRINTF(4, "coeff run: %d, level: %d (esc)\n", run, sgn?-val:val);
#endif      
      } else {
	//    dropbits(tab->len);
	run = tab->run;
	val = tab->level; 
	/* sgn = 0x1 & GETBITS(tab->len + 1, "(get_dct sign )"); //sign bit */
	sgn = 0x1 & (word >> (24-(tab->len + 1))); left -= (tab->len + 1);
#ifdef DEBUG
	DPRINTF(4,"coeff run: %d, level: %d\n",tab->run,(sgn?-1:1)*tab->level);
#endif
      }
      
      n += run;
      
      /* inverse quantisation */
      i = inverse_scan[pic.coding_ext.alternate_scan][n];
      f = (val 
	   * mb.quantiser_scale
	   * seq.header.intra_inverse_quantiser_matrix[i])/16;
      
#if 0	      
      if(!sgn) {
	if(f > 2047)
	  f = 2047;
      }
      else {
	if(f > 2048)
	  f = 2048;
      }
#endif
      inverse_quantisation_sum += f; // The last bit is the same in f and -f.
      // mb.QFS[i] = sgn ? -f : f;
      mb.QFS[i] = (f ^ -sgn) + sgn;
      
      n++;
    }
    
    /* Works like:
       dropbits(..);
       word = nextbits(24);
    */
    if(left <= 24) {
      if(offset < buf_size) {
	uint32_t new_word = FROM_BE_32(buf[offset++]);
	cur_word = (cur_word << 32) | new_word;
	left += 32;
      }
      else {
	bits_left = left;
	offs = offset;
	read_buf();
	left = bits_left;
	offset = offs;
      }
    }
    word = (cur_word << (64-left)) >> 40;
  }

  //cpc_count_usr_events(0);
  
  mb.QFS[63] ^= inverse_quantisation_sum & 1;
  
  // Clean-up
  bits_left = left;
  offs = offset;
  dropbits(0);
  
  DPRINTF(4, "nr of coeffs: %d\n", n);
}



/* 6.2.6 Block */
static
void block_non_intra(unsigned int b)
{
  unsigned int n = 0;
  int inverse_quantisation_sum = 1;
  
  /* Make a local 'getbits' implementation. */
  unsigned int left = bits_left;
  unsigned int offset = offs;
  uint64_t lcur_word = cur_word;
  uint32_t word = (lcur_word << (64-left)) >> 40; //nextbits(24);
  
  DPRINTF(3, "pattern_code(%d) set\n", b);
  
  //cpc_count_usr_events(1);    
 
  { /* Reset all coefficients to 0. */
    int m;
    for(m=0; m<16; m++)
      *(((uint64_t *)mb.QFS) + m) = 0;
  }
  
  while(1) {
    //      get_dct_non_intra(&runlevel, "dct_dc_subsequent");
    const DCTtab *tab;
    /*    const unsigned int code = nextbits(16);*/
    const unsigned int code = (word >> (24-16));
    
    if(code >= 16384)
      if(n == 0)
	tab = &DCTtabfirst[(code >> 12) - 4];
      else {
	tab = &DCTtabnext[(code >> 12) - 4];
	/* EOB only here */
      }
    else if(code >= 1024) {
      tab = &DCTtab0[(code >> 8) - 4];
      /* Escape only here */
    }
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
      fprintf(stderr, "(vlc) invalid huffman code 0x%x " 
	      "in vlc_get_block_coeff()\n", code);
      //exit_program(1);
      break; // end_of_block 
    }
    
   
    if(tab->run == 64 /*VLC_END_OF_BLOCK*/) { // end_of_block 
      //      dropbits( 2 ); // tab->len, end of block always = 2bits
      //left -= 2; see the code after the loop
      break;
    } 
    else {
      unsigned int i, f, run, val, sgn;

      if(tab->run == 65) { /* escape */
	//	  dropbits(tab->len); escape always = 6 bits
	//	  run = GETBITS(6, "(get_dct escape - run )");
	//	  val = GETBITS(12, "(get_dct escape - level )");
	/*	val = GETBITS(6 + 12 + 6, "(escape run + val)" );*/
	val = (word >> (24-(6+12+6))); left -= (6+12+6);
	run = (val >> 12) & 0x3f;
	val &= 0xfff;

#if 0
	if((val & 2047) == 0) {
	  fprintf(stderr,"invalid escape in vlc_get_block_coeff()\n");
	  exit_program(1);
	}
#endif
	
	sgn = (val >= 2048);
	if(val >= 2048)                // !!!! ?sgn? 
	  val =  4096 - val;// - 4096;
#ifdef DEBUG
	DPRINTF(4, "coeff run: %d, level: %d (esc)\n", run, sgn?-val:val);
#endif
      }
      else {
	//	  dropbits(tab->len);
	run = tab->run;
	val = tab->level; 
	/*sgn = 0x1 & GETBITS(tab->len + 1, "(get_dct sign )"); //sign bit*/
	sgn = 0x1 & (word >> (24-(tab->len + 1))); left -= (tab->len + 1);
#ifdef DEBUG
	DPRINTF(4,"coeff run: %d, level: %d\n",tab->run,(sgn?-1:1)*tab->level);
#endif
      }
      
      n += run;
      
      /* inverse quantisation */
      i = inverse_scan[pic.coding_ext.alternate_scan][n];
      // flytta ut &inverse_scan[pic.coding_ext.alternate_scan] ??
      // flytta ut mb.quantiser_scale ??
      f = ( ((val*2)+1)
	    * mb.quantiser_scale
	    * seq.header.non_intra_inverse_quantiser_matrix[i])/32;

#if 0
      if(!sgn) {
	if(f > 2047)
	  f = 2047;
      }
      else {
	if(f > 2048)
	  f = 2048;
      }
#endif
      
      inverse_quantisation_sum += f; // The last bit is the same in f and -f.
      // mb.QFS[i] = sgn ? -f : f;
      mb.QFS[i] = (f ^ -sgn) + sgn;
      
      n++;      
    }
    
    /* Works like:
       dropbits(..);
       word = nextbits(24);
    */
    if(left <= 24) {
      if(offset < buf_size) {
	uint32_t new_word = FROM_BE_32(buf[offset++]);
	lcur_word = (lcur_word << 32) | new_word;
	left += 32;
      }
      else {
	bits_left = left;
	offs = offset;
	cur_word = lcur_word;
	read_buf();
	left = bits_left;
	offset = offs;
	lcur_word = cur_word;
      }
    }
    word = (lcur_word << (64-left)) >> 40;
  }
  
  //cpc_count_usr_events(0);

  mb.QFS[63] ^= inverse_quantisation_sum & 1;
  
  // Clean-up
  bits_left = left;
  offs = offset;
  cur_word = lcur_word;
  dropbits(2); // eob-token ++
  
  DPRINTF(4, "nr of coeffs: %d\n", n);
}
#endif

/* 6.2.5.3 Coded block pattern */
static
void coded_block_pattern(void)
{
  //  uint16_t coded_block_pattern_420;
  uint8_t cbp = 0;
  
  DPRINTF(3, "coded_block_pattern\n");

  cbp = get_vlc(table_b9, "cbp (b9)");
  

  if((cbp == 0) && (seq.ext.chroma_format == 0x1)) {
    fprintf(stderr, "** cbp = 0, shall not be used with 4:2:0 chrominance\n");
    //exit_program(1);
  }
  mb.cbp = cbp;
  
  DPRINTF(4, "cpb = %u\n", mb.cbp);
#if 0
  if(seq.ext.chroma_format == 0x02) {
    mb.coded_block_pattern_1 = GETBITS(2, "coded_block_pattern_1");
  }
 
  if(seq.ext.chroma_format == 0x03) {
    mb.coded_block_pattern_2 = GETBITS(6, "coded_block_pattern_2");
  }
#endif
}


 
/* 6.2.5.2.1 Motion vector */
static
void motion_vector(unsigned int r, unsigned int s)
{
  unsigned int t;
  
  DPRINTF(3, "motion_vector(%d, %d)\n", r, s);

  for(t = 0; t < 2; t++) {   
    int delta;
    int prediction;
    int vector;
    
    unsigned int r_size = pic.coding_ext.f_code[s][t] - 1;
    
    { // Read and compute the motion vector delta
      int motion_code = get_vlc(table_b10, "motion_code[r][s][0] (b10)");
      
      if((pic.coding_ext.f_code[s][t] != 1) && (motion_code != 0)) {
	int motion_residual = GETBITS(r_size, "motion_residual[r][s][0]");
	
	delta = ((abs(motion_code) - 1) << r_size) + motion_residual + 1;
	if(motion_code < 0)
	  delta = -delta;
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
      prediction = prediction >> 1; /* DIV */
    
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
    
#if MPEG1
    // Scale the vector so that it is always measured in half pels
    if(pic.header.full_pel_vector[s])
      mb.vector[r][s][t] = vector << 1;
    else
#endif
      mb.vector[r][s][t] = vector;
  }
}


/* 6.2.5.2 Motion vectors */
static
void motion_vectors(unsigned int s)
{
  DPRINTF(3, "motion_vectors(%u)\n", s);

  if(pic.coding_ext.picture_structure == PIC_STRUCT_FRAME_PICTURE) {
    
    if((mb.modes.macroblock_type & MACROBLOCK_INTRA) &&
       pic.coding_ext.concealment_motion_vectors) {
      mb.prediction_type = PRED_TYPE_FRAME_BASED;
      mb.motion_vector_count = 1;
      mb.mv_format = MV_FORMAT_FRAME;
      mb.dmv = 0;
    } else {
      /* Table 6-17 Meaning of frame_motion_type */
      switch(mb.modes.frame_motion_type) {
      case 0x0:
	// Should never happen, value is checked when read in.
      case 0x1:
	mb.prediction_type = PRED_TYPE_FIELD_BASED;
	mb.motion_vector_count = 2;
	mb.mv_format = MV_FORMAT_FIELD;
	mb.dmv = 0;
	break;
      case 0x2:
	/* spatial_temporal_weight_class always 0 for now */
	mb.prediction_type = PRED_TYPE_FRAME_BASED;
	mb.motion_vector_count = 1;
	mb.mv_format = MV_FORMAT_FRAME;
	mb.dmv = 0;
	break;
      case 0x3:
	mb.prediction_type = PRED_TYPE_DUAL_PRIME;
	mb.motion_vector_count = 1;
	mb.mv_format = MV_FORMAT_FIELD;
	mb.dmv = 1;
	break;
      default:
	fprintf(stderr, "*** impossible prediction type\n");
	exit_program(1);
	break;
      }
    }
    
  } else {
    /* Table 6-18 Meaning of field_motion_type */
    switch(mb.modes.field_motion_type) {
    case 0x0:
      // Should never happen, value is checked when read in.
    case 0x1:
      mb.prediction_type = PRED_TYPE_FIELD_BASED;
      mb.motion_vector_count = 1;
      mb.mv_format = MV_FORMAT_FIELD;
      mb.dmv = 0;
      break;
    case 0x2:
      mb.prediction_type = PRED_TYPE_16x8_MC;
      mb.motion_vector_count = 2;
      mb.mv_format = MV_FORMAT_FIELD;
      mb.dmv = 0;
      break;
    case 0x3:
      mb.prediction_type = PRED_TYPE_DUAL_PRIME;
      mb.motion_vector_count = 1;
      mb.mv_format = MV_FORMAT_FIELD;
      mb.dmv = 1;
      break;
    default:
      fprintf(stderr, "*** impossible prediction type\n");
      exit_program(1);
      break;
    }
  }


  if(mb.motion_vector_count == 1) {
    if((mb.mv_format == MV_FORMAT_FIELD) && (mb.dmv != 1)) {
      mb.motion_vertical_field_select[0][s] 
	= GETBITS(1, "motion_vertical_field_select[0][s]");
    }
    motion_vector(0, s);
  } else {
    mb.motion_vertical_field_select[0][s] 
      = GETBITS(1, "motion_vertical_field_select[0][s]");
    motion_vector(0, s);
    mb.motion_vertical_field_select[1][s] 
      = GETBITS(1, "motion_vertical_field_select[1][s]");
    motion_vector(1, s);
  }
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
    exit_program(1);
    // should not be tested / handled here
  }
  if(mb.modes.macroblock_type == VLC_FAIL) {
    fprintf(stderr, "*** invalid macroblock type\n");
    return -1;
  }
  
  DPRINTF(5, "spatial_temporal_weight_code_flag: %01x\n", 
	  !!(mb.modes.macroblock_type & SPATIAL_TEMPORAL_WEIGHT_CODE_FLAG));

  if((mb.modes.macroblock_type & SPATIAL_TEMPORAL_WEIGHT_CODE_FLAG) &&
     (1 /*spatial_temporal_weight_code_table_index != 0*/)) {
    mb.modes.spatial_temporal_weight_code 
      = GETBITS(2, "spatial_temporal_weight_code");
  }

  if((mb.modes.macroblock_type & MACROBLOCK_MOTION_FORWARD) ||
     (mb.modes.macroblock_type & MACROBLOCK_MOTION_BACKWARD)) {
    if(pic.coding_ext.picture_structure == PIC_STRUCT_FRAME_PICTURE) {
      if(pic.coding_ext.frame_pred_frame_dct != 0) {
	/* frame_motion_type omitted from the bitstream */
	mb.modes.frame_motion_type = 0x2;
      } else {
	mb.modes.frame_motion_type = GETBITS(2, "frame_motion_type");
	if(mb.modes.frame_motion_type == 0x0) {
	  fprintf(stderr, "*** invalid frame motion type\n");
	  return -1;
	}
      }
    } else {
      mb.modes.field_motion_type = GETBITS(2, "field_motion_type");
      if(mb.modes.field_motion_type == 0x0) {
	fprintf(stderr, "*** invalid field motion type\n");
	return -1;
      }
    }
  }

  /* if(decode_dct_type) */
  if((pic.coding_ext.picture_structure == PIC_STRUCT_FRAME_PICTURE) &&
     (pic.coding_ext.frame_pred_frame_dct == 0) &&
     (mb.modes.macroblock_type & (MACROBLOCK_INTRA | MACROBLOCK_PATTERN))) {
    mb.modes.dct_type = GETBITS(1, "dct_type");
  } else {
    /* Table 6-19. Value of dct_type if dct_type is not in the bitstream.
       pic.coding_ext.frame_pred_frame_dct == 1 then mb.modes.dct_type = 0
       else dct_type is unused, either field picture or mb not coded */
    mb.modes.dct_type = 0;
  }

  return 0;
}



/* 6.2.4 Slice */
/* 6.2.5 Macroblock */
void mpeg2_slice(void)
{
  DPRINTFI(3, "slice()\n");
  DINDENT(2);
  
  reset_dc_dct_pred();
  reset_PMV();

  slice_data.slice_vertical_position = GETBITS(32, "slice_start_code") & 0xff;
  //  fprintf(stderr, "slice: %d\n", slice_data.slice_vertical_position);
  seq.mb_row = slice_data.slice_vertical_position - 1;
#if 0
  if(seq.vertical_size > 2800) {
    slice_data.slice_vertical_position_extension 
      = GETBITS(3, "slice_vertical_position_extension");
    seq.mb_row = (slice_data.slice_vertical_position_extension << 7) +
      slice_data.slice_vertical_position - 1;
  }
#endif
  

#if 0
  //TODO
  if(0) {//sequence_scalable_extension_present) {
    if(0) { //scalable_mode == DATA_PARTITIONING) {
      slice_data.priority_breakpoint = GETBITS(7, "priority_breakpoint");
    }
  }
#endif
  
  mb.quantiser_scale
    = q_scale[pic.coding_ext.q_scale_type][GETBITS(5, "quantiser_scale_code")];

  if(nextbits(1) == 1) {
    slice_data.intra_slice_flag = GETBITS(1, "intra_slice_flag");
    slice_data.intra_slice = GETBITS(1, "intra_slice");
    slice_data.reserved_bits = GETBITS(7, "reserved_bits");
    while(nextbits(1) == 1) {
      slice_data.extra_bit_slice = GETBITS(1, "extra_bit_slice");
      slice_data.extra_information_slice = 
	GETBITS(8, "extra_information_slice");
    }
  }
  slice_data.extra_bit_slice = GETBITS(1, "extra_bit_slice");
  
  
  DPRINTFI(3, "macroblocks()\n");
  DINDENT(2);
  
  /* Flag that this is the first macroblock in the slice */
  /* seq.mb_column starts at 0 */
  seq.mb_column = -1;
  
  do {
    unsigned int tmp;
    unsigned int macroblock_address_increment = 0;
    
    while(nextbits(11) == 0x00f) {
      GETBITS(11, "macroblock_stuffing");
    }

    while(nextbits(11) == 0x008) {
      GETBITS(11, "macroblock_escape");
      macroblock_address_increment += 33;
    }
    //    fprintf(stderr, "inc1: %d\n", macroblock_address_increment);
    //    fprintf(stderr, "vlc: %08x\n", nextbits(32));
    tmp = get_vlc(table_b1, "macroblock_address_increment");
    if(tmp != 0x8000) {
      macroblock_address_increment += tmp;
    } else {
      fprintf(stderr, "*mai error\n");
      macroblock_address_increment += 1;
    }
    /*
    macroblock_address_increment 
      += get_vlc(table_b1, "macroblock_address_increment");
    */
    //    fprintf(stderr, "inc2: %d\n", macroblock_address_increment);
    

    if(seq.mb_column == -1) {
      /* a slice never span rows. */
      seq.mb_column += macroblock_address_increment;

      /* The first macroblock is coded. I.e. no skipped blocks, they must have
	 been in another slice with the same startcode (vertical position). */ 
      macroblock_address_increment = 1;
    } else {
      /* a slice never span rows. */
      seq.mb_column += macroblock_address_increment;
    }

    /* Subtract one to get the number of skipped blocks instead. */
    macroblock_address_increment -= 1;
	
    DPRINTFI(4, " Macroblock: %d, row: %d, col: %d\n",
	     (seq.mb_row * seq.mb_width) + seq.mb_column,
	     seq.mb_row,
	     seq.mb_column);
    
#ifdef DEBUG
    if(macroblock_address_increment) {
      DPRINTF(3, "Skipped %d macroblocks\n",
	      macroblock_address_increment + 1);
    }
#endif
    
    /* 7.6.6 Skipped Macroblocks */
    if(macroblock_address_increment) {
      /* Skipped blocks never have any DCT coefficients (pattern). */
      
      switch(pic.header.picture_coding_type) {
      case PIC_CODING_TYPE_P:
	DPRINTFI(4, "skipped in P-picture\n");
	/* In a P-picture when a macroblock is skipped */
	reset_PMV();
	reset_vectors();
	if(pic.coding_ext.picture_structure == PIC_STRUCT_FRAME_PICTURE) {
	/* mlib is broken!!! 
	   {
	     int x = (seq.mb_column-macroblock_address_increment);
	     int y = seq.mb_row;
	     int offs_y = x * 16 + y * 16 * seq.mb_width * 16;
	     int offs_uv = x * 8 + y *  8 * (seq.mb_width * 16)/2;
	     mlib_VideoCopyRef_U8_U8(&dst_image->y[offs_y],
				     &fwd_ref_image->y[offs_y], 
				     macroblock_address_increment*16, 
				     16, (seq.mb_width * 16));
	     mlib_VideoCopyRef_U8_U8(&dst_image->u[offs_uv],
				     &fwd_ref_image->u[offs_uv],
				     macroblock_address_increment*8,
				     8, (seq.mb_width * 16)/2);
	     mlib_VideoCopyRef_U8_U8(&dst_image->v[offs_uv],
				     &fwd_ref_image->v[offs_uv],
				     macroblock_address_increment*8,
				     8, (seq.mb_width * 16)/2);
          }
	*/
#if HAVE_ALTIVEC
	  const int x = seq.mb_column - macroblock_address_increment;
	  const int y = seq.mb_row;
	  const int offs_y = x * 16 + y * 16 * seq.mb_width * 16;
	  const int offs_uv = x * 8 + y *  8 * seq.mb_width *  8;
	  mlib_VideoCopyRef_U8_U8_16x16_multiple(&dst_image->y[offs_y],
						 &fwd_ref_image->y[offs_y],
						 seq.mb_width * 16,
						 macroblock_address_increment);
	  mlib_VideoCopyRef_U8_U8_8x8_multiple(&dst_image->u[offs_uv],
					       &fwd_ref_image->u[offs_uv],
					       seq.mb_width * 8,
					       macroblock_address_increment);
	  mlib_VideoCopyRef_U8_U8_8x8_multiple(&dst_image->v[offs_uv],
					       &fwd_ref_image->v[offs_uv],
					       seq.mb_width * 8,
					       macroblock_address_increment);
#else
	  unsigned int x, y;
	  /* 7.6.6.2 P frame picture */
	  x = (seq.mb_column-macroblock_address_increment)*16;
	  //	  fprintf(stderr, "x: %d, inc: %d, row: %d\n",
	  //		  x, macroblock_address_increment, seq.mb_row);
	  for(y = seq.mb_row*16; y < (seq.mb_row+1)*16; y++) {
	    memcpy(&dst_image->y[y*(seq.mb_width*16)+x], 
		   &fwd_ref_image->y[y*(seq.mb_width*16)+x], 
		   macroblock_address_increment*16);
	  }
	  x = (seq.mb_column-macroblock_address_increment)*8;
	  for(y = seq.mb_row*8; y < (seq.mb_row+1)*8; y++) {
	    memcpy(&dst_image->u[y*(seq.mb_width*16)/2+x], 
		   &fwd_ref_image->u[y*(seq.mb_width*16)/2+x], 
		   macroblock_address_increment*8);
	  }
	  for(y = seq.mb_row*8; y < (seq.mb_row+1)*8; y++) {
	    memcpy(&dst_image->v[y*(seq.mb_width*16)/2+x], 
		   &fwd_ref_image->v[y*(seq.mb_width*16)/2+x], 
		   macroblock_address_increment*8);
	  }
#endif
	} else {
	  // Optimize this case too?  Maybe move all this to video_motion
	  unsigned int old_col = seq.mb_column;
	  /* 7.6.6.1 P field picture*/
	  // Are these two needed/wanted?
	  mb.modes.macroblock_type |= MACROBLOCK_MOTION_FORWARD;
	  mb.modes.macroblock_type &= ~MACROBLOCK_MOTION_BACKWARD;	
	  
	  mb.prediction_type = PRED_TYPE_FIELD_BASED;
	  mb.motion_vector_count = 1;
	  mb.mv_format = MV_FORMAT_FIELD;
	  mb.motion_vertical_field_select[0][0] = 
	    (pic.coding_ext.picture_structure == PIC_STRUCT_TOP_FIELD ? 0 : 1);
	  
	  /* Set mb_column so that motion_comp will use the right adress */
	  for(seq.mb_column = seq.mb_column-macroblock_address_increment;
	      seq.mb_column < old_col; seq.mb_column++) {
	    motion_comp();
	  }
	  seq.mb_column = old_col;
	}
	break;
      
      case PIC_CODING_TYPE_B:
	DPRINTFI(4, "skipped in B-frame\n");
	{
	  unsigned int old_col = seq.mb_column;
	  // The previos macroblocks vectors are used.
	  if(pic.coding_ext.picture_structure == PIC_STRUCT_FRAME_PICTURE) {
	    /* 7.6.6.4  B frame picture */
	    mb.prediction_type = PRED_TYPE_FRAME_BASED;
	    mb.motion_vector_count = 1;
	    mb.mv_format = MV_FORMAT_FRAME;
	  } else {
	    /* 7.6.6.3  B field picture */
	    mb.prediction_type = PRED_TYPE_FIELD_BASED;
	    mb.motion_vector_count = 1;
	    mb.mv_format = MV_FORMAT_FIELD;
	    mb.motion_vertical_field_select[0][0] = 
	      pic.coding_ext.picture_structure == PIC_STRUCT_TOP_FIELD ? 0 : 1;
	    mb.motion_vertical_field_select[0][1] =
	      pic.coding_ext.picture_structure == PIC_STRUCT_TOP_FIELD ? 0 : 1;
	    //exit_program(1); 
	    // FIXME ??? Are we correct for this case now? 
	  }
	  
	  /* Set mb_column so that motion_comp will use the right adress */
	  for(seq.mb_column = seq.mb_column-macroblock_address_increment;
	      seq.mb_column < old_col; seq.mb_column++) {
	    motion_comp();
	  }
	  seq.mb_column = old_col;
	}
	break;
	
      default:
	fprintf(stderr, "*** skipped blocks in I-picture\n");
	//return; ?? 
	break;
      }
      
      reset_dc_dct_pred();      
    }
    
    if(macroblock_modes() == -1) return;
  
    if(mb.modes.macroblock_type & MACROBLOCK_QUANT) {
      mb.quantiser_scale = 
	q_scale[pic.coding_ext.q_scale_type][GETBITS(5, "quantiser_scale_code")];
    }
    
    if((mb.modes.macroblock_type & MACROBLOCK_MOTION_FORWARD) ||
       ((mb.modes.macroblock_type & MACROBLOCK_INTRA) &&
	pic.coding_ext.concealment_motion_vectors)) {
      motion_vectors(0);
    }
    if(mb.modes.macroblock_type & MACROBLOCK_MOTION_BACKWARD) {
      motion_vectors(1);
    }
    if((mb.modes.macroblock_type & MACROBLOCK_INTRA) && 
       pic.coding_ext.concealment_motion_vectors) {
      marker_bit();
    }
    
    
    /* All motion vectors for the block has been decoded. Update predictors */
    
    if(mb.modes.macroblock_type & MACROBLOCK_INTRA) {
      /* Whenever an intra macroblock is decoded which has
	 no concealment motion vectors */
      if(pic.coding_ext.concealment_motion_vectors == 0) {
	reset_PMV();
	DPRINTF(4, "* 1\n");
      } else {
	pic.PMV[1][0][1] = pic.PMV[0][0][1];
	pic.PMV[1][0][0] = pic.PMV[0][0][0];
      }
    } else {
      DPRINTFI(4, "non_intra macroblock\n");
      reset_dc_dct_pred();  
      switch(mb.prediction_type) {
      case PRED_TYPE_FIELD_BASED:
	/* When used in a PIC_STRUCT_FRAME_PICTURE nothing is to be updated. */
	if(pic.coding_ext.picture_structure == PIC_STRUCT_FRAME_PICTURE)
	  break;
      case PRED_TYPE_FRAME_BASED:
	/* When used in a PIC_STRUCT_*_FIELD nothing is to be updated.
	   There is no need to test since it can't be used in these pictures.*/
	if(mb.modes.macroblock_type & MACROBLOCK_MOTION_FORWARD) {
	  pic.PMV[1][0][1] = pic.PMV[0][0][1];
	  pic.PMV[1][0][0] = pic.PMV[0][0][0];
	}
	if(mb.modes.macroblock_type & MACROBLOCK_MOTION_BACKWARD) {
	  pic.PMV[1][1][1] = pic.PMV[0][1][1];
	  pic.PMV[1][1][0] = pic.PMV[0][1][0];
	}
	if(pic.coding_ext.frame_pred_frame_dct != 0) {
	  if(((mb.modes.macroblock_type & MACROBLOCK_MOTION_FORWARD) == 0) &&
	     ((mb.modes.macroblock_type & MACROBLOCK_MOTION_BACKWARD) == 0)) {
	    reset_PMV();
	    DPRINTF(4, "* 2\n");
	  }
	}
	break;
      case PRED_TYPE_16x8_MC:
#ifdef DEBUG
	if(pic.coding_ext.picture_structure == PIC_STRUCT_FRAME_PICTURE) {
	  fprintf(stderr, "*** invalid pred_type\n");
	  return;
	  //exit_program(1);
	}
#endif
	break;
      case PRED_TYPE_DUAL_PRIME:
	if(mb.modes.macroblock_type & MACROBLOCK_MOTION_FORWARD) {
	  pic.PMV[1][0][1] = pic.PMV[0][0][1];
	  pic.PMV[1][0][0] = pic.PMV[0][0][0];
	}
	break;
      default:
	fprintf(stderr, "*** invalid pred_type\n");
	return;
	//exit_program(1);
	break;
      }
    }
    
    
    /*** 7.6.3.5 Prediction in P-pictures ***/
    if(pic.header.picture_coding_type == PIC_CODING_TYPE_P) {
      /* In a P-picture when a non-intra macroblock is decoded
	 in which macroblock_motion_forward is zero */
      if((!(mb.modes.macroblock_type & MACROBLOCK_MOTION_FORWARD)) && 
	 (!(mb.modes.macroblock_type & MACROBLOCK_INTRA))) {
	DPRINTF(4, "prediction mode Frame-base, \n" 
		"resetting motion vector predictor and motion vector\n");
	/* 7.6.3.4 */
	reset_PMV();
	if(pic.coding_ext.picture_structure == PIC_STRUCT_FRAME_PICTURE) {
	  mb.prediction_type = PRED_TYPE_FRAME_BASED;
	  mb.motion_vector_count = 1;
	  mb.mv_format = MV_FORMAT_FRAME;
	} else {
	  mb.prediction_type = PRED_TYPE_FIELD_BASED;
	  mb.motion_vector_count = 1;
	  mb.mv_format = MV_FORMAT_FIELD;
	  mb.motion_vertical_field_select[0][0] =
	    pic.coding_ext.picture_structure == PIC_STRUCT_TOP_FIELD ? 0 : 1;
	}
	mb.modes.macroblock_type |= MACROBLOCK_MOTION_FORWARD;
	mb.vector[0][0][0] = 0;
	mb.vector[0][0][1] = 0;
	mb.vector[1][0][0] = 0;
	mb.vector[1][0][1] = 0;
      }
    }
    
    
    if(mb.modes.macroblock_type & MACROBLOCK_INTRA) {
      unsigned int block_count = 6;
      unsigned int i;
      
      /* Table 6-20 block_count as a function of chroma_format */
#ifdef DEBUG
      if(seq.ext.chroma_format == 0x01) {
	block_count = 6;
      } else if(seq.ext.chroma_format == 0x02) {
	block_count = 8;
      } else if(seq.ext.chroma_format == 0x03) {
	block_count = 12;
      }
#endif
      
      /* Intra blocks always have pattern coded for all sub blocks and
	 are writen directly to the output buffers by this code. */
      for(i = 0; i < block_count; i++) {  
	DPRINTF(4, "cbpindex: %d assumed\n", i);
	block_intra(i);
	
	/* Shortcut: write the IDCT data directly into the picture buffer */
	{
	  const int x = seq.mb_column;
	  const int y = seq.mb_row;
	  const int padded_width = seq.mb_width * 16;
	  int d, stride;
	  uint8_t *dst;
	  
	  if(pic.coding_ext.picture_structure == PIC_STRUCT_FRAME_PICTURE) { 
	    if(mb.modes.dct_type) {
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
	    } else {
	      stride = padded_width / 2;
	      if(i == 4)
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
	    } else {
	      stride = padded_width;
	      if(i == 4)
		dst = &dst_image->u[x * 8 + y * 8 * stride];
	      else // i == 5
		dst = &dst_image->v[x * 8 + y * 8 * stride];
	    }
	    if(pic.coding_ext.picture_structure == PIC_STRUCT_BOTTOM_FIELD)
	      dst += stride/2;
	  }
	  mlib_VideoIDCT8x8_U8_S16(dst, (int16_t *)mb.QFS, stride);
	}
      }
    } else { /* Non-intra block */
      
      motion_comp(); // Only motion compensate don't add coefficients.
      
      if(mb.modes.macroblock_type & MACROBLOCK_PATTERN) {
	unsigned int i;
	
	coded_block_pattern();
	
	for(i = 0; i < 6; i++) {
	  if(mb.cbp & (1<<(5-i))) {
	    DPRINTF(4, "cbpindex: %d set\n", i);
	    block_non_intra(i);
	    //mlib_VideoIDCT8x8_S16_S16((int16_t *)mb.QFS, (int16_t *)mb.QFS);
	    motion_comp_add_coeff(i); // IDCT and add done in here..
	  }
	}
#if 0
	if(seq.ext.chroma_format == 0x02) {
	  for(i = 6; i < 8; i++) {
	    if(mb.coded_block_pattern_1 & (1<<(7-i))) {
	      block_non_intra(i);
	      fprintf(stderr, "ni seq.ext.chroma_format == 0x02\n");
	      //exit_program(1);
	    }
	  }
	}
	if(seq.ext.chroma_format == 0x03) {
	  for(i = 6; i < 12; i++) {
	    if(mb.coded_block_pattern_2 & (1<<(11-i))) {
	      block_non_intra(i);
	      fprintf(stderr, "ni seq.ext.chroma_format == 0x03\n");
	      //exit_program(1);
	    }
	  }
	}
#endif
      }
    }
  } while(((seq.mb_column + 1) < seq.mb_width) && (nextbits(23) != 0));
  
  DINDENT(-2);
}


