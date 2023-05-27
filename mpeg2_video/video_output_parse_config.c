#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

#include "debug_print.h"
#include "video_output_parse_config.h"


static void parse_geometry(xmlDocPtr doc, xmlNodePtr cur,
			   cfg_display_t *display)
{
  xmlChar *s;
  int val;

  display->geometry.width = 0;
  display->geometry.height = 0;

  cur = cur->xmlChildrenNode;
  
  while(cur != NULL) {
    
    if(!xmlIsBlankNode(cur)) {
      if(!strcmp("width", cur->name)) {
	if((s = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1))) {
	  val = atoi(s);
	  free(s);
	  if(val < 0) {
	    val = 0;
	  }
	  display->geometry.width = val;
	}
      } else if(!strcmp("height", cur->name)) {
	if((s = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1))) {
	  val = atoi(s);
	  free(s);
	  if(val < 0) {
	    val = 0;
	  }
	  display->geometry.height = val;
	}
      }
    }
    cur = cur->next;
  }
  
}


static void parse_resolution(xmlDocPtr doc, xmlNodePtr cur,
			     cfg_display_t *display)
{
  xmlChar *s;
  int val;

  display->resolution.horizontal_pixels = 0;
  display->resolution.vertical_pixels = 0;

  cur = cur->xmlChildrenNode;
  
  while(cur != NULL) {
    
    if(!xmlIsBlankNode(cur)) {
      if(!strcmp("horizontal_pixels", cur->name)) {
	if((s = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1))) {
	  val = atoi(s);
	  free(s);
	  if(val < 0) {
	    val = 0;
	  }
	  display->resolution.horizontal_pixels = val;
	}
      } else if(!strcmp("vertical_pixels", cur->name)) {
	if((s = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1))) {
	  val = atoi(s);
	  free(s);
	  if(val < 0) {
	    val = 0;
	  }
	  display->resolution.vertical_pixels = val;
	}
      }
    }
    cur = cur->next;
  }
  
}

	
static void parse_initial_state(xmlDocPtr doc, xmlNodePtr cur,
				cfg_display_t *display)
{
  xmlChar *s;

  display->initial_state.fullscreen = 0;

  cur = cur->xmlChildrenNode;
  
  while(cur != NULL) {
    
    if(!xmlIsBlankNode(cur)) {
      if(!strcmp("fullscreen", cur->name)) {
	if((s = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1))) {
	  if(!strcmp("yes", s)) {
	    display->initial_state.fullscreen = 1;
	  }
	  free(s);
	}
      }
    }
    cur = cur->next;
  } 
}


static void parse_display(xmlDocPtr doc, xmlNodePtr cur,
			  cfg_display_t **cur_display)
{
  xmlChar *s;
  int name_found = 0;
  
  cur = cur->xmlChildrenNode;
  
  
  while(cur != NULL) {
    
    if(!xmlIsBlankNode(cur)) {
      if(!strcmp("name", cur->name)) {
	name_found++;
	if(name_found == 1) {
	  if((s = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1))) {
	    while(*cur_display) {
	      if((*cur_display)->name != NULL) {
		if(!strcmp((*cur_display)->name, s)) {
		  break;
		}
	      }
	      cur_display = &(*cur_display)->next;
	    }
	    if(*cur_display == NULL) { 
	      *cur_display = malloc(sizeof(cfg_display_t));
	      memset(*cur_display, 0, sizeof(cfg_display_t));
	      (*cur_display)->name = s;
	    }
	  } else {
	    while(*cur_display) {
	      if((*cur_display)->name == NULL) {
		break;
	      }
	      cur_display = &(*cur_display)->next;
	    }
	    if(*cur_display == NULL) { 
	      *cur_display = malloc(sizeof(cfg_display_t));
	      memset(*cur_display, 0, sizeof(cfg_display_t));
	    }
	  }
	} else {
	  ERROR("%s", "parse_display(): more than 1 <name>\n");
	}
      } else {
	if(!name_found) {
	  name_found++;
	  ERROR("%s", "parse_display(): <name> not first, assuming empty <name>\n");
	  while(*cur_display) {
	    if((*cur_display)->name == NULL) {
	      break;
	    }
	    cur_display = &(*cur_display)->next;
	  }
	  if(*cur_display == NULL) { 
	    *cur_display = malloc(sizeof(cfg_display_t));
	    memset(*cur_display, 0, sizeof(cfg_display_t));
	  }
	}
	if(!strcmp("geometry", cur->name)) {
	  parse_geometry(doc, cur, *cur_display);
	} else if(!strcmp("resolution", cur->name)) {
	  parse_resolution(doc, cur, *cur_display);
	} else if(!strcmp("geometry_src", cur->name)) {
	  if((s = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1))) {
	    (*cur_display)->geometry_src = s;
	  }
	} else if(!strcmp("resolution_src", cur->name)) {
	  if((s = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1))) {
	    (*cur_display)->resolution_src = s;
	  }
	} else if(!strcmp("switch_resolution", cur->name)) {
	  if((s = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1))) {
	    if(!strcmp("yes", s)) {
	      (*cur_display)->switch_resolution = 1;
	    }
	    free(s);
	  }
	} else if(!strcmp("ewmh_fullscreen", cur->name)) {
	  if((s = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1))) {
	    if(!strcmp("yes", s)) {
	      (*cur_display)->ewmh_fullscreen = 1;
	    }
	    free(s);
	  }
	} else 	if(!strcmp("initial_state", cur->name)) {
	  parse_initial_state(doc, cur, *cur_display);
	}
      }
    }
    cur = cur->next;
  }
}

static void parse_window(xmlDocPtr doc, xmlNodePtr cur,
			cfg_window_t *window)
{
  //  xmlChar *s;
  
  cur = cur->xmlChildrenNode;
  
  
  while(cur != NULL) {
    
    if(!xmlIsBlankNode(cur)) {
      
    }
    cur = cur->next;
  }
}

static void parse_video(xmlDocPtr doc, xmlNodePtr cur,
			cfg_video_t *video)
{
  cur = cur->xmlChildrenNode;
  
  while(cur != NULL) {
    
    if(!xmlIsBlankNode(cur)) {
      if(!strcmp("display", cur->name)) {
	parse_display(doc, cur, &video->display);
      } else if(!strcmp("window", cur->name)) {
	parse_window(doc, cur, &video->window);
      }
    }
    cur = cur->next;
  }
}


static void parse_ogle_conf(xmlDocPtr doc, xmlNodePtr cur,
			    cfg_video_t *video)
{
  cur = cur->xmlChildrenNode;
  
  while(cur != NULL) {
    
    if(!xmlIsBlankNode(cur)) {
      if(!strcmp("video", cur->name)) {
	parse_video(doc, cur, video);
      }
    }
    cur = cur->next;
  }
}


static int parse_oglerc(char *filename, cfg_video_t *video)
{
  xmlDocPtr doc;
  xmlNodePtr cur;
  int fd;

  if((fd = open(filename, O_RDONLY)) == -1) {
    if(errno != ENOENT) {
      perror(filename);
    }
    return -1;
  } else {
    close(fd);
  }
  
  doc = xmlParseFile(filename);

  if(doc == NULL) {
    return -1;
  }
  
  cur = xmlDocGetRootElement(doc); 

  while(cur != NULL) {
    
    if(!xmlIsBlankNode(cur)) {
      if(!strcmp("ogle_conf", cur->name)) {
	parse_ogle_conf(doc, cur, video);
      }
    }
    cur = cur->next;
  }
  
  xmlFreeDoc(doc);
  
  return 0;
}


int get_video_config(cfg_video_t **video)
{
  int config_read = 0;
  char *home;

  *video = malloc(sizeof(cfg_video_t));
  
  // init values
  memset(*video, 0, sizeof(cfg_video_t));
  
  if(parse_oglerc(CONFIG_FILE, *video) != -1) {
    config_read |= 1;
  }
  
  home = getenv("HOME");
  if(home == NULL) {
    WARNING("%s", "No $HOME\n");
  } else {
    char *rcpath = NULL;
    char rcfile[] = ".oglerc";
    
    rcpath = malloc(strlen(home)+strlen(rcfile)+2);
    strcpy(rcpath, home);
    strcat(rcpath, "/");
    strcat(rcpath, rcfile);
    
    if(parse_oglerc(rcpath, *video) != -1) {
      config_read |= 2;
    }
    
    if(!config_read) {
      ERROR("get_video_config(): Couldn't read '%s'\n", rcpath);
    }
    
    free(rcpath);
  }
  
  if(!config_read) {
    ERROR("%s", "get_video_config(): Couldn't read "CONFIG_FILE"\n");
    return -1;
  }

  return 0;
}


