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
#include <unistd.h>

#include <signal.h>
#include <sys/shm.h>
#include <time.h>
#include <errno.h>

#ifdef __bsdi__
#include <sys/param.h>
#include <sys/sysctl.h>
#endif

#ifndef HAVE_CLOCK_GETTIME
#include <sys/time.h>
#endif

#include <X11/Xlib.h>

#include <ogle/msgevents.h>

#include "debug_print.h"
#include "common.h"
#include "video_types.h"
#include "timemath.h"
#include "sync.h"
#include "spu_mixer.h"
#include "video_output.h"
#include "xscreensaver-comm.h"

#include "screenshot.h"

#ifndef SHM_SHARE_MMU
#define SHM_SHARE_MMU 0
#endif

extern void display_init(int padded_width, int padded_height,
		  int picture_buffer_shmid,
		  char *picture_buffer_addr);
extern void display_reset(void);
extern void display(yuv_image_t *current_image);
extern void display_poll(yuv_image_t *current_image);
extern void display_exit(void);
extern void display_reset_screensaver(void);
extern void screenshot_mode(int mode);

int register_event_handler(int(*eh)(MsgEventQ_t *, MsgEvent_t *));
int event_handler(MsgEventQ_t *q, MsgEvent_t *ev);


char *program_name;
int dlevel;

int video_scr_nr;

int process_interrupted = 0;

Bool talk_to_xscreensaver;

static int end_of_wait;

static long clk_tck;
static long min_time_left;

AspectModeSrc_t aspect_mode = AspectModeSrcVM;
AspectModeSrc_t aspect_sender;

uint16_t aspect_new_frac_n;
uint16_t aspect_new_frac_d;

ZoomMode_t zoom_mode = ZoomModeResizeAllowed;

static int ctrl_data_shmid;
ctrl_data_t *ctrl_data;
ctrl_time_t *ctrl_time;

int msgqid = -1;
MsgEventQ_t *msgq;


static int flush_to_scrid = -1;
static int prev_scr_nr = 0;


static data_q_t *data_q_head;
data_q_t *cur_data_q;


static MsgEventClient_t gui_client = 0;

MsgEventClient_t input_client = 0;
InputMask_t input_mask = INPUT_MASK_None;

typedef struct _event_handler_t {
  int(*eh)(MsgEventQ_t *, MsgEvent_t *);
  struct _event_handler_t *next;
} event_handler_t;

static event_handler_t *eh_head = NULL;


int register_event_handler(int(*eh)(MsgEventQ_t *, MsgEvent_t *))
{
  event_handler_t *eh_ptr;
  
  eh_ptr = malloc(sizeof(event_handler_t));
  eh_ptr->next = eh_head;
  eh_ptr->eh = eh;
  eh_head = eh_ptr;
  
  return 0;
}

int event_handler(MsgEventQ_t *q, MsgEvent_t *ev)
{
  event_handler_t *eh_ptr;
  
  eh_ptr = eh_head;
  
  while(eh_ptr != NULL) {
    if(eh_ptr->eh(q, ev)) {
      return 1;
    }
    eh_ptr = eh_ptr->next;
  }
  DNOTE("event_handler: unhandled event: %d\n", ev->type);
  return 0;
}


void wait_for_q_attach(void)
{
  MsgEvent_t ev;
  
  //DNOTE("waiting for attachq\n");
  
  while(ev.type != MsgEventQAttachQ) {
    if(MsgNextEventInterruptible(msgq, &ev) == -1) {
      switch(errno) {
      case EINTR:
	continue;
	break;
      default:
	FATAL("%s", "waiting for attachq\n");
	perror("MsgNextEvent");
	exit(1);
	break;
      }
    }
    event_handler(msgq, &ev);
  }
  //DNOTE("got attachq\n");
}

static int attach_ctrl_shm(int shmid)
{
  char *shmaddr;
  
  if(ctrl_data_shmid) {
    return 0;
  }

  if(shmid >= 0) {
    if((shmaddr = shmat(shmid, NULL, SHM_SHARE_MMU)) == (void *)-1) {
      ERROR("%s", "attach_ctrl_data()\n");
      perror("shmat");
      return -1;
    }
    
    ctrl_data_shmid = shmid;
    ctrl_data = (ctrl_data_t*)shmaddr;
    ctrl_time = (ctrl_time_t *)(shmaddr+sizeof(ctrl_data_t));
  }    
  
  return 0;
}


static int attach_data_q(int q_shmid, data_q_t *data_q)
{
  int n;
  int data_shmid;
  char *q_shmaddr;
  char *data_shmaddr;

  q_head_t *q_head;
  q_elem_t *q_elems;
  data_buf_head_t *data_head;
  picture_data_elem_t *data_elems;
  yuv_image_t *image_bufs;
#ifdef HAVE_XV
  yuv_image_t *reserv_image;
#endif
  
  //DNOTE("attach_data_q: q_shmid: %d\n", q_shmid);
  
  if(q_shmid < 0) {
    ERROR("%s", "attach_data_q(), q_shmid < 0\n");
    return -1;
  }
  if((q_shmaddr = shmat(q_shmid, NULL, SHM_SHARE_MMU)) == (void *)-1) {
    ERROR("%s", "attach_data_q()\n");
    perror("shmat");
    return -1;
  }

  q_head = (q_head_t *)q_shmaddr;
  q_elems = (q_elem_t *)(q_shmaddr+sizeof(q_head_t));

  data_shmid = q_head->data_buf_shmid;

  //DNOTE("attach_data_q: data_shmid: %d\n", data_shmid);

  if(data_shmid < 0) {
    ERROR("%s", "attach_data_q(), data_shmid\n");
    return -1;
  }

  if((data_shmaddr = shmat(data_shmid, NULL, SHM_SHARE_MMU)) == (void *)-1) {
    ERROR("%s", "attach_data_q()");
    perror("shmat");
    return -1;
  }
    
  data_head = (data_buf_head_t *)data_shmaddr;
  
  data_elems = (picture_data_elem_t *)(data_shmaddr + 
				       sizeof(data_buf_head_t));
  
  //TODO ugly hack
#ifdef HAVE_XV
  image_bufs = 
    malloc((data_head->nr_of_dataelems+1) *
	   sizeof(yuv_image_t));
  for(n = 0; n < (data_head->nr_of_dataelems+1); n++) {
#else
  image_bufs = 
    malloc(data_head->nr_of_dataelems * sizeof(yuv_image_t));
  for(n = 0; n < data_head->nr_of_dataelems; n++) {
#endif
    image_bufs[n].y = data_shmaddr + data_elems[n].picture.y_offset;
    image_bufs[n].u = data_shmaddr + data_elems[n].picture.u_offset;
    image_bufs[n].v = data_shmaddr + data_elems[n].picture.v_offset;
    image_bufs[n].info = &data_elems[n];
  }
  //TODO ugly hack
#ifdef HAVE_XV
  reserv_image = &image_bufs[n-1];
#endif

  if(data_head->info.type != DataBufferType_Video) {
    ERROR("%s", "Didn't get type Video\n");
    return -1;
  }
  data_q->in_use = 0;
  data_q->eoq = 0;
  data_q->q_head = q_head;
  data_q->q_elems = q_elems;
  data_q->data_head = data_head;
  data_q->data_elems = data_elems;
  data_q->image_bufs = image_bufs;
#ifdef HAVE_XV
  data_q->reserv_image = reserv_image;
#endif
  data_q->next = NULL;

  return 0;
}


static int detach_data_q(int q_shmid, data_q_t **data_q_head)
{
  MsgEvent_t ev;
  MsgEventClient_t client;
  data_q_t **data_q_p;
  data_q_t *data_q_tmp;
  
  //DNOTE("detach_data_q q_shmid: %d\n", q_shmid);
  
  for(data_q_p=data_q_head;
      *data_q_p != NULL && (*data_q_p)->q_head->qid != q_shmid;
      data_q_p = &(*data_q_p)->next) {
  }

  if(*data_q_p == NULL) {
    ERROR("%s", "detach_data_q q_shmid not found\n");
    return -1;
  }

  client = (*data_q_p)->q_head->writer;

  if(shmdt((char *)(*data_q_p)->data_head) == -1) {
    ERROR("%s", "detach_data_q data_head");
    perror("shmdt");
  }
  
  if(shmdt((char *)(*data_q_p)->q_head) == -1) {
    ERROR("%s", "detach_data_q q_head");
    perror("shmdt");
  }

  //TODO ugly hack

  free((*data_q_p)->image_bufs);

  data_q_tmp = *data_q_p;
  *data_q_p = (*data_q_p)->next;
  free(data_q_tmp);


  ev.type = MsgEventQQDetached;
  ev.detachq.q_shmid = q_shmid;
  
  if(MsgSendEvent(msgq, client, &ev, 0) == -1) {
    DPRINTF(1, "vo: qdetached\n");
  }

  return 0;
}


static int append_picture_q(int q_shmid, data_q_t **data_q)
{
  data_q_t **data_q_p;

  for(data_q_p = data_q; *data_q_p != NULL; data_q_p =&(*data_q_p)->next);
    
  *data_q_p = malloc(sizeof(data_q_t));
  
  if(attach_data_q(q_shmid, *data_q_p) == -1) {
    free(*data_q_p);
    *data_q_p = NULL;
    return -1;
  }
  
  return 0;
}

static int attach_picture_buffer(int q_shmid)
{

  append_picture_q(q_shmid, &data_q_head);

  if(cur_data_q == NULL) {
    cur_data_q = data_q_head;
  }

  return 0;
}


static int detach_picture_q(int shmid)
{
  
  //DNOTE("detach_picture_q q_shmid: %d\n", shmid);
  
  detach_data_q(shmid, &data_q_head);

  display_reset();

  return 0;
}


static yuv_image_t *last_image_buf = NULL;
static int redraw_needed = 0;

void redraw_request(void)
{
  redraw_needed = 1;
}

void redraw_done(void)
{
  redraw_needed = 0;
}

static void redraw_screen(void)
{

  if(flush_to_scrid != -1) {
    if(ctrl_time[video_scr_nr].scr_id < flush_to_scrid) {
      redraw_done();
      return;
    } else {
      flush_to_scrid = -1;
    }
  }

  if(last_image_buf != NULL) {
    display(last_image_buf);    
  }
  redraw_done();
}


void screenshot_request(ScreenshotMode_t mode)
{
  screenshot_mode(mode);
  if(mode == 0) {
    if(last_image_buf) {
      if(last_image_buf->info->is_reference || (ctrl_data->speed < 1.0)) {
	redraw_request();
      }
    }
  } else {
    redraw_request();
  }
}

static int handle_events(MsgEventQ_t *q, MsgEvent_t *ev)
{
  MsgEvent_t s_ev;
  
  switch(ev->type) {
  case MsgEventQNotify:
    if((cur_data_q->q_head != NULL) &&
       (ev->notify.qid == cur_data_q->q_head->qid)) {
      ;
    } else {
      return 0;
    }
    break;
  case MsgEventQFlushData:
    DPRINTF(1, "vo: got flush\n");
    flush_to_scrid = ev->flushdata.to_scrid;
    flush_subpicture(flush_to_scrid);
    break;
  case MsgEventQAttachQ:
    attach_picture_buffer(ev->attachq.q_shmid);
    break;
  case MsgEventQAppendQ:
    append_picture_q(ev->attachq.q_shmid, &data_q_head);
    break;
  case MsgEventQDetachQ:
    detach_picture_q(ev->detachq.q_shmid);
    
    s_ev.type = MsgEventQQDetached;
    s_ev.detachq.q_shmid = ev->detachq.q_shmid;
    
    if(MsgSendEvent(msgq, ev->detachq.client, &s_ev, 0) == -1) {
      DPRINTF(1, "vo: qdetached\n");
    }
    
    wait_for_q_attach();
    
    break;
  case MsgEventQCtrlData:
    attach_ctrl_shm(ev->ctrldata.shmid);
    break;
  case MsgEventQGntCapability:
    if((ev->gntcapability.capability & UI_DVD_GUI) == UI_DVD_GUI)
      gui_client = ev->gntcapability.capclient;
    else
      ERROR("capabilities not requested (%d)\n", 
              ev->gntcapability.capability);
    break;
  case MsgEventQSetAspectModeSrc:
    aspect_mode = ev->setaspectmodesrc.mode_src;
    break;
  case MsgEventQSetSrcAspect:
    // hack, fix this to use an array of all aspectmodesrc's
    aspect_sender = ev->setsrcaspect.mode_src;
    aspect_new_frac_n = ev->setsrcaspect.aspect_frac_n;
    aspect_new_frac_d = ev->setsrcaspect.aspect_frac_d;
    break;
  case MsgEventQSetZoomMode:
    zoom_mode = ev->setzoommode.mode;
    redraw_request();
    break;
  case MsgEventQReqInput:
    input_mask = ev->reqinput.mask;
    input_client = ev->reqinput.client;
    break;
  case MsgEventQSpeed:
    if(ctrl_time[prev_scr_nr].sync_master <= SYNC_VIDEO) {
      set_speed(&ctrl_time[prev_scr_nr].sync_point, ev->speed.speed);
    }
    break;
  case MsgEventQSaveScreenshot:
    if(ev->savescreenshot.formatstr[0]) {
      screenshot_set_formatstr(ev->savescreenshot.formatstr);
    }
    screenshot_request(ev->savescreenshot.mode);
    break;
  default:
    //DNOTE("unrecognized event type: %d\n", ev->type);
    return 0;
    break;
  }
  return 1;
}

void wait_until_handler(int sig) 
{
  end_of_wait = 1;
  return;
}


void alarm_handler(int sig) 
{
  end_of_wait = 1;
  if(last_image_buf)
    display_poll(last_image_buf);
}

static clocktime_t wait_until(clocktime_t *scr, sync_point_t *sp)
{
  struct itimerval timer;
  clocktime_t time_left;
  clocktime_t real_time;
  struct sigaction act;
  struct sigaction oact;

  static clocktime_t last_ss_disable = {0,0};

  timer.it_interval.tv_sec = 0;
  timer.it_interval.tv_usec = 0;
  
  while(1) {
    
    end_of_wait = 0;
    
    clocktime_get(&real_time);

    if(TIME_S(real_time) - TIME_S(last_ss_disable) > 50) {
      clocktime_t prof_time;
      if(talk_to_xscreensaver) {
	nudge_xscreensaver();
	clocktime_get(&prof_time);
	timesub(&prof_time, &prof_time, &real_time);
	DNOTE("ss disable took %ld.%09ld s\n",
	      TIME_S(prof_time), TIME_SS(prof_time));
      }
      display_reset_screensaver();
      TIME_S(last_ss_disable) = TIME_S(real_time);
    }
    

    calc_realtime_left_to_scrtime(&time_left, &real_time,
				  scr, sp);
    /*
    fprintf(stderr, "rt: %ld.%09ld, scr: %ld.%09ld, left: %ld.%09ld\n",
	    real_time.tv_sec, real_time.tv_nsec,
	    scr->tv_sec, scr->tv_nsec,
	    time_left.tv_sec, time_left.tv_nsec);
    */

    if((TIME_S(time_left) > 0) || (TIME_SS(time_left) > (CT_FRACTION/10))) {
      // more then 100 ms left, lets wait and check x events every 100ms
      timer.it_value.tv_sec = 0;
      timer.it_value.tv_usec = 100000;

      act.sa_handler = alarm_handler;
      act.sa_flags = 0;

      sigaction(SIGALRM, &act, &oact);
      setitimer(ITIMER_REAL, &timer, NULL);

    } else if(TIME_SS(time_left) > min_time_left) {
      // less than 100ms but more than clktck/2 left, lets wait
      struct timespec sleeptime;
 
#if 0
      timer.it_value.tv_sec = 0;
      timer.it_value.tv_usec = TIME_SS(time_left)/(CT_FRACTION/1000000);
      
      act.sa_handler = wait_until_handler;
      act.sa_flags = 0;
      
      sigaction(SIGALRM, &act, &oact);
      setitimer(ITIMER_REAL, &timer, NULL);
#endif
      sleeptime.tv_sec = TIME_S(time_left);
      sleeptime.tv_nsec = TIME_SS(time_left)*(1000000000/CT_FRACTION);
      nanosleep(&sleeptime, NULL);
      clocktime_get(&real_time);
      calc_realtime_left_to_scrtime(&time_left, &real_time,
				    scr, sp);
      return time_left;
    } else {
      // less than clktck/2 left or negative time left, we cant sleep
      // a shorter period than clktck so we return

      return time_left;
    }
  
    while(!end_of_wait) {
      MsgEvent_t ev;
      
      // check any events that arrives 
      if(MsgNextEventInterruptible(msgq, &ev) == -1) {
	switch(errno) {
	case EINTR:
	  end_of_wait = 1;
	  break;
	default:
	  FATAL("%s", "waiting for notification");
	  perror("MsgNextEvent");
	  // might never have had dispay_init called
	  display_exit(); //clean up and exit
	  break;
	}
      } else {
	timer.it_value.tv_sec = 0; // disable timer
	timer.it_value.tv_usec = 0; // disable timer
	setitimer(ITIMER_REAL, &timer, NULL);

	event_handler(msgq, &ev);
	if(redraw_needed) {
	  redraw_screen();
	}
	break;
      }
    }
    timer.it_value.tv_sec = 0; // disable timer
    timer.it_value.tv_usec = 0; // disable timer
    setitimer(ITIMER_REAL, &timer, NULL);
    
    sigaction(SIGALRM, &oact, NULL);
  }
  
}



static int get_next_picture_q_elem_id(data_q_t *data_q)
{
  int elem;
  MsgEvent_t ev;
  struct itimerval timer;
  timer.it_interval.tv_sec = 0;
  timer.it_interval.tv_usec = 0;
  timer.it_value.tv_sec = 0;
  timer.it_value.tv_usec = 0;
  
  elem = data_q->q_head->read_nr;
  //DNOTE("get_next_picture_q_elem_id: elem: %d\n", elem);
  data_q->q_head->read_nr =
    (data_q->q_head->read_nr+1)%data_q->q_head->nr_of_qelems;

  if(!data_q->q_elems[elem].in_use) {
    data_q->q_head->reader_requests_notification = 1;
    //DNOTE("elem not in use, setting notification req\n");

    while(!data_q->q_elems[elem].in_use) {
      if(process_interrupted) {
	// might never have had dispay_init called
	display_exit();
      }
      timer.it_value.tv_usec = 100000; //0.1 sec
      setitimer(ITIMER_REAL, &timer, NULL);
      //DNOTE("waiting for notification\n");
      if(MsgNextEventInterruptible(msgq, &ev) == -1) {
	switch(errno) {
	case EINTR:
	  continue;
	  break;
	default:
	  FATAL("%s", "waiting for notification");
	  perror("MsgNextEvent");
	  // might never have had dispay_init called?
	  display_exit(); //clean up and exit
	  break;
	}
      }
      timer.it_value.tv_usec = 0; // disable timer
      setitimer(ITIMER_REAL, &timer, NULL);
      event_handler(msgq, &ev);
      if(redraw_needed) {
	redraw_screen();
      }
    }
  }

  if((data_q->q_elems[elem].data_elem_index) == -1) {
    // end of q
    data_q->eoq = 1;
    return -1;
  }
  data_q->in_use++;
  return elem;
}


static void release_picture(int q_elem_id, data_q_t *data_q)
{
  MsgEvent_t ev;
  int msg_sent = 0;
  int id;
  
  id = data_q->q_elems[q_elem_id].data_elem_index;
  /*
  DNOTE("release_picture: elem: %d, buf: %d\n",
	  q_elem_id, id);
  */
  data_q->data_elems[id].displayed = 1;
  data_q->q_elems[q_elem_id].in_use = 0;
  if(data_q->q_head->writer_requests_notification) {
    data_q->q_head->writer_requests_notification = 0;
    ev.type = MsgEventQNotify;
    do {
      if(process_interrupted) {
	display_exit();
      }
      if(MsgSendEvent(msgq, data_q->q_head->writer, &ev, IPC_NOWAIT) == -1) {
	MsgEvent_t c_ev;
	switch(errno) {
	case EAGAIN:
	  WARNING("%s", "msgq full, checking incoming messages and trying again\n");
	  while(MsgCheckEvent(msgq, &c_ev) != -1) {
	    event_handler(msgq, &c_ev);
	  }
	  break;
#ifdef EIDRM
	case EIDRM:
#endif
	case EINVAL:
	  FATAL("%s", "couldn't send notification no msgq\n");
	  display_exit(); //TODO clean up and exit
	  break;
	default:
	  FATAL("%s", "couldn't send notification\n");
	  display_exit(); //TODO clean up and exit
	  break;
	}
      } else {
	msg_sent = 1;
      }
    } while(!msg_sent);
  }

  data_q->in_use--;

}




data_q_t *get_next_data_q(data_q_t **head, data_q_t *cur_q)
{
  data_q_t **data_q;
  data_q_t *tmp_q;
  
  for(data_q = head;
      *data_q != NULL && *data_q != cur_q;
      data_q = &(*data_q)->next);
  
  if(*data_q == NULL) {
    ERROR("%s", "get_next_data_q(), couldn't find cur_q\n");
    return NULL;
  }

  tmp_q = (*data_q)->next; //pointer to the next q
  
  if(!(*data_q)->in_use) {
    display_reset();
    detach_data_q((*data_q)->q_head->qid, head);
  }

  if(tmp_q == NULL) {
    // no next q, lets wait for it
    DNOTE("%s", "get_next_data_q: no next q\n");
    wait_for_q_attach();
    if(*data_q != cur_q) {
      //cur has been detached
      tmp_q = *data_q;
    } else {
      tmp_q = (*data_q)->next;
    }
  }
  

  return tmp_q;
}

/* Erhum test... */
clocktime_t first_time;
int frame_nr = 0;

static void int_handler(int sig)
{
  process_interrupted = 1;
}

static void display_process()
{
  clocktime_t real_time, prefered_time, frame_interval;
  clocktime_t avg_time, oavg_time;

#ifndef HAVE_CLOCK_GETTIME
  clocktime_t waittmp;
#endif
  clocktime_t wait_time;
  struct sigaction sig;

  int q_elem_id = -1;
  int prev_q_elem_id = -1;
  
  int buf_id = -1;
  int prev_buf_id = -1;
  int old_q_id = -1;
  int drop = 0;
  int avg_nr = 23;
  picture_data_elem_t *pinfos;
  data_q_t *old_data_q = NULL;
  
  TIME_S(prefered_time) = 0;
  
  sig.sa_handler = int_handler;
  sig.sa_flags = 0;
  sigaction(SIGINT, &sig, NULL);

  sig.sa_handler = alarm_handler;
  sig.sa_flags = 0;
  sigaction(SIGALRM, &sig, NULL);

  talk_to_xscreensaver = look_for_good_xscreensaver();

  while(1) {
    //DNOTE("old_q_id: %d\n", old_q_id);
    old_data_q = cur_data_q;

    do {
      q_elem_id = get_next_picture_q_elem_id(cur_data_q);
      /*
      DNOTE("get_next_picture_q_elem_id: %d, buf:%d\n",
	      q_elem_id, cur_data_q->q_elems[q_elem_id].data_elem_index);
      */
      if(cur_data_q->eoq) {
	//end of q
	/*
	  last pic in the current q,
	  switch to new q, and detach the old
	  if there is no new q, wait for an attachq or appendq
	  get first pic in the new q
	*/
        if(!cur_data_q->in_use) {
          last_image_buf = NULL;
	  old_data_q = NULL;
        }
	
	cur_data_q = get_next_data_q(&data_q_head, cur_data_q);
      }

    } while(q_elem_id == -1);


    pinfos = cur_data_q->data_elems;
    buf_id = cur_data_q->q_elems[q_elem_id].data_elem_index;

    //DNOTE("last_image_buf: %d\n", last_image_buf);
    video_scr_nr = pinfos[buf_id].scr_nr;
    
    // Consume all messages for us and spu_mixer
    if(msgqid != -1) {
      MsgEvent_t ev;
      while(MsgCheckEvent(msgq, &ev) != -1) {
	event_handler(msgq, &ev);
      }
    }

    if(ctrl_time[pinfos[buf_id].scr_nr].sync_master <= SYNC_VIDEO) {
      ctrl_time[pinfos[buf_id].scr_nr].sync_master = SYNC_VIDEO;
      
      //TODO release scr_nr when done
      
      if(ctrl_time[pinfos[buf_id].scr_nr].offset_valid == OFFSET_NOT_VALID) {
	if(pinfos[buf_id].PTS_DTS_flags & 0x2) {

	  clocktime_t scr_time;
	  DNOTE("%s", "set_sync_point()\n");

	  PTS_TO_CLOCKTIME(scr_time, pinfos[buf_id].PTS);
	  clocktime_get(&real_time);

	  set_sync_point(&ctrl_time[pinfos[buf_id].scr_nr],
			 &real_time,
			 &scr_time,
			 ctrl_data->speed);
	}
      }
      /*
      if(pinfos[buf_id].PTS_DTS_flags & 0x2) {
	time_offset = get_time_base_offset(pinfos[buf_id].PTS,
					   ctrl_time, pinfos[buf_id].scr_nr);
      }
      */
      prev_scr_nr = pinfos[buf_id].scr_nr;
    }
    

    PTS_TO_CLOCKTIME(frame_interval, pinfos[buf_id].frame_interval);


    clocktime_get(&real_time);

    TIME_S(wait_time) = 0;
    TIME_SS(wait_time) = 0;

    if(flush_to_scrid != -1) {
      if(ctrl_time[video_scr_nr].scr_id >= flush_to_scrid) {
	flush_to_scrid = -1;
      }
    }
    
    if(flush_to_scrid == -1) {
      if(TIME_S(prefered_time) == 0 || TIME_SS(frame_interval) == 1) {
	prefered_time = real_time;
      } else if(ctrl_time[pinfos[buf_id].scr_nr].offset_valid == OFFSET_NOT_VALID) {
	prefered_time = real_time;
      } else /* if(TIME_S(pinfos[buf_id].pts_time) != -1) */ { 
	clocktime_t pts_time;
	PTS_TO_CLOCKTIME(pts_time, pinfos[buf_id].PTS);
	
	/*
	calc_realtime_from_scrtime(&prefered_time, &pts_time,
				   &ctrl_time[pinfos[buf_id].scr_nr].sync_point);
	*/

	wait_time =
	  wait_until(&pts_time, &ctrl_time[pinfos[buf_id].scr_nr].sync_point);

	
      }
    }
    /**
     * We have waited and now it's time to show a new picture
     */


    last_image_buf = &cur_data_q->image_bufs[buf_id];
    
    // release the old picture, if it's not done already
    if(prev_buf_id != -1) {
      if(old_q_id != cur_data_q->q_head->qid) {
	//DNOTE("old_q_id != current\n");
      }
      if((old_data_q != NULL) && (old_data_q != cur_data_q)) {
       //DNOTE("release old_q buf_id: %d\n", prev_buf_id);
	release_picture(prev_q_elem_id, old_data_q);
      } else {
	//DNOTE("release buf_id: %d\n", prev_buf_id);
	release_picture(prev_q_elem_id, cur_data_q);
      }
    } else if(prev_q_elem_id != -1) {
      //DNOTE("release2 buf_id: %d\n", prev_buf_id);
      release_picture(prev_q_elem_id, cur_data_q);
    }      
    //detach old q if any
    if(old_q_id != cur_data_q->q_head->qid) {
      if(old_q_id != -1) {
	detach_data_q(old_q_id, &data_q_head);
      }
      /*
      DNOTE("display_init: width: %d, height:%d\n",
	      cur_data_q->data_head->info.video.width,
	      cur_data_q->data_head->info.video.height);
      */
      display_init(cur_data_q->data_head->info.video.width,
		   cur_data_q->data_head->info.video.height,
		   cur_data_q->data_head->shmid,
		   (char *)cur_data_q->data_head);
    }
    
    prev_q_elem_id = q_elem_id;
    prev_buf_id = buf_id;
    old_q_id = cur_data_q->q_head->qid;
    /*
    fprintf(stderr, "*vo: waittime: %ld.%+010ld\n",
	    wait_time.tv_sec, wait_time.tv_nsec);
    */

    if(TIME_SS(wait_time) < -60*(CT_FRACTION/1000)) {
      // more than 60 ms late, drop decoded pictures
      drop = 1;
    }
    
    if(!drop) {
      frame_nr++;
      avg_nr++;
    }
    if(avg_nr == 200) {
      avg_nr = 0;
      oavg_time = avg_time;
      clocktime_get(&avg_time);
      
      fprintf(stderr, "display: frame rate: %.3f fps\n",
	      200.0/(((double)TIME_S(avg_time)+
		      (double)TIME_SS(avg_time)/CT_FRACTION)-
		     ((double)TIME_S(oavg_time)+
		      (double)TIME_SS(oavg_time)/CT_FRACTION))
	      );

    }
    /*
    clocktime_get(&real_time2);
    timesub(&diff, &prefered_time, &real_time2);
    
    fprintf(stderr, "diff: %d.%+010ld\n",
	    TIME_S(diff), TIME_SS(diff));
    */
    /*
    fprintf(stderr, "rt: %d.%09ld, pt: %d.%09ld, diff: %d.%+09ld\n",
	    TIME_S(real_time2), TIME_SS(real_time2),
	    TIME_S(prefered_time), TIME_SS(prefered_time),
	    TIME_S(diff), TIME_SS(diff));
    */

    if(!drop) {
      if(flush_to_scrid != -1) {
	if(ctrl_time[video_scr_nr].scr_id < flush_to_scrid) {
	  redraw_done();
	} else {
	  flush_to_scrid = -1;
	  display(&cur_data_q->image_bufs[buf_id]);
	  redraw_done();
	}
      } else {
	display(&cur_data_q->image_bufs[buf_id]);
	redraw_done();
      }
    } else {
      fprintf(stderr, "#");
      drop = 0;
    }

  }
}

void display_process_exit(void) {
  exit(0);
}


#ifdef __bsdi__

static int bsdi_getticks(void)
{
  int mib[2];
  size_t olen = sizeof (struct clockinfo);
  struct clockinfo ci;
  
  mib[0] = CTL_KERN;
  mib[1] = KERN_CLOCKRATE;
  if (sysctl(mib, 2, &ci, &olen, NULL, 0) < 0) {
    perror("sysctl kern.clockrate");
    return(100);
  }
  return ci.hz;
}
#endif


static void usage()
{
  fprintf(stderr, "Usage: %s  [-m <msgid>]\n", 
	  program_name);
}

int main(int argc, char **argv)
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
  
  errno = 0;

#ifdef __bsdi__
  clk_tck = bsdi_getticks();
#else
  clk_tck = sysconf(_SC_CLK_TCK);
#endif
  
  if(clk_tck <= 0) {
    // linux returns 0 as error also
    if(errno != 0 && clk_tck == -1) {
      perror("sysconf(_SC_CLK_TCK)");
    } else {
      fprintf(stderr, "sysconf(_SC_CLK_TCK), not supported\n");
    }
    //guessed default value
    clk_tck = 100;
  }

  min_time_left = (CT_FRACTION/clk_tck)/2;

  DNOTE("CLK_TCK: %ld\n", clk_tck);

  if(msgqid != -1) {
    
    if((msgq = MsgOpen(msgqid)) == NULL) {
      FATAL("%s", "couldn't get message q\n");
      exit(1);
    }
    
    register_event_handler(handle_events);
    init_spu();

    ev.type = MsgEventQRegister;
    ev.registercaps.capabilities = VIDEO_OUTPUT | DECODE_DVD_SPU;
    
    if(MsgSendEvent(msgq, CLIENT_RESOURCE_MANAGER, &ev, 0) == -1) {
      DPRINTF(1, "vo: register capabilities\n");
      exit(1); //TODO clean up and exit
    }
    
    //DNOTE("sent caps\n");
    
    ev.type = MsgEventQReqCapability;
    ev.reqcapability.capability = UI_DVD_GUI;
    if(MsgSendEvent(msgq, CLIENT_RESOURCE_MANAGER, &ev, 0) == -1) {
      FATAL("%s", "didn't get dvd_gui cap\n");
      exit(1); //TODO clean up and exit
    }
    
    //DNOTE("waiting for attachq\n");
    
    while(ev.type != MsgEventQAttachQ) {
      if(MsgNextEventInterruptible(msgq, &ev) == -1) {
	switch(errno) {
	case EINTR:
	  continue;
	  break;
	default:
	  FATAL("%s", "waiting for attachq");
	  perror("MsgNextEvent");
	  exit(1);
	  break;
	}
      }
      event_handler(msgq, &ev);
    }
    //DNOTE("got attachq\n");

    display_process();
    
  } else {
    fprintf(stderr, "what?\n");
  }
  
  exit(0);
}


