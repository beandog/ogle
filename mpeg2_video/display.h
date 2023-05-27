#ifndef OGLE_DISPLAY_H
#define OGLE_DISPLAY_H

/* Ogle - A video player
 * Copyright (C) 2001, 2002 Björn Englund, Håkan Hjort
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

#include <X11/Xlib.h>

typedef enum {
  DpyInfoOriginX11 = 1,
  DpyInfoOriginXF86VidMode,
  DpyInfoOriginUser,
  DpyInfoOriginXinerama
} DpyInfoOrigin_t;



int DpyInfoInit(Display *dpy, int screen_nr);


int DpyInfoSetUserResolution(Display *dpy, int screen_nr,
			     int horizontal_pixels,
			     int vertical_pixels);

int DpyInfoSetUserGeometry(Display *dpy, int screen_nr,
			   int width,
			   int height);


DpyInfoOrigin_t DpyInfoSetUpdateGeometry(Display *dpy, int screen_nr,
					   DpyInfoOrigin_t origin);

DpyInfoOrigin_t DpyInfoSetUpdateResolution(Display *dpy, int screen_nr,
					     DpyInfoOrigin_t origin);


int DpyInfoUpdateResolution(Display *dpy, int screen_nr, int x, int y);

int DpyInfoUpdateGeometry(Display *dpy, int screen_nr);


int DpyInfoGetScreenOffset(Display *dpy, int screen_nr, int *x, int *y);

int DpyInfoGetResolution(Display *dpy, int screen_nr,
			 int *horizontal_pixels,
			 int *vertical_pixels);

int DpyInfoGetGeometry(Display *dpy, int screen_nr,
		       int *width,
		       int *height);

int DpyInfoGetSAR(Display *dpy, int screen_nr,
		  int *sar_frac_n, int *sar_frac_d);

int DpyInfoSetEWMHFullscreen(int enabled);
int DpyInfoGetEWMHFullscreen(void);

#endif /* OGLE_DISPLAY_H */
