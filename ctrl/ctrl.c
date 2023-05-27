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

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

#include <ogle/msgevents.h>
#include "mpeg.h"
#include "common.h"
#include "queue.h"
#include "timemath.h"
#include "debug_print.h"

#ifndef SHM_SHARE_MMU
#define SHM_SHARE_MMU 0
#endif

/* SA_SIGINFO isn't implemented yet on for example NetBSD */
#if !defined(SA_SIGINFO)
#define siginfo_t void
#endif

int create_msgq(void);
int init_decoder(char *msgqid_str, char *decoderstr);
int get_buffer(int size, shm_bufinfo_t *bufinfo);
int create_q(int nr_of_elems, int buf_shmid, 
	     MsgEventClient_t writer, MsgEventClient_t reader);
int create_ctrl_data(void);
int register_stream(uint8_t stream_id, uint8_t subtype);

static void handle_events(MsgEventQ_t *q, MsgEvent_t *ev);

void int_handler(int sig);
void sigchld_handler(int sig, siginfo_t *info, void* context);

void remove_q_shm(void);
void add_q_shmid(int shmid);
void remove_q_shmid(int shmid);
void destroy_msgq(void);

void add_to_pidlist(pid_t pid, char *name);
int remove_from_pidlist(pid_t pid);

void cleanup_and_exit(void);
void slay_children(void);

int msgqid;

int ctrl_data_shmid;
ctrl_data_t *ctrl_data;

char *program_name;
int dlevel;

char msgqid_str[9];

char *input_file;

char *framerate = NULL;
char *output_bufs = NULL;
char *file_offset = NULL;
char *videodecode_debug = NULL;
char *demux_debug = NULL;

int ac3_audio_stream = -1;
int dts_audio_stream = -1;
int mpeg_audio_stream = -1;
int lpcm_audio_stream = -1;
int mpeg_video_stream = -1;
int subpicture_stream = -1;
int nav_stream = -1;
char *ui = NULL;

static int child_killed = 0;

void usage(void)
{
  fprintf(stderr, "Usage: %s [-h] [-u cli|gui] [<path>]\n", program_name);
  //"[-a <ac3_stream#>] [-m <mpeg_audio_stream#>] [-p <pcm_audio_stream#>] [-v <mpeg_video_stream#>] [-s <subpicture_stream#>] [-n] [-f <fps>] [-r <#ouput_bufs>] [-o <file_offset>] [-d <videodebug_level>] [-D <demuxdebug_level>] <input file>\n", program_name);
}


int next_client_id = CLIENT_UNINITIALIZED+1;
int demux_registered = 0;
MsgEventClient_t demux_client;


typedef enum {
  CAP_started = 1,
  CAP_running = 2
} cap_state_t;

/* 
 * a client can offer several different capabilities.
 * a client can offer several instances of the same capability.
 * several instances of the same client can exist.
 * an instance of a client can be identified by its MsgEventClient_t
 * a client ca be in two states, Started: it has been exec'd but
 * haven't registered itself.
 * Running: the client has registered itself and is ready.
 */


typedef struct _capability_t {
  struct _capability_t *next;
  int capability;
  int nr_of_instances;
  int instances_in_use;
} capability_t;

typedef struct {
  MsgEventClient_t client;          // client id
  int capabilities;                 // mask of all capabilities the client has
  capability_t *used_capabilities;  // list of the capabilities in use
  cap_state_t state;                // started, running
  pid_t pid;                        // pid of client
  char *exec_name;                  // file name of executable
} process_info_t;


typedef struct {
  MsgEventClient_t client;
  int caps;
  cap_state_t state;
  char *executable_file;
} caps_t;

static caps_t *caps_array = NULL;
static int nr_caps = 0;

int register_capabilities(MsgEventClient_t client, int caps, cap_state_t state)
{
  if(nr_caps >= 20) {
    WARNING("%s", "more than 20 capabilities registered\n");
  }
  nr_caps++;
  caps_array = realloc(caps_array, sizeof(caps_t)*nr_caps);
  caps_array[nr_caps-1].client = client;
  caps_array[nr_caps-1].caps = caps;
  caps_array[nr_caps-1].state = state;
  return 0;
}

int search_capabilities(int caps, MsgEventClient_t *client, int *ret_caps,
			cap_state_t *ret_state)
{
  int n;
  int nr = 0;

  if(client != NULL) {
    *client = CLIENT_NONE;
  }
#if DEBUG
  DNOTE("searching cap: %d\n", caps);
#endif
  
  for(n = 0; n < nr_caps; n++) {
    if((caps_array[n].caps & caps) == caps) {
      nr++;
      if(client != NULL) {
	*client = caps_array[n].client;
#if DEBUG
        DNOTE("found capclient: %ld\n", *client);
#endif
      }
      if(ret_caps != NULL) {
	*ret_caps = caps_array[n].caps;
#if DEBUG
	DNOTE("found cap: %x\n", *ret_caps);
#endif
      }

#if DEBUG
      DNOTE("state cap: %d\n", caps_array[n].state);
#endif

      if(ret_state != NULL) {
	*ret_state = caps_array[n].state;
      }
    }
  }

  
  return nr;
}


static int streamid_to_capability(uint8_t stream_id, uint8_t subtype)
{
  int cap = 0;

  if(stream_id == MPEG2_PRIVATE_STREAM_1) {
    if((subtype >= 0x80) && (subtype < 0x88)) {
      cap = DECODE_AC3_AUDIO;

    } else if((subtype >= 0x88) && (subtype < 0x90)) {
      cap = DECODE_DTS_AUDIO;
      
    } else if((subtype >= 0xA0) && (subtype < 0xA8)) {
      cap = DECODE_LPCM_AUDIO;
      
    } else if((subtype >= 0x20) && (subtype < 0x40)) {
      cap = DECODE_DVD_SPU;
    }
    
  } else if((stream_id >= 0xc0) && (stream_id < 0xe0)) {
    cap = DECODE_MPEG1_AUDIO | DECODE_MPEG2_AUDIO;
    
  } else if((stream_id >= 0xe0) && (stream_id < 0xf0)) {
    cap = DECODE_MPEG1_VIDEO | DECODE_MPEG2_VIDEO;
    
  } else if(stream_id == MPEG2_PRIVATE_STREAM_2) {
    cap = DECODE_DVD_NAV;
  }
  
  return cap;
}

static char *streamid_to_decoderstr(uint8_t stream_id, uint8_t subtype)
{
  char *name = NULL;

  if(stream_id == MPEG2_PRIVATE_STREAM_1) {
    if((subtype >= 0x80) && (subtype < 0x88)) {
      name = getenv("DVDP_AC3");
      
    } else if((subtype >= 0x88) && (subtype < 0x90)) {
      name = getenv("DVDP_DTS");

    } else if((subtype >= 0xA0) && (subtype < 0xA8)) {
      name = getenv("DVDP_LPCM");

    } else if((subtype >= 0x20) && (subtype < 0x40)) {
      name = getenv("DVDP_SPU");
    }
    
  } else if((stream_id >= 0xc0) && (stream_id < 0xe0)) {
    name = getenv("DVDP_MPEGAUDIO");
    
  } else if((stream_id >= 0xe0) && (stream_id < 0xf0)) {
    name = getenv("DVDP_VIDEO");
    
  } else if(stream_id == MPEG2_PRIVATE_STREAM_2) {
    name = getenv("DVDP_VMG");
  }
  
  return name;
}

static char *capability_to_decoderstr(int capability, int *ret_capability)
{
  char *name = NULL;
  
  
  if((capability & DECODE_AC3_AUDIO) == capability) {
    name = getenv("DVDP_AC3");
    *ret_capability = DECODE_AC3_AUDIO;
  } else if((capability & DECODE_DTS_AUDIO) == capability) {
    name = getenv("DVDP_DTS");
    *ret_capability = DECODE_DTS_AUDIO;
  } else if((capability & (DECODE_MPEG1_AUDIO | DECODE_MPEG2_AUDIO))
	    == capability) {
    name = getenv("DVDP_MPEGAUDIO");
    *ret_capability = DECODE_MPEG1_AUDIO | DECODE_MPEG2_AUDIO;
  } else if((capability & DECODE_DVD_SPU) == capability) {
    name = getenv("DVDP_SPU");
    *ret_capability = (DECODE_DVD_SPU | VIDEO_OUTPUT);
  } else if((capability & (DECODE_MPEG1_VIDEO | DECODE_MPEG2_VIDEO))
	    == capability) {
    name = getenv("DVDP_VIDEO");
    *ret_capability = DECODE_MPEG1_VIDEO | DECODE_MPEG2_VIDEO;
  } else if((capability & (DEMUX_MPEG1 | DEMUX_MPEG2_PS))
	    == capability) {
    name = getenv("DVDP_DEMUX");
    *ret_capability = DEMUX_MPEG1 | DEMUX_MPEG2_PS;
  } else if((capability & (UI_DVD_CLI))
	    == capability) {
    name = getenv("DVDP_CLI_UI");
    *ret_capability = UI_DVD_CLI;
  } else if((capability & (DECODE_DVD_NAV))
	    == capability) {
    name = getenv("DVDP_VMG");
    *ret_capability = DECODE_DVD_NAV;
  } else if((capability & (UI_DVD_GUI))
	    == capability) {
    name = getenv("DVDP_UI");
    *ret_capability = UI_DVD_GUI;
  } else if((capability & VIDEO_OUTPUT) == capability) {
    name = getenv("DVDP_VIDEO_OUT");
    *ret_capability = (DECODE_DVD_SPU | VIDEO_OUTPUT);
  }

  return name;
}



static void cleanup(void)
{
  //DNOTE("waiting for children to really die\n"); 
  
  while(sleep(2)); // Continue sleeping if interupted 

  slay_children();
  cleanup_and_exit();
}



int request_capability(MsgEventQ_t *q, int cap,
		       MsgEventClient_t *capclient, int *retcaps)
{
  MsgEvent_t r_ev;
  char *decodername;
  cap_state_t state = 0;

#if DEBUG
  DNOTE("_MsgEventQReqCapability\n");
#endif
  
  if(!search_capabilities(cap, capclient, retcaps, &state)) {
    int fullcap;
    
    decodername = capability_to_decoderstr(cap, &fullcap);
	
    if(decodername != NULL) {
      register_capabilities(0,
			    fullcap,
			    CAP_started);
      
    //DNOTE("starting decoder %d %s\n", fullcap, decodername);
      init_decoder(msgqid_str, decodername);
    }
    
  }
  
  while(search_capabilities(cap, capclient, retcaps, &state) &&
	(state != CAP_running)) {
    if(child_killed) {
      cleanup();
    }
    if(MsgNextEventInterruptible(q, &r_ev) == -1) {
      switch(errno) {
      case EINTR:
	continue;
	break;
      }
    }
    handle_events(q, &r_ev);
  }
  
  if(state == CAP_running) {
#if DEBUG
    DNOTE("sending ctrldata\n");
#endif
    r_ev.type = MsgEventQCtrlData;
    r_ev.ctrldata.shmid = ctrl_data_shmid;
    
    MsgSendEvent(q, *capclient, &r_ev, 0);
    
    return 1;
  } else {
    ERROR("%s", "didn't find capability\n");
    return 0;
  }
}


static void handle_events(MsgEventQ_t *q, MsgEvent_t *ev)
{
  MsgEvent_t s_ev;
  MsgEvent_t r_ev;
  MsgEventClient_t rcpt;
  char *decodername;
  int capability;
  
  switch(ev->type) {
  case MsgEventQInitReq:
#if DEBUG
    DNOTE("_MsgEventQInitReq, new_id: %d\n", next_client_id);
#endif
    ev->type = MsgEventQInitGnt;
    ev->initgnt.newclientid = next_client_id++;
    MsgSendEvent(q, CLIENT_UNINITIALIZED, ev, 0);
    break;
  case MsgEventQRegister:
#if DEBUG
    DNOTE("_MsgEventQRegister\n");
#endif
    register_capabilities(ev->registercaps.client,
			  ev->registercaps.capabilities,
			  CAP_running);
    break;
  case MsgEventQReqCapability:
    {
      MsgEvent_t retev;

      retev.type = MsgEventQGntCapability;
      
      if(request_capability(q, ev->reqcapability.capability,
			    &retev.gntcapability.capclient,
			    &retev.gntcapability.capability)) {
	MsgSendEvent(q, ev->reqcapability.client, &retev, 0);
      } else {
	retev.gntcapability.client = CLIENT_NONE;
	MsgSendEvent(q, ev->reqcapability.client, &retev, 0);
      }
    }
    break;
  case MsgEventQReqBuf:
    {
      shm_bufinfo_t bufinfo;
      
#if DEBUG
      DNOTE("_got request for buffer size %d\n",
	      ev->reqbuf.size);
#endif
      if(get_buffer(ev->reqbuf.size, &bufinfo) == -1) {
	bufinfo.shmid = -1;
      }
      
      s_ev.type = MsgEventQGntBuf;
      s_ev.gntbuf.shmid = bufinfo.shmid;
      s_ev.gntbuf.size = bufinfo.size;
      MsgSendEvent(q, ev->reqbuf.client, &s_ev, 0);
    }
    break;
  case MsgEventQDestroyBuf:
    {
      //DNOTE("_got destroy buffer shmid %d\n", ev->destroybuf.shmid);
      remove_q_shmid(ev->destroybuf.shmid);
    }
    break;
  case MsgEventQDestroyQ:
    {
      //DNOTE("_got destroy Q shmid %d\n", ev->detachq.q_shmid);
      remove_q_shmid(ev->detachq.q_shmid);
    }
    break;
  case MsgEventQReqStreamBuf:
    {
      int shmid;
      cap_state_t state = 0;
      
      DNOTE("_new stream %x, %x\n",
	    ev->reqstreambuf.stream_id,
	    ev->reqstreambuf.subtype);
      
      if(register_stream(ev->reqstreambuf.stream_id,
			 ev->reqstreambuf.subtype)) {
	// this stream is wanted
	
	
	// check if we have a decoder
	
	//TODO check which decoder handles the stream and start it
	//if there isn't already one running that is free to use
	// TODO clean up logic/functions
	

	capability = streamid_to_capability(ev->reqstreambuf.stream_id,
					    ev->reqstreambuf.subtype);
	// check if there is a decoder running or started
	if(!search_capabilities(capability, &rcpt, NULL, NULL)) {
	  
	  decodername = streamid_to_decoderstr(ev->reqstreambuf.stream_id,
					       ev->reqstreambuf.subtype);
	  
	  if((capability & VIDEO_OUTPUT) || (capability & DECODE_DVD_SPU)) {
	    DNOTE("%s", "registered VO or SPU started\n");
	  }
	  if(capability == DECODE_DVD_SPU) {
	    register_capabilities(0,
				  DECODE_DVD_SPU | VIDEO_OUTPUT,
				  CAP_started);
	  } else {	    
	    register_capabilities(0,
				  capability,
				  CAP_started);
	  }
 // DNOTE("starting decoder %d %s\n", capability, decodername);
	  init_decoder(msgqid_str, decodername);
 // DNOTE("started decoder %d\n", capability);
	}
	
	while(!search_capabilities(capability, &rcpt, NULL, &state) ||
	      (state != CAP_running)) {
	  if(child_killed) {
	    cleanup();
	  }
	  if(MsgNextEventInterruptible(q, &r_ev) == -1) {
	    switch(errno) {
	    case EINTR:
	      continue;
	      break;
	    }
	  }
	  handle_events(q, &r_ev);
	}
	
	// we now have a decoder running ready to decode the stream
	
	// send ctrl_data shm: let client know where the timebase
	// data is
#if DEBUG
	DNOTE("sending ctrldata\n");
#endif
	s_ev.type = MsgEventQCtrlData;
	s_ev.ctrldata.shmid = ctrl_data_shmid;
	
	MsgSendEvent(q, rcpt, &s_ev, 0);
	
	// at this point we know both reader and writer client
	// for the buffer.
	
	// lets create the buffer
	
	shmid = create_q(ev->reqstreambuf.nr_of_elems,
			 ev->reqstreambuf.data_buf_shmid,
			 ev->reqstreambuf.client,
			 rcpt);
	
	
	// let the writer know which streambuffer to connect to
	s_ev.type = MsgEventQGntStreamBuf;
	s_ev.gntstreambuf.q_shmid = shmid;
	s_ev.gntstreambuf.stream_id =
	  ev->reqstreambuf.stream_id; 
	s_ev.gntstreambuf.subtype =
	  ev->reqstreambuf.subtype; 
	
	MsgSendEvent(q, ev->reqstreambuf.client, &s_ev, 0);
	
	// connect the streambuf  to the reader
	
	s_ev.type = MsgEventQDecodeStreamBuf;
	s_ev.decodestreambuf.q_shmid = shmid;
	s_ev.decodestreambuf.stream_id =
	  ev->reqstreambuf.stream_id;
	s_ev.decodestreambuf.subtype = 
	  ev->reqstreambuf.subtype;
	
	MsgSendEvent(q, rcpt, &s_ev, 0);
	
      } else {
	// we don't want this stream
	// respond with the shmid set to -1
	
	s_ev.type = MsgEventQGntStreamBuf;
	s_ev.gntstreambuf.q_shmid = -1;
	s_ev.gntstreambuf.stream_id =
	  ev->reqstreambuf.stream_id; 
	s_ev.gntstreambuf.subtype =
	  ev->reqstreambuf.subtype; 
	
	MsgSendEvent(q, ev->reqstreambuf.client, &s_ev, 0);
      }
    }
    break;
  case MsgEventQReqPicBuf:
    {
      int shmid;
      cap_state_t state;

#if DEBUG
      DNOTE("_new pic q\n");
#endif
      // check if we have a decoder
      
      //TODO check which decoder handles the stream and start it
      //if there isn't already one running that is free to use
      // TODO clean up logic/functions
      
      //TODO hmm start here or have decoder request cap first or
      //
      if(!search_capabilities(VIDEO_OUTPUT, &rcpt, NULL, NULL)) {
	
	DNOTE("%s", "registered VO|SPU started\n");
	register_capabilities(0,
			      VIDEO_OUTPUT | DECODE_DVD_SPU,
			      CAP_started);
	
	init_decoder(msgqid_str, getenv("DVDP_VIDEO_OUT"));
	//DNOTE("started video_out\n");
      }
      while(!search_capabilities(VIDEO_OUTPUT, &rcpt, NULL, &state) ||
	    (state != CAP_running)) {
	if(child_killed) {
	  cleanup();
	}
	if(MsgNextEventInterruptible(q, &r_ev) == -1) {
	  switch(errno) {
	  case EINTR:
	    continue;
	    break;
	  }
	}
	handle_events(q, &r_ev);
      }
      DNOTE("%s", "got capability video_out\n");
      
      // we now have a decoder running ready to decode the stream
      
      // send ctrl_data shm: let client know where the timebase
      // data is
#if DEBUG
      DNOTE("sending ctrldata\n");
#endif
      s_ev.type = MsgEventQCtrlData;
      s_ev.ctrldata.shmid = ctrl_data_shmid;
	
      MsgSendEvent(q, rcpt, &s_ev, 0);
	
      // at this point we know both reader and writer client
      // for the buffer.
      
      // lets create the buffer
	
      shmid = create_q(ev->reqpicbuf.nr_of_elems,
		       ev->reqpicbuf.data_buf_shmid,
		       ev->reqpicbuf.client,
		       rcpt);
      
      
      // let the writer know which picbuffer to connect to
      s_ev.type = MsgEventQGntPicBuf;
      s_ev.gntpicbuf.q_shmid = shmid;
      
#if DEBUG
      DNOTE("create_q, q_shmid: %d picture_buf_shmid: %d\n",
	    shmid,  ev->reqpicbuf.data_buf_shmid);
#endif      
      MsgSendEvent(q, ev->reqpicbuf.client, &s_ev, 0);
      
      // connect the picbuf  to the reader
      
      s_ev.type = MsgEventQAttachQ;
      s_ev.attachq.q_shmid = shmid;
      
      MsgSendEvent(q, rcpt, &s_ev, 0);
	
    }
    break;
  case MsgEventQSpeed:
    {
      clocktime_t rt;
      
      clocktime_get(&rt);
#if DEBUG
      DNOTE("_MsgEventQSpeed\n");
      DNOTE("speed: %.2f\n", ev->speed.speed);
#endif
      ctrl_data->speed = ev->speed.speed;
      
      

      // send speed event to syncmasters
      {
	// TODO get decoders that do sync...
	//
	if(search_capabilities(DECODE_AC3_AUDIO, &rcpt, NULL, NULL)) {
	  
	  MsgSendEvent(q, rcpt, ev, 0);
	  
	}

	if(search_capabilities(VIDEO_OUTPUT, &rcpt, NULL, NULL)) {
	  
	  MsgSendEvent(q, rcpt, ev, 0);
	  
	}
      }
    }
    break;
  default:
    WARNING("handle_events: notice, msgtype %d not handled\n",
	    ev->type);
    break;
  }
}

int main(int argc, char *argv[])
{
  struct sigaction sig;
  int c;
  MsgEventQ_t q;
  MsgEvent_t ev;
  
  for(c = strlen(argv[0])-1; c > 0; c--) {
    if(argv[0][c] == '/') {
      c++;
      break;
    }
  }
  program_name = &argv[0][c];

  GET_DLEVEL();

  memset(&sig, 0, sizeof(struct sigaction));
  sig.sa_handler = int_handler;
  sig.sa_flags = 0;
  if(sigaction(SIGINT, &sig, NULL) == -1) {
    perror("ctrl: failed to set sigaction SIGINT handler");
  }

#if defined(SA_SIGINFO)
  sig.sa_sigaction = sigchld_handler;
  sig.sa_flags = SA_SIGINFO;
#else
  sig.sa_handler = (sig_t)sigchld_handler;
#endif
  if(sigaction(SIGCHLD, &sig, NULL) == -1) {
    perror("ctrl: failed to set sigaction SIGCHL handler");
  }
  
  
  //"na:v:s:m:f:r:o:p:d:u:t:h?"
  while((c = getopt(argc, argv,"u:h?" )) != EOF) {
    switch(c) {
#if 0
    case 'a':
      ac3_audio_stream = atoi(optarg);
      break;
    case 't':
      dts_audio_stream = atoi(optarg);
      break;
    case 'm':
      mpeg_audio_stream = atoi(optarg);
      break;
    case 'p':
      lpcm_audio_stream = atoi(optarg);
      break;
    case 'v':
      mpeg_video_stream = atoi(optarg);
      break;
    case 's':
      subpicture_stream = atoi(optarg);
      break;
    case 'n':
      nav_stream = 1;
      break;
    case 'f':
      framerate = optarg;
      break;
    case 'r':
      output_bufs = optarg;
      break;
    case 'o':
      file_offset = optarg;
      break;
    case 'd':
      videodecode_debug = optarg;
      break;      
    case 'D':
      demux_debug = optarg;
      break;      
#endif
    case 'u':
      ui = optarg;
      break;
    case 'h':
    case '?':
      usage();
      return 1;
      break;
    }
  }
  
  if(argc - optind > 1){
    usage();
    return 1;
  }
  
  if(argc - optind == 1){
    input_file = argv[optind];
  } else {
    input_file = NULL;
  }
  
  /* Print the version info so that we get it with bug reports. */
  NOTE("%s %s\n", PACKAGE, VERSION);
  
  ctrl_data_shmid = create_ctrl_data();
  
  /* create msgq */  
  create_msgq();
  sprintf(msgqid_str, "%d", msgqid);
  
#if DEBUG
  DNOTE("msgid: %d\n", msgqid);  
  {
    struct msqid_ds msgqinfo;
    
    msgctl(msgqid, IPC_STAT, &msgqinfo);
    
    DNOTE("max_bytes: %ld\n", (long)msgqinfo.msg_qbytes);
  }
#endif

  q.msqid = msgqid;
  q.mtype = CLIENT_RESOURCE_MANAGER;

  if(ui != NULL) {
    MsgEventClient_t ui_client;
    if(!strcmp("cli", ui)) {
      request_capability(&q, UI_DVD_CLI, &ui_client, NULL);
    } else if(!strcmp("gui", ui)) {
      request_capability(&q, UI_DVD_GUI, &ui_client, NULL);
    } else {
      FATAL("%s", "no ui specified\n");
      cleanup_and_exit();
    }
  }
  
  /* If any streams are specified on the commadline, dexmux only those */
  /*
  if((ac3_audio_stream & mpeg_audio_stream & lpcm_audio_stream &
      dts_audio_stream & mpeg_video_stream & subpicture_stream &
      nav_stream) == -1) {
    ev.demuxdefault.state = 1;
  } else {
    ev.demuxdefault.state = 0;
  }
  
  MsgSendEvent(&q, rcpt, &ev, 0);
  */
  while(1){
    if(child_killed) {
      cleanup();
    }
    if(MsgNextEventInterruptible(&q, &ev) == -1) {
      switch(errno) {
      case EINTR:
	continue;
	break;
      }
    }
    handle_events(&q, &ev);
  }
  
  
  return 0;
}


int init_decoder(char *msgqid_str, char *decoderstr)
{
  pid_t pid;
  char *eargv[16];
  char *decode_name;
  char *decode_path = decoderstr;
  int n;
  
  if(decode_path == NULL) {
    ERROR("%s", "init_decoder(): decoder not set\n");
    return -1;
  }
  
  if((decode_name = strrchr(decode_path, '/')+1) == NULL) {
    decode_name = decode_path;
  }
  if(decode_name > &decode_path[strlen(decode_path)]) {
    ERROR("%s", "init_decoder(): illegal file name?\n");
    return -1;
  }
  
  //DNOTE("init_decoder(): %s\n", decode_name);

  //starting_decoder = 1;

  /* fork/exec decoder */
  
  switch(pid = fork()) {
  case 0:
    /* child process */
    n = 0;
    eargv[n++] = decode_name;
    eargv[n++] = "-m";
    eargv[n++] = msgqid_str;

    /* TODO fix for different decoders 
    if(output_bufs != NULL) {
      eargv[n++] = "-r";
      eargv[n++] = output_bufs;
    }
    if(framerate != NULL) {
      eargv[n++] = "-f";
      eargv[n++] = framerate;
    }

    if(videodecode_debug != NULL) {
      eargv[n++] = "-d";
      eargv[n++] = videodecode_debug;
    }
    */
    // FIXME Very high Hack value
    if(!strcmp(decoderstr, getenv("DVDP_CLI_UI")) ||
       !strcmp(decoderstr, getenv("DVDP_UI"))) {
      eargv[n++] = input_file;
    }     
    
    eargv[n++] = NULL;
    
    if(execv(decode_path, eargv) == -1) {
      FATAL("init_decoder(): path: %s\n", decode_path);
      perror("execv");
    }
    exit(1);
    break;
  case -1:
    /* fork failed */
    perror("fork");
    break;
  default:
    /* parent process */
    break;
  }
  if(pid != -1) {
    add_to_pidlist(pid, decoderstr);
  }
  

  DNOTE("Started %s with pid %ld\n", decoderstr, (long)pid);
  //  starting_decoder = 0;
  return pid;
}


typedef struct {
  pid_t pid;
  char *name;
} pidname_t;

pidname_t *pidlist = NULL;
int num_pids = 0;

void add_to_pidlist(pid_t pid, char *name)
{
  int n;

  for(n = 0; n < num_pids; n++) {
    if(pidlist[n].pid == -1) {
      pidlist[n].pid = pid;
      pidlist[n].name = strdup(name);
      return;
    }
  }

  num_pids++;
  pidlist = realloc(pidlist, num_pids*sizeof(pidname_t));
  pidlist[num_pids-1].pid = pid;
  pidlist[num_pids-1].name = strdup(name);
}

int remove_from_pidlist(pid_t pid)
{
  int n;
  for(n = 0; n < num_pids; n++) {
    if(pidlist[n].pid == pid) {
      pidlist[n].pid = -1;
      free(pidlist[n].name);
      pidlist[n].name = NULL;
      return 1;
    }
  }
  
  return 0;
}

char *get_pid_name(pid_t pid)
{
  int n;
  if(pid != -1) {
    for(n = 0; n < num_pids; n++) {
      if(pid == pidlist[n].pid) {
	return pidlist[n].name;
      }
    }
  }
  return NULL;
}
  

/**
 * @todo fix how to decide which streams to decode
 *
 * -u <gui name>     starts with gui name
 * filename  which file to play
 * -v #n     decode video stream #n
 * -a #n     decode ac3 audio stream #n
 * -m #n     decode mpeg audio stream #n
 * -d #n     decode dts audio stream #n
 * -p #n     decode pcm audio stream #n
 * -n        decode mpeg private stream 2 (dvd nav data)
 * -s #n     decode dvd subpicture stream #n
 * -dvd      start with dvdgui and vm
 * 
 */
int register_stream(uint8_t stream_id, uint8_t subtype)
{
  int state;
  
  /* If any stream is specified on the commadline, dexmux only those */
  if((ac3_audio_stream & mpeg_audio_stream & lpcm_audio_stream &
      dts_audio_stream & mpeg_video_stream & subpicture_stream &
      nav_stream) == -1)
    state = 0;
  else
    state = 1;
  

  if(stream_id == MPEG2_PRIVATE_STREAM_1) {
    
    if(state) {

      if((subtype == (0x80 | (ac3_audio_stream & 0x7))) &&
	 (ac3_audio_stream >= 0) && (ac3_audio_stream < 8)) {
	return 1;
      }
      
    } else {
      
      /* dvd, it's an ac3 stream ok */
      if((subtype >= 0x80) && (subtype < 0x88)) {
	return 1;
      }
      
    }

    if(state) {

      if((subtype == (0x88 | (dts_audio_stream & 0x7))) &&
	 (dts_audio_stream >= 0) && (dts_audio_stream < 8)) {
	return 1;
      }
      
    } else {
      
      /* dvd, it's an dts stream ok */
      if((subtype >= 0x88) && (subtype < 0x90)) {
	return 1;
      }
      
    }

    if(state) {

      if((subtype == (0xA0 | (lpcm_audio_stream & 0x7))) &&
	 (lpcm_audio_stream >= 0) && (lpcm_audio_stream < 8)) {
	return 1;
      }
      
    } else {
      
      /* dvd, it's an lpcm stream ok */
      if((subtype >= 0xA0) && (subtype < 0xA8)) {
	return 1;
      }
      
    }

    if(state) {

      if((subtype == (0x20 | (subpicture_stream & 0x1f))) &&
	 (subpicture_stream >= 0)) {
	return 1;
      }

    } else {

      /* dvd, it's a spu stream ok */
      if(((subtype & 0xf0) == 0x20) || ((subtype & 0xf0) == 0x90)) {
	return 1;
      }
      
    }

  }
  
  if(state) {

    if((stream_id == (0xc0 | (mpeg_audio_stream & 0x1f))) &&
       (mpeg_audio_stream >= 0)) { 
      return 1;
    }
    
  } else {
    
    /* dvd, mpeg audio stream ok */
    if(((stream_id & 0xf0) == 0xc0) || ((stream_id & 0xf0) == 0xd0)) {
      return 1;
    }
    
  }

  if(state) {

    if((stream_id == (0xe0 | (mpeg_video_stream & 0x0f))) &&
       (mpeg_video_stream >= 0)) {
      return 1;
    }
    
  } else {
    
    /* dvd, video stream ok */
    if((stream_id & 0xf0) == 0xe0) {
      return 1;
    }
    
  }

  if(state) {

    if((stream_id == MPEG2_PRIVATE_STREAM_2) && (nav_stream >= 0)) { // nav
      return 1;
    }

  } else {

    /* dvd, nav stream ok */
    if(stream_id == MPEG2_PRIVATE_STREAM_2) { // nav packs
      return 1;
    }
    
  }

  return 0;
  
}


int create_msgq(void)
{
  if((msgqid = msgget(IPC_PRIVATE, IPC_CREAT | 0600)) == -1) {
    perror("*ctrl: msgget ipc_creat failed");
    exit(1);
  }

  return 0;
}  


void destroy_msgq(void)
{
  if(msgctl(msgqid, IPC_RMID, NULL) != 0) {
    perror("*ctrl: msgctl ipc_rmid failed");
  }
  msgqid = -1;
}


int get_buffer(int size, shm_bufinfo_t *bufinfo)
{
  int shmid;
  /* This also creates the image buffers that will be sent to attached
   * from the X server if we use Xv.  So we need to make sure that it
   * has permissions to attach / read it.  */
  if((shmid = shmget(IPC_PRIVATE,
		     size,
		     IPC_CREAT | 0644)) == -1) {
    perror("*ctrl: get_buffer, shmget failed");
    return -1;
  }

  add_q_shmid(shmid);

  bufinfo->shmid = shmid;
  bufinfo->size = size;
  
  return 0;
}

  
int create_q(int nr_of_elems, int buf_shmid,
	     MsgEventClient_t writer,  MsgEventClient_t reader)
{
  
  int shmid;
  char *shmaddr;
  q_head_t *q_head;
  q_elem_t *q_elems;
  int n;

  if((shmid = shmget(IPC_PRIVATE,
		     sizeof(q_head_t) + nr_of_elems*sizeof(q_elem_t),
		     IPC_CREAT | 0600)) == -1) {
    perror("*ctrl: create_q, shmget failed");
    return -1;
  }
  
  
  if((shmaddr = shmat(shmid, NULL, SHM_SHARE_MMU)) == (void *)-1) {
    perror("*ctrl: create_q, shmat failed");
    
    if(shmctl(shmid, IPC_RMID, NULL) != 0) {
      perror("*ctrl: create_q, shmctl ipc_rmid faild");
    }
    return -1;
  }

  add_q_shmid(shmid);
  
  q_head = (q_head_t *)shmaddr;
  

  q_head->data_buf_shmid = buf_shmid;
  q_head->nr_of_qelems = nr_of_elems;
  q_head->write_nr = 0;
  q_head->read_nr = 0;
  q_head->writer = writer;
  q_head->reader = reader;
  q_head->writer_requests_notification = 0;
  q_head->reader_requests_notification = 0;
  q_head->qid = shmid; /* to get a unique id for the q */

  q_elems = (q_elem_t *)(shmaddr+sizeof(q_head_t));
  
  for(n = 0; n < nr_of_elems; n++) {
    q_elems[n].in_use = 0;
  }
  if(shmdt(shmaddr) != 0) {
    perror("*ctrl: create_q, shmdt failed");
  }
  return shmid;
}




int create_ctrl_data(void)
{
  int shmid;
  char *shmaddr;
  ctrl_time_t *ctrl_time;
  int n;
  int nr_of_offsets = 32;
  
  if((shmid = shmget(IPC_PRIVATE,
		     sizeof(ctrl_data_t)+
		     nr_of_offsets*sizeof(ctrl_time_t),
		     IPC_CREAT | 0600)) == -1) {
    perror("*ctrl: create_ctrl_data, shmget failed");
    return -1;
  }
  
  if((shmaddr = shmat(shmid, NULL, SHM_SHARE_MMU)) == (void *)-1) {
    perror("*ctrl: create_ctrl_data, shmat failed");
    
    if(shmctl(shmid, IPC_RMID, NULL) == -1) {
      perror("*ctrl: create_ctrl_data, shmctl ipc_rmid");
    }
    
    return -1;
  }
  
  add_q_shmid(shmid);
  
  ctrl_data = (ctrl_data_t *)shmaddr;
  ctrl_data->mode = MODE_STOP;
  ctrl_data->sync_master = SYNC_NONE;
  ctrl_data->speed = 1.0;
  ctrl_time = (ctrl_time_t *)(shmaddr+sizeof(ctrl_data_t));
  
  for(n = 0; n < nr_of_offsets; n++) {
    ctrl_time[n].offset_valid = OFFSET_NOT_VALID;
  }
  
  return shmid;
}


int *shm_ids = NULL;
int nr_shmids = 0;

void add_q_shmid(int shmid)
{
  nr_shmids++;
  
  if((shm_ids = (int *)realloc(shm_ids, sizeof(int)*nr_shmids)) == NULL) {
    perror("*ctrl: add_q_shmid, realloc failed");
    nr_shmids--;
    return;
  }

  shm_ids[nr_shmids-1] = shmid;
  
}


void remove_q_shmid(int shmid)
{
  int n;
  
  for(n = 0; n < nr_shmids; n++) {
    if(shm_ids[n] == shmid) {
      DNOTE("removing shmid: %d\n", shm_ids[n]);
      if(shmctl(shm_ids[n], IPC_RMID, NULL) == -1) {
	perror("ipc_rmid");
      }
      shm_ids[n] = -1;
    }
  }
}


void remove_q_shm(void)
{
  int n;

  for(n = 0; n < nr_shmids; n++) {
    if(shm_ids[n] != -1) {
      DNOTE("removing shmid: %d\n", shm_ids[n]);
      if(shmctl(shm_ids[n], IPC_RMID, NULL) == -1) {
	perror("ctrl: ipc_rmid");
      }
    }
  }
  nr_shmids = 0;
  free(shm_ids);
  shm_ids = NULL;
  
}

void cleanup_and_exit(void)
{
  remove_q_shm();
  destroy_msgq();
  NOTE("%s", "exiting\n");
  exit(0);
}

void int_handler(int sig)
{
  /* send quit msg to demuxer and decoders
   * (and wait for ack? (timeout))
   * 
   * remove all shared memory segments
   *
   * remove all msqqueues
   *
   * exit
   */
  int n;
  pid_t pid;
  
  DNOTE("Caught signal %d, cleaning up\n", sig);

  for(n = 0; n < num_pids; n++) {
    if((pid = pidlist[n].pid) != -1) {
      child_killed = 1;
      DNOTE("killing child: %ld %s\n", (long)pid, pidlist[n].name);
      kill(pid, SIGINT);
    }
    if(!child_killed) {
      DNOTE("%s", "All children dead\n");
      child_killed = 1;
    }
  }
}


/* FreeBSD compatibility
   where are these defined on freebsd if at all? */
#ifndef CLD_EXITED
/* from /usr/include/sys/siginfo.h on solaris */
#define CLD_EXITED      1       /* child has exited */
#define CLD_KILLED      2       /* child was killed */
#define CLD_DUMPED      3       /* child has coredumped */
#define CLD_TRAPPED     4       /* traced child has stopped */
#define CLD_STOPPED     5       /* child has stopped on signal */
#define CLD_CONTINUED   6       /* stopped child has continued */
#endif
#ifndef WCONTINUED 
#define WCONTINUED 0
#define WIFCONTINUED(x) 0
#endif


void sigchld_handler(int sig, siginfo_t *info, void* context)
{
  /* 
   *
   */
  int stat_loc;
  int died = 0;
  pid_t wpid, pid;
  char *name;
  
#if defined(SA_SIGINFO)
  if(info->si_signo != SIGCHLD) {
    ERROR("sigchldhandler got signal %d\n", info->si_signo);
    return;
  }
  if(info->si_errno) {
    DNOTE("error: %s\n", strerror(info->si_errno));
  }
  //DNOTE("si_code: %d\n", info->si_code);

  //DNOTE("si_pid: %ld\n", (long)info->si_pid);

  if((name = get_pid_name(info->si_pid)) == NULL) {
    name = "?";
  }
  
  switch(info->si_code) {
  case CLD_STOPPED:
    DNOTE("child: %ld stopped (%s)\n", (long)info->si_pid, name);
    break;
  case CLD_CONTINUED:
    DNOTE("child: %ld continued (%s)\n", (long)info->si_pid, name);
    break;
  case CLD_KILLED:
    DNOTE("child: %ld killed (%s)\n", (long)info->si_pid, name);
    died = 1;
    break;
  case CLD_DUMPED:
    DNOTE("child: %ld dumped (%s)\n", (long)info->si_pid, name);
    died = 1;
    break;
  case CLD_TRAPPED:
    DNOTE("child: %ld trapped (%s)\n", (long)info->si_pid, name);
    died = 1;
    break;
  case CLD_EXITED:
    DNOTE("child: %ld exited with %d (%s)\n",
	    (long)info->si_pid, info->si_status, name);
    died = 1;
    break;
#if defined(SI_NOINFO)  // Solaris only
  case SI_NOINFO:
    DNOTE("pid %ld, sigchld: no info (%s)\n", (long)info->si_pid, name);
    break;
#endif
  default:
    DNOTE("pid %ld, unknown sigchld si_code: %d (%s)\n",
	    (long)info->si_pid, info->si_code, name);
    break;
  }
  wpid = info->si_pid;
#else /* defined(SA_SIGINFO) */
  wpid = -1;
#endif
  
  while(1) {
    if((pid = waitpid(wpid, &stat_loc, WCONTINUED | WUNTRACED)) == -1) {
      perror("ctrl: waitpid failed");
      switch(errno) {
      case EINTR:
	continue;
      default:
	return;
      }
    }
    break;
  }
  
  if((name = get_pid_name(pid)) == NULL) {
    name = "?";
  }

  if(WIFEXITED(stat_loc)) {
    died = 1;
    DNOTE("pid: %ld exited with status: %d (%s)\n",
	    (long)pid, WEXITSTATUS(stat_loc), name);
  } else if(WIFSIGNALED(stat_loc)) {
    died = 1;
    DNOTE("pid: %ld terminated on signal: %d (%s)\n",
	    (long)pid, WTERMSIG(stat_loc), name);
  } else if(WIFSTOPPED(stat_loc)) {
    DNOTE("pid: %ld stopped on signal: %d (%s)\n",
	    (long)pid, WSTOPSIG(stat_loc), name);
  } else if(WIFCONTINUED(stat_loc)) {
    DNOTE("pid: %ld continued (%s)\n", (long)pid, name);
  } else {
    DNOTE("pid: %ld (%s)\n", (long)pid, name);
  }
  if(died) {
    int n;
    if(!remove_from_pidlist(pid)) {
      DNOTE("%s", "pid died before registering\n");
    }
    for(n = 0; n < num_pids; n++) {
      if((pid = pidlist[n].pid) != -1) {
	child_killed = 1;
	//DNOTE("killing child: %ld %s\n", (long)pid, pidlist[n].name);
	kill(pid, SIGINT);
      }
    }
    if(!child_killed) {
      DNOTE("%s", "all children dead\n");
      child_killed = 2;
    }
  }
}


void slay_children(void)
{
  int n;
  pid_t pid;

  for(n = 0; n < num_pids; n++) {
    if((pid = pidlist[n].pid) != -1) {
      child_killed = 2;
      DNOTE("slaying child: %ld %s\n", (long)pid, pidlist[n].name);
      kill(pid, SIGKILL);
    }
  }
  if(!child_killed) {
    DNOTE("%s", "No children left in pidlist\n");
    child_killed = 2;
  }
}
