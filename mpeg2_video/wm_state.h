#ifndef WM_STATE_H
#define WM_STATE_H

#include <X11/Xlib.h>

typedef enum {
  WINDOW_STATE_NORMAL,
  WINDOW_STATE_FULLSCREEN
} WindowState_t;


int ChangeWindowState(Display *dpy, Window win, WindowState_t state);

#endif
