/* Ogle - A video player
 * Copyright (C) 2000, 2001 Håkan Hjort
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
#include <assert.h>
#include <time.h>

#include <ogle/msgevents.h>
#include <ogle/dvdevents.h>
#include "debug_print.h"

#include <dvdread/nav_types.h>
#include <dvdread/nav_read.h>
#include <dvdread/nav_print.h>
#include "vm.h"
#include "interpret_config.h"


extern int wait_q(MsgEventQ_t *msgq, MsgEvent_t *ev); // com.c
extern int get_q(MsgEventQ_t *msgq, unsigned char *buffer);
extern void wait_for_init(MsgEventQ_t *msgq);
extern void handle_events(MsgEventQ_t *msgq, MsgEvent_t *ev);
extern int send_demux(MsgEventQ_t *msgq, MsgEvent_t *ev);
extern int send_spu(MsgEventQ_t *msgq, MsgEvent_t *ev);
extern char *get_dvdroot(void);
extern void free_dvdroot(void);

extern dvd_state_t state;
extern unsigned char discid[16];

MsgEvent_t dvdroot_return_ev;
MsgEventClient_t dvdroot_return_client;

static void do_run(void);
static int process_user_data(MsgEvent_t ev, pci_t *pci, dsi_t *dsi,
			     cell_playback_t *cell, 
			     int block, int *still_time);

static void time_convert(DVDTimecode_t *dest, dvd_time_t *source)
{
  dest->Hours   = bcd2int(source->hour);
  dest->Minutes = bcd2int(source->minute);
  dest->Seconds = bcd2int(source->second);
  dest->Frames  = bcd2int(source->frame_u & 0x3f);
}


MsgEventQ_t *msgq;

char *program_name;
int dlevel;

void usage(void)
{
  fprintf(stderr, "Usage: %s  -m <msgqid>\n", 
          program_name);
}

int main(int argc, char *argv[])
{
  int msgqid = -1;
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
      exit(1);
    }
  }
  
  if(msgqid == -1) {
    fprintf(stderr, "what?\n");
    exit(1);
  }

  srand(getpid());

  {
    MsgEvent_t ev;
    
    if((msgq = MsgOpen(msgqid)) == NULL) {
      FATAL("%s", "couldn't get message q\n");
      exit(1);
    }
    
    ev.type = MsgEventQRegister;
    ev.registercaps.capabilities = DECODE_DVD_NAV;
    if(MsgSendEvent(msgq, CLIENT_RESOURCE_MANAGER, &ev, 0) == -1) {
      FATAL("%s", "registering capabilities\n");
      exit(1);
    }
    
    ev.type = MsgEventQReqCapability;
    ev.reqcapability.capability = DEMUX_MPEG2_PS | DEMUX_MPEG1;
    if(MsgSendEvent(msgq, CLIENT_RESOURCE_MANAGER, &ev, 0) == -1) {
      FATAL("%s", "didn't get demux cap\n");
      exit(1);
    }
    
    ev.type = MsgEventQReqCapability;
    ev.reqcapability.capability = DECODE_DVD_SPU;
    if(MsgSendEvent(msgq, CLIENT_RESOURCE_MANAGER, &ev, 0) == -1) {
      FATAL("%s", "didn't get spu cap\n");
      exit(1);
    }
    
    vm_reset();

    interpret_config();

    while(1) {
      wait_for_init(msgq);
    
      /*  Call start here */
      // hack placed here because it calls DVDOpen...
      DNOTE("Opening DVD at \"%s\"\n", get_dvdroot());
      if(vm_init(get_dvdroot()) == -1) {
	// Failure, don't quit... just let the app know we didn't succeed
	dvdroot_return_ev.dvdctrl.cmd.retval.val = DVD_E_RootNotSet;
	MsgSendEvent(msgq, dvdroot_return_client, &dvdroot_return_ev, 0);
	free_dvdroot();
      } else {
	// Success, send ok and breakout of loop
	dvdroot_return_ev.dvdctrl.cmd.retval.val = DVD_E_Ok;
	MsgSendEvent(msgq, dvdroot_return_client, &dvdroot_return_ev, 0);
	break;
      }
    }
    
    ev.type = MsgEventQDemuxDVDRoot;
    strncpy(ev.demuxdvdroot.path, get_dvdroot(), sizeof(ev.demuxdvdroot.path));
    ev.demuxdvdroot.path[sizeof(ev.demuxdvdroot.path)-1] = '\0';
    if(send_demux(msgq, &ev) == -1) {
      FATAL("%s", "failed sending dvdroot to demuxer\n");
      exit(1);
    }
    
    ev.type = MsgEventQDemuxStream;
    ev.demuxstream.stream_id = 0xe0; // Mpeg1/2 Video 
    ev.demuxstream.subtype = 0;    
    if(send_demux(msgq, &ev) == -1) {
      FATAL("%s", "failed setting demux video stream id\n");
      exit(1);
    }
    
    ev.type = MsgEventQDemuxStream;
    ev.demuxstream.stream_id = 0xbd; // AC3 1
    ev.demuxstream.subtype = 0x80;    
    if(send_demux(msgq, &ev) == -1) {
      FATAL("%s", "failed setting demux AC3 stream id\n");
      exit(1);
    }
    
    ev.type = MsgEventQDemuxStream;
    ev.demuxstream.stream_id = 0xbd; // SPU 1
    ev.demuxstream.subtype = 0x20;    
    if(send_demux(msgq, &ev) == -1) {
      FATAL("%s", "failed setting demux subpicture stream id\n");
      exit(1);
    }
    
    ev.type = MsgEventQDemuxStream;
    ev.demuxstream.stream_id = 0xbf; // NAV
    ev.demuxstream.subtype = 0;    
    if(send_demux(msgq, &ev) == -1) {
      FATAL("%s", "failed setting demux NAV stream id\n");
      exit(1);
    }
  }
  
  //vm_reset(get_dvdroot());
  do_run();
  
  return 0;
}



/**
 * Update any info the demuxer needs, and then tell the demuxer
 * what range of sectors to process.
 */
static void send_demux_sectors(int start_sector, int nr_sectors, 
			       FlowCtrl_t flush) {
  static int video_aspect = -1;
  //  static int audio_stream_id = -1;
  static int subp_stream_id = -1;
  MsgEvent_t ev;
  
  /* Tell video out what aspect ratio this pice has */ 
  {
    video_attr_t attr = vm_get_video_attr();
    if(attr.display_aspect_ratio != video_aspect) {
      video_aspect = attr.display_aspect_ratio;
      
      //DNOTE("sending aspect %s\n", video_aspect ? "16:9" : "4:3");
      
      ev.type = MsgEventQSetSrcAspect;
      ev.setsrcaspect.mode_src = AspectModeSrcVM;
      if(video_aspect) {
	ev.setsrcaspect.aspect_frac_n = 16;
	ev.setsrcaspect.aspect_frac_d = 9;
      } else {
	ev.setsrcaspect.aspect_frac_n = 4;
	ev.setsrcaspect.aspect_frac_d = 3;
      }
      /* !!! FIXME should be sent to video out not spu */
      if(send_spu(msgq, &ev) == -1) {
	ERROR("%s", "failed to send aspect info\n");
      }
    }     
  }
  /* Tell the demuxer which audio stream to demux */ 
  {
    int sN = vm_get_audio_stream(state.AST_REG);
    if(sN < 0 || sN > 7) sN = 7; // XXX == -1 for _no audio_
    {
      static uint8_t old_id = 0xbd;
      static uint8_t old_subtype = 0x80;
      uint8_t new_id;
      uint8_t new_subtype;
      audio_attr_t attr;
      int audio_stream_id = sN;
      
      new_id = 0;
      new_subtype = 0;
      
      attr = vm_get_audio_attr(sN);
      
      switch(attr.audio_format) {
      case 0:
	//af = DVD_AUDIO_FORMAT_AC3;
	new_id = 0xbd; //private stream 1
	new_subtype = 0x80 + audio_stream_id; // AC-3
	break;
      case 2:
	//af = DVD_AUDIO_FORMAT_MPEG1;
	new_id = 0xC0 + audio_stream_id; //mpeg audio
      case 3:
	//af = DVD_AUDIO_FORMAT_MPEG2;
	new_id = 0xC0 + audio_stream_id; //mpeg audio
	break;
      case 4:
	//af = DVD_AUDIO_FORMAT_LPCM;
	new_id = 0xbd; //private stream 1
	new_subtype = 0xA0 + audio_stream_id; // lpcm
	break;
      case 6:
	//af = DVD_AUDIO_FORMAT_DTS;
	new_id = 0xbd; //private stream 1
	new_subtype = 0x88 + audio_stream_id; // dts
	break;
      default:
	NOTE("%s", "please send a bug report, unknown Audio format!");
	break;
      }
      
      if(old_id != new_id || old_subtype != new_subtype) {
	DNOTE("sending audio demuxstream %d\n", sN);
	DNOTE("oid: %02x, ost: %02x, nid: %02x, nst: %02x\n",
	      old_id, old_subtype, new_id, new_subtype);
	ev.type = MsgEventQDemuxStreamChange2;
	ev.demuxstreamchange2.old_stream_id = old_id;
	ev.demuxstreamchange2.old_subtype = old_subtype;
	ev.demuxstreamchange2.new_stream_id = new_id;
	ev.demuxstreamchange2.new_subtype = new_subtype;
	if(send_demux(msgq, &ev) == -1) {
	  ERROR("%s", "failed to send audio demuxstream\n");
	}
      }
      old_id = new_id;
      old_subtype = new_subtype;
    }
  }

  /* Tell the demuxer which subpicture stream to demux */ 
  {
    int sN = vm_get_subp_active_stream();
    if(sN < 0 || sN > 31) sN = 31; // XXX == -1 for _no audio_
    if(sN != subp_stream_id) {
      subp_stream_id = sN;
      
      DNOTE("sending subp demuxstream %d\n", sN);
    
      ev.type = MsgEventQDemuxStreamChange;
      ev.demuxstreamchange.stream_id = 0xbd; // SPU
      ev.demuxstreamchange.subtype = 0x20 | subp_stream_id;
      if(send_demux(msgq, &ev) == -1) {
	ERROR("%s", "failed to send Subpicture demuxstream\n");
      }
    }
  }

  /* Tell the demuxer what file and which sectors to demux. */
  ev.type = MsgEventQDemuxDVD;
  if(state.domain == VMGM_DOMAIN || state.domain == FP_DOMAIN)
    ev.demuxdvd.titlenum = 0;
  else
    ev.demuxdvd.titlenum = state.vtsN;
  if(state.domain == VTS_DOMAIN)
    ev.demuxdvd.domain = DVD_READ_TITLE_VOBS;
  else
    ev.demuxdvd.domain = DVD_READ_MENU_VOBS;
  ev.demuxdvd.block_offset = start_sector;
  ev.demuxdvd.block_count = nr_sectors;
  ev.demuxdvd.flowcmd = flush;
  if(send_demux(msgq, &ev) == -1) {
    FATAL("%s", "failed to send demux dvd block range\n");
    exit(1);
  }
  //DNOTE("sent demux dvd block range (%d,%d)\n", start_sector, nr_sectors);
}

static void send_spu_palette(uint32_t palette[16]) {
  MsgEvent_t ev;
  int i;
  
  ev.type = MsgEventQSPUPalette;
  for(i = 0; i < 16; i++) {
    ev.spupalette.colors[i] = palette[i];
  }
  
  //DNOTE("sending subpicture palette\n");
  
  if(send_spu(msgq, &ev) == -1) {
    ERROR("%s", "failed sending subpicture palette\n");
  }
}

static void send_highlight(int x_start, int y_start, int x_end, int y_end, 
			   uint32_t btn_coli) {
  MsgEvent_t ev;
  int i;
  
  ev.type = MsgEventQSPUHighlight;
  ev.spuhighlight.x_start = x_start;
  ev.spuhighlight.y_start = y_start;
  ev.spuhighlight.x_end = x_end;
  ev.spuhighlight.y_end = y_end;
  for(i = 0; i < 4; i++)
    ev.spuhighlight.color[i] = 0xf & (btn_coli >> (16 + 4*i));
  for(i = 0; i < 4; i++)
    ev.spuhighlight.contrast[i] = 0xf & (btn_coli >> (4*i));

  if(send_spu(msgq, &ev) == -1) {
    ERROR("%s", "faild sending highlight info\n");
  }
}

/** 
 * Check if mouse coords are over any highlighted area.
 * 
 * @return The button number if the the coordinate is enclosed in the area.
 * Zero otherwise.
 */
int mouse_over_hl(pci_t *pci, unsigned int x, unsigned int y) {
  int button = 1;
  while(button <= pci->hli.hl_gi.btn_ns) {
    if( (x >= pci->hli.btnit[button-1].x_start)
	&& (x <= pci->hli.btnit[button-1].x_end) 
	&& (y >= pci->hli.btnit[button-1].y_start) 
	&& (y <= pci->hli.btnit[button-1].y_end )) 
      return button;
    button++;
  }
  return 0;
}

/** 
 * Update the highligted button in response to an input event.
 * Also send highlight information to the spu_mixer.
 *
 * @return One if the (possibly updated) button is activated.
 * Zero otherwise.
 */
static int process_button(DVDCtrlEvent_t *ce, pci_t *pci, uint16_t *btn_reg) {
  /* Keep the button register value in a local variable. */
  uint16_t button_nr = (*btn_reg) >> 10;
  int tmp, is_action = 0;

  /* MORE CODE HERE :) */
  
  /* Paranoia.. */
  
  // No highlight/button pci info to use or no buttons
  if((pci->hli.hl_gi.hli_ss & 0x03) == 0 || pci->hli.hl_gi.btn_ns == 0)
    return 0;
  
  // Selected button > than max button? then cap it
  if(button_nr > pci->hli.hl_gi.btn_ns)
    button_nr = pci->hli.hl_gi.btn_ns;
  
  // Selected button should never be 0.
  if(button_nr == 0) {
    //FATAL("%s", "send bug report, button number is 0, this is invalid.");
    button_nr = 1;
    *btn_reg = button_nr << 10;
  } 
    
  switch(ce->type) {
  case DVDCtrlUpperButtonSelect:
    button_nr = pci->hli.btnit[button_nr - 1].up;
    break;
  case DVDCtrlLowerButtonSelect:
    button_nr = pci->hli.btnit[button_nr - 1].down;
    break;
  case DVDCtrlLeftButtonSelect:
    button_nr = pci->hli.btnit[button_nr - 1].left;
    break;
  case DVDCtrlRightButtonSelect:
    button_nr = pci->hli.btnit[button_nr - 1].right;
    break;
  case DVDCtrlButtonActivate:
    is_action = 1;
    break;
  case DVDCtrlButtonSelectAndActivate:
    is_action = 1;
    /* Fall through */
  case DVDCtrlButtonSelect:
    tmp = ce->button.nr - pci->hli.hl_gi.btn_ofn;
    if(tmp > 0 && tmp <= pci->hli.hl_gi.nsl_btn_ns)
      button_nr = tmp;
    else /* not a valid button */
      is_action = 0;
    break;
  case DVDCtrlMouseActivate:
    is_action = 1;
    /* Fall through */
  case DVDCtrlMouseSelect:
    {
      int button;
      //int width, height;
      //vm_get_video_res(&width, &height);
      button = mouse_over_hl(pci, ce->mouse.x, ce->mouse.y);
      if(button)
	button_nr = button;
      else
	is_action = 0;
    }
    break;
  default:
    DNOTE("ignoring dvdctrl event (%d)\n", ce->type);
    break;
  }
  
  /* Must check if the current selected button has auto_action_mode !!! */
  /* Don't do auto action if it's been selected with the mouse... ?? */
  switch(pci->hli.btnit[button_nr - 1].auto_action_mode) {
  case 0:
    break;
  case 1:
    if(ce->type == DVDCtrlMouseSelect) {
      /* auto_action buttons can't be select if they are not activated
	 keep the previous selected button */
      button_nr = (*btn_reg) >> 10;
    } else {
      DNOTE("%s", "auto_action_mode set!\n");
      is_action = 1;
    }
    break;
  case 2:
  case 3:
  default:
    FATAL("send bug report, unknown auto_action_mode(%d) btn: %d\n", 
	  pci->hli.btnit[button_nr - 1].auto_action_mode, button_nr);
    navPrint_PCI(pci);
    exit(1);
  }
  
  /* If a new button has been selected or if one has been activated. */
  /* Determine the correct area and send the information to the spu decoder. */
  /* Don't send if its the same as last time. */
  if(is_action || button_nr != ((*btn_reg) >> 10)) {
    btni_t *button;
    button = &pci->hli.btnit[button_nr - 1];
    send_highlight(button->x_start, button->y_start, 
		   button->x_end, button->y_end, 
		   pci->hli.btn_colit.btn_coli[button->btn_coln-1][is_action]);
  }
  
  /* Write the (updated) value back to the button register. */
  *btn_reg = button_nr << 10;
  
  return is_action;
}


/** 
 * Update the highligted button in response to a new pci packet.
 * Also send highlight information to the spu_mixer.
 * 
 * @return One if the (possibly updated) button is activated.
 * Zero otherwise.
 */
static void process_pci(pci_t *pci, uint16_t *btn_reg) {
  /* Keep the button register value in a local variable. */
  uint16_t button_nr = (*btn_reg) >> 10;
	  
  /* Check if this is alright, i.e. pci->hli.hl_gi.hli_ss == 1 only 
   * for the first menu pic packet? Should be.
   * What about looping menus? Will reset it every loop.. */
  if(pci->hli.hl_gi.hli_ss == 1) {
    if(pci->hli.hl_gi.fosl_btnn != 0) {
      button_nr = pci->hli.hl_gi.fosl_btnn;
      DNOTE("forced select button %d\n", pci->hli.hl_gi.fosl_btnn);
    }
  }
  
  /* SPRM[8] can be changed by 
     A) user operations  
         user operations can only change SPRM[8] if buttons exist.
     B) navigation commands  
         navigation commands can change SPRM[8] always.
     C) highlight information
     
     if no buttons exist SPRM[8] is kept at it's value 
     (except when navigation commands change it)

     if SPRM[8] doesn't have a valid value  (button_nr > nr_buttons)  
     button_nr = nr_buttons  (except when nr_buttons == 0, 
     then button_nr isn't changed at all.
  */
  if((pci->hli.hl_gi.hli_ss & 0x03) != 0 
     && button_nr > pci->hli.hl_gi.btn_ns
     && pci->hli.hl_gi.btn_ns != 0) {
    button_nr = pci->hli.hl_gi.btn_ns;
  }

  /* Determine the correct highlight and send the information to the spu. */
  if((pci->hli.hl_gi.hli_ss & 0x03) == 0 || 
     button_nr > pci->hli.hl_gi.btn_ns) {
    /* Turn off the highlight. */
    send_highlight(0, 0, 0, 0, 0 /* Transparent */);
  } else {
    /* Possible optimization: don't send if its the same as last time. 
       As in same hli info, same button number and same select/action state. */
    btni_t *button = &pci->hli.btnit[button_nr - 1];
    send_highlight(button->x_start, button->y_start, 
		   button->x_end, button->y_end, 
		   pci->hli.btn_colit.btn_coli[button->btn_coln-1][0]);
  }
  
  /* Write the (updated) value back to the button register. */  
  *btn_reg = button_nr << 10;
}


int process_seek(int seconds, dsi_t *dsi, cell_playback_t *cell)
{
  int res = 0;
  dvd_time_t current_time;
  
// bit 0: v0: *Video data* does not exist in the VOBU at the address
//        v1: *video data* does exists in the VOBU on the address
// bit 1: indicates whether theres *video data* between 
//        current vobu and last vobu. ??
// if address = 3fff ffff -> vobu does not exist
#define VALID_XWDA(OFFSET) \
  (((OFFSET) & SRI_END_OF_CELL) != SRI_END_OF_CELL && \
  ((OFFSET) & 0x80000000))
  
  vm_get_current_time(&current_time, &(dsi->dsi_gi.c_eltm));
  // Try using the Time Map Tables, should we use VOBU seeks for
  // small seek (< 8s) anyway? as they (may) have better resolution.
  // Fall back if we're crossing a cell bounduary...
  if(vm_time_play(&current_time, seconds)) {
    return 1; // Successfull 
  } else {
    // We have 120 60 30 10 7.5 7 6.5 ... 0.5 seconds markers
    if(seconds > 0) {
      const unsigned int time[19] = { 240, 120, 60, 20, 15, 14, 13, 12, 11, 
				       10,   9,  8,  7,  6,  5,  4,  3,  2, 1};
      const unsigned int hsec = seconds * 2;
      unsigned int diff, idx = 0;
	  
      diff = abs(hsec - time[0]);
      while(idx < 19 && abs(hsec - time[idx]) <= diff) {
	diff = abs(hsec - time[idx]);
	idx++;
      }
      idx--; // Restore it to the one that got us the diff
      
      // Make sure we have a VOBU that 'exists' (with in the cell)
      // What about the 'top' two bits here?  If there is no video at the
      // seek destination?  Check with the menus in Coruptor.
      while(idx < 19 && !VALID_XWDA(dsi->vobu_sri.fwda[idx])) {
	idx++;
      }
      if(idx < 19) {
	// Fake this, as a jump with blockN as destination
	// blockN is relative the start of the cell
	state.blockN = dsi->dsi_gi.nv_pck_lbn +
	  (dsi->vobu_sri.fwda[idx] & 0x3fffffff) - cell->first_sector;
	res = 1;
      } else
	res = 0; // no new block found.. must be at the end of the cell..
    } else {
      const unsigned int time[19] = { 240, 120, 60, 20, 15, 14, 13, 12, 11, 
				       10,   9,  8,  7,  6,  5,  4,  3,  2, 1};
      const unsigned int hsec = (-seconds) * 2; // -
      unsigned int diff, idx = 0;
      
      diff = abs(hsec - time[0]);
      
      while(idx < 19 && abs(hsec - time[idx]) <= diff) {
	diff = abs(hsec - time[idx]);
	idx++;
      }
      idx--; // Restore it to the one that got us the diff
      
      // Make sure we have a VOBU that 'exicsts' (with in the cell)
      // What about the 'top' two bits here?  If there is no video at the
      // seek destination?  Check with the menus in Coruptor.
      while(idx < 19 && !VALID_XWDA(dsi->vobu_sri.bwda[18-idx])) {
	idx++;
      }
      if(idx < 19) {
	// Fake this, as a jump with blockN as destination
	// blockN is relative the start of the cell
	state.blockN = dsi->dsi_gi.nv_pck_lbn -
	  (dsi->vobu_sri.bwda[18-idx] & 0x3fffffff) - cell->first_sector;
	res = 1;
      } else
	res = 0; // no new_block found.. must be at the end of the cell..    
    }
  }
  return res;
}


/* Do user input processing. Like audio change, 
 * subpicture change and answer attribute query requests.
 * access menus, pause, play, jump forward/backward...
 */
int process_user_data(MsgEvent_t ev, pci_t *pci, dsi_t *dsi, 
		      cell_playback_t *cell, int block, int *still_time)
{
  int res = 0;
      
  //fprintf(stderr, "nav: User input, MsgEvent.type: %d\n", ev.type);
  
  switch(ev.dvdctrl.cmd.type) {
  case DVDCtrlLeftButtonSelect:
  case DVDCtrlRightButtonSelect:
  case DVDCtrlUpperButtonSelect:
  case DVDCtrlLowerButtonSelect:
  case DVDCtrlButtonActivate:
  case DVDCtrlButtonSelect:
  case DVDCtrlButtonSelectAndActivate:
  case DVDCtrlMouseSelect:
  case DVDCtrlMouseActivate:
    
    // A button has already been activated, discard this event??
    
    if(cell->first_sector <= pci->pci_gi.nv_pck_lbn
       && cell->last_vobu_start_sector >= pci->pci_gi.nv_pck_lbn) {
      /* Update selected/activated button, send highlight info to spu */
      /* Returns true if a button is activated */
      if(process_button(&ev.dvdctrl.cmd, pci, &state.HL_BTNN_REG)) {
	int button_nr = state.HL_BTNN_REG >> 10;
	res = vm_eval_cmd(&pci->hli.btnit[button_nr - 1].cmd);
      }
    }
    break;
  
  case DVDCtrlTimeSkip:
    if(dsi->dsi_gi.nv_pck_lbn == -1) { // we are waiting for a new nav block
      res = 0;
      break;
    }
    res = process_seek(ev.dvdctrl.cmd.timeskip.seconds, dsi, cell);
    if(res)
      NOTE("%s", "Doing time seek\n");
    break;  
  
  case DVDCtrlMenuCall:
    NOTE("Jumping to Menu %d\n", ev.dvdctrl.cmd.menucall.menuid);
    res = vm_menu_call(ev.dvdctrl.cmd.menucall.menuid, block);
    if(!res)
      NOTE("%s", "No such menu!\n");
    break;
	  
  case DVDCtrlResume:
    res = vm_resume();
    break;
	  
  case DVDCtrlGoUp:
    res = vm_goup_pgc();
    break;
	  
  case DVDCtrlBackwardScan:
    DNOTE("unknown (not handled) DVDCtrlEvent %d\n",
	  ev.dvdctrl.cmd.type);
    break;
	
  case DVDCtrlPauseOn:
  case DVDCtrlPauseOff:
  case DVDCtrlForwardScan:  
    {
      MsgEvent_t send_ev;
      static double last_speed = 1.0;
      send_ev.type = MsgEventQSpeed;
      if(ev.dvdctrl.cmd.type == DVDCtrlForwardScan) {
	send_ev.speed.speed = ev.dvdctrl.cmd.scan.speed;
	last_speed = ev.dvdctrl.cmd.scan.speed;
      }
      /* Perhaps we should remeber the speed before the pause. */
      else if(ev.dvdctrl.cmd.type == DVDCtrlPauseOn)
	send_ev.speed.speed = 0.000000001;
      else if(ev.dvdctrl.cmd.type == DVDCtrlPauseOff)
	send_ev.speed.speed = last_speed;
	    
      /* Hack to exit STILL_MODE if we're in it. */
      if(cell->first_sector + block > cell->last_vobu_start_sector &&
	 *still_time > 0) {
	*still_time = 0;
      }
      MsgSendEvent(msgq, CLIENT_RESOURCE_MANAGER, &send_ev, 0);
    }
    break;
	  
  case DVDCtrlNextPGSearch:
    res = vm_next_pg();
    break;
  case DVDCtrlPrevPGSearch:
    res = vm_prev_pg();
    break;
  case DVDCtrlTopPGSearch:
    res = vm_top_pg();
    break;
	  
  case DVDCtrlPTTSearch:
    res = vm_jump_ptt(ev.dvdctrl.cmd.pttsearch.ptt);
    break;
  case DVDCtrlPTTPlay:
    res = vm_jump_title_ptt(ev.dvdctrl.cmd.pttplay.title,
			    ev.dvdctrl.cmd.pttplay.ptt);
    break;	
  case DVDCtrlTitlePlay:
    res = vm_jump_title(ev.dvdctrl.cmd.titleplay.title);
    break;
  case DVDCtrlTimeSearch:
    // not in One_Random_PGC_Title or Multi_PGC_Title
    //dsi.dsi_gi.c_eltm; /* Current 'nav' time */
    //ev.dvdctrl.cmd.timesearch.time; /* wanted time */
    //dsi.vobu_sri.[FWDA|BWDA]; /* Table for small searches */
    break;
  case DVDCtrlTimePlay:
    // not in One_Random_PGC_Title or Multi_PGC_Title
    DNOTE("unknown (not handled) DVDCtrlEvent %d\n",
	  ev.dvdctrl.cmd.type);
    break;
	  
	  
  case DVDCtrlStop:
    DNOTE("unknown (not handled) DVDCtrlEvent %d\n",
	  ev.dvdctrl.cmd.type);
    break;
	  
  case DVDCtrlAngleChange:
    /* FIXME $$$ need to actually change the playback angle too, no? */
    state.AGL_REG = ev.dvdctrl.cmd.anglechange.anglenr;
    break;
  case DVDCtrlAudioStreamChange: // FIXME $$$ Temorary hack
    state.AST_REG = ev.dvdctrl.cmd.audiostreamchange.streamnr; // XXX
    break;
  case DVDCtrlSubpictureStreamChange: // FIXME $$$ Temorary hack
    state.SPST_REG &= 0x40; // Keep the on/off bit.
    state.SPST_REG |= ev.dvdctrl.cmd.subpicturestreamchange.streamnr;
    NOTE("DVDCtrlSubpictureStreamChange %x\n", state.SPST_REG);
    break;
  case DVDCtrlSetSubpictureState:
    if(ev.dvdctrl.cmd.subpicturestate.display == DVDTrue)
      state.SPST_REG |= 0x40; // Turn it on
    else
      state.SPST_REG &= ~0x40; // Turn it off
    NOTE("DVDCtrlSetSubpictureState 0x%x\n", state.SPST_REG);
    break;
  case DVDCtrlGetCurrentDomain:
    {
      MsgEvent_t send_ev;
      int domain;
      domain = vm_get_domain();
      send_ev.type = MsgEventQDVDCtrl;
      send_ev.dvdctrl.cmd.type = DVDCtrlCurrentDomain;
      send_ev.dvdctrl.cmd.domain.domain = domain;
      MsgSendEvent(msgq, ev.any.client, &send_ev, 0);
    }
    break;
  case DVDCtrlGetCurrentLocation:
    {
      MsgEvent_t send_ev;
      dvd_time_t current_time;
      dvd_time_t total_time;
      DVDLocation_t *location;
      send_ev.type = MsgEventQDVDCtrl;
      send_ev.dvdctrl.cmd.type = DVDCtrlCurrentLocation;
      /* should not return when domain is wrong( /= title). */
      /* how to get current time for searches in menu/system space? */
      /* a bit of a hack */
      location = &send_ev.dvdctrl.cmd.location.location;
      location->title = state.TTN_REG;
      location->ptt = state.PTTN_REG;
      vm_get_total_time(&total_time);
      time_convert(&location->title_total, &total_time);
      vm_get_current_time(&current_time, &(pci->pci_gi.e_eltm));
      time_convert(&location->title_current, &current_time);
      MsgSendEvent(msgq, ev.any.client, &send_ev, 0);
    }
    break;
  case DVDCtrlGetDVDVolumeInfo:
    {
      int nofv,vol,side,noft;
      MsgEvent_t send_ev;
      send_ev.type = MsgEventQDVDCtrl;
      send_ev.dvdctrl.cmd.type = DVDCtrlDVDVolumeInfo;
      vm_get_volume_info(&nofv,&vol,&side,&noft);
      send_ev.dvdctrl.cmd.volumeinfo.volumeinfo.nrofvolumes = nofv;
      send_ev.dvdctrl.cmd.volumeinfo.volumeinfo.volume = vol;
      send_ev.dvdctrl.cmd.volumeinfo.volumeinfo.side = side;
      send_ev.dvdctrl.cmd.volumeinfo.volumeinfo.nroftitles = noft;
      MsgSendEvent(msgq, ev.any.client, &send_ev, 0);
    }
    break;
  case DVDCtrlGetTitles:
    {
      MsgEvent_t send_ev;
      int titles;
      titles = vm_get_titles();
      send_ev.type = MsgEventQDVDCtrl;
      send_ev.dvdctrl.cmd.type = DVDCtrlTitles;
      send_ev.dvdctrl.cmd.titles.titles = titles;
      MsgSendEvent(msgq, ev.any.client, &send_ev, 0);
    }
    break;
  case DVDCtrlGetNumberOfPTTs:
    {
      MsgEvent_t send_ev;
      int ptts, title;
      title = ev.dvdctrl.cmd.parts.title;
      ptts = vm_get_ptts_for_title(title);
      send_ev.type = MsgEventQDVDCtrl;
      send_ev.dvdctrl.cmd.type = DVDCtrlNumberOfPTTs;
      send_ev.dvdctrl.cmd.parts.title = title;
      send_ev.dvdctrl.cmd.parts.ptts = ptts;
      MsgSendEvent(msgq, ev.any.client, &send_ev, 0);
    }
    break;
  case DVDCtrlGetCurrentAudio:
    {
      MsgEvent_t send_ev;
      int nS, cS;
      vm_get_audio_info(&nS, &cS);
      send_ev.type = MsgEventQDVDCtrl;
      send_ev.dvdctrl.cmd.type = DVDCtrlCurrentAudio;
      send_ev.dvdctrl.cmd.currentaudio.nrofstreams = nS;
      send_ev.dvdctrl.cmd.currentaudio.currentstream = cS;
      MsgSendEvent(msgq, ev.any.client, &send_ev, 0);
    }
    break;
  case DVDCtrlIsAudioStreamEnabled:
    {
      MsgEvent_t send_ev;
      int streamN = ev.dvdctrl.cmd.audiostreamenabled.streamnr;
      send_ev.type = MsgEventQDVDCtrl;
      send_ev.dvdctrl.cmd.type = DVDCtrlAudioStreamEnabled;
      send_ev.dvdctrl.cmd.audiostreamenabled.streamnr = streamN;
      send_ev.dvdctrl.cmd.audiostreamenabled.enabled =
	(vm_get_audio_stream(streamN) != -1) ? DVDTrue : DVDFalse;
      MsgSendEvent(msgq, ev.any.client, &send_ev, 0);	    
    }
    break;
  case DVDCtrlGetCurrentUOPS: // FIXME XXX $$$ Not done
    {
      DVDUOP_t res = 0;
      MsgEvent_t send_ev;
      send_ev.type = MsgEventQDVDCtrl;
      send_ev.dvdctrl.cmd.currentuops.type = DVDCtrlCurrentUOPS;
      {
	user_ops_t p_uops = vm_get_uops();
	/* This mess is needed...
	 * becuse we didn't do a endian swap in libdvdread */
	res |= (p_uops.title_or_time_play ? 0 : UOP_FLAG_TitleOrTimePlay);
	res |= (p_uops.chapter_search_or_play ? 0 : UOP_FLAG_ChapterSearchOrPlay);
	res |= (p_uops.title_play ? 0 : UOP_FLAG_TitlePlay);
	res |= (p_uops.stop ? 0 : UOP_FLAG_Stop);
	res |= (p_uops.go_up ? 0 : UOP_FLAG_GoUp);
	res |= (p_uops.time_or_chapter_search ? 0 : UOP_FLAG_TimeOrChapterSearch);
	res |= (p_uops.prev_or_top_pg_search ? 0 : UOP_FLAG_PrevOrTopPGSearch);
	res |= (p_uops.next_pg_search ? 0 : UOP_FLAG_NextPGSearch);

	res |= (p_uops.title_menu_call ? 0 : UOP_FLAG_TitleMenuCall);
	res |= (p_uops.root_menu_call ? 0 : UOP_FLAG_RootMenuCall);
	res |= (p_uops.subpic_menu_call ? 0 : UOP_FLAG_SubPicMenuCall);
	res |= (p_uops.audio_menu_call ? 0 : UOP_FLAG_AudioMenuCall);
	res |= (p_uops.angle_menu_call ? 0 : UOP_FLAG_AngleMenuCall);
	res |= (p_uops.chapter_menu_call ? 0 : UOP_FLAG_ChapterMenuCall);
	     
	res |= (p_uops.resume ? 0 : UOP_FLAG_Resume);
	res |= (p_uops.button_select_or_activate ? 0 : UOP_FLAG_ButtonSelectOrActivate);
	res |= (p_uops.still_off ? 0 : UOP_FLAG_StillOff);
	res |= (p_uops.pause_on ? 0 : UOP_FLAG_PauseOn);
	res |= (p_uops.audio_stream_change ? 0 : UOP_FLAG_AudioStreamChange);
	res |= (p_uops.subpic_stream_change ? 0 : UOP_FLAG_SubPicStreamChange);
	res |= (p_uops.angle_change ? 0 : UOP_FLAG_AngleChange);
	res |= (p_uops.karaoke_audio_pres_mode_change ? 0 : UOP_FLAG_KaraokeAudioPresModeChange);
	res |= (p_uops.video_pres_mode_change ? 0 : UOP_FLAG_VideoPresModeChange);
      }
      send_ev.dvdctrl.cmd.currentuops.uops = res;
      MsgSendEvent(msgq, ev.any.client, &send_ev, 0);
    }
    break;
  case DVDCtrlGetAudioAttributes: // FIXME XXX $$$ Not done
    {
      MsgEvent_t send_ev;
      int streamN = ev.dvdctrl.cmd.audioattributes.streamnr;
      send_ev.type = MsgEventQDVDCtrl;
      send_ev.dvdctrl.cmd.type = DVDCtrlAudioAttributes;
      send_ev.dvdctrl.cmd.audioattributes.streamnr = streamN;
      {
	DVDAudioFormat_t af = DVD_AUDIO_FORMAT_Other;
	audio_attr_t attr = vm_get_audio_attr(streamN);
	memset(&send_ev.dvdctrl.cmd.audioattributes.attr, 0, 
	       sizeof(DVDAudioAttributes_t)); //TBD
	switch(attr.audio_format) {
	case 0:
	  af = DVD_AUDIO_FORMAT_AC3;
	  break;
	case 2:
	  af = DVD_AUDIO_FORMAT_MPEG1;
	  break;
	case 3:
	  af = DVD_AUDIO_FORMAT_MPEG2;
	  break;
	case 4:
	  af = DVD_AUDIO_FORMAT_LPCM;
	  break;
	case 6:
	  af = DVD_AUDIO_FORMAT_DTS;
	  break;
	default:
	  NOTE("please send a bug report, unknown Audio format %d!", 
	       attr.audio_format);
	  break;
	}
	send_ev.dvdctrl.cmd.audioattributes.attr.AudioFormat 
	  = af;
	send_ev.dvdctrl.cmd.audioattributes.attr.AppMode 
	  = attr.application_mode;
	send_ev.dvdctrl.cmd.audioattributes.attr.LanguageExtension
	  = attr.lang_extension;
	send_ev.dvdctrl.cmd.audioattributes.attr.Language 
	  = attr.lang_code;
      }
      MsgSendEvent(msgq, ev.any.client, &send_ev, 0);	    
    }
    break;
  case DVDCtrlGetCurrentSubpicture:
    {
      MsgEvent_t send_ev;
      int nS, cS;
      vm_get_subp_info(&nS, &cS);
      send_ev.type = MsgEventQDVDCtrl;
      send_ev.dvdctrl.cmd.type = DVDCtrlCurrentSubpicture;
      send_ev.dvdctrl.cmd.currentsubpicture.nrofstreams = nS;
      send_ev.dvdctrl.cmd.currentsubpicture.currentstream = cS & ~0x40;
      send_ev.dvdctrl.cmd.currentsubpicture.display 
	= (cS & 0x40) ? DVDTrue : DVDFalse;
      MsgSendEvent(msgq, ev.any.client, &send_ev, 0);
    }
    break;
  case DVDCtrlIsSubpictureStreamEnabled:
    {
      MsgEvent_t send_ev;
      int streamN = ev.dvdctrl.cmd.subpicturestreamenabled.streamnr;
      send_ev.type = MsgEventQDVDCtrl;
      send_ev.dvdctrl.cmd.type = DVDCtrlSubpictureStreamEnabled;
      send_ev.dvdctrl.cmd.subpicturestreamenabled.streamnr = streamN;
      send_ev.dvdctrl.cmd.subpicturestreamenabled.enabled =
	(vm_get_subp_stream(streamN) != -1) ? DVDTrue : DVDFalse;
      MsgSendEvent(msgq, ev.any.client, &send_ev, 0);	    
    }
    break;
  case DVDCtrlGetSubpictureAttributes: // FIXME XXX $$$ Not done
    {
      MsgEvent_t send_ev;
      int streamN = ev.dvdctrl.cmd.subpictureattributes.streamnr;
      send_ev.type = MsgEventQDVDCtrl;
      send_ev.dvdctrl.cmd.type = DVDCtrlSubpictureAttributes;
      send_ev.dvdctrl.cmd.subpictureattributes.streamnr = streamN;
      {
	subp_attr_t attr = vm_get_subp_attr(streamN);
	memset(&send_ev.dvdctrl.cmd.subpictureattributes.attr, 0, 
	       sizeof(DVDSubpictureAttributes_t)); //TBD
	send_ev.dvdctrl.cmd.subpictureattributes.attr.Language 
	  = attr.lang_code;
      }
      MsgSendEvent(msgq, ev.any.client, &send_ev, 0);
    }	  
    break;
  case DVDCtrlGetCurrentAngle:
    {
      MsgEvent_t send_ev;
      int nS, cS;
      vm_get_angle_info(&nS, &cS);
      send_ev.type = MsgEventQDVDCtrl;
      send_ev.dvdctrl.cmd.type = DVDCtrlCurrentAngle;
      send_ev.dvdctrl.cmd.currentangle.anglesavailable = nS;
      send_ev.dvdctrl.cmd.currentangle.anglenr = cS;
      MsgSendEvent(msgq, ev.any.client, &send_ev, 0);
    }
    break;
  case DVDCtrlGetState:
    {
      MsgEvent_t send_ev;
      char *state_str;
      DVDCtrlLongStateEvent_t *state_ev;
      state_str = vm_get_state_str(block);
      
      send_ev.type = MsgEventQDVDCtrlLong;
      send_ev.dvdctrllong.cmd.type = DVDCtrlLongState;
      state_ev = &(send_ev.dvdctrllong.cmd.state);
      if(state_str != NULL) {
	strncpy(state_ev->xmlstr, state_str, sizeof(state_ev->xmlstr));
	state_ev->xmlstr[sizeof(state_ev->xmlstr)-1] = '\0';
	
      } else {
	state_ev->xmlstr[0] = '\0';
      }
      
      MsgSendEvent(msgq, ev.any.client, &send_ev, 0);
      
      free(state_str);
    }
    break;
  case DVDCtrlGetDiscID:
    {
      MsgEvent_t send_ev;

      send_ev.type = MsgEventQDVDCtrl;
      send_ev.dvdctrl.cmd.type = DVDCtrlDiscID;
      memcpy(send_ev.dvdctrl.cmd.discid.id, discid, sizeof(discid));
      MsgSendEvent(msgq, ev.any.client, &send_ev, 0);
    }
    break;
  case DVDCtrlGetVolIds:
    {
      MsgEvent_t send_ev;
      int voltype;
      DVDCtrlLongVolIdsEvent_t *ids;

      send_ev.type = MsgEventQDVDCtrlLong;
      send_ev.dvdctrllong.cmd.type = DVDCtrlLongVolIds;
      
      ids = &(send_ev.dvdctrllong.cmd.volids);
      
      voltype = ev.dvdctrl.cmd.volids.voltype;

      ids->voltype = 0;
      if(voltype == 0) {
	if(vm_get_udf_volids(ids->volid, sizeof(ids->volid),
			     ids->volsetid, sizeof(ids->volsetid)) == 0) {
	  ids->voltype = 1;
	} else if(vm_get_iso_volids(ids->volid, sizeof(ids->volid),
				    ids->volsetid, 
				    sizeof(ids->volsetid)) == 0) {
	  ids->voltype = 2;
	}
      } else if(voltype == 1) {
	if(vm_get_udf_volids(ids->volid, sizeof(ids->volid),
			     ids->volsetid, sizeof(ids->volsetid)) == 0) {
	  ids->voltype = 1;
	}
      } else if(voltype == 2) {
	if(vm_get_iso_volids(ids->volid, sizeof(ids->volid),
			     ids->volsetid, sizeof(ids->volsetid)) == 0) {
	  ids->voltype = 2;
	}
      } 
	
      MsgSendEvent(msgq, ev.any.client, &send_ev, 0);
    }
    break;
  default:
    DNOTE("unknown (not handled) DVDCtrlEvent %d\n",
	  ev.dvdctrl.cmd.type);
    break;
  }
  return res;
}

int process_long_user_data(MsgEvent_t ev, pci_t *pci, cell_playback_t *cell,
			   int block, int *still_time)
{
  int res = 0;
      
  //fprintf(stderr, "nav: User input, MsgEvent.type: %d\n", ev.type);
  
  switch(ev.dvdctrllong.cmd.type) {
  case DVDCtrlLongSetState:
    res = vm_set_state_str(ev.dvdctrllong.cmd.state.xmlstr);
    break;
  default:
    DNOTE("unknown (not handled) DVDCtrlLongEvent %d\n",
	  ev.dvdctrllong.cmd.type);
    break;
  }
  return res;
}



static int block;
static int still_time;
static cell_playback_t *cell;

static int pending_lbn;

#define INF_STILL_TIME (10 * 0xff)

static void do_init_cell(int flush) {
  
  cell = &state.pgc->cell_playback[state.cellN - 1];
  still_time = 10 * cell->still_time;

  block = state.blockN;
  assert(cell->first_sector + block <= cell->last_vobu_start_sector);

  // FIXME XXX $$$ Only send when needed, and do send even if not playing
  // from start? (should we do pre_commands when jumping to say part 3?)
  /* Send the palette to the spu. */
  send_spu_palette(state.pgc->palette);
  
  /* Get the pci/dsi data */
  if(flush)
    send_demux_sectors(cell->first_sector + block, 1, FlowCtrlFlush);
  else
    send_demux_sectors(cell->first_sector + block, 1, FlowCtrlNone);
  
  pending_lbn = cell->first_sector + block;
}


static void do_run(void) {
  pci_t pci;
  dsi_t dsi;
  
  vm_start(); // see hack in main
  do_init_cell(0);
  pci.pci_gi.nv_pck_lbn = -1;
  dsi.dsi_gi.nv_pck_lbn = -1;
  
  while(1) {
    MsgEvent_t ev;
    int got_data;
    
    // For now.. later use the time instead..
    /* Have we read the last dsi packet we asked for? Then request the next. */
    if(pending_lbn == dsi.dsi_gi.nv_pck_lbn
       && cell->first_sector + block <= cell->last_vobu_start_sector) {
      int complete_video;
      
      /* If there is video data in this vobu, but not in the next. Then this 
	 data must be a complete image, so let the decoder know this. */
      if((dsi.vobu_sri.next_vobu & 0x80000000) == 0 
	 && dsi.dsi_gi.vobu_1stref_ea != 0 /* there is video in this */) {
	complete_video = FlowCtrlCompleteVideoUnit;
	//DNOTE("FlowCtrlCompleteVideoUnit = 1;\n");
      } else {
	complete_video = FlowCtrlNone;
      }
      
      /* Demux/play the content of this vobu. */
      if(dsi.dsi_gi.vobu_ea != 0) {
	send_demux_sectors(cell->first_sector + block + 1, 
			   dsi.dsi_gi.vobu_ea, complete_video);
      }
      
      /* VOBU still ? */
      /* if(cell->playback_mode) .. */
      /* need to defer the playing of the next VOBU untill the still
       * is interrupted.  */
	 
/* start - get_next_vobu() */     	 
      /* The next vobu is where... (make this a function?) */
      /* angle change points are at next ILVU, not sure if 
	 one VOBU = one ILVU */ 
      if(0 /*angle && change_angle*/) {
	/* if( seamless )
	   else // non seamless
	*/
	;
      } else {
	/* .. top two bits are flags */  
	block += dsi.vobu_sri.next_vobu & 0x3fffffff;
      }
      
      
      /* TODO XXX $$$ Test earlier and merge the requests if posible? */
      /* If there is more data in this cell to demux, then get the
       * next nav pack. */
      if(cell->first_sector + block <= cell->last_vobu_start_sector) {
	send_demux_sectors(cell->first_sector + block, 1, FlowCtrlNone);
	pending_lbn = cell->first_sector + block;
      } else {
	//DNOTE("end of cell\n");
	; // end of cell!
	if(still_time == INF_STILL_TIME) // Inf. still time
	  NOTE("%s", "Still picture select an item to continue.\n");
	else if(still_time != 0)
	  NOTE("Pause for %d seconds,\n", still_time/10);
#if 0 
	/* TODO XXX $$$ This should only be done at the correct time */
	/* Handle forced activate button here */
	if(pci.hli.hl_gi.foac_btnn != 0 && 
	   (pci.hli.hl_gi.hli_ss & 0x03) != 0) {
	  uint16_t button_nr = state.HL_BTNN_REG >> 10;
	  /* Forced action 0x3f means selected button, 
	     otherwise use the specified button */
	  if(pci.hli.hl_gi.foac_btnn != 0x3f)
	    button_nr = pci.hli.hl_gi.foac_btnn;
	  if(button_nr > pci.hli.hl_gi.btn_ns)
	    ; // error selected but out of range...
	  state.HL_BTNN_REG = button_nr << 10;
	  
	  if(vm_eval_cmd(&pci.hli.btnit[button_nr - 1].cmd)) {
	    do_init_cell(/* ?? */ 0);
	    dsi.dsi_gi.nv_pck_lbn = -1;
	  }
	}
#endif
      }
    }
    
    
    // Wait for data/input or for cell still time to end
    {
      if(cell->first_sector + block <= cell->last_vobu_start_sector) {
	got_data = wait_q(msgq, &ev); // Wait for a data packet or a message
      
      } else { 
	/* Handle cell still time here */
	got_data = 0;
	if(still_time == INF_STILL_TIME) // Inf. still time
	  MsgNextEvent(msgq, &ev);
	else
	  while(still_time && MsgCheckEvent(msgq, &ev)) {
	    struct timespec req = {0, 100000000}; // 0.1s 
	    nanosleep(&req, NULL);
	    still_time--;
	  }
	
	if(!still_time) // No more still time (or there never was any..)
	  if(MsgCheckEvent(msgq, &ev)) { // and no more messages
	    // Let the vm run and give us a new cell to play
	    vm_get_next_cell();
	    do_init_cell(/* No jump */ 0);
	    dsi.dsi_gi.nv_pck_lbn = -1;
	  }
      }
    }
    /* If we are here we either have a message or an available data packet */ 
    
    
    /* User input events */
    if(!got_data) { // Then it must be a message (or error?)
      int res = 0;
      
      switch(ev.type) {
      case MsgEventQDVDCtrl:
	/* Do user input processing. Like audio change, 
	 * subpicture change and answer attribute query requests.
	 * access menus, pause, play, jump forward/backward...
	 */

	res = process_user_data(ev, &pci, &dsi, cell, block, &still_time);
	break;
      case MsgEventQDVDCtrlLong:

	res = process_long_user_data(ev, &pci, cell, block, &still_time);
	break;
	
      default:
	handle_events(msgq, &ev);
	/* If( new dvdroot ) {
	   vm_reset(get_dvdroot());
	   block = 0;
	   res = 1;
	   }
	*/
      }      
      if(res != 0) {/* a jump has occured */
	do_init_cell(/* Flush streams */1);
	dsi.dsi_gi.nv_pck_lbn = -1;
      }
      
    } else { // We got a data to read.
      unsigned char buffer[2048];
      int len;
      
      len = get_q(msgq, &buffer[0]);
      
      if(buffer[0] == PS2_PCI_SUBSTREAM_ID) {
	navRead_PCI(&pci, &buffer[1]);
	/* Is this the packet we are waiting for? */
	if(pci.pci_gi.nv_pck_lbn != pending_lbn) {
	  //fprintf(stdout, "nav: Droped PCI packet\n");
	  pci.pci_gi.nv_pck_lbn = -1;
	  continue;
	}
	//fprintf(stdout, "nav: Got PCI packet\n");
	/*
	if(pci.hli.hl_gi.hli_ss & 0x03) {
	  fprintf(stdout, "nav: Menu detected\n");
	  navPrint_PCI(&pci);
	}
	*/
	/* Evaluate and Instantiate the new pci packet */
	process_pci(&pci, &state.HL_BTNN_REG);
        
      } else if(buffer[0] == PS2_DSI_SUBSTREAM_ID) {
	navRead_DSI(&dsi, &buffer[1]);
	if(dsi.dsi_gi.nv_pck_lbn != pending_lbn) {
	  //fprintf(stdout, "nav: Droped DSI packet\n");
	  dsi.dsi_gi.nv_pck_lbn = -1;
	  continue;
	}
	//fprintf(stdout, "nav: Got DSI packet\n");
	//navPrint_DSI(&dsi);

      } else {
	int i;
	ERROR("Unknown NAV packet type (%02x)\n", buffer[0]);
	for(i=0;i<20;i++)
	  printf("%02x ", buffer[i]);
      }
    }
    
  }
}

