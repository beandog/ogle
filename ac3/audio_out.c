/* Ogle - A video player
 * Copyright (C) 2000, 2001, 2002 Björn Englund, Håkan Hjort
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
#include <unistd.h>
#include <inttypes.h>

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/shm.h>
#include <fcntl.h>

#ifndef SHM_SHARE_MMU
#define SHM_SHARE_MMU 0
#endif

#include <ogle/msgevents.h>

#include "debug_print.h"
#include "common.h"
#include "queue.h"
#include "timemath.h"
#include "sync.h"
#include "mpeg.h"

#include "parse_config.h"
#include "decode.h"


/* temporary */
int flush_audio(void);

static int get_q();
static int attach_ctrl_shm(int shmid);
static int attach_stream_buffer(uint8_t stream_id, uint8_t subtype, int shmid);

static void handle_events(MsgEventQ_t *q, MsgEvent_t *ev);

static int ctrl_data_shmid;

ctrl_data_t *ctrl_data;
ctrl_time_t *ctrl_time;

static int stream_shmid;
static char *stream_shmaddr;

static int data_buf_shmid;
static char *data_buf_shmaddr;

static int msgqid = -1;
static MsgEventQ_t *msgq;

static int flush_to_scrid = -1;
static int prev_scr_nr = 0;

char *program_name;
int dlevel;

void usage()
{
  fprintf(stderr, "Usage: %s  [-m <msgid>]\n", program_name);
}

adec_handle_t *ahandle = NULL;

int main(int argc, char *argv[])
{
  MsgEvent_t ev;
  int c;
  
  program_name = argv[0];
  GET_DLEVEL();

  /* Parse command line options */
  while ((c = getopt(argc, argv, "m:h?")) != EOF) {
    switch (c) {
    case 'm':
      msgqid = atoi(optarg);
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
  
  if(parse_config() == -1) {
    FATAL("%s", "Couldn't read config files\n");
    exit(1);
  }


  if(msgqid != -1) {
    if((msgq = MsgOpen(msgqid)) == NULL) {
      FATAL("%s", "couldn't get message q\n");
      exit(1);
    }
    
    ev.type = MsgEventQRegister;
    ev.registercaps.capabilities = 
      DECODE_AC3_AUDIO | 
      DECODE_MPEG1_AUDIO | DECODE_MPEG2_AUDIO |
      DECODE_LPCM_AUDIO | DECODE_DTS_AUDIO;
    if(MsgSendEvent(msgq, CLIENT_RESOURCE_MANAGER, &ev, 0) == -1) {
      DPRINTF(1, "a52: register capabilities\n");
    }
    
    while(ev.type != MsgEventQDecodeStreamBuf) {
      if(MsgNextEvent(msgq, &ev) != -1) {
	handle_events(msgq, &ev);
      }
    }

  } else {
    FATAL("%s", "what? need a msgid\n");
  }


  while(1) {
    get_q();
  }

  return 0;
}


static void handle_events(MsgEventQ_t *q, MsgEvent_t *ev)
{
  
  switch(ev->type) {
  case MsgEventQNotify:
    DPRINTF(1, "a52: got notify\n");
    break;
  case MsgEventQFlushData:
    DPRINTF(1, "a52: got flush\n");
    flush_to_scrid = ev->flushdata.to_scrid;
    break;
  case MsgEventQDecodeStreamBuf:
    DPRINTF(1, "a52: got stream %x, %x buffer \n",
	    ev->decodestreambuf.stream_id,
	    ev->decodestreambuf.subtype);
    attach_stream_buffer(ev->decodestreambuf.stream_id,
			  ev->decodestreambuf.subtype,
			  ev->decodestreambuf.q_shmid);
    
    break;
  case MsgEventQCtrlData:
    attach_ctrl_shm(ev->ctrldata.shmid);
    break;
  case MsgEventQSpeed:
    if(ev->speed.speed == 1.0) {
      set_speed(&ctrl_time[prev_scr_nr].sync_point, ev->speed.speed);
    } else {
      if(ctrl_time[prev_scr_nr].sync_point.speed == 1.0) {
	set_speed(&ctrl_time[prev_scr_nr].sync_point, ev->speed.speed);
	if((ctrl_time[prev_scr_nr].sync_master == SYNC_AUDIO)) {
	  ctrl_time[prev_scr_nr].sync_master = SYNC_NONE;
	}
      }
    }
    break;
  default:
    DNOTE("unrecognized event type: %d\n", ev->type);
    break;
  }
}




int attach_ctrl_shm(int shmid)
{
  char *shmaddr;
  
  if(shmid >= 0) {
    if((shmaddr = shmat(shmid, NULL, SHM_SHARE_MMU)) == (void *)-1) {
      perror("a52: attach_ctrl_data(), shmat()");
      return -1;
    }
    
    ctrl_data_shmid = shmid;
    ctrl_data = (ctrl_data_t*)shmaddr;
    ctrl_time = (ctrl_time_t *)(shmaddr+sizeof(ctrl_data_t));
  }    
  
  return 0;
}

int attach_stream_buffer(uint8_t stream_id, uint8_t subtype, int shmid)
{
  char *shmaddr;
  q_head_t *q_head;

  //DNOTE("a52_decoder: shmid: %d\n", shmid);
  
  if(shmid >= 0) {
    if((shmaddr = shmat(shmid, NULL, SHM_SHARE_MMU)) == (void *)-1) {
      perror("ac52_decoder: attach_decoder_buffer(), shmat()");
      return -1;
    }
    
    stream_shmid = shmid;
    stream_shmaddr = shmaddr;
  }    

  q_head = (q_head_t *)stream_shmaddr;
  shmid = q_head->data_buf_shmid;
  
  if(shmid >= 0) {
    if((shmaddr = shmat(shmid, NULL, SHM_SHARE_MMU)) == (void *)-1) {
      perror("a52_decoder: attach_data_buffer(), shmat()");
      return -1;
    }
    
    data_buf_shmid = shmid;
    data_buf_shmaddr = shmaddr;
  }    

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
  uint8_t *data_buffer;
  uint8_t PTS_DTS_flags;
  uint64_t PTS;
  uint64_t DTS;
  int scr_nr;
  int packet_data_offset;
  int packet_data_len;
  PacketType_t packet_type;
  uint32_t packet_offset;
  int pts_offset = 0;
  int decode_offset = 0;
  int decode_len = 0;
  MsgEvent_t ev;
  uint8_t stream_id;
  uint8_t subtype;
  AudioType_t new_audio_type;


  q_head = (q_head_t *)stream_shmaddr;
  q_elems = (q_elem_t *)(stream_shmaddr+sizeof(q_head_t));
  elem = q_head->read_nr;

  while(MsgCheckEvent(msgq, &ev) != -1) {
    handle_events(msgq, &ev);
  }
  
  if(!q_elems[elem].in_use) {
    q_head->reader_requests_notification = 1;
    
    while(!q_elems[elem].in_use) {
      DPRINTF(1, "a52: waiting for notification1\n");
      if(MsgNextEvent(msgq, &ev) != -1) {
	handle_events(msgq, &ev);
      }
    }
  }

  data_head = (data_buf_head_t *)data_buf_shmaddr;
  data_buffer = data_buf_shmaddr + data_head->buffer_start_offset;
  data_elems = (data_elem_t *)(data_buf_shmaddr+sizeof(data_buf_head_t));
  
  data_elem = &data_elems[q_elems[elem].data_elem_index];
  
  PTS_DTS_flags = data_elem->PTS_DTS_flags;
  PTS = data_elem->PTS;
  DTS = data_elem->DTS;
  scr_nr = data_elem->scr_nr;
  /*
    p[0] is the number of frames of audio which have a sync code in this pack
    p[1]<<8 | p[2] is the starting index of the frame which 
      the PTS value belong to
  */
#if 0
  {
    uint8_t *tbuf;
    uint8_t *lbuf;  
    uint8_t *cbuf;  
    int toff, tlen, pts_off;
    toff = data_elem->packet_data_offset;
    tbuf = data_buffer;
    tlen = data_elem->packet_data_len-3;
    lbuf = tbuf+toff+3;
    cbuf = data_buffer+toff;
    
    fprintf(stderr, "%02x %02x %02x %02x, %02x %02x\n",
	    cbuf[-1], cbuf[0], cbuf[1], cbuf[2], cbuf[3], cbuf[4]);

    pts_off = (data_buffer+toff)[1]<<8 | (data_buffer+toff)[2];

    fprintf(stderr, "buflen: %d, ", tlen); 
    fprintf(stderr, "nr of frames with sync: %d, offset to pts frame: %d\n",
	    (data_buffer+toff)[0], pts_off);

    if(pts_off != 0) {
      pts_off--;
      if(tlen-pts_off >= 7) {
	int length, coded_flags, sample_rate, bit_rate;
	
	length = a52_syncinfo(tbuf+toff+3+pts_off,
			      &coded_flags, &sample_rate, &bit_rate);
	fprintf(stderr, "%02x %02x %02x, %02x %02x %02x %02x %02x %02x %02x, %02x %02x %02x\n", 
		(lbuf+pts_off)[-3],
		(lbuf+pts_off)[-2],
		(lbuf+pts_off)[-1],
		(lbuf+pts_off)[0],
		(lbuf+pts_off)[1],
		(lbuf+pts_off)[2],
		(lbuf+pts_off)[3],
		(lbuf+pts_off)[4],
		(lbuf+pts_off)[5],
		(lbuf+pts_off)[6],
		(lbuf+pts_off)[7],
		(lbuf+pts_off)[8],
		(lbuf+pts_off)[9]);
	
	fprintf(stderr, "frame_len: %d\n", length);
      } else {
	fprintf(stderr, "not enough headerbytes\n");
      }
    } else {
      fprintf(stderr, "**** no pts offset *****\n");
    }
  }
#endif
  packet_type = data_elem->packet_type;
  packet_offset = data_elem->packet_offset;

  packet_data_offset = data_elem->packet_data_offset;
  packet_data_len = data_elem->packet_data_len;
  
  new_audio_type = AudioType_None;
  
  if(packet_type == PacketType_MPEG1 || packet_type == PacketType_PES) {
    stream_id = (data_buffer+packet_offset)[3];
    
    if(stream_id == MPEG2_PRIVATE_STREAM_1) {
      subtype = (data_buffer+packet_data_offset)[0];
      
      if((subtype >= 0x80) && (subtype < 0x88)) {
	//ac-3
	new_audio_type = AudioType_AC3;
	decode_offset = packet_data_offset+4;
	decode_len = packet_data_len-4;

	pts_offset = (data_buffer+packet_data_offset)[2]<<8 |
	  (data_buffer+packet_data_offset)[3];

	if(pts_offset ==  0) {
	  if(PTS_DTS_flags) {

	  fprintf(stderr, "**pts_offset: 0,  nr_frames: %d, pts_flags: %d report bug\n",
		  (data_buffer+packet_data_offset)[1],
		  PTS_DTS_flags);
	  } else if((data_buffer+packet_data_offset)[1] != 0) {
	  fprintf(stderr, "**pts_offset: 0,  nr_frames: %d, pts_flags: %d report bug\n",
		  (data_buffer+packet_data_offset)[1],
		  PTS_DTS_flags);
	  }	    
	} else {
	  pts_offset--;
	}

#if 0
	fprintf(stderr, "subid: %02x, ", 
		(data_buffer+packet_data_offset)[0]);
#endif
#if 0
	fprintf(stderr, "*nr_frames: %d\n", 
		(data_buffer+packet_data_offset)[1]);
	fprintf(stderr, "pts_offset: %d\n", pts_offset);
#endif
      } else if((subtype >= 0x88) && (subtype < 0x90)) {
	//dts
	new_audio_type = AudioType_DTS;
	decode_offset = packet_data_offset+4;
	decode_len = packet_data_len-4;

	pts_offset = (data_buffer+packet_data_offset)[2]<<8 |
	  (data_buffer+packet_data_offset)[3];
	//nr_frames = (data_buffer+packet_data_offset)[1];
#if 0
	fprintf(stderr, "PTS_DTS_flags: %x\n",
		PTS_DTS_flags);
	WARNING("DTS Audio not implemented\n");
	{
	  int n, m;
	  for(n = 0; n < 2016; n+=32) {
	    fprintf(stderr, "HEADER %03x:", n);
	    for(m = 0; m < 32; m++) {
	      fprintf(stderr, ",%02x",(data_buffer+packet_data_offset)[n+m]);
	    }
	    fprintf(stderr, "\n");
	  }
	}

	fprintf(stderr, "packet data len: 0x%x (%d)\n",
		packet_data_len, packet_data_len);
#endif	
      } else if((subtype >= 0xA0) && (subtype < 0xA8)) {
	//lpcm
	// lpcm dvd
	// frame rate 600Hz (48/96kHz)
	// 16/20/24 bits
	// 8ch
	// max 6.144Mbps
	// [0] private stream sub type
	// [1] number_of_frame_headers
	// [2-3] first_access_unit_pointer (offset from [3])
	// [4] audio_frame_number 
	//          (of first access unit (wraps at 19 ?))
	//          (20 frames at 1/600Hz = 1/30 (24 frames for 25fps?)
	// [5]: 
	//      b7-b6: quantization_word_length,
	//            0: 16bits, 1: 20bits, 2: 24bits, 3: reserved
	//      b5: reserved
	//      b4: audio_sampling_frequency,
	//            0: 48 kHz, 1: 96 kHz
	//      b3: reserved
	//      b2-b0: number_of_audio_channels
	//            0: mono (2ch ? dual-mono: L=R)
	//            1: 2ch (stereo)
	//            2: 3 channel 
        //            3: 4 ch 
        //            4: 5 ch 
        //            5: 6 ch 
        //            6: 7 ch 
        //            7: 8 ch 
	// [6]: dynamic_range
	new_audio_type = AudioType_LPCM;
	decode_offset = packet_data_offset+4;
	decode_len = packet_data_len-4;

	pts_offset = (data_buffer+packet_data_offset)[2]<<8 |
	  (data_buffer+packet_data_offset)[3];
	if(pts_offset > 0) {
	  pts_offset--;
	}
      } else {
	ERROR("Unhandled PrivateStream1 subtype: %02x\n", subtype);
      }

    } else if((stream_id & MPEG_AUDIO_STREAM_MASK) == MPEG_AUDIO_STREAM) {
      static int first = 1;
      new_audio_type = AudioType_MPEG;
      if(first) {
	DNOTE("%s", "MPEG Audio\n");
	first = 0;
      }
      decode_offset = packet_data_offset;
      decode_len = packet_data_len;
      if(!PTS_DTS_flags) {
	pts_offset = -1;
      } else {
	pts_offset = 0;
      }
    } else {
      ERROR("Unhandled stream_id: %02x\n", stream_id);
    }
  } else {
    ERROR("Unhandled packet type: %d\n", packet_type);
  }
  
  //if there is an old adec handle, close/drain/flush
  if(ahandle) {
    if(adec_type(ahandle) != new_audio_type) {
      adec_free(ahandle);
      ahandle = adec_init(new_audio_type);
    }
  } else {
    ahandle = adec_init(new_audio_type);
  }


	  
  
  if(flush_to_scrid != -1) {
    if(ctrl_time[scr_nr].scr_id < flush_to_scrid) {

      q_head->read_nr = (q_head->read_nr+1)%q_head->nr_of_qelems;
      
      // release elem
      data_elem->in_use = 0;
      q_elems[elem].in_use = 0;
      
      if(q_head->writer_requests_notification) {
	q_head->writer_requests_notification = 0;
	ev.type = MsgEventQNotify;
	if(MsgSendEvent(msgq, q_head->writer, &ev, 0) == -1) {
	  WARNING("%s", "couldn't send notification\n");
	}
      }
      //DNOTE("flushed, packet droped on scr\n");
      return 0;
    } else {
      //TODO flush audio now that we have flushed all packets
      DNOTE("%s", "flushing audio driver\n");
      flush_audio();
      DNOTE("%s", "flushed audio\n");
      flush_to_scrid = -1;
    }
  }
  
  prev_scr_nr = scr_nr;
  q_head->read_nr = (q_head->read_nr+1)%q_head->nr_of_qelems;
  
  if(ahandle && ctrl_data->speed == 1.0) {
    adec_decode(ahandle, data_buffer+decode_offset, decode_len, 
		pts_offset, PTS, scr_nr);
  } else {
    if(PTS_DTS_flags &&
       (ctrl_time[scr_nr].offset_valid != OFFSET_NOT_VALID)) {
      clocktime_t time_left, real_time, scr_time;
      clocktime_get(&real_time);
      PTS_TO_CLOCKTIME(scr_time, PTS);
      calc_realtime_left_to_scrtime(&time_left, &real_time, &scr_time,
				    &(ctrl_time[scr_nr].sync_point));
      
      while((TIME_S(time_left) >= 0) && (TIME_SS(time_left) >= 0)) {
	if((TIME_S(time_left)>0) || (TIME_SS(time_left) > (CT_FRACTION/10))) {
	  TIME_S(time_left) = 0;
	  TIME_SS(time_left) = CT_FRACTION/10;
	}
#ifndef HAVE_CLOCK_GETTIME
	struct timespec tl;
	tl.tv_sec = TIME_S(time_left);
	tl.tv_nsec = TIME_SS(time_left) * 1000;
	
	nanosleep(&tl, NULL);
#else
	nanosleep(&time_left, NULL);
#endif
	clocktime_get(&real_time);
	calc_realtime_left_to_scrtime(&time_left, &real_time, &scr_time,
				      &(ctrl_time[scr_nr].sync_point));
      }
    }
  }
  
  // release elem
  data_elem->in_use = 0;
  q_elems[elem].in_use = 0;
  
  if(q_head->writer_requests_notification) {
    q_head->writer_requests_notification = 0;
    ev.type = MsgEventQNotify;
    if(MsgSendEvent(msgq, q_head->writer, &ev, 0) == -1) {
      WARNING("%s", "couldn't send notification\n");
    }
  }

  return 0;
}


int flush_audio(void)
{
  //flush sound_card
  if(ahandle)
    adec_flush(ahandle);

  return 0;
}
