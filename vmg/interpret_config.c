#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include <string.h>

#include <ogle/dvdcontrol.h>

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

#include "debug_print.h"
#include "vm.h"


static void interpret_nav_defaults(xmlDocPtr doc, xmlNodePtr cur)
{
  DVDLangID_t lang;
  DVDCountryID_t country;
  DVDParentalLevel_t plevel;
  DVDPlayerRegion_t region;
  xmlChar *s;

  cur = cur->xmlChildrenNode;
  
  while(cur != NULL) {
    
    if(!strcmp("DefaultMenuLanguage", cur->name)) {
      if((s = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1))) {
	//fprintf(stderr, "DefaultMenuLanguage = '%s'\n", s);
	lang = s[0] << 8 | s[1];
	set_sprm(0, lang);
	free(s);
      }
    } else if(!strcmp("DefaultAudioLanguage", cur->name)) {
      if((s = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1))) {
	//fprintf(stderr, "DefaultAudioLanguage = %s\n", s);
	lang = s[0] << 8 | s[1];
	set_sprm(16, lang);
	free(s);
      }
    } else if(!strcmp("DefaultSubtitleLanguage", cur->name)) {
      if((s = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1))) {
	//fprintf(stderr, "DefaultSubtitleLanguage = %s\n", s);
	lang = s[0] << 8 | s[1];
	set_sprm(18, lang);
	free(s);
      } 
    } else if(!strcmp("ParentalCountry", cur->name)) {
      if((s = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1))) {
	//fprintf(stderr, "ParentalCountry = %s\n", s);
	country = s[0] << 8 | s[1];
	set_sprm(12, country);
	free(s);
      } 
    } else if(!strcmp("ParentalLevel", cur->name)) {
      if((s = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1))) {
	//fprintf(stderr, "ParentalLevel = %s\n", s);
	plevel = atoi(s);
	set_sprm(13, plevel);
	free(s);
      }
    } else if(!strcmp("PlayerRegion", cur->name)) {
      if((s = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1))) {
	//fprintf(stderr, "PlayerRegion = %s\n", s);
	region = (uint8_t)atoi(s);
	if((region >= 1) && (region <= 8)) {
	  region = 1 << (region-1);
	  set_sprm(20, region);
	} else {
	  ERROR("interpret_nav_defaults(): <PlayerRegion> %u not in range [1,8]\n", region);
	}
	free(s);
      }
    }

    cur = cur->next;
  }

}


static void interpret_nav(xmlDocPtr doc, xmlNodePtr cur)
{
  cur = cur->xmlChildrenNode;

  while(cur != NULL) {
    
    if(!xmlIsBlankNode(cur)) {
      if(!strcmp("defaults", cur->name)) {
	  interpret_nav_defaults(doc, cur);
      }
    }
    cur = cur->next;
  }
}


static void interpret_dvd(xmlDocPtr doc, xmlNodePtr cur)
{
  cur = cur->xmlChildrenNode;
  
  while(cur != NULL) {
    
    if(!xmlIsBlankNode(cur)) {
      if(!strcmp("nav", cur->name)) {
	interpret_nav(doc, cur);
      }
    }
    cur = cur->next;
  }
}


static void interpret_ogle_conf(xmlDocPtr doc, xmlNodePtr cur)
{
  cur = cur->xmlChildrenNode;
  
  while(cur != NULL) {
    
    if(!xmlIsBlankNode(cur)) {
      if(!strcmp("dvd", cur->name)) {
	interpret_dvd(doc, cur);
      }
    }
    cur = cur->next;
  }
}


int interpret_oglerc(char *filename)
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

  if(doc != NULL) {

    cur = xmlDocGetRootElement(doc); 

    while(cur != NULL) {
      
      if(!xmlIsBlankNode(cur)) {
	if(!strcmp("ogle_conf", cur->name)) {
	  interpret_ogle_conf(doc, cur);
	}
      }
      cur = cur->next;
    }
    
    xmlFreeDoc(doc);

    return 0;

  } else {
    return -1;
  }
}


int interpret_config(void)
{
  int config_read = 0;
  char *home;


  if(interpret_oglerc(CONFIG_FILE) != -1) {
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
    
    if(interpret_oglerc(rcpath) != -1) {
      config_read |= 2;
    }
    
    if(!config_read) {
      ERROR("interpret_config(): Couldn't read '%s'\n", rcpath);
    }
    free(rcpath);
  }

  if(!config_read) {
    ERROR("%s", "interpret_config(): Couldn't read "CONFIG_FILE"\n");
    return -1;
  }
    
  return 0;
}


