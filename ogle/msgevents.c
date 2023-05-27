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
#include <sys/ipc.h>
#include <sys/msg.h>
#include <errno.h>

#include "dvd.h"
#include "dvdevents.h"
#include "msgevents.h"

#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

#if (defined(BSD) && (BSD >= 199306))
#include <unistd.h>
#endif

//#define DEBUG
#ifdef DEBUG

static char *MsgEventType_str[] = {
  "MsgEventQNone",
  "Reserved",
  "MsgEventQInitReq",
  "MsgEventQInitGnt",  
  "MsgEventQRegister",
  "MsgEventQNotify",
  "MsgEventQReqCapability",
  "MsgEventQGntCapability",
  "MsgEventQPlayCtrl",
  "MsgEventQChangeFile",
  "MsgEventQReqStreamBuf", // 10
  "MsgEventQGntStreamBuf",
  "MsgEventQDecodeStreamBuf",
  "MsgEventQReqBuf",
  "MsgEventQGntBuf",
  "MsgEventQCtrlData",
  "MsgEventQReqPicBuf",
  "MsgEventQGntPicBuf",
  "MsgEventQAttachQ",
  "MsgEventQSPUPalette",
  "MsgEventQSPUHighlight", // 20
  "MsgEventQSpeed",
  "MsgEventQDVDCtrl",
  "MsgEventQFlow",
  "MsgEventQFlushData",
  "MsgEventQDemuxStream",
  "MsgEventQDemuxStreamChange",
  "MsgEventQDemuxDefault",
  "MsgEventQDVDCtrlLong",
  "MsgEventQDemuxDVD", // 29
  "MsgEventQDemuxDVDRoot",
  "MsgEventQSetAspectModeSrc",
  "MsgEventQSetSrcAspect",
  "MsgEventQSetZoom",
  "MsgEventQReqInput",
  "MsgEventQInputButtonPress",
  "MsgEventQInputButtonRelease",
  "MsgEventQInputPointerMotion",
  "MsgEventQInputKeyPress",
  "MsgEventQInputKeyRelease",
  "MsgEventQDestroyBuf",
  "MsgEventQAppendQ",
  "MsgEventQDetachQ",
  "MsgEventQQDetached",
  "MsgEventQDestroyQ",
  "MsgEventQDemuxStreamChange2",
  "MsgEventQSaveScreenshot",
  NULL
};

void PrintMsgEventType(MsgEventType_t type)
{
  fprintf(stderr, MsgEventType_str[type]);
}
#endif

#ifdef SOCKIPC
MsgEventQ_t *MsgOpen(MsgEventQType_t type, char *name, int namelen)
{
  MsgEventQ_t *ret = NULL;
  msg_t msg;
  MsgQInitReqEvent_t initreq;
  MsgQInitGntEvent_t initgnt;
  
  msg.mtype = CLIENT_RESOURCE_MANAGER; // the recipient of this message
  initreq.type = MsgEventQInitReq;   // we want a handle
  memcpy(msg.event_data, &initreq, sizeof(MsgQInitReqEvent_t));
  
  switch(type) {
  case MsgEventQType_msgq:
    {
      int msqid = atoi(name);
      
      if(msgsnd(msqid, (void *)&msg, sizeof(MsgQInitReqEvent_t), 0) == -1) {
	perror("MsgOpen, snd");
	return NULL;
	
      } else {
	if(msgrcv(msqid, (void *)&msg, sizeof(MsgEvent_t),
		  CLIENT_UNINITIALIZED, 0) == -1) {
	  perror("MsgOpen, rcv");
	  return NULL;
	} else {
	  ret = (MsgEventQ_t *)malloc(sizeof(MsgEventQ_t));
	  
	  ret->msg.type = MsgEventQType_msgq;
	  ret->msq.msqid = msqid;       // which msq to wait for messages on
	  memcpy(&initgnt, msg.event_data, sizeof(MsgQInitGntEvent_t));
	  ret->msg.mtype = initgnt.newclientid; // mtype to wait for
	  
	}
      }
    }
    break;
  case MsgEventQType_socket:
    {
      int sd;
      struct sockaddr_un unix_addr = { 0 };
      
      if((sd = socket(PF_UNIX, SOCK_DGRAM, 0)) == -1) {
	perror("socket");
	return NULL;
      }

      unix_addr.sun_family = AF_UNIX;
      
      if(strlen(name) >= sizeof(unix_addr.sun_path)) {
	return NULL;
      }      
      strcpy(unix_addr.sun_path, name);
      
      if(connect(sd, (struct sockaddr *)&unix_addr, sizeof(unix_addr)) == -1) {
	perror("connect");
	return NULL;
      }
      



      if(msgsnd(msqid, (void *)&msg, sizeof(MsgQInitReqEvent_t), 0) == -1) {
	perror("MsgOpen, snd");
	return NULL;
	
      } else {
	if(msgrcv(msqid, (void *)&msg, sizeof(MsgEvent_t),
		  CLIENT_UNINITIALIZED, 0) == -1) {
	  perror("MsgOpen, rcv");
	  return NULL;
	} else {
	  ret = (MsgEventQ_t *)malloc(sizeof(MsgEventQ_t));
	  
	  ret->msg.type = MsgEventQType_msgq;
	  ret->msq.msqid = msqid;       // which msq to wait for messages on
	  memcpy(&initgnt, msg.event_data, sizeof(MsgQInitGntEvent_t));
	  ret->msg.mtype = initgnt.newclientid; // mtype to wait for
	  
	}
      }
      
      
    }
    break;
  case MsgEventQType_pipe:
    break;
  }
  
  return ret;
  
}
#else
MsgEventQ_t *MsgOpen(int msqid)
{
  MsgEventQ_t *ret = NULL;
  msg_t msg;
  MsgQInitReqEvent_t initreq;
  MsgQInitGntEvent_t initgnt;
  
  msg.mtype = CLIENT_RESOURCE_MANAGER; // the recipient of this message
  initreq.type = MsgEventQInitReq;   // we want a handle
  memcpy(msg.event_data, &initreq, sizeof(MsgQInitReqEvent_t));
  

  if(msgsnd(msqid, (void *)&msg, sizeof(MsgQInitReqEvent_t), 0) == -1) {
    perror("MsgOpen, snd");
    return NULL;
    
  } else {
    if(msgrcv(msqid, (void *)&msg, sizeof(MsgEvent_t),
             CLIENT_UNINITIALIZED, 0) == -1) {
      perror("MsgOpen, rcv");
      return NULL;
    } else {
      ret = (MsgEventQ_t *)malloc(sizeof(MsgEventQ_t));
      
      ret->msqid = msqid;       // which msq to wait for messages on
      memcpy(&initgnt, msg.event_data, sizeof(MsgQInitGntEvent_t));
      ret->mtype = initgnt.newclientid; // mtype to wait for
    
    }
  }
  
  return ret;
  
}
#endif



/**
 * Close the message connection.
 * TODO: tell resource manager that there isn't anyone listening
 * for messages here any more. 
 */
void MsgClose(MsgEventQ_t *q)
{

  fprintf(stderr, "msg close FIX\n");
  
  
  // just in case someone tries to access this pointer after close
  q->msqid = 0;
  q->mtype = 0;
  
  free(q);
  
}

static int MsgNextEvent_internal(MsgEventQ_t *q, MsgEvent_t *event_return, int interruptible)
{
  msg_t msg;
  
  while(1) {
    if(msgrcv(q->msqid, (void *)&msg, sizeof(MsgEvent_t),
	      q->mtype, 0) == -1) {
      switch(errno) {
      case EINTR:  // syscall interrupted 
	if(!interruptible) {
	  continue;
	}
	break;
      default:
	perror("MsgNextEvent");
	break;
      }
      return -1;
    } else {
      
      memcpy(event_return, msg.event_data, sizeof(MsgEvent_t));
      return 0;
    }
  }    

}

int MsgNextEvent(MsgEventQ_t *q, MsgEvent_t *event_return) {
  return MsgNextEvent_internal(q, event_return, 0);
}

int MsgNextEventInterruptible(MsgEventQ_t *q, MsgEvent_t *event_return) {
  return MsgNextEvent_internal(q, event_return, 1);
}


#if (defined(BSD) && (BSD >= 199306))
int MsgNextEventNonBlocking(MsgEventQ_t *q, MsgEvent_t *event_return)
{
  msg_t msg;
  
  while (1) {
    if(msgrcv(q->msqid, (void *)&msg, sizeof(MsgEvent_t),
             q->mtype, IPC_NOWAIT) == -1) {
      switch(errno) {
#ifdef ENOMSG
      case ENOMSG:
#endif
      case EAGAIN:
      case EINTR:     // interrupted by system call/signal, try again
       usleep(10000);
       continue;
       break;
      default:
       perror("MsgNextEvent");
       break;
      }
      return -1;
    } else {
      
      memcpy(event_return, msg.event_data, sizeof(MsgEvent_t));
      return 0;
    }
  }

}
#endif

int MsgCheckEvent(MsgEventQ_t *q, MsgEvent_t *event_return)
{
  msg_t msg;
  
  while (1) {
    if(msgrcv(q->msqid, (void *)&msg, sizeof(MsgEvent_t),
	      q->mtype, IPC_NOWAIT) == -1) {
      switch(errno) {
#ifdef ENOMSG
      case ENOMSG:
#endif
      case EAGAIN:
	break;
      case EINTR:     // interrupted by system call/signal, try again
	continue;
	break;
      default:
	perror("MsgNextEvent");
	break;
      }
      return -1;
    } else {
      
      memcpy(event_return, msg.event_data, sizeof(MsgEvent_t));
      return 0;
    }
  }

}

int MsgSendEvent(MsgEventQ_t *q, MsgEventClient_t client,
		 MsgEvent_t *event_send, int msgflg)
{
  msg_t msg;
  int size = 0;
  msg.mtype = client; // the recipient of this message
  event_send->any.client = q->mtype; // the sender  

  switch(event_send->type) {
  case MsgEventQInitReq:
    size = sizeof(MsgQInitReqEvent_t);
    break;
  case MsgEventQInitGnt:  
    size = sizeof(MsgQInitGntEvent_t);
    break;
  case MsgEventQRegister:
    size = sizeof(MsgQRegisterCapsEvent_t);
    break;
  case MsgEventQNotify:
    size = sizeof(MsgQNotifyEvent_t);
    break;
  case MsgEventQReqCapability:
    size = sizeof(MsgQReqCapabilityEvent_t);
    break;
  case MsgEventQGntCapability:
    size = sizeof(MsgQGntCapabilityEvent_t);
    break;
  case MsgEventQPlayCtrl:
    size = sizeof(MsgQPlayCtrlEvent_t);
    break;
  case MsgEventQChangeFile:
    size = sizeof(MsgQAnyEvent_t)+strlen(event_send->changefile.filename)+1;
    break;
  case MsgEventQReqStreamBuf:
    size = sizeof(MsgQReqStreamBufEvent_t);
    break;
  case MsgEventQGntStreamBuf:
    size = sizeof(MsgQGntStreamBufEvent_t);
    break;
  case MsgEventQDecodeStreamBuf:
    size = sizeof(MsgQDecodeStreamBufEvent_t);
    break;
  case MsgEventQReqBuf:
    size = sizeof(MsgQReqBufEvent_t);
    break;
  case MsgEventQGntBuf:
    size = sizeof(MsgQGntBufEvent_t);
    break;
  case MsgEventQDestroyBuf:
    size = sizeof(MsgQDestroyBufEvent_t);
    break;
  case MsgEventQCtrlData:
    size = sizeof(MsgQCtrlDataEvent_t);
    break;
  case MsgEventQReqPicBuf:
    size = sizeof(MsgQReqPicBufEvent_t);
    break;
  case MsgEventQGntPicBuf:
    size = sizeof(MsgQGntPicBufEvent_t);
    break;
  case MsgEventQAttachQ:
  case MsgEventQAppendQ:
    size = sizeof(MsgQAttachQEvent_t);
    break;
  case MsgEventQDetachQ:
  case MsgEventQQDetached:
  case MsgEventQDestroyQ:
    size = sizeof(MsgQDetachQEvent_t);
    break;
  case MsgEventQSPUPalette:
    size = sizeof(MsgQSPUPaletteEvent_t);
    break;
  case MsgEventQSPUHighlight:
    size = sizeof(MsgQSPUHighlightEvent_t);
    break;
  case MsgEventQSpeed:
    size = sizeof(MsgQSpeedEvent_t);
    break;
  case MsgEventQDVDCtrl:
    size = sizeof(MsgQDVDCtrlEvent_t);
    break;
  case MsgEventQFlow:
    size = sizeof(MsgQFlowEvent_t);
    break;
  case MsgEventQFlushData:
    size = sizeof(MsgQFlushDataEvent_t);
    break;
  case MsgEventQDemuxStream:
    size = sizeof(MsgQDemuxStreamEvent_t);
    break;
  case MsgEventQDemuxStreamChange:
    size = sizeof(MsgQDemuxStreamChangeEvent_t);
    break;
  case MsgEventQDemuxStreamChange2:
    size = sizeof(MsgQDemuxStreamChange2Event_t);
    break;
  case MsgEventQDemuxDefault:
    size = sizeof(MsgQDemuxDefaultEvent_t);
    break;
  case MsgEventQDVDCtrlLong:
    switch(event_send->dvdctrllong.cmd.type) {
    case DVDCtrlLongSetDVDRoot:
      size = sizeof(MsgQAnyEvent_t) + sizeof(DVDCtrlLongAnyEvent_t)
	+ strlen(event_send->dvdctrllong.cmd.dvdroot.path)+1;
      break;
    case DVDCtrlLongSetState:
      size = sizeof(MsgQAnyEvent_t) + sizeof(DVDCtrlLongAnyEvent_t)
	+ strlen(event_send->dvdctrllong.cmd.state.xmlstr)+1;
      break;
    case DVDCtrlLongVolIds:
      size = sizeof(MsgQAnyEvent_t) + sizeof(DVDCtrlLongVolIdsEvent_t);
      break;
    default:
      size = sizeof(MsgQDVDCtrlLongEvent_t);
      break;
    }
    break;
  case MsgEventQDemuxDVD:
    size = sizeof(MsgQDemuxDVDEvent_t);
    break;
  case MsgEventQDemuxDVDRoot:
    size = sizeof(MsgQAnyEvent_t)+strlen(event_send->demuxdvdroot.path)+1;
    break;
  case MsgEventQSetAspectModeSrc:
    size = sizeof(MsgQSetAspectModeSrcEvent_t);
    break;
  case MsgEventQSetSrcAspect:
    size = sizeof(MsgQSetSrcAspectEvent_t);
    break;
  case MsgEventQSetZoomMode:
    size = sizeof(MsgQSetZoomModeEvent_t);
    break;
  case MsgEventQReqInput:
    size = sizeof(MsgQReqInputEvent_t);
    break;
  case MsgEventQInputButtonPress:
  case MsgEventQInputButtonRelease:
  case MsgEventQInputPointerMotion:
  case MsgEventQInputKeyPress:
  case MsgEventQInputKeyRelease:
    size = sizeof(MsgQInputEvent_t);
    break;
  case MsgEventQSaveScreenshot:
    size = sizeof(MsgQAnyEvent_t) + sizeof(ScreenshotMode_t) + 
      strlen(event_send->savescreenshot.formatstr)+1;
    break;
  default:
    fprintf(stderr, "MsgSendEvent: Unknown event: %d\n", event_send->type);
    return -1;
  }

  memcpy(msg.event_data, event_send, size);

#ifdef DEBUG
  fprintf(stderr, "Sending '%s' from: %ld to: %ld\n",
	  MsgEventType_str[msg.event.type],
	  msg.event.any.client,
	  msg.mtype);
#endif
  
  while(1) {
    if(msgsnd(q->msqid, (void *)&msg, size, msgflg) == -1) {
      switch(errno) {
      case EINTR: //interrupted by syscall/signal, try again
	continue;
	break;
      default:
	perror("MsgSendEvent");
	break;
      }
      return -1;
    } else {
      return 0;
    }
  }
  
}


  
/*
  
  1 semaphore for each msg queue receiver
  
  one msgarray for each receiver


  msgsnd(msgqid, msg, size, nowait/wait) {
  
  semwait(msgqsem[msgqid]);
  qnr = qnrs[msgqid];
  qnrs[msgqid]++;
  sempost(msgqsem[msgqid]);

  fill 



  }
  
  if(msgsnd(q->msqid, (void *)&msg, size, 0) == -1) {
    perror("MsgSendEvent");
    return -1;
  }



 */
