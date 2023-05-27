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

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/msg.h>
#include <errno.h>

#include <ogle/msgevents.h>

#include <dvdread/dvd_reader.h>

#include "debug_print.h"
#include "ogle_endian.h"
#include "programstream.h"
#include "common.h"
#include "queue.h"
#include "mpeg.h"


#ifndef SHM_SHARE_MMU
#define SHM_SHARE_MMU 0
#endif

typedef enum {
  STREAM_NOT_REGISTERED = 0,
  STREAM_DISCARD = 1,
  STREAM_DECODE = 2,
  STREAM_MUTED = 3
} stream_state_t;
  
typedef struct {
  int infile;
  FILE *file;
  int shmid;
  char *shmaddr;
  stream_state_t state; // not_registerd, in_use, muted, ...
} buf_data_t;

buf_data_t id_reg[256];
buf_data_t id_reg_ps1[256];

int register_id(uint8_t id, int subtype);
int id_registered(uint8_t id, uint8_t subtype);
int init_id_reg(stream_state_t default_state);
//int wait_for_msg(mq_cmdtype_t cmdtype);
//int eval_msg(mq_cmd_t *cmd);
int get_buffer(int size);
int attach_decoder_buffer(uint8_t stream_id, uint8_t subtype, int shmid);
int id_stat(uint8_t id, uint8_t subtype);
char *id_qaddr(uint8_t id, uint8_t subtype);
FILE *id_file(uint8_t id, uint8_t subtype);
void id_add(uint8_t stream_id, uint8_t subtype, stream_state_t state, int shmid, char *shmaddr, FILE *file);
int put_in_q(char *q_addr, int off, int len, uint8_t PTS_DTS_flags,
	     uint64_t PTS, uint64_t DTS, int is_new_file, int extra_cmd,
	     PacketType_t packet_type, uint32_t packet_offset);
int attach_buffer(int shmid, int size);
//int chk_for_msg(void);
void loadinputfile(char *infilename);
void add_to_demux_q(MsgEvent_t *ev);
static void handle_events(MsgEvent_t *ev);
int id_infile(uint8_t id, uint8_t subtype);
void id_setinfile(uint8_t id, uint8_t subtype, int newfile);
uint8_t type_registered(uint8_t id, uint8_t subtype);
int switch_to_stream(uint8_t id, uint8_t subtype);
int switch_from_to_stream(uint8_t oldid, uint8_t oldsubtype,
			  uint8_t newid, uint8_t newsubtype);
int id_has_output(uint8_t stream_id, uint8_t subtype);
int id_get_output(uint8_t id, int subtype);

typedef struct {
  uint8_t *buf_start;
  int len;
  int in_use;
} q_elem;

static int msgqid = -1;

static MsgEventQ_t *msgq;

int scr_discontinuity = 0;
uint64_t SCR_base;
uint16_t SCR_ext;
uint8_t SCR_flags;


int packnr = 0;


#define MPEG1 0x0
#define MPEG2 0x1

uint8_t    *disk_buf;

int shmid;
char *shmaddr;
int shmsize;

static int ctrl_data_shmid;
static ctrl_data_t *ctrl_data;
static ctrl_time_t *ctrl_time;

char *data_buf_addr;


int off_from;
int off_to;
int demux_cmd;

extern char *optarg;
extern int   optind, opterr, optopt;

/* Variables for getbits */
unsigned int bits_left = 64;
uint64_t     cur_word = 0;

unsigned int nextbits(unsigned int nr_of_bits);

int synced = 0;

int audio       = 0;
int debug       = 0;

char *program_name;
int dlevel;
FILE *video_file;
FILE *audio_file;
FILE *subtitle_file;
FILE *ps1_file;
int video_stream = -1;

int   infilefd;
void* infileaddr;
long  infilelen;
uint32_t offs;

char cur_filename[PATH_MAX+1];
int new_file;

// #define DEBUG



#ifdef STATS
    static uint32_t stat_video_unaligned_packet_offset = 0;
    static uint32_t stat_video_unaligned_packet_end    = 0;
    static uint32_t stat_video_n_packets               = 0;
    static uint32_t stat_audio_n_packets               = 0;
    static uint32_t stat_subpicture_n_packets          = 0;
    static uint32_t stat_n_packets                     = 0;
#endif //STATS

void usage(void)
{
  fprintf(stderr, "Usage: %s [-v <video file>] [-a <audio file>] [-s <subtitle file> -i <subtitle_id>] [-p subid=<substreambaseid>,nr=<streamnr>,file=<outputfile>] [-d <debug level>] <input file>\n", 
	  program_name);
}


/* 2.3 Definition of bytealigned() function */
int bytealigned(void)
{
  return !(bits_left%8);
}

#ifdef DEBUG
#define GETBITS(a,b) getbits(a,b)
#else
#define GETBITS(a,b) getbits(a)
#endif


#ifdef DEBUG
uint32_t getbits(unsigned int nr, char *func)
#else
static inline uint32_t getbits(unsigned int nr)
#endif
{
  uint32_t result;
  
  result = (cur_word << (64-bits_left)) >> 32;
  result = result >> (32-nr);
  bits_left -=nr;
  if(bits_left <= 32) {
    uint32_t new_word = FROM_BE_32(*(uint32_t *)(&disk_buf[offs]));
    offs+=4;
    cur_word = (cur_word << 32) | new_word;
    bits_left = bits_left+32;
  }
  
  DPRINTF(5, "%s getbits(%u): %0*i, 0x%0*x, ", 
             func, nr, (nr-1)/3+1, result, (nr-1)/4+1, result);
  DPRINTBITS(6, nr, result);
  DPRINTF(5, "\n");
  
  return result;
}


static inline void drop_bytes(int len)
{  
  uint rest;
  uint todo;

  // This is always byte-aligned. Trust us.

  len  -= bits_left/8;
  rest  = len % 4;
  offs += (len - rest);
  todo  = bits_left+rest*8;

  while(todo > 32) {
    GETBITS(32, "skip");
    todo -= 32;
  }
  GETBITS(todo, "skip");

  return;
}


#if 1
static dvd_reader_t *dvdroot;
static dvd_file_t *dvdfile;

int dvd_open_root(char *path)
{
  
  if((dvdroot = DVDOpen(path)) == NULL) {
    FATAL("%s", "Couldn't open dvd\n");
    exit(1);
  }
  
  return 0;
} 

int dvd_close_root(void) {

  DVDClose(dvdroot);
  
  return 0;
}

static int dvd_file_num = -1;
static dvd_read_domain_t dvd_file_dom = -1;

int dvd_change_file(int titlenum, dvd_read_domain_t domain)
{
  // If same as current do nothing
  if(titlenum == dvd_file_num && domain == dvd_file_dom) {
    return 0;
  }
  if(dvdfile != NULL && dvdroot != NULL) {
    //fprintf(stderr, "demux: closing open file when opening new\n");
    DVDCloseFile(dvdfile);
  }
  if((dvdfile = DVDOpenFile(dvdroot, titlenum, domain)) == NULL) {
    FATAL("%s", "Couldn't open dvdfile\n");
    exit(1);
  }
  dvd_file_num = titlenum;
  dvd_file_dom = domain;

  return 0;
}

void dvd_close_file(void)
{
  DVDCloseFile(dvdfile);
  dvd_file_num = -1;
  dvd_file_dom = -1;  
}


int dvd_read_block(char *buf, int boffset, int nblocks)
{
  int blocks_read;
  int tries = 0;

  do {
    blocks_read = DVDReadBlocks(dvdfile, boffset, nblocks, buf);
    
    switch(blocks_read) {
    case -1:
      FATAL("%s", "dvdreadblocks failed\n");
      exit(1);
    case 0:
      WARNING("%s", "dvdreadblocks returned 0\n");
      if(tries > 3) {
	fprintf(stderr, "\n\n" 
"Ogle can't read any data.\n"
"Make sure that the CSS authentication works correctly.\n"
"See also the FAQ at http://www.dtek.chalmers.se/~dvd/faq.shtml\n"
"Three common problems are:\n"
"no write permission on you DVD drives device node.\n"
"you are trying to play a DVD from a region other than the one \n"
"of the DVD drive.\n"
"or you have never set the region on the drive.\n\n"
"For setting the region; a program called regionset will do this.\n"
"Search for dvd_disc or dvdkit on freshmeat.net\n"
"Beware that you can only sset the region 5 times!\n\n");
	exit(1);
      }
      tries++;
      break;
    default:
      break;
    }
    if(blocks_read != nblocks) {
      WARNING("dvdreadblocks only got %d, wanted %d\n",
	      blocks_read, nblocks);
    }
    buf += blocks_read * 2048;
    nblocks -= blocks_read;
    boffset += blocks_read;
  } while(nblocks > 0);
  return 0;
}

int fill_buffer(int title, dvd_read_domain_t domain, int boffset, int nblocks)
{
  int next_write_pos;
  static int free_block_offs = 0;
  data_buf_head_t *data_buf_head;
  data_elem_t *data_elems;
  int data_elem_nr;
  int buf_empty = 0;
  int first_data_elem_nr;
  int off;
  int blocks_in_buf;
  int size;

  //fprintf(stderr, "demux: fill_buffer: title: %d, domain: %d, off: %d, blocks: %d\n", title, domain, boffset, nblocks);
  
  //  get_next_demux_range(&title, &domain, &boffset, &blocks);
  
  data_buf_head = (data_buf_head_t *)data_buf_addr;
  if(data_buf_head == NULL) {
    fprintf(stderr, "*demux: fill_buffer, no buffer\n");
    exit(1);
  }
  blocks_in_buf = data_buf_head->buffer_size/2048;
  //fprintf(stderr, "blocks_in_buf: %d\n", blocks_in_buf);

  data_elems = (data_elem_t *)(data_buf_addr+sizeof(data_buf_head_t));
  data_elem_nr = data_buf_head->write_nr;
  
  first_data_elem_nr = data_elem_nr;
  //fprintf(stderr, "first_data_elem_nr: %d\n", first_data_elem_nr);

  while(1) {
    if(data_elems[data_elem_nr].in_use || buf_empty) {
      /* this block is in use, check if we have enough free space */
      
      /* offset in buffer of the used block we found */
      if(buf_empty) {
	off = blocks_in_buf;
	free_block_offs = 0;
      } else {
	off = data_elems[data_elem_nr].packet_offset / 2048;
      }
      
      if(off < free_block_offs) {
	
	/* if the block in use is before the first free block
	 * we can either use the blocks 
	 * between the first free block and the end of the buffer
	 * or
	 * use the blocks
	 * between the beginning of the buffer and the used block in use
	 */
	
	/* nr of blocks from first free block to end of buffer */
	
	size = blocks_in_buf - free_block_offs;
	next_write_pos = free_block_offs;
	
	if(size < nblocks) {
	  /* nr of blocks from start of buffer to the block in use */ 
	  size = off;
	  next_write_pos = 0;
	}
	
      } else {
	
	/* the block in use is after the first free block in the buffer */
	/* nr of blocks from first free block to the block in use */
	size = off - free_block_offs;
	next_write_pos = free_block_offs;
	
      }
      
      if(size < nblocks) {
	/* the nr of contigously available blocks is too small, 
	 * wait for more free blocks */
	fprintf(stderr, "*demux: SEND A BUG REPORT: need more free space, not implemented\n");
      } else {
	/* we have enough free blocks */
	break;
      }
    }
    
    /* this block is not in use, check next one */
    
    data_elem_nr = 
      (data_elem_nr+1) % data_buf_head->nr_of_dataelems;
    
    /* check if we have looped through all data_elems */
    if(!buf_empty) {
      if(data_elem_nr == first_data_elem_nr) {
	buf_empty = 1;
      }
    }
  }
  
  dvd_change_file(title, domain);
  dvd_read_block(&disk_buf[next_write_pos*2048], boffset, nblocks);
  //fprintf(stderr, "next_write_pos: %d\n", next_write_pos);
  //fprintf(stderr, "dvd_read_block: dst: %u, offset: %d, blocks: %d\n", next_write_pos*2048, boffset, nblocks);

  free_block_offs = next_write_pos + nblocks;
  //fprintf(stderr, "free_block_offs: %d\n", free_block_offs);
  off_from = next_write_pos * 2048;
  off_to = free_block_offs * 2048;
  
  //fprintf(stderr, "off_from: %d, off_to: %d\n", off_from, off_to);
  return 0;
}

#endif


unsigned int nextbits(unsigned int nr_of_bits)
{
  uint32_t result = (cur_word << (64-bits_left)) >> 32;
  DPRINTF(4, "nextbits %08x\n",(result >> (32-nr_of_bits )));
  return result >> (32-nr_of_bits);
}


static inline void marker_bit(void)                                                           {
  if(!GETBITS(1, "markerbit")) {
    WARNING("%s", "Incorrect marker_bit in stream\n");
    //exit(1);
  }
}
 
/* 2.3 Definition of next_start_code() function */
void next_start_code(void)
{ 
  while(!bytealigned()) {
    GETBITS(1, "next_start_code");
  } 
  while(nextbits(24) != 0x000001) {
    GETBITS(8, "next_start_code");
  } 
}

/* Table 2-24 -- Stream_id table */
void dprintf_stream_id (int debuglevel, int stream_id)
{
#ifdef DEBUG
  DPRINTF(debuglevel, "0x%02x ", stream_id);
  if ((stream_id & 0xf0) == 0xb0) // 1011 xxxx
    switch (stream_id & 0x0f) { 
      case 0x8:
        DPRINTF(debuglevel, "[all audio streams]\n");
        return;
      case 0x9:
        DPRINTF(debuglevel, "[all video streams]\n");
        return;
      case 0xc:
        DPRINTF(debuglevel, "[program stream map]\n");
        return;
      case 0xd:
        DPRINTF(debuglevel, "[private_stream_1]\n");
        return;
      case 0xe:
        DPRINTF(debuglevel, "[padding stream]\n");
        return;
      case 0xf:
        DPRINTF(debuglevel, "[private_stream_2]\n");
        return;
    }
  else if ((stream_id & 0xe0) == 0xc0) { // 110x xxxx
    DPRINTF(debuglevel, "[audio stream number %i]\n", stream_id & 0x1f);
    return;
  } else if ((stream_id & 0xf0) == 0xe0) {
    DPRINTF(debuglevel, "[video stream number %i]\n", stream_id & 0x1f);
    return;
  } else if ((stream_id & 0xf0) == 0xf0) {
    switch (stream_id & 0x0f) {
      case 0x0:
        DPRINTF(debuglevel, "[ECM]\n");
        return;
      case 0x1:
        DPRINTF(debuglevel, "[EMM]\n");
        return;
      case 0x2:
        DPRINTF(debuglevel, "[DSM CC]\n");
        return;
      case 0x3:
        DPRINTF(debuglevel, "[ISO/IEC 13522 stream]\n");
        return;
      case 0xf:
        DPRINTF(debuglevel, "[program stream directory]\n");
        return;
      default:
        DPRINTF(debuglevel, "[reserved data stream - number %i]\n",
                stream_id & 0x0f);
        return;
    }
  }
  DPRINTF(debuglevel, "[illegal stream_id]\n");
#else
#endif
}

typedef enum {
  CMD_CTRL,
  CMD_FILE
} cmd_type_t;


typedef struct {
  MsgEventType_t cmd_type;
  union {
    MsgQPlayCtrlEvent_t ctrl;
    char *file;
  } cmd;
} demux_q_t;

static MsgEvent_t demux_q[5];
static int demux_q_start = 0;
static int demux_q_len = 0;

void get_next_demux_q(void)
{
  MsgEvent_t ev;
  MsgEvent_t *q_ev;
  
  int new_demux_range = 0;
  if(id_stat(0xe0, 0) == STREAM_DECODE) {
    if(demux_cmd & FlowCtrlCompleteVideoUnit) {
      put_in_q(id_qaddr(0xe0, 0), 0, 0, 0, 0, 0, 0, demux_cmd, 0, 0);
    }
  }
  while(!new_demux_range) {
    while(!demux_q_len) {
      if(MsgNextEvent(msgq, &ev) != -1) {
	switch(ev.type) {
	default:
	  handle_events(&ev);
	  //fprintf(stderr, "demux: unrecognized command\n");
	  break;
	}
      }
    }
    
    q_ev = &demux_q[demux_q_start];
    
    switch(q_ev->type) {
    case MsgEventQPlayCtrl:
      switch(q_ev->playctrl.cmd) {
      case PlayCtrlCmdPlayFromTo:
	off_from = q_ev->playctrl.from;
	off_to = q_ev->playctrl.to;
	demux_cmd = q_ev->playctrl.flowcmd;
	new_demux_range = 1;
	break;
      default:
	fprintf(stderr, "demux: playctrl cmd not handled\n");
      }
      break;
    case MsgEventQChangeFile:
      loadinputfile(q_ev->changefile.filename);
      //free(q_ev->cmd.file);
      break;
    case MsgEventQDemuxDVD:
      fill_buffer(q_ev->demuxdvd.titlenum, q_ev->demuxdvd.domain,
		  q_ev->demuxdvd.block_offset, q_ev->demuxdvd.block_count);
      new_demux_range = 1;
      demux_cmd = q_ev->demuxdvd.flowcmd;
      break;
    case MsgEventQDemuxDVDRoot:
      dvd_open_root(q_ev->demuxdvdroot.path);
      break;
    default:
      fprintf(stderr, "demux: that's not possible\n");
      break;
    }
    demux_q_start = (demux_q_start+1)%5;
    demux_q_len--;
  }
}

void add_to_demux_q(MsgEvent_t *ev)
{
  int pos;
  
  if(demux_q_len < 5) {
    pos = (demux_q_start + demux_q_len)%5;
    memcpy(&demux_q[pos], ev, sizeof(MsgEvent_t));
    demux_q_len++;
    
  } else {
    fprintf(stderr, "**demux: add_to_demux_q: q full\n");
  }
  
}

/* demuxer state */

int system_header_set = 0;
int stream_open = 0;



void system_header(void)
{
  uint16_t header_length;
  uint32_t rate_bound;
  uint8_t  audio_bound;
  uint8_t  fixed_flag;
  uint8_t  CSPS_flag;
  uint8_t  system_audio_lock_flag;
  uint8_t  system_video_lock_flag;
  uint8_t  video_bound;
  uint8_t  stream_id;
  uint8_t  P_STD_buffer_bound_scale;
  uint16_t P_STD_buffer_size_bound;
  int min_bufsize = 0;
  
  DPRINTF(1, "system_header()\n");

  GETBITS(32, "system_header_start_code");
  header_length          = GETBITS(16,"header_length");
  marker_bit();
  rate_bound             = GETBITS(22,"rate_bound");
  marker_bit();          
  audio_bound            = GETBITS(6,"audio_bound");
  fixed_flag             = GETBITS(1,"fixed_flag");
  CSPS_flag              = GETBITS(1,"CSPS_flag");
  system_audio_lock_flag = GETBITS(1,"system_audio_lock_flag");
  system_video_lock_flag = GETBITS(1,"system_video_lock_flag");
  marker_bit();
  video_bound            = GETBITS(5,"video_bound");
  GETBITS(8, "reserved_byte");

  DPRINTF(1, "header_length:          %hu\n", header_length);
  DPRINTF(1, "rate_bound:             %u [%u bits/s]\n",
          rate_bound, rate_bound*50*8);
  DPRINTF(1, "audio_bound:            %u\n", audio_bound);
  DPRINTF(1, "fixed_flag:             %u\n", fixed_flag);
  DPRINTF(1, "CSPS_flag:              %u\n", CSPS_flag);
  DPRINTF(1, "system_audio_lock_flag: %u\n", system_audio_lock_flag);
  DPRINTF(1, "system_video_lock_flag: %u\n", system_video_lock_flag);
  DPRINTF(1, "video_bound:            %u\n", video_bound);
  
  while(nextbits(1) == 1) {
    stream_id = GETBITS(8, "stream_id");
    GETBITS(2, "11");
    P_STD_buffer_bound_scale = GETBITS(1, "P_STD_buffer_bound_scale");
    P_STD_buffer_size_bound  = GETBITS(13, "P_STD_buffer_size_bound");
    DPRINTF(1, "stream_id:                ");
    dprintf_stream_id(1, stream_id);
    DPRINTF(1, "P-STD_buffer_bound_scale: %u\n", P_STD_buffer_bound_scale);
    DPRINTF(1, "P-STD_buffer_size_bound:  %u [%u bytes]\n",
        P_STD_buffer_size_bound,
	    P_STD_buffer_size_bound*(P_STD_buffer_bound_scale?1024:128));
    min_bufsize+= P_STD_buffer_size_bound*(P_STD_buffer_bound_scale?1024:128);
  }

  if(!system_header_set) {
    system_header_set = 1;
    if(msgqid != -1) {
      // this is now allocated before we start reading
      //get_buffer(min_bufsize);
    }
  }
  
}



int pack_header(void)
{
  uint64_t system_clock_reference;
  uint64_t system_clock_reference_base;
  uint16_t system_clock_reference_extension;
  uint32_t program_mux_rate;
  uint8_t  pack_stuffing_length;
  int i;
  uint8_t fixed_bits;
  int version;
  static uint64_t prev_scr = 0;
  
  DPRINTF(2, "pack_header()\n");

  GETBITS(32,"pack_start_code");
  
  fixed_bits = nextbits(4);
  if(fixed_bits == 0x02) {
    version = MPEG1;
  } else if((fixed_bits >> 2) == 0x01) {
    version = MPEG2;
  } else {
    version = -1;
    fprintf(stderr, "demux: unknown MPEG version\n");
  }
  
  
  switch(version) {
  case MPEG2:
    GETBITS(2, "01");
    
    system_clock_reference_base = 
      GETBITS(3,"system_clock_reference_base [32..30]") << 30;
    marker_bit();
    system_clock_reference_base |= 
      GETBITS(15, "system_clock_reference_base [29..15]") << 15;
    marker_bit();
    system_clock_reference_base |= 
      GETBITS(15, "system_clock_reference_base [14..0]");
    marker_bit();
    system_clock_reference_extension = 
      GETBITS(9, "system_clock_reference_extension");
    /* 2.5.2 or 2.5.3.4 (definition of system_clock_reference) */
    system_clock_reference = 
      system_clock_reference_base * 300 + system_clock_reference_extension;
    SCR_base = system_clock_reference_base;
    SCR_ext = system_clock_reference_extension;
    SCR_flags = 0x3;

    /*
    fprintf(stderr, "**** scr base %llx, ext %x, tot %llx\n",
	    system_clock_reference_base,
	    system_clock_reference_extension,
	    system_clock_reference);
    */

    marker_bit();
    program_mux_rate = GETBITS(22, "program_mux_rate");
    marker_bit();
    marker_bit();
    GETBITS(5, "reserved");
    pack_stuffing_length = GETBITS(3, "pack_stuffing_length");
    for(i=0;i<pack_stuffing_length;i++) {
      GETBITS(8 ,"stuffing_byte");
    }
    DPRINTF(1, "system_clock_reference_base: %llu\n",
	    system_clock_reference_base);
    DPRINTF(1, "system_clock_reference_extension: %u\n",
	    system_clock_reference_extension);
    DPRINTF(1, "system_clock_reference: %llu [%6f s]\n", 
	    system_clock_reference, ((double)system_clock_reference)/27000000.0);

    if(system_clock_reference < prev_scr) {
      scr_discontinuity = 1;
      DNOTE("%s", "Backward scr discontinuity\n");
      DNOTE("system_clock_reference: [%6f s]\n", 
	    ((double)system_clock_reference)/27000000.0);
      DNOTE("prev system_clock_reference: [%6f s]\n", 
	    ((double)prev_scr)/27000000.0);
      
    } else if((system_clock_reference - prev_scr) > 2700000*7) {
      scr_discontinuity = 1;
      DNOTE("%s", "Forward scr discontinuity\n");
      DNOTE("system_clock_reference: [%6f s]\n", 
	    ((double)system_clock_reference)/27000000.0);
      DNOTE("prev system_clock_reference: [%6f s]\n", 
	    ((double)prev_scr)/27000000.0);
    }
    prev_scr = system_clock_reference;
    
    DPRINTF(1, "program_mux_rate: %u [%u bits/s]\n",
	    program_mux_rate, program_mux_rate*50*8);
    DPRINTF(3, "pack_stuffing_length: %u\n",
	    pack_stuffing_length);
    break;
  case MPEG1:
    GETBITS(4, "0010");
    
    system_clock_reference_base = 
      GETBITS(3,"system_clock_reference_base [32..30]") << 30;
    marker_bit();
    system_clock_reference_base |= 
      GETBITS(15, "system_clock_reference_base [29..15]") << 15;
    marker_bit();
    system_clock_reference_base |= 
      GETBITS(15, "system_clock_reference_base [14..0]");
    marker_bit();
    marker_bit();
    program_mux_rate = GETBITS(22, "program_mux_rate");
    marker_bit();
    DPRINTF(2, "system_clock_reference_base: %llu\n",
	    system_clock_reference_base);
    DPRINTF(2, "program_mux_rate: %u [%u bits/s]\n",
	    program_mux_rate, program_mux_rate*50*8);

    // no extension in MPEG-1 ??
    system_clock_reference = system_clock_reference_base * 300;
    SCR_base = system_clock_reference_base;
    SCR_flags = 0x1;
    DPRINTF(1, "system_clock_reference: %llu [%6f s]\n", 
	    system_clock_reference, ((double)system_clock_reference)/27000000.0);

    
    if(system_clock_reference < prev_scr) {
      scr_discontinuity = 1;
      DNOTE("%s", "*** Backward scr discontinuity ******\n");
      DNOTE("system_clock_reference: [%6f s]\n", 
	    ((double)system_clock_reference)/27000000.0);
      DNOTE("prev system_clock_reference: [%6f s]\n", 
	    ((double)prev_scr)/27000000.0);
      
    } else if((system_clock_reference - prev_scr) > 2700000) {
      scr_discontinuity = 1;
      DNOTE("%s", "*** Forward scr discontinuity ******\n");
      DNOTE("system_clock_reference: [%6f s]\n", 
	    ((double)system_clock_reference)/27000000.0);
      DNOTE("prev system_clock_reference: [%6f s]\n", 
	    ((double)prev_scr)/27000000.0);
    }
    prev_scr = system_clock_reference;
    
    
    break;
  }
  if(nextbits(32) == MPEG2_PS_SYSTEM_HEADER_START_CODE) {
    system_header();
  }

  return version;

}


void push_stream_data(uint8_t stream_id, int len,
		      uint8_t PTS_DTS_flags,
		      uint64_t PTS,
		      uint64_t DTS,
		      PacketType_t packet_type,
		      uint32_t packet_offs)
{
  uint8_t *data = &disk_buf[offs-(bits_left/8)];
  uint8_t subtype;
  uint32_t packet_data_offset = offs-(bits_left/8);

  //  fprintf(stderr, "pack nr: %d, stream_id: %02x (%02x), offs: %d, len: %d",
  //	  packnr, stream_id, data[0], offs-(bits_left/8), len);
  //  if(SCR_flags) {
  //    fprintf(stderr, ", SCR_base: %llx", SCR_base);
  //  }
  
  //  if(PTS_DTS_flags & 0x2) {
  //    fprintf(stderr, ", PTS: %llx", PTS);
  //  }

  //  fprintf(stderr, "\n");
  //fprintf(stderr, "stream_id: %02x\n", stream_id);

#ifdef STATS
  stat_n_packets++;
#endif //STATS

  DPRINTF(5, "bitsleft: %d\n", bits_left);

  if(stream_id == MPEG2_PRIVATE_STREAM_1) {
    subtype = data[0];
  } else {
    subtype = 0;
  }

  if((!id_registered(stream_id, subtype)) && system_header_set) {
    register_id(stream_id, subtype);
  }

  //fprintf(stderr, "Packet id: %02x, %02x\n", stream_id, subtype);

  if(id_stat(stream_id, subtype) == STREAM_DECODE) {
    if(!id_has_output(stream_id, subtype)) {
      id_get_output(stream_id, subtype);
    }

    //  if(stream_id == MPEG2_PRIVATE_STREAM_1) { 
    /*
      if((stream_id == 0xe0) || (stream_id == 0xc0) ||
      ((stream_id == MPEG2_PRIVATE_STREAM_1) &&
      (subtype == 0x80)) ||
      (stream_id == MPEG2_PRIVATE_STREAM_2)) {
    */
    //fprintf(stderr, "demux: put_in_q stream_id: %x %x\n",
    //      stream_id, subtype);

    if(msgqid != -1) {
      int infile;
      int is_newfile;
      infile = id_infile(stream_id, subtype);
      if(infile != new_file) {
	id_setinfile(stream_id, subtype, new_file);
	is_newfile = 1;
      } else {
	is_newfile = 0;
      }

      if(stream_id == MPEG2_PRIVATE_STREAM_1) {
	
	if((subtype >= 0x80) && (subtype < 0x88)) {
	  // ac3
	  /*
	    p[0] is the substream id
	    p[1] is the number of frames of audio which have a sync code 
	         in this pack
	    p[2]<<8 | p[3] is the starting index of the frame for which 
		           the PTS value corresponds
	  */
	  put_in_q(id_qaddr(stream_id, subtype), packet_data_offset, len,
		   PTS_DTS_flags, PTS, DTS, is_newfile, 0,
		   packet_type, packet_offs);
	} else if((subtype >= 0x88) && (subtype < 0x90)) {
	  // dts
	  put_in_q(id_qaddr(stream_id, subtype), packet_data_offset, len,
		   PTS_DTS_flags, PTS, DTS, is_newfile, 0,
		   packet_type, packet_offs);
	} else if((subtype >= 0xA0) && (subtype < 0xA8)) {
	  // pcm
	  put_in_q(id_qaddr(stream_id, subtype), packet_data_offset, len,
		   PTS_DTS_flags, PTS, DTS, is_newfile, 0,
		   packet_type, packet_offs);
	} else if((subtype >= 0x20) && (subtype < 0x40)) {
	  // spu
	  put_in_q(id_qaddr(stream_id, subtype), packet_data_offset, len,
		   PTS_DTS_flags, PTS, DTS, is_newfile, 0,
		   packet_type, packet_offs);
	} else {
	  put_in_q(id_qaddr(stream_id, subtype), packet_data_offset, len,
		   PTS_DTS_flags, PTS, DTS, is_newfile, 0,
		   packet_type, packet_offs);
	}
      } else {
	put_in_q(id_qaddr(stream_id, subtype), packet_data_offset, len,
		 PTS_DTS_flags, PTS, DTS, is_newfile, 0,
		 packet_type, packet_offs);
      }
    } else {
      if(stream_id == MPEG2_PRIVATE_STREAM_1) {
	
	if((subtype >= 0x80) && (subtype < 0x88)) {
	  // ac3
#if 1
	  fwrite(&disk_buf[packet_data_offset+4], len-4, 1,
		 id_file(stream_id, subtype));
#else
	  fwrite(&disk_buf[packet_data_offset], 4, 1,
		 id_file(stream_id, subtype));
#endif	
	  
	} else if((subtype >= 0x88) && (subtype < 0x90)) {
	  // dts
#if 1
	  fwrite(&disk_buf[packet_data_offset+1], len-1, 1,
		 id_file(stream_id, subtype));
#else
	  fwrite(&disk_buf[packet_data_offset], 64, 1,
		 id_file(stream_id, subtype));
#endif
	} else if((subtype >= 0xA0) && (subtype < 0xA8)) {
	  // pcm
#if 0
	  fwrite(&disk_buf[packet_data_offset+1], len-1, 1,
		 id_file(stream_id, subtype));
#else
	  if(!((unsigned int)(&disk_buf[packet_data_offset]) & 0x1)) {
	    // should be odd address
	    DNOTE("%s", "lpcm at even address\n");
	  }
	  if(!PTS_DTS_flags) {
	    DNOTE("%s", "lpcm without PTS\n");
	  }
	  fwrite(&disk_buf[packet_data_offset], 8, 1,
		 id_file(stream_id, subtype));
#endif
	} else if((subtype >= 0x20) && (subtype < 0x40)) {
	  // spu
	  fwrite(&disk_buf[packet_data_offset+1], len-1, 1,
		 id_file(stream_id, subtype));
	} else {
	  fwrite(&disk_buf[packet_data_offset], len, 1,
		 id_file(stream_id, subtype));
	}
      } else {
	fwrite(&disk_buf[packet_data_offset], len, 1,
	       id_file(stream_id, subtype));
      }
      
    }
    
  }
  
  drop_bytes(len);
  
}


void PES_packet(void)
{
  uint16_t PES_packet_length;
  uint8_t stream_id;
  uint8_t PES_scrambling_control;
  uint8_t PES_priority;
  uint8_t data_alignment_indicator;
  uint8_t copyright;
  uint8_t original_or_copy;
  uint8_t PTS_DTS_flags = 0;
  uint8_t ESCR_flag;
  uint8_t ES_rate_flag;
  uint8_t DSM_trick_mode_flag;
  uint8_t additional_copy_info_flag;
  uint8_t PES_CRC_flag;
  uint8_t PES_extension_flag;
  uint8_t PES_header_data_length;
  
  uint64_t PTS = 0;
  uint64_t DTS = 0;
  uint64_t ESCR_base;
  uint32_t ESCR_extension;
  
  uint32_t ES_rate;
  uint8_t trick_mode_control;
  uint8_t field_id;
  uint8_t intra_slice_refresh;
  uint8_t frequency_truncation;

  uint8_t field_rep_cntrl;

  uint8_t additional_copy_info;
  uint16_t previous_PES_packet_CRC;
  uint8_t PES_private_data_flag = 0;
  uint8_t pack_header_field_flag = 0;
  uint8_t program_packet_sequence_counter_flag = 0;
  uint8_t program_packet_sequence_counter = 0;
  uint8_t P_STD_buffer_flag = 0;
  uint8_t PES_extension_field_flag = 0;
  uint8_t pack_field_length = 0;
  uint8_t PES_extension_field_length = 0;
  unsigned int bytes_read = 0;

  uint8_t original_stuff_length;
  uint8_t P_STD_buffer_scale;
  uint16_t P_STD_buffer_size;
  uint16_t N;
  uint32_t pes_packet_offs;

  DPRINTF(2, "PES_packet()\n");

  pes_packet_offs = offs-(bits_left/8);

  GETBITS(24 ,"packet_start_code_prefix");
  stream_id = GETBITS(8, "stream_id");
  PES_packet_length = GETBITS(16, "PES_packet_length");
  DPRINTF(2, "stream_id:         ");
  dprintf_stream_id(2, stream_id);
  DPRINTF(2, "PES_packet_length: %u\n", PES_packet_length);
  
  if((stream_id != MPEG2_PRIVATE_STREAM_2) &&
     (stream_id != MPEG2_PADDING_STREAM)) {

    DPRINTF(1, "packet() stream: %02x\n", stream_id);

    GETBITS(2, "10");
    PES_scrambling_control    = GETBITS(2, "PES_scrambling_control");
    PES_priority              = GETBITS(1, "PES_priority");
    data_alignment_indicator  = GETBITS(1, "data_alignment_indicator");
    copyright                 = GETBITS(1, "copyright");
    original_or_copy          = GETBITS(1, "original_or_copy");
    PTS_DTS_flags             = GETBITS(2, "PTS_DTS_flags");
    ESCR_flag                 = GETBITS(1, "ESCR_flag");
    ES_rate_flag              = GETBITS(1, "ES_rate_flag");
    DSM_trick_mode_flag       = GETBITS(1, "DSM_trick_mode_flag");
    additional_copy_info_flag = GETBITS(1, "additional_copy_info_flag");
    PES_CRC_flag              = GETBITS(1, "PES_CRC_flag");
    PES_extension_flag        = GETBITS(1, "PES_extension_flag");
    PES_header_data_length    = GETBITS(8, "PES_header_data_length");
    
    bytes_read = 3;

    if(PES_scrambling_control != 0) {
      WARNING("Found a scrambled PES packet! (%d)\n", 
	      PES_scrambling_control);
    }
    
    if(PTS_DTS_flags == 0x2) {
      GETBITS(4, "0010");
      PTS                     = GETBITS(3, "PTS [32..30]")<<30;
      marker_bit();
      PTS                    |= GETBITS(15, "PTS [29..15]")<<15;
      marker_bit();
      PTS                    |= GETBITS(15, "PTS [14..0]");
      marker_bit();
      
      bytes_read += 5;
      DPRINTF(1, "PES_packet() PTS: %llu [%6f s]\n", PTS, ((double)PTS)/90E3);
    }

    if(PTS_DTS_flags == 0x3) {
      GETBITS(4, "0011");
      PTS                     = GETBITS(3, "PTS [32..30]")<<30;
      marker_bit();
      PTS                    |= GETBITS(15, "PTS [29..15]")<<15;
      marker_bit();
      PTS                    |= GETBITS(15, "PTS [14..0]");
      marker_bit();
      DPRINTF(1, "PES_packet() PTS: %llu [%6f s]\n", PTS, ((double)PTS)/90E3);

      GETBITS(4, "0001");
      DTS                     = GETBITS(3, "DTS [32..30]")<<30;
      marker_bit();
      DTS                    |= GETBITS(15, "DTS [29..15]")<<15;
      marker_bit();
      DTS                    |= GETBITS(15, "DTS [14..0]");
      marker_bit();
      DPRINTF(1, "PES_packet() DTS: %llu [%6f s]\n", DTS, ((double)DTS)/90E3);
      
      bytes_read += 10;
    }


    if(ESCR_flag == 0x01) {

      GETBITS(2, "reserved");
      ESCR_base                     = GETBITS(3, "ESCR_base [32..30]")<<30;
      marker_bit();
      ESCR_base                    |= GETBITS(15, "ESCR_base [29..15]")<<15;
      marker_bit();
      ESCR_base                    |= GETBITS(15, "ESCR_base [14..0]");
      marker_bit();
      ESCR_extension                = GETBITS(9, "ESCR_extension");
      marker_bit();
      DPRINTF(1, "ESCR_base: %llu [%6f s]\n", ESCR_base, ((double)ESCR_base)/90E3);

      bytes_read += 6;
    }
    
    if(ES_rate_flag == 0x01) {
      marker_bit();
      ES_rate =                       GETBITS(22, "ES_rate");
      marker_bit();

      bytes_read += 3;
    }
    
    if(DSM_trick_mode_flag == 0x01) {
      trick_mode_control = GETBITS(3, "trick_mode_control");
      
      if(trick_mode_control == 0x00) {
	field_id = GETBITS(2, "field_id");
	intra_slice_refresh = GETBITS(1, "intra_slice_refresh");
	frequency_truncation = GETBITS(2, "frequency_truncation");
      } else if(trick_mode_control == 0x01) {
	field_rep_cntrl = GETBITS(5, "field_rep_cntrl");
      } else if(trick_mode_control == 0x02) {
	field_id = GETBITS(2, "field_id");
        GETBITS(3, "reserved");
      } else if(trick_mode_control == 0x03) {
	field_id = GETBITS(2, "field_id");
	intra_slice_refresh = GETBITS(1, "intra_slice_refresh");
	frequency_truncation = GETBITS(2, "frequency_truncation");
      }

      bytes_read += 1;
    }
    
    if(additional_copy_info_flag == 0x01) {
      marker_bit();
      additional_copy_info = GETBITS(7, "additional_copy_info");

      bytes_read += 1;
    }

    if(PES_CRC_flag == 0x01) {
      previous_PES_packet_CRC = GETBITS(16, "previous_PES_packet_CRC");

      bytes_read += 2;
    }

    if(PES_extension_flag == 0x01) {
      PES_private_data_flag = GETBITS(1, "PES_private_data_flag");
      pack_header_field_flag = GETBITS(1, "pack_header_field_flag");
      program_packet_sequence_counter_flag =
	GETBITS(1, "program_packet_sequence_counter_flag");
      P_STD_buffer_flag = GETBITS(1, "P_STD_buffer_flag");
      GETBITS(3, "reserved");
      PES_extension_field_flag = GETBITS(1, "PES_extension_field_flag");

      bytes_read += 1;
    }

    if(PES_private_data_flag == 0x01) {
      //TODO
      GETBITS(32, "PES_private_data");
      GETBITS(32, "PES_private_data");
      GETBITS(32, "PES_private_data");
      GETBITS(32, "PES_private_data");

      bytes_read += 16;
    }

    if(pack_header_field_flag == 0x01) {
      pack_field_length = GETBITS(8, "pack_field_length");
      pack_header();

      bytes_read += 1;
      bytes_read += pack_field_length;
      /*
	{
	int n;
	for(n = 0; n < pack_field_length; n++) {
	fprintf(stderr, "%02x, ", data[i+n+1]);
	}
	fprintf(stderr, "\n");
	}
      */
    }

    if(program_packet_sequence_counter_flag == 0x01) {
      marker_bit();
      program_packet_sequence_counter = GETBITS(7, "program_packet_sequence_counter");
      marker_bit();
      original_stuff_length = GETBITS(7, "original_stuff_length");
      bytes_read += 2;
    }
    
    if(P_STD_buffer_flag == 0x01) {
      GETBITS(2, "01");
      P_STD_buffer_scale = GETBITS(1, "P_STD_buffer_scale");
      P_STD_buffer_size = GETBITS(13, "P_STD_buffer_size");

      bytes_read += 2;
    }      
    
    if(PES_extension_field_flag == 0x01) {
      int i;
      marker_bit();
      PES_extension_field_length = GETBITS(7, "PES_extension_field_length");
      for(i=0; i<PES_extension_field_length; i++) {
	GETBITS(8, "reserved");
      }

      bytes_read += 1;
      bytes_read += PES_extension_field_length;
    }
    
    N = (PES_header_data_length+3)-bytes_read;
    drop_bytes(N);
    bytes_read += N;

    N = PES_packet_length-bytes_read;
    
    //FIXME  kolla specen...    
    //FIXME  push pes..    

    
    push_stream_data(stream_id, N, PTS_DTS_flags, PTS, DTS, 
		     PacketType_PES, pes_packet_offs);
    
  } else if(stream_id == MPEG2_PRIVATE_STREAM_2) {
    push_stream_data(stream_id, PES_packet_length,
		     PTS_DTS_flags, PTS, DTS, 
		     PacketType_PES, pes_packet_offs);
    //fprintf(stderr, "*PRIVATE_stream_2, %d\n", PES_packet_length);
  } else if(stream_id == MPEG2_PADDING_STREAM) {
    drop_bytes(PES_packet_length);
    //    push_stream_data(stream_id, PES_packet_length);
  }
}



// MPEG-1 packet

void packet(void)
{
  uint8_t stream_id;
  uint16_t packet_length;
  uint8_t P_STD_buffer_scale;
  uint16_t P_STD_buffer_size;
  uint64_t PTS = 0;
  uint64_t DTS = 0;
  int N;
  uint8_t pts_dts_flags = 0;
  uint32_t mpeg1_packet_offs;
  
  mpeg1_packet_offs = offs-(bits_left/8);

  GETBITS(24 ,"packet_start_code_prefix");
  stream_id = GETBITS(8, "stream_id");
  packet_length = GETBITS(16, "packet_length");
  N = packet_length;
  
  if(stream_id != MPEG2_PRIVATE_STREAM_2) {
    
    DPRINTF(1, "packet() stream: %02x\n", stream_id);
    while(nextbits(8) == 0xff) {
      GETBITS(8, "stuffing byte");
      N--;
    }
    
    if(nextbits(2) == 0x1) {
      GETBITS(2, "01");
      P_STD_buffer_scale = GETBITS(1, "P_STD_buffer_scale");
      P_STD_buffer_size = GETBITS(13, "P_STD_buffer_size");
      
      N -= 2;
    }
    if(nextbits(4) == 0x2) {
      pts_dts_flags = 0x2;
      GETBITS(4, "0010");
      PTS                     = GETBITS(3, "PTS [32..30]")<<30;
      marker_bit();
      PTS                    |= GETBITS(15, "PTS [29..15]")<<15;
      marker_bit();
      PTS                    |= GETBITS(15, "PTS [14..0]");
      marker_bit();
      
      DPRINTF(1, "packet() PTS: %llu [%6f s]\n", PTS, ((double)PTS)/90E3);
      
      N -= 5;
    } else if(nextbits(4) == 0x3) {
      pts_dts_flags = 0x3;
      GETBITS(4, "0011");
      PTS                     = GETBITS(3, "PTS [32..30]")<<30;
      marker_bit();
      PTS                    |= GETBITS(15, "PTS [29..15]")<<15;
      marker_bit();
      PTS                    |= GETBITS(15, "PTS [14..0]");
      marker_bit();
      DPRINTF(1, "packet() PTS: %llu [%6f s]\n", PTS, ((double)PTS)/90E3);

      GETBITS(4, "0001");
      DTS                     = GETBITS(3, "DTS [32..30]")<<30;
      marker_bit();
      DTS                    |= GETBITS(15, "DTS [29..15]")<<15;
      marker_bit();
      DTS                    |= GETBITS(15, "DTS [14..0]");
      marker_bit();
      DPRINTF(1, "packet() DTS: %llu [%6f s]\n", DTS, ((double)DTS)/90E3);

      N -= 10;
    } else {
      
      GETBITS(8, "00001111");
      N--;
    }
  }

  push_stream_data(stream_id, N, pts_dts_flags, PTS, DTS,
		   PacketType_MPEG1, mpeg1_packet_offs);
  
}


void pack(void)
{
  uint32_t start_code;
  uint8_t stream_id;
  uint8_t is_PES = 0;
  int mpeg_version;
  MsgEvent_t ev;

  SCR_flags = 0;

  /* TODOD clean up */
  if(msgqid != -1) {
    if(off_to != -1) {
      if(off_to <= offs-(bits_left/8)) {
	//fprintf(stderr, "demux: off_to %d offs %d pack\n", off_to, offs);
	off_to = -1;
	get_next_demux_q();
      }
    }
    if(off_from != -1) {
      //fprintf(stderr, "demux: off_from pack\n");
      offs = off_from;
      bits_left = 64;
      off_from = -1;
      GETBITS(32, "skip1");
      GETBITS(32, "skip2");
    }
    
    while(MsgCheckEvent(msgq, &ev) != -1) {
      handle_events(&ev);
    }
  }
  
  mpeg_version = pack_header();
  switch(mpeg_version) {
  case MPEG1:

    /* TODO clean up */
    if(msgqid != -1) {
      if(off_to != -1) {
	if(off_to <= offs-(bits_left/8)) {
	  //fprintf(stderr, "demux: off_to %d offs %d mpeg1\n", off_to, offs);
	  off_to = -1;
	  get_next_demux_q();
	  //wait_for_msg(CMD_CTRL_CMD);
	}
      }
      if(off_from != -1) {
	//fprintf(stderr, "demux: off_from mpeg1\n");
	offs = off_from;
	bits_left = 64;
	off_from = -1;
	GETBITS(32, "skip1");
	GETBITS(32, "skip2");
      }

      while(MsgCheckEvent(msgq, &ev) != -1) {
	handle_events(&ev);
      }
    }

    next_start_code();
    while(nextbits(32) >= 0x000001BC) {
      packet();

      /* TODO clean up */
      if(msgqid != -1) {
	if(off_to != -1) {
	  if(off_to <= offs-(bits_left/8)) {
	    //fprintf(stderr, "demux: off_to %d offs %d packet\n", off_to, offs);
	    off_to = -1;
	    get_next_demux_q();
	    //wait_for_msg(CMD_CTRL_CMD);
	  }
	}
	if(off_from != -1) {
	  //fprintf(stderr, "demux: off_from packet\n");
	  offs = off_from;
	  bits_left = 64;
	  off_from = -1;
	  GETBITS(32, "skip1");
	  GETBITS(32, "skip2");
	}
	while(MsgCheckEvent(msgq, &ev) != -1) {
	  handle_events(&ev);
	}
      }

      next_start_code();
    }
    break;
  case MPEG2:
    while((((start_code = nextbits(32))>>8)&0x00ffffff) ==
	  MPEG2_PES_PACKET_START_CODE_PREFIX) {
      stream_id = (start_code&0xff);
      
      is_PES = 0;
      if((stream_id >= 0xc0) && (stream_id < 0xe0)) {
	/* ISO/IEC 11172-3/13818-3 audio stream */
	is_PES = 1;
	
      } else if((stream_id >= 0xe0) && (stream_id < 0xf0)) {
	/* ISO/IEC 11172-3/13818-3 video stream */
	is_PES = 1;

      } else {
	switch(stream_id) {
	case 0xBC:
	  /* program stream map */
	  fprintf(stderr, "demux: Program Stream map\n");
	case 0xBD:
	  /* private stream 1 */
	case 0xBE:
	  /* padding stream */
	case 0xBF:
	  /* private stream 2 */
	  is_PES = 1;
	  break;
	case 0xBA:
				//fprintf(stderr, "Pack Start Code\n");
	  is_PES = 0;
	  break;
	default:
	  is_PES = 0;
	  fprintf(stderr, "demux: unknown stream_id: 0x%02x\n", stream_id);
	  break;
	}
      }
      if(!is_PES) {
	break;
      }
      
      PES_packet();
      SCR_flags = 0;

      /* TODO clean up */
      if(msgqid != -1) {
	if(off_to != -1) {
	  if(off_to <= offs-(bits_left/8)) {
	    //fprintf(stderr, "demux: off_to %d offs %d mpeg2\n", off_to, offs);
	    off_to = -1;
	    get_next_demux_q();
	    //wait_for_msg(CMD_CTRL_CMD);
	  }
	}
	if(off_from != -1) {
	  //fprintf(stderr, "demux: off_from mpeg2\n");
	  offs = off_from;
	  bits_left = 64;
	  off_from = -1;
	  GETBITS(32, "skip1");
	  GETBITS(32, "skip2");
	}
	while(MsgCheckEvent(msgq, &ev) != -1) {
	  handle_events(&ev);
	}
      }
    }
    break;
  }

  packnr++;

}

void MPEG2_program_stream(void)
{

  DPRINTF(2,"MPEG2_program_stream()\n");
  do {
    pack();
  } while(nextbits(32) == MPEG2_PS_PACK_START_CODE); 
  if(GETBITS(32, "MPEG2_PS_PROGRAM_END_CODE") == MPEG2_PS_PROGRAM_END_CODE) {
    DPRINTF(1, "MPEG Program End\n");
    //system_header_set = 0;
  } else {
    synced = 0;
    WARNING("Lost Sync at offset: %u bytes\n", offs);
  }
}


void segvhandler (int id)
{
  FATAL("%s", "Segmentation fault\n");

#ifdef STATS
  printf("\n----------------------------------------------\n");
  printf("Unaligned video packets:\t\t%u\n", 
	 stat_video_unaligned_packet_offset);
  printf("Video packets with unaligned ends:\t%u\n",
	 stat_video_unaligned_packet_end);
  printf("Total video packets:\t\t\t%u\n", 
	 stat_video_n_packets);
  printf("Total audio packets:\t\t\t%u\n", 
	 stat_audio_n_packets);
  printf("Total subpicture packets:\t\t%u\n", 
	 stat_subpicture_n_packets);
  printf("Total packets:\t\t\t\t%u\n",
	 stat_n_packets);
#endif //STATS

  exit(0);
}

void loadinputfile(char *infilename)
{
  static struct stat statbuf;
  int rv;

  if(disk_buf != NULL) {
    munmap(disk_buf, statbuf.st_size);
  }
  
  infilefd = open(infilename, O_RDONLY);
  if(infilefd == -1) {
    perror(infilename);
    exit(1);
  }
  
  strcpy(cur_filename, infilename);

  rv = fstat(infilefd, &statbuf);
  if (rv == -1) {
    perror("fstat");
    exit(1);
  }
  disk_buf = (uint8_t *)mmap(NULL, statbuf.st_size, PROT_READ, MAP_SHARED, infilefd, 0);
  close(infilefd);
  if(disk_buf == MAP_FAILED) {
    perror("mmap");
    exit(1);
  }

  infilelen = statbuf.st_size;
#ifdef HAVE_MADVISE
  rv = madvise(disk_buf, infilelen, MADV_SEQUENTIAL);
  if(rv == -1) {
    perror("madvise");
    exit(1);
  }
#endif
  DPRINTF(1, "All mmap systems ok!\n");

  new_file++;
}


char *stream_opts[] = {
#define OPT_STREAM_NR    0
  "nr",
#define OPT_FILE         1
  "file",
#define OPT_SUBID        2
  "subid",
  NULL
};


int attach_ctrl_shm(int shmid)
{
  char *shmaddr;
  
  if(shmid >= 0) {
    if((shmaddr = shmat(shmid, NULL, SHM_SHARE_MMU)) == (void *)-1) {
      perror("demux: attach_ctrl_data(), shmat()");
      return -1;
    }
    
    ctrl_data_shmid = shmid;
    ctrl_data = (ctrl_data_t*)shmaddr;
    ctrl_time = (ctrl_time_t *)(shmaddr+sizeof(ctrl_data_t));
  }    
  
  return 0;
}

int main(int argc, char **argv)
{
  int c, rv; 
  struct sigaction sig;
  char *options;
  char *opt_value;
  int stream_nr;
  int subid;
  char *file;
  uint8_t stream_id;
  int lost_sync = 1;
  
  program_name = argv[0];
  GET_DLEVEL();
  init_id_reg(STREAM_DISCARD);
  
  /* Parse command line options */
  while ((c = getopt(argc, argv, "v:a:s:i:d:m:o:p:h?")) != EOF) {
    switch (c) {
    case 'v':
      stream_nr = -1;
      file = NULL;
      options = optarg;
      while (*options != '\0') {
	switch(getsubopt(&options, stream_opts, &opt_value)) {
	case OPT_STREAM_NR:
	  if(opt_value == NULL) {
	    exit(1);
	  }
	  
	  stream_nr = atoi(opt_value);
	  break;
	case OPT_FILE:
	  if(opt_value == NULL) {
	    exit(1);
	  }
	  
	  file = opt_value;
	  break;
	default:
	  fprintf(stderr, "*demux: Unknown suboption\n");
	  exit(1);
	  break;
	}
      }
      
      if((stream_nr == -1)) {
	fprintf(stderr, "*demux: Missing suboptions\n");
	exit(1);
      } 
      
      if((stream_nr < 0) && (stream_nr > 0xf)) {
	fprintf(stderr, "*demux: Invalid stream nr\n");
	exit(1);
      }
      
      stream_id = (0xe0 | stream_nr);
      
      if(file != NULL) {
	video_file = fopen(file,"w");
	if(!video_file) {
	  perror(optarg);
	  exit(1);
	}

	id_add(stream_id, 0, STREAM_DECODE, -1, NULL, video_file);
	
      } else {
	fprintf(stderr, "Video stream %d disabled\n", stream_nr);
	
	id_add(stream_id, 0, STREAM_DISCARD, -1, NULL, NULL);
      }
	
	  

      break;
    case 'p':
      stream_nr = -1;
      file = NULL;
      subid = -1;
      options = optarg;
      while (*options != '\0') {
	switch(getsubopt(&options, stream_opts, &opt_value)) {
	case OPT_SUBID:
	  if(opt_value == NULL) {
	    exit(1);
	  }
	  subid = strtol(opt_value, NULL, 0);
	  break;
	case OPT_STREAM_NR:
	  if(opt_value == NULL) {
	    exit(1);
	  }
	  
	  stream_nr = atoi(opt_value);
	  break;
	case OPT_FILE:
	  if(opt_value == NULL) {
	    exit(1);
	  }
	  
	  file = opt_value;
	  break;
	default:
	  fprintf(stderr, "*demux: Unknown suboption\n");
	  exit(1);
	  break;
	}
      }
      
      
      if((subid == -1)) {
	fprintf(stderr, "*demux: Missing suboptions\n");
	exit(1);
      } else if(subid < 0 || subid > 0xff) {
	fprintf(stderr, "*demux: subid out of range\n");
	exit(1);
      }

      if((stream_nr == -1)) {
	fprintf(stderr, "*demux: Missing suboptions\n");
	exit(1);
      } 

      
      if(!(stream_nr < subid) || (subid | stream_nr) > 0xff) {
	fprintf(stderr, "*demux: subid/stream_nr combination invalid\n");
	exit(1);
      }

      stream_id = (subid | stream_nr);
      
      if(file != NULL) {
	ps1_file = fopen(file,"w");
	if(!ps1_file) {
	  perror(optarg);
	  exit(1);
	}

	id_add(MPEG2_PRIVATE_STREAM_1, stream_id, STREAM_DECODE, -1, NULL, ps1_file);
	
      } else {
	fprintf(stderr, "Private stream 1 subid %d disabled\n", stream_nr);
	
	id_add(MPEG2_PRIVATE_STREAM_1, stream_id, STREAM_DISCARD, -1, NULL, NULL);
      }

      break;
    case 'a':
      stream_nr = -1;
      file = NULL;
      options = optarg;
      while (*options != '\0') {
	switch(getsubopt(&options, stream_opts, &opt_value)) {
	case OPT_STREAM_NR:
	  if(opt_value == NULL) {
	    exit(1);
	  }
	  
	  stream_nr = atoi(opt_value);
	  break;
	case OPT_FILE:
	  if(opt_value == NULL) {
	    exit(1);
	  }
	  
	  file = opt_value;
	  break;
	default:
	  fprintf(stderr, "*demux: Unknown suboption\n");
	  exit(1);
	  break;
	}
      }
      
      if((stream_nr == -1)) {
	fprintf(stderr, "*demux: Missing suboptions\n");
	exit(1);
      } 
      
      if((stream_nr < 0) && (stream_nr > 0xf)) {
	fprintf(stderr, "*demux: Invalid stream nr\n");
	exit(1);
      }
      
      stream_id = (0xc0 | stream_nr);
      
      if(file != NULL) {
	audio_file = fopen(file,"w");
	if(!audio_file) {
	  perror(optarg);
	  exit(1);
	}

	id_add(stream_id, 0, STREAM_DECODE, -1, NULL, audio_file);
	
      } else {
	fprintf(stderr, "Audio stream %d disabled\n", stream_nr);
	
	id_add(stream_id, 0, STREAM_DISCARD, -1, NULL, NULL);
      }
	
	  

      break;

    case 's':

      stream_nr = -1;
      file = NULL;
      options = optarg;
      while (*options != '\0') {
	switch(getsubopt(&options, stream_opts, &opt_value)) {
	case OPT_STREAM_NR:
	  if(opt_value == NULL) {
	    exit(1);
	  }
	  
	  stream_nr = atoi(opt_value);
	  break;
	case OPT_FILE:
	  if(opt_value == NULL) {
	    exit(1);
	  }
	  
	  file = opt_value;
	  break;
	default:
	  fprintf(stderr, "*demux: Unknown suboption: %s\n", options);
	  exit(1);
	  break;
	}
      }
      
      if((stream_nr == -1)) {
	fprintf(stderr, "*demux: Missing suboptions\n");
	exit(1);
      } 
      
      if((stream_nr < 0) && (stream_nr > 0x1f)) {
	fprintf(stderr, "*demux: Invalid stream nr\n");
	exit(1);
      }
      
      stream_id = (0x20 | stream_nr);
      
      if(file != NULL) {
	subtitle_file = fopen(file,"w");
	if(!subtitle_file) {
	  perror(optarg);
	  exit(1);
	}

	id_add(MPEG2_PRIVATE_STREAM_1, stream_id, STREAM_DECODE, -1, NULL, subtitle_file);
	
      } else {
	fprintf(stderr, "Subtitle stream %d disabled\n", stream_nr);
	
	id_add(MPEG2_PRIVATE_STREAM_1, stream_id, STREAM_DISCARD, -1, NULL, NULL);
      }


      break;
    case 'd':
      debug = atoi(optarg);
      break;
    case 'm':
      msgqid = atoi(optarg);
      break;
    case 'o':
      offs = atoi(optarg);
      break;
    case 'h':
    case '?':
      usage();
      return 1;
    }
  }

  if(msgqid == -1) {
    if(argc - optind != 1){
      usage();
      return 1;
    }
  }


  if(msgqid != -1) {
    MsgEvent_t regev;
    int fileopen = 0;
    init_id_reg(STREAM_DISCARD);
    // get a handle
    if((msgq = MsgOpen(msgqid)) == NULL) {
      FATAL("%s", "Couldn't get message q\n");
      exit(1);
    }
#if DEBUG
    fprintf(stderr, "demux: msgq opened, clientnr: %ld\n", msgq->mtype);
#endif
    
    // register with the resource manager and tell what we can do
    regev.type = MsgEventQRegister;
    regev.registercaps.capabilities = DEMUX_MPEG1 | DEMUX_MPEG2_PS;
    if(MsgSendEvent(msgq, CLIENT_RESOURCE_MANAGER, &regev, 0) == -1) {
      fprintf(stderr, "demux: register\n");
    }
#if DEBUG
    fprintf(stderr, "demux: sent register\n");
#endif
    

    /* wait for load_file command */
    //get_next_demux_q();
    
    while(!fileopen) {
      if(MsgNextEvent(msgq, &regev) != -1) {
	switch(regev.type) {
	case MsgEventQChangeFile:
#if DEBUG
	  fprintf(stderr, "demux: got changefile\n");
#endif
	  loadinputfile(regev.changefile.filename);
	  fileopen = 1;
	  break;
	case MsgEventQCtrlData:
#if DEBUG
	  fprintf(stderr, "demux: got ctrldata\n");
#endif
	  attach_ctrl_shm(regev.ctrldata.shmid);
	  break;
	case MsgEventQDemuxDVDRoot:
#if DEBUG
	  fprintf(stderr, "demux: got dvd root\n");
#endif
	  handle_events(&regev);
	  fileopen = 1;
	  break;
	default:
	  handle_events(&regev);
	/*
	  fprintf(stderr, "demux: msg not wanted %d, from %ld\n",
	  regev.type, regev.any.client);
	*/
	  break;
	}
      }
    }
    //fprintf(stderr, "demux: file opened\n");
    get_buffer(4*1024*1024);

  } else {
    loadinputfile(argv[optind]);
  }
  // Setup signal handler for SEGV, to detect that we ran 
  // out of file without a test.
  
  sig.sa_handler = segvhandler;
  sig.sa_flags = 0;
  rv = sigaction(SIGSEGV, &sig, NULL);
  if(rv == -1) {
    perror("*demux: sighandler");
    exit(1);
  }
  
#if DEBUG
  fprintf(stderr, "demux: get next demux q\n");
#endif
  if(msgqid != -1) {
    get_next_demux_q();
  }

  if(off_from != -1) {
    //fprintf(stderr, "demux: off_from pack\n");
    offs = off_from;
    bits_left = 64;
    off_from = -1;
      GETBITS(32, "skip1");
      GETBITS(32, "skip2");
  } else {
    fprintf(stderr, "*demux: internal error1\n");
    exit(1);
  }

  while(1) {
    //    fprintf(stderr, "Searching for Program Stream\n");
    if(nextbits(32) == MPEG2_PS_PACK_START_CODE) {
      if(!synced) {
	synced = 1;
#if DEBUG
	fprintf(stderr, "demux: Found Program Stream\n");
#endif
      }
      MPEG2_program_stream();
      lost_sync = 1;
    } else if(nextbits(32) == MPEG2_VS_SEQUENCE_HEADER_CODE) {
      fprintf(stderr, "demux: Found Video Sequence Header, "
	      "seems to be an Elementary Stream (video)\n");
      lost_sync = 1;
    }
    GETBITS(8, "resync");
    DPRINTF(1, "resyncing");
    if(lost_sync) {
      lost_sync = 0;
      WARNING("%s", "Lost sync, resyncing\n");
    }
  }
}


static void handle_events(MsgEvent_t *ev)
{
  
  switch(ev->type) {
  case MsgEventQNotify:
    DPRINTF(1, "demux: got notify\n");
    break;
  case MsgEventQChangeFile:
    DPRINTF(1, "demux: change file: %s\n", ev->changefile.filename);
    add_to_demux_q(ev);
    break;
  case MsgEventQGntStreamBuf:
    DPRINTF(1, "demux: got stream %x, %x buffer \n",
	    ev->gntstreambuf.stream_id,
	    ev->gntstreambuf.subtype);
    attach_decoder_buffer(ev->gntstreambuf.stream_id,
			  ev->gntstreambuf.subtype,
			  ev->gntstreambuf.q_shmid);
    
    break;
  case MsgEventQPlayCtrl:
    add_to_demux_q(ev);
    break;
  case MsgEventQDemuxStream:
    if(ev->demuxstream.stream_id != MPEG2_PRIVATE_STREAM_1) {
      id_reg[ev->demuxstream.stream_id].state = STREAM_DECODE;
    } else {
      id_reg_ps1[ev->demuxstream.subtype].state = STREAM_DECODE;
    }
    break;
  case MsgEventQDemuxStreamChange:
    switch_to_stream(ev->demuxstream.stream_id, ev->demuxstream.subtype);
    break;
  case MsgEventQDemuxStreamChange2:
    switch_from_to_stream(ev->demuxstreamchange2.old_stream_id,
			  ev->demuxstreamchange2.old_subtype,
			  ev->demuxstreamchange2.new_stream_id,
			  ev->demuxstreamchange2.new_subtype);
    break;
  case MsgEventQDemuxDefault:
    init_id_reg(ev->demuxdefault.state);
    break;
  case MsgEventQDemuxDVD:
    add_to_demux_q(ev);
    break;
  case MsgEventQDemuxDVDRoot:
    add_to_demux_q(ev);
    break;
  default:
#if DEBUG
    fprintf(stderr, "demux: unrecognized command: %d\n", ev->type);
#endif
    break;
  }
}

int register_id(uint8_t id, int subtype)
{
  MsgEvent_t ev;
  
  data_buf_head_t *data_buf_head;
  int qsize;
  
  if(msgqid != -1) {
    
    data_buf_head = (data_buf_head_t *)data_buf_addr;
    
    /* send create decoder request msg*/
    ev.type = MsgEventQReqStreamBuf;
    ev.reqstreambuf.stream_id = id;
    ev.reqstreambuf.subtype = subtype;

    /* TODO how should we decide buf sizes? */
    switch(id) {
    case MPEG2_PRIVATE_STREAM_1:
      if((subtype&~0x1f) == 0x80) { // ac3, dts
	qsize = 100;
      } else if((subtype&~0x1f) == 0x20) { // spu
	qsize = 100;
      } else {
	qsize = 100;
      }
      break;
    case MPEG2_PRIVATE_STREAM_2:
      qsize = 100;
      break;
    default:
      qsize = 300;
      break;
    }

    ev.reqstreambuf.nr_of_elems = qsize;
    ev.reqstreambuf.data_buf_shmid = data_buf_head->shmid;
      
    if(MsgSendEvent(msgq, CLIENT_RESOURCE_MANAGER, &ev, 0) == -1) {
      fprintf(stderr, "demux: couldn't send streambuf request\n");
    }
    
    /* wait for answer */
    
    while(!id_registered(id, subtype)) {
      if(MsgNextEvent(msgq, &ev) != -1) {
	if(ev.type == MsgEventQGntStreamBuf) {
	  DPRINTF(1, "demux: got stream %x, %x buffer \n",
		  ev.gntstreambuf.stream_id,
		  ev.gntstreambuf.subtype);
	  attach_decoder_buffer(ev.gntstreambuf.stream_id,
				ev.gntstreambuf.subtype,
				ev.gntstreambuf.q_shmid);
	} else {
	  handle_events(&ev);
	}
      }
    }

  } else {
    /* TODO fix */
    /*
      id_add(...);

    */
  }
  
  return 0;
}

int id_get_output(uint8_t id, int subtype)
{
  MsgEvent_t ev;
  
  data_buf_head_t *data_buf_head;
  int qsize;
  
  if(msgqid != -1) {
    
    data_buf_head = (data_buf_head_t *)data_buf_addr;
    
    /* send create decoder request msg*/
    ev.type = MsgEventQReqStreamBuf;
    ev.reqstreambuf.stream_id = id;
    ev.reqstreambuf.subtype = subtype;

    /* TODO how should we decide buf sizes? */
    switch(id) {
    case MPEG2_PRIVATE_STREAM_1:
      if((subtype&~0x1f) == 0x80) {
	qsize = 100;
      } else if((subtype&~0x1f) == 0x20) {
	qsize = 100;
      } else {
	qsize = 100;
      }
      break;
    case MPEG2_PRIVATE_STREAM_2:
      qsize = 100;
      break;
    default:
      qsize = 300;
      break;
    }

    ev.reqstreambuf.nr_of_elems = qsize;
    ev.reqstreambuf.data_buf_shmid = data_buf_head->shmid;
      
    if(MsgSendEvent(msgq, CLIENT_RESOURCE_MANAGER, &ev, 0) == -1) {
      fprintf(stderr, "demux: couldn't send streambuf request\n");
    }
    
    /* wait for answer */
    
    while(!id_has_output(id, subtype)) {
      if(MsgNextEvent(msgq, &ev) != -1) {
	if(ev.type == MsgEventQGntStreamBuf) {
	  DPRINTF(1, "demux: got stream %x, %x buffer \n",
		  ev.gntstreambuf.stream_id,
		  ev.gntstreambuf.subtype);
	  attach_decoder_buffer(ev.gntstreambuf.stream_id,
				ev.gntstreambuf.subtype,
				ev.gntstreambuf.q_shmid);
	} else {
	  handle_events(&ev);
	}
      }
    }

  } else {
    /* TODO fix */
    /*
      id_add(...);

    */
  }
  
  return 0;
}

int id_stat(uint8_t id, uint8_t subtype)
{
    if(id != MPEG2_PRIVATE_STREAM_1) {
      return id_reg[id].state;
    } else {
      return id_reg_ps1[subtype].state;
    }
}

int id_infile(uint8_t id, uint8_t subtype)
{
  if(id != MPEG2_PRIVATE_STREAM_1) {
    return id_reg[id].infile;
  } else {
    return id_reg_ps1[subtype].infile;
  }
}

void id_setinfile(uint8_t id, uint8_t subtype, int newfile)
{
  if(id != MPEG2_PRIVATE_STREAM_1) {
    id_reg[id].infile = newfile;
  } else {
    id_reg_ps1[subtype].infile = newfile;
  }
}

char *id_qaddr(uint8_t id, uint8_t subtype)
{
  if(id != MPEG2_PRIVATE_STREAM_1) {
    return id_reg[id].shmaddr;
  } else {
    return id_reg_ps1[subtype].shmaddr;
  }
}

FILE *id_file(uint8_t id, uint8_t subtype)
{
  if(id != MPEG2_PRIVATE_STREAM_1) {
    return id_reg[id].file;
  } else {
    return id_reg_ps1[subtype].file;
  }
}
  
int id_registered(uint8_t id, uint8_t subtype)
{
  if(id != MPEG2_PRIVATE_STREAM_1) {
    if(id_reg[id].state != STREAM_NOT_REGISTERED) {
      return 1;
    }
  } else {
    if(id_reg_ps1[subtype].state != STREAM_NOT_REGISTERED) {
      return 1;
    }
  }

  return 0;
}


uint8_t type_registered(uint8_t id, uint8_t subtype)
{
  int n;
  uint8_t idtype;
  uint8_t idrange;
  
  if(id != MPEG2_PRIVATE_STREAM_1) {
    switch(id & 0xf0) {
    case 0xe0:
      idtype = 0xe0;
      idrange = 16;
      break;
    case 0xc0:
    case 0xd0:
      idtype = 0xc0;
      idrange = 32;
      break;
    default:
      fprintf(stderr, "demux: type_registered(), type not handled\n");
      return 0;
      break;
    }
    
    for(n = idtype; n < (idtype+idrange); n++) {
      if(id_stat(n, 0) == STREAM_DECODE) {
	return n;
      }
    }
  } else {
    switch(subtype & 0xf0) {
    case 0x20:
    case 0x30:
      idtype = 0x20;
      idrange = 32;
      break;
    case 0x80:
      if(subtype < 0x88) {
	idtype = 0x80;
	idrange = 8;
      } else {
	idtype = 0x88;
	idrange = 8;
      }
      break;
    default:
      fprintf(stderr, "demux: type_registered(), subtype not handled\n");
      return 0;
      break;
    }
    
    for(n = idtype; n < (idtype+idrange); n++) {
      if(id_stat(id, n) == STREAM_DECODE) {
	return n;
      }
    }
  } 
  return 0;
  
}

int switch_to_stream(uint8_t id, uint8_t subtype)
{
  uint8_t oldid;
  
  oldid = type_registered(id, subtype);
  if(oldid == 0) {
    if(id != MPEG2_PRIVATE_STREAM_1) {
      id_reg[id].state = STREAM_DECODE;
    } else {
      id_reg_ps1[subtype].state = STREAM_DISCARD;
    }
    return 1;
  }
  
  if(id != MPEG2_PRIVATE_STREAM_1) {
    if(id == oldid) {
      return 1;
    }
  } else {
    if(subtype == oldid) {
      return 1;
    }
  }
  
  
  if(id != MPEG2_PRIVATE_STREAM_1) {
    id_reg[id] = id_reg[oldid];
    id_reg[oldid].shmid = -1;
    id_reg[oldid].shmaddr = NULL;
    id_reg[oldid].state = STREAM_DISCARD;
    id_reg[oldid].file = NULL;
  } else {
    id_reg_ps1[subtype] = id_reg_ps1[oldid];
    id_reg_ps1[oldid].shmid = -1;
    id_reg_ps1[oldid].shmaddr = NULL;
    id_reg_ps1[oldid].state = STREAM_DISCARD;
    id_reg_ps1[oldid].file = NULL;
  }
  return 1;
}


int switch_from_to_stream(uint8_t oldid, uint8_t oldsubtype,
			  uint8_t newid, uint8_t newsubtype)
{
  
  if(oldid == 0) {
    if(newid != MPEG2_PRIVATE_STREAM_1) {
      id_reg[newid].state = STREAM_DECODE;
    } else {
      id_reg_ps1[newsubtype].state = STREAM_DECODE;
    }
    return 1;
  }
  
  if(newid != MPEG2_PRIVATE_STREAM_1) {
    if(newid == oldid) {
      DNOTE("%s", "equal(not private)\n");
      return 1;
    }
  } else {
    if(oldid == MPEG2_PRIVATE_STREAM_1) {
      if(newsubtype == oldsubtype) {
	DNOTE("%s", "equal(private)\n");
	return 1;
      }
    }
  }
  
  
  if(newid != MPEG2_PRIVATE_STREAM_1) {
    DNOTE("%s", "newid not private:\n");
    if(oldid != MPEG2_PRIVATE_STREAM_1) {
      DNOTE("%s", "oldid not private\n");
      id_reg[newid] = id_reg[oldid];
      id_reg[oldid].shmid = -1;
      id_reg[oldid].shmaddr = NULL;
      id_reg[oldid].state = STREAM_DISCARD;
      id_reg[oldid].file = NULL;
    } else {
      DNOTE("%s", "oldid private\n");
      id_reg[newid] = id_reg_ps1[oldsubtype];
      id_reg_ps1[oldsubtype].shmid = -1;
      id_reg_ps1[oldsubtype].shmaddr = NULL;
      id_reg_ps1[oldsubtype].state = STREAM_DISCARD;
      id_reg_ps1[oldsubtype].file = NULL;  
    }
  } else {
    DNOTE("%s", "newid private:\n");
    if(oldid != MPEG2_PRIVATE_STREAM_1) {
      DNOTE("%s", "oldid not private\n");
      id_reg_ps1[newsubtype] = id_reg[oldid];
      id_reg[oldid].shmid = -1;
      id_reg[oldid].shmaddr = NULL;
      id_reg[oldid].state = STREAM_DISCARD;
      id_reg[oldid].file = NULL;
    } else {
      DNOTE("%s", "oldid private\n");
      id_reg_ps1[newsubtype] = id_reg_ps1[oldsubtype];
      id_reg_ps1[oldsubtype].shmid = -1;
      id_reg_ps1[oldsubtype].shmaddr = NULL;
      id_reg_ps1[oldsubtype].state = STREAM_DISCARD;
      id_reg_ps1[oldsubtype].file = NULL;
    }
  }
  return 1;
}


int id_has_output(uint8_t stream_id, uint8_t subtype)
{
  
  if(stream_id != MPEG2_PRIVATE_STREAM_1) {
    if(id_reg[stream_id].shmid != -1) {
      return 1;
    } else {
      return 0;
    }
  } else {
    if(id_reg_ps1[subtype].shmid != -1) {
      return 1;
    } else {
      return 0;
    }
  }
}


void id_add(uint8_t stream_id, uint8_t subtype, stream_state_t state, int shmid, char *shmaddr, FILE *file)
{
    if(stream_id != MPEG2_PRIVATE_STREAM_1) {
      id_reg[stream_id].shmid = shmid;
      id_reg[stream_id].shmaddr = shmaddr;
      id_reg[stream_id].state = state;
      id_reg[stream_id].file = file;
      id_reg[stream_id].infile = -1;
      
    } else {
      id_reg_ps1[subtype].shmid = shmid;
      id_reg_ps1[subtype].shmaddr = shmaddr;
      id_reg_ps1[subtype].state = state;
      id_reg_ps1[subtype].file = file;
      id_reg_ps1[subtype].infile = -1;
 
    }
}
  

int init_id_reg(stream_state_t default_state)
{
  int n;
  
  for(n = 0; n < 256; n++) {
    id_reg[n].state = default_state;
    id_reg[n].shmid = -1;
    id_reg[n].shmaddr = NULL;
    id_reg[n].file = NULL;
  }

  for(n = 0; n < 256; n++) {
    id_reg_ps1[n].state = default_state;
    id_reg_ps1[n].shmid = -1;
    id_reg_ps1[n].shmaddr = NULL;
    id_reg_ps1[n].file = NULL;
  }
  
  id_reg[0xBE].state = STREAM_DISCARD; //padding stream
  
  return 0;
}


int attach_decoder_buffer(uint8_t stream_id, uint8_t subtype, int shmid)
{
  char *shmaddr;

#if DEBUG
  fprintf(stderr, "demux: shmid: %d\n", shmid);
#endif
  if(shmid >= 0) {
    if((shmaddr = shmat(shmid, NULL, SHM_SHARE_MMU)) == (void *)-1) {
      perror("demux: attach_decoder_buffer(), shmat()");
      return -1;
    }
    id_add(stream_id, subtype, STREAM_DECODE, shmid, shmaddr, NULL);
    
  } else {
    id_add(stream_id, subtype, STREAM_DISCARD, -1, NULL, NULL);
  }
    
  return 0;  
}


int attach_buffer(int shmid, int size)
{
  char *shmaddr;
  data_buf_head_t *data_buf_head;
  data_elem_t *data_elems;
  int n;
  
#if DEBUG
  fprintf(stderr, "demux: attach_buffer() shmid: %d\n", shmid);
#endif
  
  if(shmid >= 0) {
    if((shmaddr = shmat(shmid, NULL, SHM_SHARE_MMU)) == (void *)-1) {
      perror("demux: attach_buffer(), shmat()");
      exit(1);
    }

    data_buf_addr = shmaddr;
    data_buf_head = (data_buf_head_t *)data_buf_addr;
    data_buf_head->shmid = shmid;
    data_buf_head->nr_of_dataelems = 900;
    data_buf_head->write_nr = 0;
    data_buf_head->buffer_start_offset = (sizeof(data_buf_head_t) +
					  data_buf_head->nr_of_dataelems *
					  sizeof(data_elem_t) +
					  2047)/2048*2048;
    data_buf_head->buffer_size = (size - data_buf_head->buffer_start_offset)
      / 2048*2048;
    
    disk_buf = data_buf_addr + data_buf_head->buffer_start_offset;
    
#if DEBUG
    fprintf(stderr, "demux: setup disk_buf: %lu\n", (unsigned long)disk_buf);
#endif
    data_elems = (data_elem_t *)(data_buf_addr+sizeof(data_buf_head_t));
    for(n = 0; n < data_buf_head->nr_of_dataelems; n++) {
      data_elems[n].in_use = 0;
    }
    
  } else {
    return -1;
  }
    
  return 0;
}



int get_buffer(int size)
{
  MsgEvent_t ev;
  
  size+= sizeof(data_buf_head_t)+900*sizeof(data_elem_t);

  ev.type = MsgEventQReqBuf;
  ev.reqbuf.size = size;
  
  if(MsgSendEvent(msgq, CLIENT_RESOURCE_MANAGER, &ev, 0) == -1) {
    fprintf(stderr, "demux: couldn't send buffer request\n");
  }
  
  while(ev.type != MsgEventQGntBuf) {
    if(MsgNextEvent(msgq, &ev) != -1) {
      if(ev.type == MsgEventQGntBuf) {
	DPRINTF(1, "demux: got buffer id %d, size %d\n",
		ev.gntbuf.shmid,
		ev.gntbuf.size);
	return attach_buffer(ev.gntbuf.shmid,
			     ev.gntbuf.size);
      } else {
	handle_events(&ev);
      }
    }
  }
  
  return 0;
}


void flush_all_streams(int scr_id)
{
  MsgEvent_t ev;
  int n;
  q_head_t *q_head = NULL;
  
  // send flush msg
  ev.type = MsgEventQFlushData;
  ev.flushdata.to_scrid = scr_id;
  
  for(n = 0; n < 256; n++) {
    if((n != MPEG2_PRIVATE_STREAM_1) && (id_reg[n].state == STREAM_DECODE)) {
      q_head = (q_head_t *)id_reg[n].shmaddr;
      if(q_head != NULL) {
	//fprintf(stderr, "demux: flushing stream %02x\n", n);
	if(MsgSendEvent(msgq, q_head->reader, &ev, 0) == -1) {
	  fprintf(stderr, "demux: couldn't send flush\n");
	}
      }
    }
  }

  for(n = 0; n < 256; n++) {
    if(id_reg_ps1[n].state == STREAM_DECODE) {
      q_head = (q_head_t *)id_reg_ps1[n].shmaddr;
      if(q_head != NULL) {
	//fprintf(stderr, "demux: flushing substream %02x\n", n);
	if(MsgSendEvent(msgq, q_head->reader, &ev, 0) == -1) {
	  fprintf(stderr, "demux: couldn't send flush\n");
	}
      }
    }
  }
  
}

int put_in_q(char *q_addr, int off, int len, uint8_t PTS_DTS_flags,
	     uint64_t PTS, uint64_t DTS, int is_new_file, int extra_cmd,
	     PacketType_t packet_type, uint32_t packet_offset)
{
  q_head_t *q_head = NULL;
  q_elem_t *q_elem;
  data_buf_head_t *data_buf_head;
  data_elem_t *data_elems;
  int data_elem_nr;
  int elem;
  MsgEvent_t ev;
  int nr_waits = 0;
  
  static int scr_id = 0;
  static int scr_nr = 0;
  
  /* First find an entry in the big shared buffer pool. */
  
  data_buf_head = (data_buf_head_t *)data_buf_addr;
  data_elems = (data_elem_t *)(data_buf_addr+sizeof(data_buf_head_t));
  data_elem_nr = data_buf_head->write_nr;
  
  /* It's a circular list and if the next packet is still in use we need
   * to wait for it to become available. 
   * We might also do something smarter here but provided that we have a
   * large enough buffer this will not be common.
   */

  // TODO clean and simplify
  while(data_elems[data_elem_nr].in_use) {
    nr_waits++;
    /* If this element is in use we have to wait untill it is released.
     * We know which consumer q we need to wait on. 
     * Normaly a wait on a consumer queue is because there are no free buf 
     * entries (i.e the wait should suspend on the first try). Here we
     * instead want to wait untill the number of empty buffers changes
     * (i.e. not necessarily between 0 and 1). 
     * Because of this we will loop and call ip_sem_wait untill it suspends
     * us or the buffer is no longer in use. 
     * This will effectivly lower the maximum number of buffers untill
     * we are at the 'normal case' of waiting for there to be 1 free buffer.
     * Then we need to check if it was this specific buffer that was consumed
     * if not then try again.
     */
    /* This is a contention case, normally we shouldn't get here */
    
    //fprintf(stderr, "demux: put_in_q(), elem %d in use\n", data_elem_nr);
    
    q_head = (q_head_t *)data_elems[data_elem_nr].q_addr;
    
    q_head->writer_requests_notification = 1;

    while(data_elems[data_elem_nr].in_use) {
      DPRINTF(1, "demux: waiting for notification\n");
      if(MsgNextEvent(msgq, &ev) != -1) {
	handle_events(&ev);
      }
    }
    //q_head->writer_requests_notification = 0;
    

  }
  /* Now the element should be free. 'paranoia check' */
  if(data_elems[data_elem_nr].in_use) {
    fprintf(stderr, "demux: somethings wrong, elem %d still in use\n",
	    data_elem_nr);
  }
  
  /* If decremented earlier, then increment it now to restore the normal 
   * maximum free buffers for the queue. */
  
  
  
  if(PTS_DTS_flags & 0x2) {
    DPRINTF(1, "PTS: %llu.%09llu\n", 
	    PTS/90000, (PTS%90000)*(1000000000/90000));
  }

  data_elems[data_elem_nr].PTS_DTS_flags = PTS_DTS_flags;
  data_elems[data_elem_nr].PTS = PTS;
  data_elems[data_elem_nr].DTS = DTS;
  data_elems[data_elem_nr].SCR_base = SCR_base;
  data_elems[data_elem_nr].SCR_ext = SCR_ext;
  data_elems[data_elem_nr].SCR_flags = SCR_flags;
  data_elems[data_elem_nr].packet_data_offset = off;
  data_elems[data_elem_nr].packet_data_len = len;
  data_elems[data_elem_nr].q_addr = q_addr;
  data_elems[data_elem_nr].packet_type = packet_type;
  data_elems[data_elem_nr].packet_offset = packet_offset;
  /*
    if(is_new_file) {
    strcpy(data_elems[data_elem_nr].filename, cur_filename);
    } else {
  */
  data_elems[data_elem_nr].filename[0] = '\0';
  /*
    }
  */
  data_elems[data_elem_nr].flowcmd = extra_cmd;

  if(scr_discontinuity || (demux_cmd & FlowCtrlFlush)) {
    scr_discontinuity = 0;
    scr_nr = (scr_nr+1)%32;
    scr_id++;
    ctrl_time[scr_nr].scr_id = scr_id;
    ctrl_time[scr_nr].offset_valid = OFFSET_NOT_VALID;
    ctrl_time[scr_nr].sync_master = SYNC_NONE;
    DNOTE("demux: changed to scr_nr: %d\n", scr_nr);
  }
  
  if(demux_cmd & FlowCtrlFlush) {
    flush_all_streams(scr_id);
    demux_cmd &= ~FlowCtrlFlush;
  }

  data_elems[data_elem_nr].scr_nr = scr_nr;
  data_elems[data_elem_nr].in_use = 1;

  data_buf_head->write_nr = 
    (data_buf_head->write_nr+1) % data_buf_head->nr_of_dataelems;

  //fprintf(stderr, "demux: put_in_q() off: %d, len %d\n", off, len);
  
  
  
  
  /* Now put the data buffer in the right consumer queue. */
  q_head = (q_head_t *)q_addr;
  q_elem = (q_elem_t *)(q_addr+sizeof(q_head_t));
  elem = q_head->write_nr;
  
  if(q_elem[elem].in_use) {
    q_head->writer_requests_notification = 1;
    
    while(q_elem[elem].in_use) {
      DPRINTF(1, "demux: waiting for notification2\n");
      if(MsgNextEvent(msgq, &ev) != -1) {
	handle_events(&ev);
      }
    }
    //    q_head->writer_requests_notification = 0;
  }
  
  q_elem[elem].data_elem_index = data_elem_nr;
  q_elem[elem].in_use = 1;
  
  q_head->write_nr = (q_head->write_nr+1)%q_head->nr_of_qelems;
  
  if(q_head->reader_requests_notification) {
    q_head->reader_requests_notification = 0;
    ev.type = MsgEventQNotify;
    ev.notify.qid = q_head->qid;
    if(MsgSendEvent(msgq, q_head->reader, &ev, 0) == -1) {
      fprintf(stderr, "demux: couldn't send notification\n");
    }
  }

  return 0;
}

