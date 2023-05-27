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
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/shm.h>
#include <sys/msg.h>
#include <errno.h>
#include <string.h>

#include <ogle/msgevents.h>
#include "debug_print.h"
#include "c_getbits.h"
#include "common.h"
#include "queue.h"
#include "timemath.h"
#include "sync.h"
#ifndef SHM_SHARE_MMU
#define SHM_SHARE_MMU 0
#endif



extern data_q_t *data_q_head;
extern int ctrl_data_shmid;
extern ctrl_data_t *ctrl_data;
extern ctrl_time_t *ctrl_time;

extern uint8_t PTS_DTS_flags;
extern uint64_t PTS;
extern uint64_t DTS;
extern int scr_nr;

extern int msgqid;
extern int output_client;
extern int input_stream;

int stream_shmid = -1;
char *stream_shmaddr;

char *data_buf_shmaddr;

int flush_to_scrid = -1;

int get_q();
int attach_stream_buffer(uint8_t stream_id, uint8_t subtype, int shmid);
extern int detach_data_q(int q_shmid, data_q_t **data_q_list);
int attach_ctrl_shm(int shmid);
void handle_events(MsgEventQ_t *q, MsgEvent_t *ev);


MsgEventQ_t *msgq;

FILE *infile;
char *infilename;

#ifdef GETBITSMMAP // Mmap i/o
uint32_t *buf;
uint32_t buf_size;

uint32_t packet_offset;
uint32_t packet_length;

uint8_t *mmap_base = NULL;
uint8_t *data_buffer = NULL;

#else // Normal i/o
uint32_t buf[BUF_SIZE_MAX] __attribute__ ((aligned (8)));

#endif

#ifdef GETBITS32
unsigned int backed = 0;
unsigned int buf_pos = 0;
unsigned int bits_left = 32;
uint32_t cur_word = 0;
#endif

#ifdef GETBITS64
int offs = 0;
unsigned int bits_left = 64;
uint64_t cur_word = 0;
#endif


#ifdef GETBITSMMAP // Support functions


static void change_file(char *new_filename)
{
  int filefd;
  static struct stat statbuf;
  int rv;
  static char *cur_filename = NULL;


  if(new_filename == NULL) {
    return;
  }
  //if no filename do nothing
  if(new_filename[0] == '\0') {
    return;
  }

  // if same filename do nothing
  if(cur_filename != NULL && strcmp(cur_filename, new_filename) == 0) {
    return;
  }

  if(mmap_base != NULL) {
    munmap(mmap_base, statbuf.st_size);
  }
  if(cur_filename != NULL) {
    free(cur_filename);
  }
  
  filefd = open(new_filename, O_RDONLY);
  if(filefd == -1) {
    perror(new_filename);
    exit(1);
  }

  cur_filename = strdup(new_filename);
  rv = fstat(filefd, &statbuf);
  if(rv == -1) {
    perror("fstat");
    exit(1);
  }
  mmap_base = (uint8_t *)mmap(NULL, statbuf.st_size, 
			      PROT_READ, MAP_SHARED, filefd,0);
  close(filefd);
  if(mmap_base == MAP_FAILED) {
    perror("mmap");
    exit(1);
  }


  
#ifdef HAVE_MADVISE
  rv = madvise(mmap_base, statbuf.st_size, MADV_SEQUENTIAL);
  if(rv == -1) {
    perror("madvise");
    exit(1);
  }
#endif
  DPRINTF(1, "All mmap systems ok!\n");
}


void get_next_packet()
{
  if(msgqid == -1) {
    if(mmap_base == NULL) {
      clocktime_t real_time;
      clocktime_t scr_time;
      
      change_file(infilename);
      packet_offset = 0;
      packet_length = 1000000000;
      clocktime_get(&real_time);
      ctrl_time = malloc(sizeof(ctrl_time_t));
      scr_nr = 0;
      ctrl_time[scr_nr].offset_valid = OFFSET_VALID;
      PTS_TO_CLOCKTIME(scr_time, PTS);
      set_sync_point(&ctrl_time[scr_nr],
		     &real_time,
		     &scr_time,
		     ctrl_data->speed);

    } else {
      packet_length = 1000000000;


    }
  } else {
    
    if(stream_shmid != -1) {
      get_q();
    } 
    
  }
}  


void read_buf()
{
  //  uint8_t *packet_base = &mmap_base[packet_offset];
  uint8_t *packet_base = &data_buffer[packet_offset];
  int end_bytes;
  int i, j;
  
  /* How many bytes are there left? (0, 1, 2 or 3). */
  end_bytes = &packet_base[packet_length] - (uint8_t *)&buf[buf_size];
  
  /* Read them, as we have at least 32 bits free they will fit. */
  i = 0;
  while( i < end_bytes ) {
    //cur_word=cur_word|(((uint64_t)packet_base[packet_length-end_bytes+i++])<<(56-bits_left)); //+
    cur_word=(cur_word << 8) | packet_base[packet_length - end_bytes + i++];
    bits_left += 8;
  }
   
  /* If we have enough 'free' bits so that we always can align
     the buff[] pointer to a 4 byte boundary. */
  if( (64-bits_left) >= 24 ) {
    int start_bytes;
    get_next_packet(); // Get new packet struct
    // packet_base = &mmap_base[packet_offset];
    packet_base = &data_buffer[packet_offset];
    
    /* How many bytes to the next 4 byte boundary? (0, 1, 2 or 3). */
    start_bytes = (4 - ((long)packet_base % 4)) % 4; 
    
    /* Do we have that many bytes in the packet? */
    j = start_bytes < packet_length ? start_bytes : packet_length;
    
    /* Read them, as we have at least 24 bits free they will fit. */
    i = 0;
    while( j-- ) {
      //cur_word=cur_word|(((uint64_t)packet_base[i++])<<(56-bits_left)); //+
      cur_word  = (cur_word << 8) | packet_base[i++];
      bits_left += 8;
    }
     
    buf = (uint32_t *)&packet_base[start_bytes];
    buf_size = (packet_length - start_bytes) / 4; // Number of 32 bit words
    offs = 0;
    
    /* If there were fewer bytes than needed to get to the first 4 byte boundary,
       then we need to make this inte a valid 'empty' packet. */
    if( start_bytes > packet_length ) {
      /* This make the computation of end_bytes come 0 and 
	 forces us to call read_buff() the next time get/dropbits are used. */
      packet_length = start_bytes;
      buf_size = 0;
    }
    
    /* Make sure we have enough bits before we return */
    if(bits_left <= 32) {
      /* If the buffer is empty get the next one. */
      if(offs >= buf_size)
	read_buf();
      else {
	uint32_t new_word = FROM_BE_32(buf[offs++]);
	//cur_word = cur_word | (((uint64_t)new_word) << (32-bits_left)); //+
	cur_word = (cur_word << 32) | new_word;
	bits_left += 32;
      }
    }
  
  } else {
    /* The trick!! 
       We have enough data to return. Infact it's so much data that we 
       can't be certain that we can read enough of the next packet to 
       align the buff[ ] pointer to a 4 byte boundary.
       Fake it so that we still are at the end of the packet but make
       sure that we don't read the last bytes again. */
    
    packet_length -= end_bytes;
  }

}
#else // Normal (no-mmap) file io support functions

void read_buf()
{
  if(!fread(&buf[0], READ_SIZE, 1 , infile)) {
    if(feof(infile)) {
      fprintf(stderr, "*End Of File\n");
      exit_program(0);
    } else {
      fprintf(stderr, "**File Error\n");
      exit_program(0);
    }
  }
  offs = 0;
}

#endif



#ifdef GETBITS32 // 'Normal' getbits, word based support functions
void back_word(void)
{
  backed = 1;
  
  if(buf_pos == 0) {
    buf_pos = 1;
  } else {
    buf_pos = 0;
  }
  cur_word = buf[buf_pos];
}

void next_word(void)
{

  if(buf_pos == 0) {
    buf_pos = 1;
  } else {
    buf_pos = 0;
  }
  
  
  if(backed == 0) {
    if(!fread(&buf[buf_pos], 1, 4, infile)) {
      if(feof(infile)) {
	fprintf(stderr, "*End Of File\n");
	exit_program(0);
      } else {
	fprintf(stderr, "**File Error\n");
	exit_program(0);
      }
    }
  } else {
    backed = 0;
  }
  cur_word = buf[buf_pos];

}
#endif


/* ---------------------------------------------------------------------- */




void handle_events(MsgEventQ_t *q, MsgEvent_t *ev)
{
  
  switch(ev->type) {
  case MsgEventQNotify:
    DPRINTF(1, "video_decode: got notification\n");
    break;
  case MsgEventQFlushData:
    DPRINTF(1, "vs: got flush\n");
    flush_to_scrid = ev->flushdata.to_scrid;
    break;
  case MsgEventQDecodeStreamBuf:
    DPRINTF(1, "video_decode: got stream %x, %x buffer \n",
	    ev->decodestreambuf.stream_id,
	    ev->decodestreambuf.subtype);
    attach_stream_buffer(ev->decodestreambuf.stream_id,
			 ev->decodestreambuf.subtype,
			 ev->decodestreambuf.q_shmid);
    
    break;
  case MsgEventQCtrlData:
    attach_ctrl_shm(ev->ctrldata.shmid);
    break;
  case MsgEventQQDetached:
    detach_data_q(ev->detachq.q_shmid, &data_q_head);
    break;
  default:
    fprintf(stderr, "*video_decoder: unrecognized event type\n");
    break;
  }
}


int attach_stream_buffer(uint8_t stream_id, uint8_t subtype, int shmid)
{
  char *shmaddr;
  q_head_t *q_head;
  
  //DNOTE("video_dec: shmid: %d\n", shmid);
  
  if(shmid >= 0) {
    if((shmaddr = shmat(shmid, NULL, SHM_SHARE_MMU)) == (void *)-1) {
      perror("video_stream: attach_decoder_buffer(), shmat()");
      return -1;
    }
    
    stream_shmid = shmid;
    stream_shmaddr = shmaddr;
    
  }    

  q_head = (q_head_t *)stream_shmaddr;
  shmid = q_head->data_buf_shmid;
  //DNOTE("video_dec: data_buf shmid: %d\n", shmid);

  if(shmid >= 0) {
    if((shmaddr = shmat(shmid, NULL, SHM_SHARE_MMU)) == (void *)-1) {
      perror("video_stream: attach_buffer(), shmat()");
      return -1;
    }
    
    data_buf_shmaddr = shmaddr;
    
  }    
  
  input_stream = 1;
  
  return 0;
  
}


int get_q()
{
  q_head_t *q_head;
  q_elem_t *q_elems;
  data_buf_head_t *data_head;
  data_elem_t *data_elems;
  data_elem_t *data_elem;
  int elem;
  MsgEvent_t ev;
  //  uint8_t PTS_DTS_flags;
  //  uint64_t PTS;
  //  uint64_t DTS;

  int off;
  int len;
  static int have_buf = 0;
  static uint8_t *tmp_base;
  static uint8_t dummy_buf[8] = {0, 0, 0, 0, 0, 0, 0, 0};

  //fprintf(stderr, "video_dec: get_q()\n");
  
  q_head = (q_head_t *)stream_shmaddr;
  q_elems = (q_elem_t *)(stream_shmaddr+sizeof(q_head_t));
  elem = q_head->read_nr;
  
  data_elems = (data_elem_t *)(data_buf_shmaddr+sizeof(data_buf_head_t));
  
  // release prev elem 
  // if we have 50 or the last packet indicated a compleat video unit
  if(have_buf > 50 
     || (have_buf && (data_elems[q_elems[elem].data_elem_index].flowcmd 
		      == FlowCtrlCompleteVideoUnit))) {
    int n;
    int m;
    data_elem_t *delem;
        
    data_elems = (data_elem_t *)(data_buf_shmaddr+sizeof(data_buf_head_t));
    m = elem;
    
    for(n = have_buf; n > 0; n--) {
      delem = &data_elems[q_elems[m].data_elem_index];
      delem->in_use = 0;
      q_elems[m].in_use = 0;
      m--;
      if(m < 0) {
	m = q_head->nr_of_qelems-1;
      }
      have_buf--;
    }

    if(q_head->writer_requests_notification) {
      q_head->writer_requests_notification = 0;
      ev.type = MsgEventQNotify;
      if(MsgSendEvent(msgq, q_head->writer, &ev, 0) == -1) {
	fprintf(stderr, "video_decode: couldn't send notification\n");
      }
    }

    q_head->read_nr = (q_head->read_nr+1)%q_head->nr_of_qelems;
    elem = q_head->read_nr;  
  }
  
  if(have_buf) {
    q_head->read_nr = (q_head->read_nr+1)%q_head->nr_of_qelems;
    elem = q_head->read_nr;  
  }
  // wait for buffer
  if(!q_elems[elem].in_use) {
    q_head->reader_requests_notification = 1;
    
    while(!q_elems[elem].in_use) {
      //fprintf(stderr, "video_decode: waiting for notification1\n");
      if(MsgNextEvent(msgq, &ev) != -1) {
	handle_events(msgq, &ev);
      }
    }
  }

  have_buf++;

  data_elems = (data_elem_t *)(data_buf_shmaddr+sizeof(data_buf_head_t));
  data_head = (data_buf_head_t *)data_buf_shmaddr;
  data_buffer = data_buf_shmaddr + data_head->buffer_start_offset;
  data_elem = &data_elems[q_elems[elem].data_elem_index];
  
  PTS_DTS_flags = data_elem->PTS_DTS_flags;
  if(PTS_DTS_flags & 0x2) {
    PTS = data_elem->PTS;
    scr_nr = data_elem->scr_nr;
  }
  if(PTS_DTS_flags & 0x1) {
    DTS = data_elem->DTS;
  }
  
  
  //change_file(data_elem->filename);
  
  off = data_elem->packet_data_offset;
  len = data_elem->packet_data_len;
  
#if 0
  switch(data_elem->flowcmd) {
  case FlowCtrlCompleteVideoUnit:
    if(mmap_base != dummy_buf) {
      tmp_base = mmap_base;
      mmap_base = dummy_buf;
      off = 0;
      len = 8;
    }
    break;
  case FlowCtrlNone:
  default:
    if(mmap_base == dummy_buf) {
      mmap_base = tmp_base;
    } 
    break;
  }
#endif
 switch(data_elem->flowcmd) {
  case FlowCtrlCompleteVideoUnit:
    if(data_buffer != dummy_buf) {
      tmp_base = data_buffer;
      data_buffer = dummy_buf;
      off = 0;
      len = 8;
    }
    break;
  case FlowCtrlNone:
  default:
    if(data_buffer == dummy_buf) {
      data_buffer = tmp_base;
    } 
    break;
  }

  if(PTS_DTS_flags & 0x2) {
    if(flush_to_scrid != -1) {
      if(ctrl_time[scr_nr].scr_id < flush_to_scrid) {
      } else {
	flush_to_scrid = -1;
      }
    }
  }
  
  packet_offset = off;
  packet_length = len;
  /*
  fprintf(stderr, "video_dec: got q off: %d, len: %d\n",
	  packet_offset, packet_length);
  
  fprintf(stderr, "ac3: flags: %01x, pts: %llx, dts: %llx\noff: %d, len: %d\n",
	  PTS_DTS_flags, PTS, DTS, off, len);
  */


  return 0;
}


int attach_ctrl_shm(int shmid)
{
  char *shmaddr;
  
  if(shmid >= 0) {
    if((shmaddr = shmat(shmid, NULL, SHM_SHARE_MMU)) == (void *)-1) {
      perror("attach_ctrl_data(), shmat()");
      return -1;
    }
    
    ctrl_data_shmid = shmid;
    ctrl_data = (ctrl_data_t*)shmaddr;
    ctrl_time = (ctrl_time_t *)(shmaddr+sizeof(ctrl_data_t));
  }
  
  return 0;  
}




