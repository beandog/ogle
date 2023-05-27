#ifndef VIDEO_TABLES_H_INCLUDED
#define VIDEO_TABLES_H_INCLUDED

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

#include "video_types.h"

/* previously used by B-14 */
#define DCT_DC_FIRST          0
#define DCT_DC_SUBSEQUENT     1

#define VLC_FAIL           0x8000
#define VLC_ESCAPE         255
#define VLC_END_OF_BLOCK   254 

/* Table B-12 --- Variable length codes for dct_dc_size_luminance */
extern const vlc_table_t table_b12[];

/* Table B-13 --- Variable length codes for dct_dc_size_chrominance */
extern const vlc_table_t table_b13[];


/* Table B-14 --- DCT coefficients Table zero */
extern const vlc_rl_table table_b14[];

/* Table B-15 --- DCT coefficients Table one */
extern const vlc_rl_table table_b15[];


/* Used by Table B-2/3/4 */
#define MACROBLOCK_QUANT                    0x20
#define MACROBLOCK_MOTION_FORWARD           0x10
#define MACROBLOCK_MOTION_BACKWARD          0x08
#define MACROBLOCK_PATTERN                  0x04
#define MACROBLOCK_INTRA                    0x02
#define SPATIAL_TEMPORAL_WEIGHT_CODE_FLAG   0x01

/* Table B-2  Variable length codes for macroblock_type in I-pictures */
extern const vlc_table_t table_b2[];

/* Table B-3  Variable length codes for macroblock_type in P-pictures */
extern const vlc_table_t table_b3[];

/* Table B-4  Variable length codes for macroblock_type in B-pictures */
extern const vlc_table_t table_b4[];

#define VLC_MACROBLOCK_ESCAPE 255

/* Table B-1 --- Variable length codes for macroblock_address_increment */
extern const vlc_table_t table_b1[];

/* Table B-10 --- Variable length codes for motion_code */
extern const vlc_table_t table_b10[];

/* Table B-11  Variable length codes for dmvector[t] */
extern const vlc_table_t table_b11[];

/* Table B-9 --- Variable length codes for coded_block_pattern. */
extern const vlc_table_t table_b9[];

/* Derived from Figure 7-1. Definition of scan[a][v][u] */
extern const uint8_t inverse_scan[2][64];

/* Table 7-6. Relation between quantiser_scale and quantiser_scale_code */
extern const uint8_t q_scale[2][32];



/* 6.3.7 Quant matrix extension */
extern const int8_t default_intra_inverse_quantiser_matrix[8][8];

extern const int8_t default_non_intra_inverse_quantiser_matrix[8][8];




/* Table B-14, DCT coefficients table zero,
 * codes 0100 ... 1xxx (used for first (DC) coefficient)
 */
extern const DCTtab DCTtabfirst[12];

/* Table B-14, DCT coefficients table zero,
 * codes 0100 ... 1xxx (used for all other coefficients)
 */
extern const DCTtab DCTtabnext[12];

/* Table B-14, DCT coefficients table zero,
 * codes 000001xx ... 00111xxx
 */
extern const DCTtab DCTtab0[60];

/* Table B-15, DCT coefficients table one,
 * codes 000001xx ... 11111111
*/
extern const DCTtab DCTtab0a[252];

/* Table B-14, DCT coefficients table zero,
 * codes 0000001000 ... 0000001111
 */
extern const DCTtab DCTtab1[8];

/* Table B-15, DCT coefficients table one,
 * codes 000000100x ... 000000111x
 */
extern const DCTtab DCTtab1a[8];

/* Table B-14/15, DCT coefficients table zero / one,
 * codes 000000010000 ... 000000011111
 */
extern const DCTtab DCTtab2[16];

/* Table B-14/15, DCT coefficients table zero / one,
 * codes 0000000010000 ... 0000000011111
 */
extern const DCTtab DCTtab3[16];

/* Table B-14/15, DCT coefficients table zero / one,
 * codes 00000000010000 ... 00000000011111
 */
extern const DCTtab DCTtab4[16];

/* Table B-14/15, DCT coefficients table zero / one,
 * codes 000000000010000 ... 000000000011111
 */
extern const DCTtab DCTtab5[16];

/* Table B-14/15, DCT coefficients table zero / one,
 * codes 0000000000010000 ... 0000000000011111
 */
extern const DCTtab DCTtab6[16];

#endif /* VIDEO_TABLES_H_INCLUDED */
