/* Ogle - A video player
 * Copyright (C) 2000, 2001, 2002 Björn Englund, Håkan Hjort
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
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

#include "debug_print.h"
#include "audio_types.h"
#include "parse_config.h"

/* Put all of these in a struct that is returned instead */
static char *audio_device = NULL;

/* speaker config */

static channel_config_t *ch_conf = NULL;
static int nr_ch_conf = 0;

/* liba52 specific config */
static double a52_level = 1;
static int a52_drc = 0;
static int a52_stereo_mode = 2;

static char *audio_driver = NULL;

/* alsa specific interface name */
static char *audio_alsa_name = NULL;

/* sync */
static char *sync_type = NULL;
static int sync_resample = 0;
static int sync_offset = 0;

char *get_audio_driver(void)
{
  return audio_driver;
}

char *get_audio_device(void)
{
  return audio_device;
}

char *get_audio_alsa_name(void)
{
  return audio_alsa_name;
}


static void parse_alsa(xmlDocPtr doc, xmlNodePtr cur)
{
  xmlChar *s = NULL;
  
  cur = cur->xmlChildrenNode;
  
  while(cur != NULL) {
    
    if(!xmlIsBlankNode(cur)) {
      if(!strcmp("name", cur->name)) {
	if((s = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1))) {
	  if(audio_alsa_name != NULL) {
	    free(audio_alsa_name);
	  }
	  audio_alsa_name = strdup(s);
	}
      }
      if(s) {
	free(s);
	s = NULL;
      }
    }
    cur = cur->next;
  }
}


static void parse_device(xmlDocPtr doc, xmlNodePtr cur)
{
  xmlChar *s = NULL;
  
  cur = cur->xmlChildrenNode;
  
  while(cur != NULL) {
    
    if(!xmlIsBlankNode(cur)) {
      if(!strcmp("path", cur->name)) {
	if((s = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1))) {
	  if(audio_device != NULL) {
	    free(audio_device);
	  }
	  audio_device = strdup(s);
	}
      } else if(!strcmp("driver", cur->name)) {
	if((s = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1))) {
	  if(audio_driver != NULL) {
	    free(audio_driver);
	  }
	  audio_driver = strdup(s);
	}
      } else if(!strcmp("alsa", cur->name)) {
        parse_alsa(doc, cur);
      }
      if(s) {
	free(s);
	s = NULL;
      }
    }
    cur = cur->next;
  }
}

int get_channel_configs(channel_config_t **conf)
{
  *conf = ch_conf;
  return nr_ch_conf;
}


static void parse_channel_config(xmlDocPtr doc, xmlNodePtr cur,
				 channel_config_t *conf)
{
  int first = 1;
  xmlChar *s = NULL;
  
  cur = cur->xmlChildrenNode;
  
  while(cur != NULL) {
    
    if(!xmlIsBlankNode(cur)) {
      if(!strcmp("chtype", cur->name)) {	
	if((s = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1))) {
	  ChannelType_t ct;
          if(!strcmp("Left", s)) {
	    ct = ChannelType_Left;
	  } else if(!strcmp("Right", s)) {
	    ct = ChannelType_Right;
	  } else if(!strcmp("Center", s)) {
	    ct = ChannelType_Center;
	  } else if(!strcmp("RearRight", s)) {
	    ct = ChannelType_RightSurround;
	  } else if(!strcmp("RearLeft", s)) {
	    ct = ChannelType_LeftSurround;
	  } else if(!strcmp("LFE", s)) {
	    ct = ChannelType_LFE;
	  } else if(!strcmp("RearCenter", s)) {
	    ct = ChannelType_CenterSurround;
	  } else if(!strcmp("0", s)) {
	    ct = ChannelType_Null;
	  } else if(!strcmp("AC3", s)) {
	    ct = ChannelType_AC3;
	  } else if(!strcmp("DTS", s)) {
	    ct = ChannelType_DTS;
	  } else if(!strcmp("MPEG", s)) {
	    ct = ChannelType_MPEG;
	  } else if(!strcmp("LPCM", s)) {
	    ct = ChannelType_LPCM;
	  } else if(!strcmp("Mono", s)) {
	    ct = ChannelType_Mono;
	  } else {
	    ct = ChannelType_Unspecified;
	    WARNING("'%s' is not a valid <chtype>\n", s); 
	  }
	  
	  if(ct != ChannelType_Unspecified) {
	    conf->nr_ch++;
	    conf->chtype = realloc(conf->chtype,
				   sizeof(ChannelType_t) * conf->nr_ch);
	    conf->chtype[conf->nr_ch-1] = ct;
	  }
	  free(s);
	  s = NULL;
	} else {
	  WARNING("%s", "<chtype> is empty\n"); 
	}
      }
      first = 0;
    }
    cur = cur->next;
  }
}


static void parse_speakers(xmlDocPtr doc, xmlNodePtr cur)
{
  cur = cur->xmlChildrenNode;
  
  //override old information
  if(ch_conf) {
    int n;
    for(n = 0; n < nr_ch_conf; n++) {
      if(ch_conf[n].chtype) {
	free(ch_conf[n].chtype);
	ch_conf[n].chtype = NULL;
	ch_conf[n].nr_ch = 0;
      }
    }
    free(ch_conf);
    ch_conf = NULL;
    nr_ch_conf = 0;
  }
  
  while(cur != NULL) {
    
    if(!xmlIsBlankNode(cur)) {
      if(!strcmp("channel_config", cur->name)) {
	nr_ch_conf++;
	ch_conf = realloc(ch_conf, sizeof(channel_config_t) * nr_ch_conf);
	ch_conf[nr_ch_conf-1].nr_ch = 0;
	ch_conf[nr_ch_conf-1].chtype = NULL;
	parse_channel_config(doc, cur, &ch_conf[nr_ch_conf-1]);
      }
    }
    cur = cur->next;
  }
}


double get_a52_level(void)
{
  return a52_level;
}

int get_a52_drc(void)
{
  return a52_drc;
}

int get_a52_stereo_mode(void)
{
  return a52_stereo_mode;
}

static void parse_liba52(xmlDocPtr doc, xmlNodePtr cur)
{
  xmlChar *s = NULL;
  
  cur = cur->xmlChildrenNode;
  
  while(cur != NULL) {
    
    if(!xmlIsBlankNode(cur)) {
      if(!strcmp("downmix_level", cur->name)) {
	if((s = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1))) {
          a52_level = atof(s);
	}
      } else if(!strcmp("drc", cur->name)) {
	if((s = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1))) {
	  if(!strcmp("yes", s)) {
	    a52_drc = 1;
	  } else {
	    a52_drc = 0;
	  }
	}
      } else if(!strcmp("stereo_mode", cur->name)) {
	if((s = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1))) {
	  if(!strcmp("main", s)) {
	    a52_stereo_mode = 0;
	  } else if(!strcmp("front", s)) {
	    a52_stereo_mode = 1;
	  } else { // "dolby"
	    a52_stereo_mode = 2;
	  }
	}
      }
      if(s) {
	free(s);
	s = NULL;
      }
    }
    cur = cur->next;
  }
}


char *get_sync_type(void)
{
  return sync_type;
}

int get_sync_resample(void)
{
  return sync_resample;
}

int get_sync_offset(void)
{
  return sync_offset;
}


static void parse_sync(xmlDocPtr doc, xmlNodePtr cur)
{
  xmlChar *s = NULL;
  
  cur = cur->xmlChildrenNode;
  
  while(cur != NULL) {
    
    if(!xmlIsBlankNode(cur)) {
      if(!strcmp("type", cur->name)) {
	if((s = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1))) {
	  if(sync_type != NULL) {
	    free(sync_type);
	  }
	  sync_type = strdup(s);
	}
      } else if(!strcmp("resample", cur->name)) {
	if((s = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1))) {
	  if(!strcmp("yes", s)) {
	    sync_resample = 1;
	  } else {
	    sync_resample = 0;
	  }
	}
      } else if(!strcmp("offset", cur->name)) {
	if((s = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1))) {
          sync_offset = atoi(s);
	}
      }

      if(s) {
	free(s);
	s = NULL;
      }
      
    }
    cur = cur->next;
  }
}


static void parse_audio(xmlDocPtr doc, xmlNodePtr cur)
{
  cur = cur->xmlChildrenNode;
  
  while(cur != NULL) {
    
    if(!xmlIsBlankNode(cur)) {
      if(!strcmp("device", cur->name)) {
	parse_device(doc, cur);
      } else if(!strcmp("speakers", cur->name)) {
        parse_speakers(doc, cur);
      } else if(!strcmp("liba52", cur->name)) {
        parse_liba52(doc, cur);
      } else if(!strcmp("sync", cur->name)) {
        parse_sync(doc, cur);
      }
    }
    cur = cur->next;
  }
}


static void parse_ogle_conf(xmlDocPtr doc, xmlNodePtr cur)
{
  cur = cur->xmlChildrenNode;
  
  while(cur != NULL) {
    
    if(!xmlIsBlankNode(cur)) {
      if(!strcmp("audio", cur->name)) {
	parse_audio(doc, cur);
      }
    }
    cur = cur->next;
  }
}


int parse_oglerc(char *filename)
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
	parse_ogle_conf(doc, cur);
      }
    }
    cur = cur->next;
  }
  
  xmlFreeDoc(doc);
  
  return 0;
}


int parse_config(void)
{
  int config_read = 0;
  char *home;
  

  if(parse_oglerc(CONFIG_FILE) != -1) {
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
    
    if(parse_oglerc(rcpath) != -1) {
      config_read |= 2;
    }
    if(!config_read) {
      ERROR("parse_config(): Couldn't read '%s'\n", rcpath);
    }
    free(rcpath);
  }
  
  if(!config_read) {
    ERROR("%s", "parse_config(): Couldn't read "CONFIG_FILE"\n");
    return -1;
  }

  
  return 0;
}


