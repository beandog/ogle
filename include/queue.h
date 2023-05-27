#ifndef QUEUE_H_INCLUDED
#define QUEUE_H_INCLUDED

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

#include "ogle/dvd.h"
#include "ogle/dvdevents.h"
#include "ogle/msgevents.h"

typedef enum {
  PacketType_MPEG1 = 0,
  PacketType_PES   = 1
} PacketType_t;

typedef struct {
  uint64_t PTS;
  uint64_t DTS;
  uint64_t SCR_base;
  uint16_t SCR_ext;
  int in_use;
  uint8_t PTS_DTS_flags;
  uint8_t SCR_flags;
  int scr_nr;
  uint32_t packet_data_offset;
  uint32_t packet_data_len;
  char *q_addr;
  char filename[PATH_MAX+1]; // hack for mmap
  int flowcmd;
  PacketType_t packet_type;
  uint32_t packet_offset;
} data_elem_t;


typedef struct {
  int y_offset; //[480][720];  //y-component image
  int u_offset; //[480/2][720/2]; //u-component
  int v_offset; //[480/2][720/2]; //v-component
  uint16_t start_x, start_y;
  uint16_t horizontal_size;
  uint16_t vertical_size;
  uint16_t padded_width, padded_height;
  uint16_t display_start_x;
  uint16_t display_start_y;
  uint16_t display_width;
  uint16_t display_height;
  int sar_frac_n;
  int sar_frac_d;
  float sar;
} yuv_picture_t;


typedef struct {
  uint64_t PTS;
  uint64_t DTS;
  uint64_t SCR_base;
  uint16_t SCR_ext;
  int displayed;
  int is_reference;
  uint8_t PTS_DTS_flags;
  uint8_t SCR_flags;
  int scr_nr;
  yuv_picture_t picture;
  int picture_off;
  int picture_len;
  long int frame_interval;
  char *q_addr;
} picture_data_elem_t;


typedef struct {
  uint8_t *y; //[480][720];  //y-component image
  uint8_t *u; //[480/2][720/2]; //u-component
  uint8_t *v; //[480/2][720/2]; //v-component
  picture_data_elem_t *info;
} yuv_image_t;

typedef enum {
  DataBufferType_Raw,
  DataBufferType_Video,
  DataBufferType_Audio,
  DataBufferType_MpegPES,
  DataBufferType_MpegES,
  DataBufferType_MpegPS,
  DataBufferType_MpegTS
} DataBufferType_t;

typedef struct {
  DataBufferType_t type;
  int format;
  int width;
  int height;
  int stride;
} DataBufferVideoInfo_t;

typedef struct {
  DataBufferType_t type;
  int format;
} DataBufferAudioInfo_t;

typedef struct {
  DataBufferType_t type;
} DataBufferRawInfo_t;

typedef union {
  DataBufferType_t type;
  DataBufferRawInfo_t raw;
  DataBufferVideoInfo_t video;
  DataBufferAudioInfo_t audio;
} DataBufferInfo_t;

typedef struct {
  int shmid;
  DataBufferInfo_t info;
  int nr_of_dataelems;
  int write_nr;
  int read_nr;
  int buffer_start_offset;
  int buffer_size;
  int pad;         //8 byte alignment needed for data elements
} data_buf_head_t;

typedef struct {
  int data_elem_index;
  int in_use;
} q_elem_t;


#define BUFS_FULL 0
#define BUFS_EMPTY 1
typedef struct {
  int qid;
  int data_buf_shmid;
  int nr_of_qelems;
  int write_nr;
  int read_nr;
  MsgEventClient_t writer;
  MsgEventClient_t reader;
  int writer_requests_notification; //writer sets/unsets this
  int reader_requests_notification; //reader sets/unsets this 
} q_head_t;


typedef struct _data_q_t {
  int in_use;
  int eoq;
  q_head_t *q_head;
  q_elem_t *q_elems;
  data_buf_head_t *data_head;
  picture_data_elem_t *data_elems;
  yuv_image_t *image_bufs;
#ifdef HAVE_XV
  yuv_image_t *reserv_image;
#endif
  struct _data_q_t *next;
} data_q_t;


#endif /* QUEUE_H_INCLUDED */
