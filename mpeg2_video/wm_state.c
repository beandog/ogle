#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>

#include <inttypes.h>
#include <string.h>

#include "debug_print.h"
#include "display.h"
#include "wm_state.h"


static WindowState_t current_state = WINDOW_STATE_NORMAL;

typedef struct {
  int x;
  int y;
  int width;
  int height;
} geometry_t;

static geometry_t normal_state_geometry;

static unsigned long req_serial;        /* used for error handling */
static int (*prev_xerrhandler)(Display *dpy, XErrorEvent *ev);

static void remove_motif_decorations(Display *dpy, Window win);
static void disable_motif_decorations(Display *dpy, Window win);


static int EWMH_wm;
static int gnome_wm;
static int gnome_wm_layers;
static int has_ewmh_state_fullscreen;
static char *wm_name = NULL;

static Atom wm_state_atom;


#define MWM_HINTS_DECORATIONS   (1L << 1)

typedef struct {
  uint32_t flags;
  uint32_t functions;
  uint32_t decorations;
  int32_t  input_mode;
  uint32_t status;
} mwmhints_t;

static void remove_motif_decorations(Display *dpy, Window win)
{
  Atom WM_HINTS;
  
  /* can only change decorations while unmapped ? */

  
  /* First try to set MWM hints */
  WM_HINTS = XInternAtom(dpy, "_MOTIF_WM_HINTS", True);
  if(WM_HINTS != None) {
    /* Hints used by Motif compliant window managers */
    mwmhints_t MWMHints = { MWM_HINTS_DECORATIONS, 0, 0, 0, 0 };
    
    XChangeProperty(dpy, win,
		    WM_HINTS, WM_HINTS, 32,
		    PropModeReplace,
		    (unsigned char *)&MWMHints,
		    sizeof(MWMHints)/sizeof(long));
  } else {
    WARNING("%s", "_MOTIF_WM_HINTS atom not found\n");
  }

}

static void disable_motif_decorations(Display *dpy, Window win)
{
  Atom WM_HINTS;
  
  /* can only change decorations while unmapped ? */

  
  /* First try to unset MWM hints */
  WM_HINTS = XInternAtom(dpy, "_MOTIF_WM_HINTS", True);
  if(WM_HINTS != None) {
    /* Hints used by Motif compliant window managers */
    XDeleteProperty(dpy, win, WM_HINTS);
  } else {
    WARNING("%s", "_MOTIF_WM_HINTS atom not found\n");
  }
  
}


static void calc_coords(Display *dpy, Window win, int *x, int *y, XEvent *ev)
{
  int dest_x_ret;
  int dest_y_ret;
  Window dest_win;
  
  DNOTE("configure x: %d, y: %d\n",
	ev->xconfigure.x,  ev->xconfigure.y);
  if(ev->xconfigure.send_event == True) {

    //DNOTE("send_event: True\n");

    *x = ev->xconfigure.x;
    *y = ev->xconfigure.y;
    
    XTranslateCoordinates(dpy, win, DefaultRootWindow(dpy), 
			  0,
			  0,
			  &dest_x_ret,
			  &dest_y_ret,
			  &dest_win);
    
    if(*x != dest_x_ret) {
      DNOTE("wm_x: %d, xtranslate_x: %d\n", *x, dest_x_ret);
      *x = dest_x_ret;
    }

    if(*y != dest_y_ret) {
      DNOTE("wm_y: %d, xtranslate_y: %d\n", *y, dest_y_ret);
      *y = dest_y_ret;
    }
    
  } else {
    
    //DNOTE("send_event: False\n");
    
    XTranslateCoordinates(dpy, win, DefaultRootWindow(dpy), 
			  0,
			  0,
			  &dest_x_ret,
			  &dest_y_ret,
			  &dest_win);
    DNOTE("xtranslate x: %d, y: %d\n", dest_x_ret, dest_y_ret);
    *x = dest_x_ret;
    *y = dest_y_ret;
    
  }

  //DNOTE("x: %d, y: %d\n", *x, *y);
}




static void save_normal_geometry(Display *dpy, Window win)
{

  // Ugly hack so we can reposition ourself when going from fullscreen
  
  Window root_return;
  int x,y;
  unsigned int bwidth, depth;
  int dest_x_ret, dest_y_ret;
  Window dest_win;
  
  XGetGeometry(dpy, win, &root_return, &x, &y,
	       &normal_state_geometry.width,
	       &normal_state_geometry.height,
	       &bwidth, &depth);
  
  XTranslateCoordinates(dpy, win, DefaultRootWindow(dpy), 
			0,
			0,
			&dest_x_ret,
			&dest_y_ret,
			&dest_win);
  
  normal_state_geometry.x = dest_x_ret;
  normal_state_geometry.y = dest_y_ret;
  
  DNOTE("x_ret: %d, y_ret: %d\n", dest_x_ret, dest_y_ret);
  DNOTE("normal_state_geometry: x: %d, y: %d, w: %d, h: %d, bw: %d, d: %d\n",
	x, y,
	normal_state_geometry.width,
	normal_state_geometry.height,
	bwidth, depth);
  
}


static void restore_normal_geometry(Display *dpy, Window win)
{
  XWindowChanges win_changes;
  XEvent ev, ret_ev;
  int x, y;
  int n;
  // Try to resize
  win_changes.x = normal_state_geometry.x;
  win_changes.y = normal_state_geometry.y;
  win_changes.width = normal_state_geometry.width;
  win_changes.height = normal_state_geometry.height;
  //win_changes.stack_mode = Above;
  XReconfigureWMWindow(dpy, win, 0,
		       CWX | CWY |
		       CWWidth | CWHeight,
		       &win_changes);

  
  // Wait for a configure notify
  do {
    XNextEvent(dpy, &ev);
  } while(ev.type != ConfigureNotify);
  
  // save the configure event so we can return it
  ret_ev = ev;
  
  calc_coords(dpy, win, &x, &y, &ev);

  for(n = 0; n < 10; n++) {
    while(XCheckTypedEvent(dpy, ConfigureNotify, &ev) == True) {
      ret_ev = ev;
      calc_coords(dpy, win, &x, &y, &ev);
    }
    if(abs(x-win_changes.x) < 100 &&
       abs(y-win_changes.y) < 100) {
      break;
    }
    usleep(100000);
  }
    
  //DNOTE("no more configure notify\n");
  //try to move one more time, maybe we didn't get all configure events?
  if(x != win_changes.x || y != win_changes.y) {
    DNOTE("window is not at %d, %d. Trying to move again\n",
	  win_changes.x, win_changes.y);
    
    XSync(dpy, True);
    XReconfigureWMWindow(dpy, win, 0, CWX | CWY,
			 &win_changes);

    /*    
    do {
      XNextEvent(dpy, &ev);
    } while(ev.type != ConfigureNotify);
    
    ret_ev = ev;
    
    calc_coords(dpy, win, &x, &y, &ev);
    */
    while(XCheckTypedEvent(dpy, ConfigureNotify, &ev) == True) {
      ret_ev = ev;
      calc_coords(dpy, win, &x, &y, &ev);
    }
  } 
  // ugly hack, but what can you do
  //  when we don't end up at (win_changes.x, win_changes.y)
  //  try to compensate and move one more time
  if((x != win_changes.x || y != win_changes.y) &&
     abs(x - win_changes.x) < 100 && abs(y - win_changes.y) < 100) {
    XWindowChanges win_compensate;
    win_compensate.x = win_changes.x+win_changes.x-x;
    win_compensate.y = win_changes.y+win_changes.y-y;
    DNOTE("window is not at %d, %d\n", win_changes.x, win_changes.y);
    DNOTE("Compensating by moving to x: %d= %d+%d-%d, y: %d= %d+%d-%d\n",
	  win_compensate.x, win_changes.x, win_changes.x, x,
	  win_compensate.y, win_changes.y, win_changes.y, y);
    
    XSync(dpy, True);
    XReconfigureWMWindow(dpy, win, 0, CWX | CWY,
			 &win_compensate);
    
    do {
      XNextEvent(dpy, &ev);
    } while(ev.type != ConfigureNotify);
    
    ret_ev = ev;

    calc_coords(dpy, win, &x, &y, &ev);
    
    while(XCheckTypedEvent(dpy, ConfigureNotify, &ev) == True) {
      ret_ev = ev;
      calc_coords(dpy, win, &x, &y, &ev);
    }
    
    if(x != win_changes.x || y != win_changes.y) {
      DNOTE("Couldn't place window at %d,%d\n",
	    win_changes.x, win_changes.y);
      DNOTE("Window is at %d, %d\n",
	    x, y);
    } else {
      //DNOTE("Fixed, window is now at %d,%d\n", win_changes.x, win_changes.y);
    }
    
  }
  
  XPutBackEvent(dpy, &ev);
  
}


int get_wm_state(Display *dpy, Window win)
{
  Atom actual_type_return;
  int actual_format_return;
  unsigned long nitems_return;
  unsigned long bytes_after_return;
  unsigned char* prop_return;
  int wm_state;
  
  XGetWindowProperty(dpy, win, wm_state_atom, 0, 1,
		     False, wm_state_atom,
		     &actual_type_return,
		     &actual_format_return,
		     &nitems_return,
		     &bytes_after_return,
		     &prop_return);
  if(actual_type_return == None) {
    return -1; //the property does not exist
  } else if(actual_type_return != wm_state_atom) {
    WARNING("WM_STATE is not of type WM_STATE: %lu\n", 
	    (unsigned long)actual_type_return);
    return -1; //wrong property type
  } else {
    wm_state = (int)(((unsigned long *)prop_return)[0]);
    XFree(prop_return);
    return wm_state;
  }
}  

static Bool withdraw_predicate(Display *dpy, XEvent *ev, XPointer arg)
{
  if(ev->type == UnmapNotify ||
     ev->type == ReparentNotify ||
     ev->type == PropertyNotify) {
    return True;
  } else {
    return False;
  }
}


void withdraw_window(Display *dpy, Window win)
{
  XEvent ev;
  int n;
  int wm_state;
  int wait_for_wm_state = 1;
  int wait_for_reparent = 1;
  int wait_for_unmap = 1;
  Window root_return, parent_return;
  Window *children_return = NULL;
  unsigned int nchildren_return;


  wm_state = get_wm_state(dpy, win);
  if(wm_state <= 0) {
    // window is already withdrawn or no wm (supporting wm_state) is running
    wait_for_wm_state = 0;
  }

  if(XQueryTree(dpy, win, &root_return, &parent_return,
		&children_return, &nchildren_return)) {
    if(root_return == parent_return) {
      //no wm running or the wm is not reparenting
      // if the wm is not reparenting it should support wm_state
      wait_for_reparent = 0;
    }
    if(children_return) {
      XFree(children_return);
    }
  } else {
    WARNING("%s", "XQueryTree failed\n");
    root_return = DefaultRootWindow(dpy);
  }
  
  XWithdrawWindow(dpy, win, DefaultScreen(dpy));
  
  // Wait for window to be unmapped/withdrawn
  // If a wm is running wait for WM_STATE==withdrawn
  // or a reparent event (only works for a reparenting wm)
  // to know when the wm is done with our window
  

  // Always wait until we get an unmap notification.
  // Also wait ~0.5s for reparent/wmstate==withdraw else continue
  // and assume the wm is broken
  for(n = 0;
      wait_for_unmap || (n < 50 && (wait_for_reparent || wait_for_wm_state));
      n++) {
    
    while(XCheckIfEvent(dpy, &ev, withdraw_predicate, NULL) == True) {
      switch(ev.type) {
      case UnmapNotify:
	wait_for_unmap = 0;
	break;
      case ReparentNotify:
	if(ev.xreparent.parent == root_return) {
	  wait_for_reparent = 0;
	  //we don't have to wait for wmstate also
	  //wait_for_wm_state = 0;
	}
	break;
      case PropertyNotify:
	if(ev.xproperty.atom == wm_state_atom) {
	  wm_state = get_wm_state(dpy, win);
	  if(wm_state <= 0) {
	    //window has been withdrawn
	    wait_for_wm_state = 0;
	    //don't wait for reparent now
	    wait_for_reparent = 0;
	  }
	}
      default:
	break;
      }
    }
    usleep(10000);
  }
  if(n == 50) {
    DNOTE("%s", "Withdraw window timeout, broken WM ?\n");
  }
}


static void switch_to_fullscreen_state(Display *dpy, Window win)
{
  XEvent ev;
  XEvent ret_ev;
  XWindowAttributes attrs;
  XWindowChanges win_changes;
  int x, y;
  int n;
  XSizeHints *sizehints;
  // We don't want to have to replace the window manually when remapping it
  sizehints = XAllocSizeHints();
  sizehints->flags = USPosition | PWinGravity;
  sizehints->x = 0; // obsolete but should be set in case
  sizehints->y = 0; // there is an old wm used
  sizehints->win_gravity = NorthWestGravity;
  save_normal_geometry(dpy, win);
  
  if(!has_ewmh_state_fullscreen) {

    // We have to be unmapped to change motif decoration hints 
    
    withdraw_window(dpy, win);
    
    remove_motif_decorations(dpy, win);
    
    XSetWMNormalHints(dpy, win, sizehints);
    XFree(sizehints);

    XMapWindow(dpy, win);
    
    /* wait for the window to be mapped */
    
    do {
      XNextEvent(dpy, &ev);
    } while(ev.type != MapNotify);
    

    /* remove any outstanding configure_notifies */
    XSync(dpy, True);
    
    
    
    // Try to resize, place the window at correct place and on top
    
    
    XGetWindowAttributes(dpy, win, &attrs);
    
    DpyInfoGetScreenOffset(dpy, XScreenNumberOfScreen(attrs.screen),
			   &win_changes.x, &win_changes.y);
    
    DpyInfoGetResolution(dpy, XScreenNumberOfScreen(attrs.screen),
			 &win_changes.width, &win_changes.height);
    
    win_changes.stack_mode = Above;
    DNOTE("Placing window at %d,%d with size %d,%d\n",
	  win_changes.x, win_changes.y,
	  win_changes.width, win_changes.height);

    XReconfigureWMWindow(dpy, win, 0, CWX | CWY |
			 CWWidth | CWHeight |
			 CWStackMode,
			 &win_changes);
    
    
    
    
    
    //todo: check if these are correct and how to detect a kwm
#if 0
    
    /* Now try to set KWM hints */
    WM_HINTS = XInternAtom(mydisplay, "KWM_WIN_DECORATION", True);
    if(WM_HINTS != None) {
      long KWMHints = 0;
      
      XChangeProperty(mydisplay, window.win,
		      WM_HINTS, WM_HINTS, 32,
		      PropModeReplace,
		      (unsigned char *)&KWMHints,
		      sizeof(KWMHints)/sizeof(long));
      found_wm = 1;
    }
    
    /* Now try to unset KWM hints */
    WM_HINTS = XInternAtom(mydisplay, "KWM_WIN_DECORATION", True);
    if(WM_HINTS != None) {
      XDeleteProperty(mydisplay, window.win, WM_HINTS);
      found_wm = 1;
    }
    
    
#endif
    
    // If we have a gnome compliant wm that supports layers, put
    // us above the dock/panel
    
    if(gnome_wm_layers) {
      XEvent ev;
      ev.type = ClientMessage;
      ev.xclient.window = win;
      ev.xclient.message_type = XInternAtom(dpy, "_WIN_LAYER", True);
      ev.xclient.format = 32;
      ev.xclient.data.l[0] = 10;
      XSendEvent(dpy, DefaultRootWindow(dpy), False,
		 SubstructureNotifyMask, &ev);
    } else {
      if(wm_name != NULL) {
	if(strcmp(wm_name, "KWin") == 0) {
	  XEvent ev;
	  ev.type = ClientMessage;
	  ev.xclient.window = win;
	  ev.xclient.message_type = XInternAtom(dpy, "_NET_WM_STATE", True);
	  ev.xclient.format = 32;
	  ev.xclient.data.l[0] = 1; // _NET_WM_STATE_ADD
	  ev.xclient.data.l[1] = XInternAtom(dpy, "_NET_WM_STATE_STAYS_ON_TOP", False);
	  ev.xclient.data.l[2] = 0;
	  
	  XSendEvent(dpy, DefaultRootWindow(dpy), False,
		     SubstructureNotifyMask, &ev);
	}
      }
    }
    
    // Wait for a configure notify
    do {
      XNextEvent(dpy, &ev);
    } while(ev.type != ConfigureNotify);
    
    // save the configure event so we can return it
    ret_ev = ev;
    
    calc_coords(dpy, win, &x, &y, &ev);

    for(n = 0; n < 10; n++) {
      while(XCheckTypedEvent(dpy, ConfigureNotify, &ev) == True) {
	ret_ev = ev;
	calc_coords(dpy, win, &x, &y, &ev);
      }
      if(abs(x-win_changes.x) < 100 &&
	 abs(y-win_changes.y) < 100) {
	break;
      }
      usleep(100000);
    }
    
    //DNOTE("no more configure notify\n");
    //try to move one more time, maybe we didn't get all configure events?
    if(x != win_changes.x || y != win_changes.y) {
      DNOTE("%s", "window is not at screen start. Trying to move again\n");
      
      XSync(dpy, True);
      XReconfigureWMWindow(dpy, win, 0, CWX | CWY,
			   &win_changes);

      /*      
      do {
	XNextEvent(dpy, &ev);
      } while(ev.type != ConfigureNotify);
      
      ret_ev = ev;
      
      calc_coords(dpy, win, &x, &y, &ev);
      */
      while(XCheckTypedEvent(dpy, ConfigureNotify, &ev) == True) {
	ret_ev = ev;
	calc_coords(dpy, win, &x, &y, &ev);
      }
    } 
    // ugly hack, but what can you do when the wm's not removing decorations
    //  if we don't end up at (win_changes.x, win_changes.y)
    //  try to compensate and move one more time


    if((x != win_changes.x || y != win_changes.y) &&
       abs(x - win_changes.x) < 100 && abs(y - win_changes.y) < 100) {
      XWindowChanges win_compensate;
      win_compensate.x = win_changes.x+win_changes.x-x;
      win_compensate.y = win_changes.y+win_changes.y-y;
      DNOTE("%s", "window is not at screen start\n");
      DNOTE("Compensating by moving to x: %d= %d+%d-%d, y: %d= %d+%d-%d\n",
	    win_compensate.x, win_changes.x, win_changes.x, x,
	    win_compensate.y, win_changes.y, win_changes.y, y);
      
      XSync(dpy, True);
      XReconfigureWMWindow(dpy, win, 0, CWX | CWY,
			   &win_compensate);
      
      do {
	XNextEvent(dpy, &ev);
      } while(ev.type != ConfigureNotify);
      
      ret_ev = ev;
      
      calc_coords(dpy, win, &x, &y, &ev);
      
      while(XCheckTypedEvent(dpy, ConfigureNotify, &ev) == True) {
	ret_ev = ev;
	calc_coords(dpy, win, &x, &y, &ev);
      }
      
      if(x != win_changes.x || y != win_changes.y) {
	DNOTE("Couldn't place window at %d,%d\n",
	      win_changes.x, win_changes.y);
	DNOTE("Window is at %d, %d\n",
	      x, y);
      } else {
	//DNOTE("Fixed, window is now at %d,%d\n", win_changes.x, win_changes.y);
      }
      
    }
    
    
    XPutBackEvent(dpy, &ret_ev);
  } else {
    // ewmh_state_fullscreen is supported
    XEvent ev;
    
    ev.type = ClientMessage;
    ev.xclient.window = win;
    ev.xclient.message_type = XInternAtom(dpy, "_NET_WM_STATE", False);
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = 1; // _NET_WM_STATE_ADD not an atom just a define
    ev.xclient.data.l[1] = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
    ev.xclient.data.l[2] = 0;
    
    XSendEvent(dpy, DefaultRootWindow(dpy), False,
	       SubstructureNotifyMask, &ev);
    
    
  }
  
  current_state = WINDOW_STATE_FULLSCREEN;
  
}


static void switch_to_normal_state(Display *dpy, Window win)
{
  XEvent ev;
  XSizeHints *sizehints;

  if(!has_ewmh_state_fullscreen) {
    // We don't want to have to replace the window manually when remapping it
    sizehints = XAllocSizeHints();
    sizehints->flags = USPosition;
    sizehints->x = 0; // obsolete but should be set in case
    sizehints->y = 0; // an old wm is used
    
    // We have to be unmapped to change motif decoration hints 

    withdraw_window(dpy, win);
    
    disable_motif_decorations(dpy, win);
    
    XSetWMNormalHints(dpy, win, sizehints);
    XFree(sizehints);

    XMapWindow(dpy, win);
    
    /* wait for the window to be mapped */
    
    do {
      XNextEvent(dpy, &ev);
    } while(ev.type != MapNotify);
    
    /* remove any outstanding configure_notifies */
    XSync(dpy, True);
    
    
    
    // get us back to the position and size we had before fullscreen
    restore_normal_geometry(dpy, win);
    
    // If we have a gnome compliant wm that supports layers, put
    // us in the normal layer
    
    if(gnome_wm_layers) {
      XEvent ev;
      ev.type = ClientMessage;
      ev.xclient.window = win;
      ev.xclient.message_type = XInternAtom(dpy, "_WIN_LAYER", True);
      ev.xclient.format = 32;
      ev.xclient.data.l[0] = 4;
      XSendEvent(dpy, DefaultRootWindow(dpy), False,
		 SubstructureNotifyMask, &ev);
    } else {
      if(wm_name != NULL) {
	if(strcmp(wm_name, "KWin") == 0) {
	  XEvent ev;
	  ev.type = ClientMessage;
	  ev.xclient.window = win;
	  ev.xclient.message_type = XInternAtom(dpy, "_NET_WM_STATE", True);
	  ev.xclient.format = 32;
	  ev.xclient.data.l[0] = 0; // _NET_WM_STATE_REMOVE
	  ev.xclient.data.l[1] = XInternAtom(dpy, "_NET_WM_STATE_STAYS_ON_TOP", False);
	  ev.xclient.data.l[2] = 0;
	  
	  XSendEvent(dpy, DefaultRootWindow(dpy), False,
		     SubstructureNotifyMask, &ev);
	}
      }
    }
  } else {
    // ewmh_state_fullscreen is supported
    XEvent ev;
    
    ev.type = ClientMessage;
    ev.xclient.window = win;
    ev.xclient.message_type = XInternAtom(dpy, "_NET_WM_STATE", False);
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = 0; //_NET_WM_STATE_REMOVE not an atom just a define
    ev.xclient.data.l[1] = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
    ev.xclient.data.l[2] = 0;
    
    XSendEvent(dpy, DefaultRootWindow(dpy), False,
	       SubstructureNotifyMask, &ev);
  }
  current_state = WINDOW_STATE_NORMAL;
}

static int xprop_errorhandler(Display *dpy, XErrorEvent *ev)
{
  if(ev->serial == req_serial) {
    /* this is an error to the XGetWindowProperty request
     * most probable the window specified by the property
     * _WIN_SUPPORTING_WM_CHECK on the root window no longer exists
     */
    WARNING("%s", "xprop_errhandler: error in XGetWindowProperty\n");
    return 0;
  } else {
    /* if we get another error we should handle it,
     * so we give it to the previous errorhandler
     */
    ERROR("%s", "xprop_errhandler: unexpected error\n");
    return prev_xerrhandler(dpy, ev);
  }
}

static int check_for_gnome_wm(Display *dpy)
{
  Atom atom;
  Atom type_return;
  int format_return;
  unsigned long nitems_return;
  unsigned long bytes_after_return;
  unsigned char *prop_return = NULL;
  Window win_id;
  int status;

  atom = XInternAtom(dpy, "_WIN_SUPPORTING_WM_CHECK", True);
  if(atom == None) {
    return 0;
  }
  
  if(XGetWindowProperty(dpy, DefaultRootWindow(dpy), atom, 0,
		     1, False, XA_CARDINAL,
		     &type_return, &format_return, &nitems_return,
			&bytes_after_return, &prop_return) != Success) {
    WARNING("%s", "XGetWindowProperty failed in check_for_gnome\n");
    return 0;
  }
  
  if(type_return == None) {
    DNOTE("%s", "check_for_gnome: _WIN_SUPPORTING_WM_CHECK does not exist\n");
    return 0;
  }

  if(type_return != XA_CARDINAL) {
    WARNING("%s", "check_for_gnome: _WIN_SUPPORTING_WM_CHECK has wrong type\n");
    if(prop_return != NULL) {
      XFree(prop_return);
    }
    return 0;
  }

  if(type_return == XA_CARDINAL) {
    //DNOTE("check_for_gnome: format: %d\n", format_return);
    //DNOTE("check_for_gnome: nitmes: %ld\n", nitems_return);
    //DNOTE("check_for_gnome: bytes_after: %ld\n", bytes_after_return);
    if(format_return == 32 && nitems_return == 1 && bytes_after_return == 0) {

      win_id = *(long *)prop_return;

      //DNOTE("win_id: %ld\n", win_id);

      XFree(prop_return);
      prop_return = NULL;
      
      /* make sure we don't have any unhandled errors */
      XSync(dpy, False);

      /* set error handler so we can check if XGetWindowProperty failed */
      prev_xerrhandler = XSetErrorHandler(xprop_errorhandler);

      /* get the serial of the XGetWindowProperty request */
      req_serial = NextRequest(dpy);

      /* try to get property
       * this can fail if we have a property with the win_id on the
       * root window, but the win_id is no longer valid.
       * example: we have had a gnome wm running but are now running
       * a non gnome wm.
       * question: shouldn't the gnome wm remove the property from
       * the root window when exiting ? bug in sawfish?
       */

      status = XGetWindowProperty(dpy, win_id, atom, 0,
				  1, False, XA_CARDINAL,
				  &type_return, &format_return, &nitems_return,
				  &bytes_after_return, &prop_return);

      /* make sure XGetWindowProperty has been processed and any errors
	 have been returned to us */
      XSync(dpy, False);
      
      /* revert to the previous xerrorhandler */
      XSetErrorHandler(prev_xerrhandler);

      if(status != Success) {
	WARNING("%s", "XGetWindowProperty failed in check_for_gnome\n");
	return 0;
      }
      
      if(type_return == None) {
	DNOTE("%s", "check_for_gnome: _WIN_SUPPORTING_WM_CHECK does not exist in specified win\n");
	return 0;
      }
      
      if(type_return != XA_CARDINAL) {
	WARNING("%s", "check_for_gnome: property has wrong type\n");
	if(prop_return != NULL) {
	  XFree(prop_return);
	}
	return 0;
      }
      
      if(type_return == XA_CARDINAL) {
	//DNOTE("check_for_gnome: format: %d\n", format_return);
	//DNOTE("check_for_gnome: nitmes: %ld\n", nitems_return);
	//DNOTE("check_for_gnome: bytes_after: %ld\n", bytes_after_return);
	if(format_return == 32 && nitems_return == 1 &&
	   bytes_after_return == 0) {

	  //DNOTE("win_id: %ld\n", *(long *)prop_return);

	  if(win_id == *(long *)prop_return) {
	    // We have successfully detected a GNOME compliant Window Manager
	    XFree(prop_return);
	    return 1;
	  }
	}
	XFree(prop_return);
	return 0;
      }
    }
    XFree(prop_return);
    return 0;
  }
  return 0;
}


/* returns 1 if a window manager compliant to the
 * Extended Window Manager Hints (EWMH) spec is running.
 * (version 1.2)
 * Oterhwise returns 0.
 */

static int check_for_EWMH_wm(Display *dpy, char **wm_name_return)
{
  Atom atom;
  Atom type_return;
  Atom type_utf8;
  int format_return;
  unsigned long nitems_return;
  unsigned long bytes_after_return;
  unsigned char *prop_return = NULL;
  Window win_id;
  int status;

  atom = XInternAtom(dpy, "_NET_SUPPORTING_WM_CHECK", True);
  if(atom == None) {
    return 0;
  }

  
  
  
  
  if(XGetWindowProperty(dpy, DefaultRootWindow(dpy), atom, 0,
			1, False, XA_WINDOW,
			&type_return, &format_return, &nitems_return,
			&bytes_after_return, &prop_return) != Success) {
    WARNING("%s", "XGetWindowProperty failed in check_for_EWMH\n");
    return 0;
  }
  
  if(type_return == None) {
    WARNING("%s", "check_for_EWMH: property does not exist\n");
    return 0;
  }

  if(type_return != XA_WINDOW) {
    
    WARNING("%s", "check_for_EWMH: XA_WINDOW property has wrong type\n");
    if(prop_return != NULL) {
      XFree(prop_return);
    }
    return 0;
  } else {
    
    //DNOTE("check_for_EWMH: format: %d\n", format_return);
    //DNOTE("check_for_EWMH: nitmes: %ld\n", nitems_return);
    //DNOTE("check_for_EWMH: bytes_after: %ld\n", bytes_after_return);
    if(format_return == 32 && nitems_return == 1 && bytes_after_return == 0) {

      win_id = *(long *)prop_return;

      //DNOTE("win_id: %ld\n", win_id);

      XFree(prop_return);
      prop_return = NULL;
      
      /* make sure we don't have any unhandled errors */
      XSync(dpy, False);

      /* set error handler so we can check if XGetWindowProperty failed */
      prev_xerrhandler = XSetErrorHandler(xprop_errorhandler);

      /* get the serial of the XGetWindowProperty request */
      req_serial = NextRequest(dpy);

      /* try to get property
       * this can fail if we have a property with the win_id on the
       * root window, but the win_id is no longer valid.
       * This is to check for a stale window manager
       */
      
      status = XGetWindowProperty(dpy, win_id, atom, 0,
				  1, False, XA_WINDOW,
				  &type_return, &format_return, &nitems_return,
				  &bytes_after_return, &prop_return);
      
      /* make sure XGetWindowProperty has been processed and any errors
	 have been returned to us */
      XSync(dpy, False);
      
      /* revert to the previous xerrorhandler */
      XSetErrorHandler(prev_xerrhandler);
      
      if(status != Success) {
	WARNING("%s", "XGetWindowProperty failed in check_for_EWMH\n");
	return 0;
      }
      
      if(type_return == None) {
	WARNING("%s", "check_for_EWMH: property does not exist\n");
	return 0;
      }
      
      if(type_return != XA_WINDOW) {
	WARNING("check_for_EWMH: property has wrong type (%ld)\n",
		type_return);
	if(prop_return != NULL) {
	  XFree(prop_return);
	}
	return 0;
      }
      
      if(type_return == XA_WINDOW) {
	//DNOTE("check_for_EWMH: format: %d\n", format_return);
	//DNOTE("check_for_EWMH: nitmes: %ld\n", nitems_return);
	//DNOTE("check_for_EWMH: bytes_after: %ld\n", bytes_after_return);

	if(format_return == 32 && nitems_return == 1 &&
	   bytes_after_return == 0) {

	  //DNOTE("win_id: %ld\n", *(long *)prop_return);

	  if(win_id == *(long *)prop_return) {
	    // We have successfully detected a EWMH compliant Window Manager
	    XFree(prop_return);


	    /* check the name of the wm */

	    atom = XInternAtom(dpy, "_NET_WM_NAME", True);
	    if(atom != None) {
	      
	      if(XGetWindowProperty(dpy, win_id, atom, 0,
				    4, False, XA_STRING,
				    &type_return, &format_return,
				    &nitems_return,
				    &bytes_after_return,
				    &prop_return) != Success) {
		
		WARNING("%s", "XGetWindowProperty failed in wm_name\n");
		return 0;
	      }
	      
	      if(type_return == None) {
		WARNING("%s", "wm_name: property does not exist\n");
		return 0;
	      }
	      
	      if(type_return != XA_STRING) {
		type_utf8 = XInternAtom(dpy, "UTF8_STRING", True);
		if(type_utf8 == None) {
		  WARNING("%s", "not UTF8_STRING either\n");
		  return 0;
		}
		
		if(type_return == type_utf8) {
		  if(XGetWindowProperty(dpy, win_id, atom, 0,
					4, False, type_utf8,
					&type_return, &format_return,
					&nitems_return,
					&bytes_after_return,
					&prop_return) != Success) {
		    
		    WARNING("%s", "XGetWindowProperty failed in wm_name\n");
		    return 0;
		  }

		  //DNOTE("wm_name: property of type UTF8_STRING\n");
		  
		  //DNOTE("wm_name: format: %d\n", format_return);
		  //DNOTE("wm_name: nitmes: %ld\n", nitems_return);
		  //DNOTE("wm_name: bytes_after: %ld\n", bytes_after_return);
		  if(format_return == 8) {
		    DNOTE("wm_name: %s\n", (char *)prop_return);
		    *wm_name_return = strdup((char *)prop_return);
		  }
		  
		}
		if(prop_return != NULL) {
		  XFree(prop_return);
		}
	      } else {
		
		//DNOTE("wm_name: format: %d\n", format_return);
		//DNOTE("wm_name: nitmes: %ld\n", nitems_return);
		//DNOTE("wm_name: bytes_after: %ld\n", bytes_after_return);
		if(format_return == 8) {
		  DNOTE("wm_name: %s\n", (char *)prop_return);
		  *wm_name_return = strdup((char *)prop_return);
		}
	        XFree(prop_return);
		/* end name check */
	      }
	    }
	    return 1;
	  }
	}
	XFree(prop_return);
	return 0;
      }
    }
    XFree(prop_return);
    return 0;
  }
  return 0;
}


/* returns 1 if a window manager compliant to the
 * Extended Window Manager Hints (EWMH) spec.
 * supports the _NET_WM_STATE_FULLSCREEN window state
 * Oterhwise returns 0.
 */

static int check_for_state_fullscreen(Display *dpy)
{
  Atom net_supported, net_wm_state, net_wm_state_fullscreen;
  Atom type_return;
  int format_return;
  unsigned long nitems_return;
  unsigned long bytes_after_return;
  unsigned char *prop_return = NULL;
  int nr_items = 40;
  int item_offset = 0;
  int supports_net_wm_state = 0;
  int supports_net_wm_state_fullscreen = 0;

  net_supported = XInternAtom(dpy, "_NET_SUPPORTED", False);
  if(net_supported == None) {
    return 0;
  }
  
  net_wm_state = XInternAtom(dpy, "_NET_WM_STATE", False);
  if(net_wm_state == None) {
    return 0;
  }

  net_wm_state_fullscreen = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN",
					False);
  if(net_wm_state_fullscreen == None) {
    return 0;
  }

  
  
  
  do {
    if(XGetWindowProperty(dpy, DefaultRootWindow(dpy), net_supported,
			  item_offset, nr_items, False, XA_ATOM,
			  &type_return, &format_return, &nitems_return,
			  &bytes_after_return, &prop_return) != Success) {
      WARNING("%s", "XGetWindowProperty failed in check_for_state_fullscreen\n");
      return 0;
    }
    
    if(type_return == None) {
      WARNING("%s", "check_for_state_fullscreen: property does not exist\n");
      return 0;
    }
    
    if(type_return != XA_ATOM) {
      
      WARNING("%s", "check_for_state_fullscreen: XA_ATOM property has wrong type\n");
      if(prop_return != NULL) {
	XFree(prop_return);
      }
      return 0;
    } else {
      // _NET_SUPPORTED is set, lets see what is in it
      
      //DNOTE("check_for_state_fullscreen: format: %d\n", format_return);
      //DNOTE("check_for_state_fullscreen: nitmes: %ld\n", nitems_return);
      //DNOTE("check_for_state_fullscreen: bytes_after: %ld\n", bytes_after_return);
      
      if(format_return == 32) {
	int n;
	
	for(n = 0; n < nitems_return; n++) {
	  if(((long *)prop_return)[n] == net_wm_state) {
	    supports_net_wm_state = 1;
	  } else if(((long *)prop_return)[n] == net_wm_state_fullscreen) {
	    supports_net_wm_state_fullscreen = 1;
	  }
	  
	  if(supports_net_wm_state && supports_net_wm_state_fullscreen) {
	    XFree(prop_return);
	    prop_return = NULL;
	    
	    return 1;
	  }
	}
	
	
	XFree(prop_return);
	prop_return = NULL;
      
	
	
      } else {
	XFree(prop_return);
	return 0;
      }
    }
    
    item_offset+= nr_items;
    
  } while(bytes_after_return > 0);
  return 0;

}


static int check_for_gnome_wm_layers(Display *dpy)
{
  Atom layer_atom;
  Atom atom;
  Atom type_return;
  int format_return;
  unsigned long nitems_return;
  unsigned long bytes_after_return;
  unsigned char *prop_return = NULL;
  int index = 0;
  
  atom = XInternAtom(dpy, "_WIN_PROTOCOLS", True);
  if(atom == None) {
    WARNING("%s", "no atom: _WIN_PROTOCOLS");
    return 0;
  }
  
  layer_atom = XInternAtom(dpy, "_WIN_LAYER", True);
  if(layer_atom == None) {
    WARNING("%s", "no atom: _WIN_LAYER");
    return 0;
  }
  
  do {
  
    if(XGetWindowProperty(dpy, DefaultRootWindow(dpy), atom, index,
			  1, False, XA_ATOM,
			  &type_return, &format_return, &nitems_return,
			  &bytes_after_return, &prop_return) != Success) {
      WARNING("%s", "XGetWindowProperty failed in check_for_gnome\n");
      return 0;
    }
    index++;
    
    if(type_return == None) {
      WARNING("%s", "check_for_layer: property does not exist\n");
      return 0;
    }
    
    if(type_return != XA_ATOM) {
      WARNING("%s", "check_for_layer: property has wrong type\n");
      if(prop_return != NULL) {
	XFree(prop_return);
      }
      return 0;
    }
    
    if(type_return == XA_ATOM) {
      //DNOTE("check_for_layer: format: %d\n", format_return);
      //DNOTE("check_for_layer: nitmes: %ld\n", nitems_return);
      //DNOTE("check_for_layer: bytes_after: %ld\n", bytes_after_return);
      if(format_return == 32 &&
	 nitems_return == 1) {

	if(layer_atom == *(long *)prop_return) {
	  // we found _WIN_LAYER
	  
	  //DNOTE("_WIN_LAYER found\n");
	  XFree(prop_return);
	  return 1;
	}
      }
      XFree(prop_return);
    }
  } while(bytes_after_return != 0);
  return 0;
}

void init_wm_state(Display *dpy, Window win)
{
  static int initialized = 0;
  
  if(initialized) {
    return;
  }
  
  wm_state_atom = XInternAtom(dpy, "WM_STATE", False);
  initialized = 1;
  
}

int ChangeWindowState(Display *dpy, Window win, WindowState_t state)
{
  init_wm_state(dpy, win);

  if(state != current_state) {

    EWMH_wm = check_for_EWMH_wm(dpy, &wm_name);
    if(EWMH_wm) {
      if(DpyInfoGetEWMHFullscreen()) {
	has_ewmh_state_fullscreen = check_for_state_fullscreen(dpy);
      } else {
	has_ewmh_state_fullscreen = 0;
      }
    }
    
    gnome_wm = check_for_gnome_wm(dpy);
    
    //DNOTE("EWMH_wm: %s\n", EWMH_wm ? "True" : "False");
    //DNOTE("gnome_wm: %s\n", gnome_wm ? "True" : "False");
    
    if(gnome_wm) {
      gnome_wm_layers = check_for_gnome_wm_layers(dpy);
    }
    
    switch(state) {
    case WINDOW_STATE_FULLSCREEN:
      switch_to_fullscreen_state(dpy, win);
      break;
    case WINDOW_STATE_NORMAL:
      switch_to_normal_state(dpy, win);
      break;
    default:
      ERROR("%s", "unknown window state\n");
      break;
    }
    if(wm_name != NULL) {
      free(wm_name);
      wm_name = NULL;
    }
  }
  
  return 1;
  
}




