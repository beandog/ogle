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
#ifdef HAVE_XF86VIDMODE
#include <X11/extensions/xf86vmode.h>
#endif
#ifdef HAVE_XINERAMA
#include <X11/extensions/Xinerama.h>
#endif

#include "display.h"
#include "debug_print.h"


typedef struct {
  int horizontal_pixels;
  int vertical_pixels;
} resolution_t;

typedef struct {
  int width;
  int height;
} geometry_t;

typedef struct {
  resolution_t src_res;
  resolution_t disp_res;
} fullscreen_resolution_t;

typedef struct {
  int x;
  int y;
} position_t;

typedef struct {
  position_t screen_offset;
  resolution_t resolution;
  geometry_t geometry;
  resolution_t user_resolution;
  geometry_t user_geometry;

  int sar_frac_n;  /* Display (i.e. monitor) sample aspect ratio. */
  int sar_frac_d;  /* calculated in update_geometry and update_resolution */

  DpyInfoOrigin_t geometry_origin;
  DpyInfoOrigin_t resolution_origin;
  resolution_t normal_dpy_res;
  fullscreen_resolution_t *fullscreen_dpy_res;
  int ewmh_fullscreen;
} dpy_info_t;

dpy_info_t dpyinfo;


int DpyInfoGetScreenOffset(Display *dpy, int screen_nr,
			   int *x, int *y)
{
  *x = dpyinfo.screen_offset.x;
  *y = dpyinfo.screen_offset.y;

  return 1;
}


int DpyInfoGetResolution(Display *dpy, int screen_nr,
			 int *horizontal_pixels,
			 int *vertical_pixels)
{
  *horizontal_pixels = dpyinfo.resolution.horizontal_pixels;
  *vertical_pixels = dpyinfo.resolution.vertical_pixels;

  return 1;
}


int DpyInfoGetGeometry(Display *dpy, int screen_nr,
		       int *width,
		       int *height)
{
  *width = dpyinfo.geometry.width;
  *height = dpyinfo.geometry.height;

  return 1;
}


int DpyInfoGetSAR(Display *dpy, int screen_nr,
		  int *sar_frac_n, int *sar_frac_d)
{
  if(dpyinfo.sar_frac_n > 0 && dpyinfo.sar_frac_d > 0) {
    *sar_frac_n = dpyinfo.sar_frac_n;
    *sar_frac_d = dpyinfo.sar_frac_d;
    return 1;
  }
  
  return 0;
}


int DpyInfoSetUserResolution(Display *dpy, int screen_nr,
			     int horizontal_pixels,
			     int vertical_pixels)
{
  if(horizontal_pixels > 0 && vertical_pixels > 0) {
    dpyinfo.user_resolution.horizontal_pixels = horizontal_pixels;
    dpyinfo.user_resolution.vertical_pixels = vertical_pixels;
    return 1;
  }

  return 0;
}


int DpyInfoSetUserGeometry(Display *dpy, int screen_nr,
			   int width,
			   int height)
{
  if(width > 0 && height > 0) {
    dpyinfo.user_geometry.width = width;
    dpyinfo.user_geometry.height = height;
    return 1;
  }
  
  return 0;
}


static void update_sar(dpy_info_t *info) {
  info->sar_frac_n = info->geometry.height*info->resolution.horizontal_pixels;
  info->sar_frac_d = info->geometry.width*info->resolution.vertical_pixels;
}

static int update_resolution_user(dpy_info_t *info,
				  Display *dpy, int screen_nr)
{
  if(info->user_resolution.horizontal_pixels > 0 &&
     info->user_resolution.vertical_pixels > 0) {
    
    info->resolution.horizontal_pixels =
      info->user_resolution.horizontal_pixels;
    info->resolution.vertical_pixels =
      info->user_resolution.vertical_pixels;
    
    update_sar(info);
    
    return 1;
  }
  
  return 0;
}
 

static int update_resolution_x11(dpy_info_t *info, Display *dpy, int screen_nr)
{

  info->resolution.horizontal_pixels = DisplayWidth(dpy, screen_nr);
  info->resolution.vertical_pixels = DisplayHeight(dpy, screen_nr);
  
  update_sar(info);
  
  return 1;
}
  

#ifdef HAVE_XF86VIDMODE
    
static int update_resolution_xf86vidmode(dpy_info_t *info, Display *dpy,
					 int screen_nr)
{
  int dotclk;
  int x, y;
  XF86VidModeModeLine modeline;

  if(XF86VidModeGetModeLine(dpy, screen_nr, &dotclk, &modeline)) {
    if(modeline.privsize != 0) {
      XFree(modeline.private);
    }
    info->resolution.horizontal_pixels = modeline.hdisplay;
    info->resolution.vertical_pixels = modeline.vdisplay;

    if(XF86VidModeGetViewPort(dpy, screen_nr, &x, &y)) {
      info->screen_offset.x = x;
      info->screen_offset.y = y;
    }
    
    update_sar(info);
    
    return 1;
  } 
  
  return 0;
}

#endif


#ifdef HAVE_XINERAMA
    
static int update_resolution_xinerama(dpy_info_t *info, Display *dpy,
				      int x, int y)
{
  XineramaScreenInfo *xinerama_screens;
  XineramaScreenInfo *s;
  int nr_xinerama_screens;
  
  xinerama_screens = XineramaQueryScreens(dpy, &nr_xinerama_screens);
  
  if(nr_xinerama_screens > 0) {
    int n;
    for(n = 0; n < nr_xinerama_screens; n++) {
      s = &xinerama_screens[n];
      if((s->x_org <= x) && (x < (s->x_org + s->width)) &&
	 (s->y_org <= y) && (y < (s->y_org + s->height))) {
	// we have found our screen
	
	info->screen_offset.x = s->x_org;
	info->screen_offset.y = s->y_org;

	info->resolution.horizontal_pixels = s->width;
	info->resolution.vertical_pixels = s->height;
    
	update_sar(info);
	XFree(xinerama_screens);
	return 1;
      }
    }
    // the window isn't positioned on a screen, use the first
    // TODO maybe this should be fixed to use the closest instead?
    s = &xinerama_screens[0];

    info->screen_offset.x = s->x_org;
    info->screen_offset.y = s->y_org;

    info->resolution.horizontal_pixels = s->width;
    info->resolution.vertical_pixels = s->height;
    
    update_sar(info);
    XFree(xinerama_screens);
    return 1;
  } 
  
  return 0;
}

#endif


int DpyInfoUpdateResolution(Display *dpy, int screen_nr, int x, int y)
{
  int ret;
  
  switch(dpyinfo.resolution_origin) {
  case DpyInfoOriginX11:
    ret = update_resolution_x11(&dpyinfo, dpy, screen_nr);
    break;
#ifdef HAVE_XF86VIDMODE
  case DpyInfoOriginXF86VidMode:
    ret = update_resolution_xf86vidmode(&dpyinfo, dpy, screen_nr);
    break;
#endif
  case DpyInfoOriginUser:
    ret = update_resolution_user(&dpyinfo, dpy, screen_nr);
    break;
#ifdef HAVE_XINERAMA
  case DpyInfoOriginXinerama:
    ret = update_resolution_xinerama(&dpyinfo, dpy, x, y);
    break;
#endif
  default:
    return 0;
    break;
  }

  return ret;
}


static int update_geometry_user(dpy_info_t *info, Display *dpy, int screen_nr)
{
  if(info->user_geometry.width > 0 && 
     info->user_geometry.height > 0) {
    info->geometry.width = info->user_geometry.width;
    info->geometry.height = info->user_geometry.height;

    update_sar(info);
    
    return 1;
  }
  
  return 0;
}
 

static int update_geometry_x11(dpy_info_t *info, Display *dpy, int screen_nr)
{

  info->geometry.width = DisplayWidthMM(dpy, screen_nr);
  info->geometry.height = DisplayHeightMM(dpy, screen_nr);
  
  update_sar(info);
  
  return 1;
}


int DpyInfoUpdateGeometry(Display *dpy, int screen_nr)
{
  int ret;
  
  switch(dpyinfo.geometry_origin) {
  case DpyInfoOriginX11:
    ret = update_geometry_x11(&dpyinfo, dpy, screen_nr);
    break;
  case DpyInfoOriginUser:
    ret = update_geometry_user(&dpyinfo, dpy, screen_nr);
    break;
  default:
    return 0;
    break;
  }

  return ret;
}



int DpyInfoInit(Display *dpy, int screen_nr)
{
  dpyinfo.screen_offset.x = 0;
  dpyinfo.screen_offset.y = 0;
  dpyinfo.resolution.horizontal_pixels = 0;
  dpyinfo.resolution.vertical_pixels = 0;
  dpyinfo.geometry.width = 0;
  dpyinfo.geometry.height = 0;

  dpyinfo.user_resolution.horizontal_pixels = 0;
  dpyinfo.user_resolution.vertical_pixels = 0;
  dpyinfo.user_geometry.width = 0;
  dpyinfo.user_geometry.height = 0;

  dpyinfo.resolution_origin = DpyInfoOriginX11;
  dpyinfo.geometry_origin = DpyInfoOriginX11;
  dpyinfo.ewmh_fullscreen = 1;

  return 1;
}



DpyInfoOrigin_t DpyInfoSetUpdateGeometry(Display *dpy, int screen_nr,
					   DpyInfoOrigin_t origin)
{
  switch(origin) {
  case DpyInfoOriginUser:
    if(update_geometry_user(&dpyinfo, dpy, screen_nr)) {
      dpyinfo.geometry_origin = DpyInfoOriginUser;
      return dpyinfo.geometry_origin;
    }
  case DpyInfoOriginX11:
  default:
    if(update_geometry_x11(&dpyinfo, dpy, screen_nr)) {
      dpyinfo.geometry_origin = DpyInfoOriginX11;
      return dpyinfo.geometry_origin;
    }
  }
  
  return 0;
}


DpyInfoOrigin_t DpyInfoSetUpdateResolution(Display *dpy, int screen_nr,
					     DpyInfoOrigin_t origin)
{
#if defined(HAVE_XINERAMA) || defined(HAVE_XF86VIDMODE)
  int event_base, error_base;
#endif

  switch(origin) {
  case DpyInfoOriginUser:
    if(update_resolution_user(&dpyinfo, dpy, screen_nr)) {
      dpyinfo.resolution_origin = DpyInfoOriginUser;
      return dpyinfo.resolution_origin;
    }
  case DpyInfoOriginXinerama:
#ifdef HAVE_XINERAMA
    if(XineramaQueryExtension(dpy, &event_base, &error_base) &&
       XineramaIsActive(dpy)) {
      if(update_resolution_xinerama(&dpyinfo, dpy, 0, 0)) {
	dpyinfo.resolution_origin = DpyInfoOriginXinerama;
	return dpyinfo.resolution_origin;
      }
    } else {
      NOTE("%s", "Xinerama extension not found/active\n");
    }
#endif
  case DpyInfoOriginXF86VidMode:
#ifdef HAVE_XF86VIDMODE
    if(XF86VidModeQueryExtension(dpy, &event_base, &error_base)) {
      if(update_resolution_xf86vidmode(&dpyinfo, dpy, screen_nr)) {
	dpyinfo.resolution_origin = DpyInfoOriginXF86VidMode;
	return dpyinfo.resolution_origin;
      }
    } else {
      NOTE("%s", "XF86VidMode extension not found\n");
    }
#endif
  case DpyInfoOriginX11:
  default:
    if(update_resolution_x11(&dpyinfo, dpy, screen_nr)) {
      dpyinfo.resolution_origin = DpyInfoOriginX11;
      return dpyinfo.resolution_origin;
    }
  }
  
  return 0;
}

int DpyInfoSetEWMHFullscreen(int enabled)
{
  dpyinfo.ewmh_fullscreen = enabled;
  return 1;
}

int DpyInfoGetEWMHFullscreen(void)
{
  return dpyinfo.ewmh_fullscreen;
}
