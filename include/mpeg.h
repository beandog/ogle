#ifndef MPEG_H_INCLUDED
#define MPEG_H_INCLUDED

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


/* program stream 
 * ==============
 */

#define MPEG2_PS_PACK_START_CODE 0x000001BA
#define MPEG2_PS_PROGRAM_END_CODE 0x000001B9
#define MPEG2_PS_SYSTEM_HEADER_START_CODE 0x000001BB
#define MPEG2_PES_PACKET_START_CODE_PREFIX 0x000001

#define MPEG2_PS_PACK_START_CODE_SIZE 32
#define MPEG2_PS_PROGRAM_END_CODE_SIZE 32
#define MPEG2_PS_SYSTEM_HEADER_START_CODE_SIZE 32
#define MPEG2_PES_PACKET_START_CODE_PREFIX_SIZE 24


#define MPEG2_PRIVATE_STREAM_1 0xBD
#define MPEG2_PADDING_STREAM 0xBE
#define MPEG2_PRIVATE_STREAM_2 0xBF

#define MPEG_AUDIO_STREAM      0xC0
#define MPEG_AUDIO_STREAM_MASK 0xE0

#define MPEG_VIDEO_STREAM      0xE0
#define MPEG_VIDEO_STREAM_MASK 0xF0

/* video stream
 * ============
 */

/*
#define MPEG2_VS_START_CODE_PREFIX 0x000001

#define MPEG2_VS_PICTURE_START_CODE   0x00
#define MPEG2_VS_USER_DATA_START_CODE 0xB2
#define MPEG2_VS_SEQUENCE_HEADER_CODE 0xB2
#define MPEG2_VS_SEQUENCE_ERROR_CODE  0xB2
#define MPEG2_VS_EXTENSION_START_CODE 0xB2
#define MPEG2_VS_SEQUENCE_END_CODE    0xB2
#define MPEG2_VS_GROUP_START_CODE     0xB2
*/


/* Table 6-1  Start code values */
#define MPEG2_VS_PICTURE_START_CODE       0x00000100
#define MPEG2_VS_SLICE_START_CODE_LOWEST  0x00000101
#define MPEG2_VS_SLICE_START_CODE_HIGHEST 0x000001AF
#define MPEG2_VS_USER_DATA_START_CODE     0x000001B2
#define MPEG2_VS_SEQUENCE_HEADER_CODE     0x000001B3
#define MPEG2_VS_SEQUENCE_ERROR_CODE      0x000001B4
#define MPEG2_VS_EXTENSION_START_CODE     0x000001B5
#define MPEG2_VS_SEQUENCE_END_CODE        0x000001B7
#define MPEG2_VS_GROUP_START_CODE         0x000001B8

/* Table 6-2. extension_start_code_identifier codes. */
#define MPEG2_VS_SEQUENCE_EXTENSION_ID                  0x1
#define MPEG2_VS_SEQUENCE_DISPLAY_EXTENSION_ID          0x2
#define MPEG2_VS_QUANT_MATRIX_EXTENSION_ID              0x3
#define MPEG2_VS_SEQUENCE_SCALABLE_EXTENSION_ID         0x5
#define MPEG2_VS_PICTURE_DISPLAY_EXTENSION_ID           0x7
#define MPEG2_VS_PICTURE_CODING_EXTENSION_ID            0x8
#define MPEG2_VS_PICTURE_SPATIAL_SCALABLE_EXTENSION_ID  0x9
#define MPEG2_VS_PICTURE_TEMPORAL_SCALABLE_EXTENSION_ID 0xA

#endif /* MPEG_H_INCLUDED */
