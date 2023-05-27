/* Ogle - A video player
 * Copyright (C) 2000, 2001 Vilhelm Bergman
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

#include <jpeglib.h>
#include <setjmp.h>
#include <X11/Xlib.h>
#include "common.h"
#include "debug_print.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>

static char *new_file(void);

JSAMPLE * jpg_buffer;	/* Points to large array of R,G,B-order data */
int image_height;	/* Number of rows in image */
int image_width;	/* Number of columns in image */

GLOBAL(void)
     write_JPEG_file (char * filename, J_COLOR_SPACE type, int quality,
		      uint16_t x_density, uint16_t y_density)
{
  struct jpeg_compress_struct cinfo;
  struct jpeg_error_mgr jerr;

  FILE * outfile;		/* target file */
  JSAMPROW row_pointer[1];	/* pointer to JSAMPLE row[s] */
  int row_stride;		/* physical row width in image buffer */

  cinfo.err = jpeg_std_error(&jerr);

  jpeg_create_compress(&cinfo);

  if ((outfile = fopen(filename, "wb")) == NULL) {
    FATAL("can't open %s\n", filename);
    exit(1);
  }
  jpeg_stdio_dest(&cinfo, outfile);

  cinfo.image_width = image_width; 	/* image width and height, in pixels */
  cinfo.image_height = image_height;
  cinfo.input_components = 3;		/* # of color components per pixel */
  cinfo.in_color_space = type;          /* colorspace of input image */

  jpeg_set_defaults(&cinfo);

  cinfo.density_unit = 0;
  cinfo.X_density = x_density;
  cinfo.Y_density = y_density;

  jpeg_set_quality(&cinfo, quality, TRUE /* limit to baseline-JPEG values */);

  jpeg_start_compress(&cinfo, TRUE);

  row_stride = image_width * 3;	/* JSAMPLEs per row in jpg_buffer */

  while (cinfo.next_scanline < cinfo.image_height) {
    row_pointer[0] = & jpg_buffer[cinfo.next_scanline * row_stride];
    (void) jpeg_write_scanlines(&cinfo, row_pointer, 1);
  }
  jpeg_finish_compress(&cinfo);
  fclose(outfile);
  jpeg_destroy_compress(&cinfo);
}

struct my_error_mgr {
  struct jpeg_error_mgr pub;	/* "public" fields */

  jmp_buf setjmp_buffer;	/* for return to caller */
};

typedef struct my_error_mgr * my_error_ptr;

METHODDEF(void)
     my_error_exit (j_common_ptr cinfo)
{
  /* cinfo->err really points to a my_error_mgr struct, so coerce pointer */
  my_error_ptr myerr = (my_error_ptr) cinfo->err;

  /* Always display the message. */
  /* We could postpone this until after returning, if we chose. */
  (*cinfo->err->output_message) (cinfo);

  /* Return control to the setjmp point */
  longjmp(myerr->setjmp_buffer, 1);
}



void screenshot_rgb_jpg(unsigned char *data,
			unsigned int width, unsigned int height,
			int sar_frac_n, int sar_frac_d) {
  int x,y;
  char *file = new_file(); //support for progressive screenshots

  image_height = height; /* Number of rows in image */
  image_width  = width;	/* Number of columns in image */  

  
  DNOTE("%s", "screenshot_rgb_jpg()\n");
  jpg_buffer = (JSAMPLE*)malloc(sizeof(JSAMPLE)
				* image_height * image_width * 3);
  
  if(jpg_buffer == NULL) {
    FATAL("%s", "FEL!\n");
    exit(1);
  }
  
  for(y=0; y < image_height ; y++) {
    for(x=0; x < image_width; x++) {
      jpg_buffer[y * image_width * 3 + x*3 +0 ] = 
	data [y * image_width * 4 + x*4 +3 ];

      jpg_buffer[y * image_width * 3 + x*3 +1 ] = 
	data [y * image_width * 4 + x*4 +2 ]; 
      
      jpg_buffer[y * image_width * 3 + x*3 +2 ] = 
	data [y * image_width * 4 + x*4 +1 ] ;
    }
  }
  write_JPEG_file (file, JCS_RGB, 100, sar_frac_n, sar_frac_d);
  free(jpg_buffer);
}



void screenshot_yuv_jpg(yuv_image_t *yuv_data, XImage *ximg,
			int sar_frac_n, int sar_frac_d) {
  int x,y;
  char *file = new_file(); //support for progressive screenshots
  
  uint8_t* py = yuv_data->y;
  uint8_t* pu = yuv_data->u; 
  uint8_t* pv = yuv_data->v; 
  
  //ximg->height; /* Number of rows in image */
  //ximg->width;	/* Number of columns in image */  

  image_height = yuv_data->info->picture.vertical_size;
  image_width  = yuv_data->info->picture.horizontal_size;
  
  DNOTE("%s", "screenshot_yuv_jpg()\n");
  jpg_buffer = (JSAMPLE*)malloc(sizeof(JSAMPLE)
				* image_height * image_width * 3);
  
  if(jpg_buffer == NULL) {
    FATAL("%s", "FEL!\n");
    exit(1);
  }
  
  
  for(y=0; y < image_height ; y++) {
    for(x=0; x < image_width; x++) {
      jpg_buffer[y * image_width * 3 + x*3 +0 ] = (*py++);
    }
  }

  for(y=0; y < image_height ; y+=2) {
    for(x=0; x < image_width; x+=2) {
      
      jpg_buffer[y * image_width * 3 + x*3 +1 ] = (*pu);
      jpg_buffer[y * image_width * 3 + (x+1)*3 +1 ] = (*pu);
      jpg_buffer[(y+1) * image_width * 3 + x*3 +1 ] = (*pu);
      jpg_buffer[(y+1) * image_width * 3 + (x+1)*3 +1 ] = (*pu++);
      
      jpg_buffer[y     * image_width * 3 + x*3 +2 ] = (*pv);
      jpg_buffer[y     * image_width * 3 + (x+1)*3 +2 ] = (*pv);
      jpg_buffer[(y+1) * image_width * 3 + x*3 +2 ] = (*pv);
      jpg_buffer[(y+1) * image_width * 3 + (x+1)*3 +2 ] = (*pv++);
    }
  }
  write_JPEG_file ( file, JCS_YCbCr, 100, sar_frac_n, sar_frac_d);
  free(jpg_buffer);
}

static char *user_formatstr = NULL;
static int file_nr = 0;

int screenshot_set_formatstr(char *str)
{
  if(str) {
    file_nr = 0;
    if(user_formatstr) {
      free(user_formatstr);
    }
    
    user_formatstr = strdup(str);
  }

  return 0;
}


static int get_width(char **p, char *format_end, char **o, char *file_end)
{
  int width = 0;
  char *end;
  width = strtol(*p, &end, 10);
  *p = end;

  return width;
}

static unsigned char width_char_list[] = { 'i', 0 };

static int width_char(unsigned char c) {
  unsigned char *l = width_char_list;
  
  for(; *l != 0; l++) {
    if(c == *l) {
      return 1;
    }
  }
  return 0;
}
  
static int get_format(char **p, char *format_end, char **o, char *file_end)
{
  int width = -1;
  int leading_zero = 0;
  
  if(isdigit((int)**p)) {
    if(**p == '0') {
      leading_zero = 1;
    }
    width = get_width(p, format_end, o, file_end);
  }
  
  if(*p >= format_end) {
    //str end and no format, return format error
    return -1;
  }

  if((width != -1) && (!width_char(**p))) {
    //check if the next char can have a width, else return format error
    return -1;
  }

  switch(**p) {
  case '%':
    **o = **p;
    (*o)++;
    break;
  case 'i':
    (*o)+= snprintf(*o, file_end - *o,
		  leading_zero ? "%0*d" : "%*d",
		  width, file_nr);
    break;
  default:
    //format error
    return -1;
  }
  if(*o >= file_end) {
    //too long string
    return -1;
  }
  
  return 0;
}

static char *strffile(char *format)
{
  static char file[PATH_MAX];
  char *p, *o;
  char *format_end;

  char *file_end = &file[PATH_MAX-1];
  
  if(!format) {
    return NULL;
  }
  format_end = format+strlen(format);
  p = format;
  o = file;

  for(p = format; p < format_end && o < file_end  ; p++) {
    if(*p != '%') {
      *o = *p;
      o++;
    } else {
      p++;
      if(get_format(&p, format_end, &o, file_end) == -1) {
	return NULL;
      }
    }
  }
  if(o >= file_end) {
    return NULL;
  }
  
  *o = '\0';

  return file;
}

static char *new_file(void) {
  int fd;
  static char *formatted_name = NULL;
  static char oldname[PATH_MAX] = { 0 };
  
    while(file_nr < 10000) {
      formatted_name = strffile(user_formatstr);
      if(!formatted_name) {
	//format not set or illegal format -> set the default format
	screenshot_set_formatstr("shot%04i.jpg");
	continue;
      }

      fd = open(formatted_name, O_EXCL | O_CREAT, 0644);
      if(fd != -1) { 
	close(fd); 
	file_nr++;
	return formatted_name; 
      }
      if(errno != EEXIST) { 
	return "screenshot.jpg"; 
      } else {
	if(strcmp(formatted_name, oldname) == 0) {
	  //the generated filename doesn't change
	  return "screenshot.jpg";
	}
      }
      strncpy(oldname, formatted_name, sizeof(oldname));
      oldname[sizeof(oldname)-1] = '\0';

      file_nr++;
    }
    return formatted_name;
}
