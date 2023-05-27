/* Ogle - A video player
 * Copyright (C) 2003 Björn Englund
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
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <libxml/parser.h>

#include "vm.h"
#include "vm_serialize.h"


static int serialize_registers (xmlNodePtr cur, registers_t *regs)
{
  char str_buf[121];
  char *buf;
  int len;
  int n,i;
  int len_needed;
  int register_mode = 1;

  buf = str_buf;
  len = sizeof(str_buf);
  len_needed = 0;

  for(i = 0; i < 24; i++) {
    n = snprintf(buf, len, ",%hx",  regs->SPRM[i]);
    if(n >= len) {
      //len too short, continue so we get the total length needed;
      len_needed+=n;
      buf+=(len-1);
      len = 1;
    } else {
      buf+=n;
      len-=n;
      len_needed+=n;
    }
  }
  
  if(xmlNewTextChild(cur, NULL, "sprm", str_buf) == NULL) {
    return 0;
  }

  buf = str_buf;
  len = sizeof(str_buf);
  
  for(i = 0; i < 16; i++) {
    n = snprintf(buf, len, "%c%hx", (register_mode ? ',':';'), regs->GPRM[i]);
    if(n >= len) {
      //len too short, continue so we get the total length needed;
      len_needed+=n;
      buf+=(len-1);
      len = 1;
    } else {
      buf+=n;
      len-=n;
      len_needed+=n;
    }
  }

  if((cur = xmlNewTextChild(cur, NULL, "gprm", str_buf)) == NULL) {
    return 0;
  }
  
  return 1;
}


#define NAVSTATE_VERSION "0.0.0"

char *vm_serialize_dvd_state(dvd_state_t *state)
{
  char str_buf[26];
  int len = sizeof(str_buf);
  char *buf;
  int n,i;
  char *ret_str;
  xmlDocPtr doc;
  xmlNodePtr cur;
  xmlBufferPtr xmlbuf;
  

  buf = str_buf;

  if((doc = xmlNewDoc("1.0")) == NULL) {
    return NULL;
  }
  
  if((cur = xmlNewDocNode(doc, NULL, "navstate", NULL)) == NULL) {
    xmlFreeDoc(doc);
    return NULL;
  }
  
  if(xmlDocSetRootElement(doc, cur)) {
    xmlFreeNode(cur);
    xmlFreeDoc(doc);
    return NULL;
  }

  xmlNewProp(cur, "version", NAVSTATE_VERSION);
  
  
  if(!serialize_registers(cur, &(state->registers))) {
    xmlFreeDoc(doc);
    return NULL;
  }

  snprintf(buf, len, "%d", state->domain);
  if((xmlNewTextChild(cur, NULL, "domain", buf)) == NULL) {
    xmlFreeDoc(doc);
    return NULL;
  }

  snprintf(buf, len, "%d", state->vtsN);
  if((xmlNewTextChild(cur, NULL, "vts", buf)) == NULL) {
    xmlFreeDoc(doc);
    return NULL;
  }

  snprintf(buf, len, "%d", state->pgcN);
  if((xmlNewTextChild(cur, NULL, "pgc", buf)) == NULL) {
    xmlFreeDoc(doc);
    return NULL;
  }

  snprintf(buf, len, "%d", state->pgN);
  if((xmlNewTextChild(cur, NULL, "pg", buf)) == NULL) {
    xmlFreeDoc(doc);
    return NULL;
  }

  snprintf(buf, len, "%d", state->cellN);
  if((xmlNewTextChild(cur, NULL, "cell", buf)) == NULL) {
    xmlFreeDoc(doc);
    return NULL;
  }

  snprintf(buf, len, "%d", state->blockN);
  if((xmlNewTextChild(cur, NULL, "block", buf)) == NULL) {
    xmlFreeDoc(doc);
    return NULL;
  }

  snprintf(buf, len, "%d", state->mode);
  if((xmlNewTextChild(cur, NULL, "mode", buf)) == NULL) {
    xmlFreeDoc(doc);
    return NULL;
  }

  snprintf(buf, len, "%d", state->rsm_vtsN);
  if((xmlNewTextChild(cur, NULL, "rsmvts", buf)) == NULL) {
    xmlFreeDoc(doc);
    return NULL;
  }

  snprintf(buf, len, "%d", state->rsm_pgcN);
  if((xmlNewTextChild(cur, NULL, "rsmpgc", buf)) == NULL) {
    xmlFreeDoc(doc);
    return NULL;
  }

  snprintf(buf, len, "%d", state->rsm_cellN);
  if((xmlNewTextChild(cur, NULL, "rsmcell", buf)) == NULL) {
    xmlFreeDoc(doc);
    return NULL;
  }

  snprintf(buf, len, "%d", state->rsm_blockN);
  if((xmlNewTextChild(cur, NULL, "rsmblock", buf)) == NULL) {
    xmlFreeDoc(doc);
    return NULL;
  }

  for(i = 0; i < 5; i++) {
    n = snprintf(buf, len, ",%hx", state->rsm_regs[i]);
    buf+=n;
    len-=n;
  }
  buf = str_buf;
  len = sizeof(str_buf);
  if((xmlNewTextChild(cur, NULL, "rsmregs", buf)) == NULL) {
    xmlFreeDoc(doc);
    return NULL;
  }
  
  if((xmlbuf = xmlBufferCreate()) == NULL) {
    xmlFreeDoc(doc);
    return NULL;
  }
  
  xmlNodeDump(xmlbuf, doc, cur, 0, 0);
  ret_str = strdup(xmlBufferContent(xmlbuf));
  xmlBufferFree(xmlbuf);
  xmlFreeDoc(doc);

  return ret_str;
}

int vm_deserialize_dvd_state(char *xmlstr, dvd_state_t *state)
{
  xmlDocPtr doc;
  xmlNodePtr cur;
  char *data;

  doc = xmlParseDoc(xmlstr);
  
  
  if((cur = xmlDocGetRootElement(doc)) == NULL) {
    return 0;
  }

  cur = cur->xmlChildrenNode;
  
  while(cur != NULL) {
    char *reg;
    if((!xmlStrcmp(cur->name, (const xmlChar *)"sprm"))) {
      if((data = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1))) {
	int i,c;
	reg = data;
	for(i = 0; i < 24; i++) {
	  c = 0;
	  sscanf(reg, ",%hx%n", &(state->registers.SPRM[i]), &c);
	  reg+=c;
	}
	xmlFree(data);
      }
    } else if((!xmlStrcmp(cur->name, (const xmlChar *)"gprm"))) {
      if((data = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1))) {
	int i,c;
	reg = data;
	for(i = 0; i < 16; i++) {
	  char regmode;
	  c = 0;
	  sscanf(reg, "%c%hx%n", &regmode, &(state->registers.GPRM[i]), &c);
	  reg+=c;
	  if(regmode == ',') {
	    //register mode
	  } else {
	    //counter mode
	  }
	}
	xmlFree(data);
      }
    } else if((!xmlStrcmp(cur->name, (const xmlChar *)"domain"))) {
      if((data = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1))) {
	sscanf(data, "%d", (int *)&(state->domain));	
	xmlFree(data);
      }
    } else if((!xmlStrcmp(cur->name, (const xmlChar *)"vts"))) {
      if((data = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1))) {
	sscanf(data, "%d", &(state->vtsN));	
	xmlFree(data);
      }
    } else if((!xmlStrcmp(cur->name, (const xmlChar *)"pgc"))) {
      if((data = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1))) {
	sscanf(data, "%d", &(state->pgcN));	
	xmlFree(data);
      }
    } else if((!xmlStrcmp(cur->name, (const xmlChar *)"pg"))) {
      if((data = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1))) {
	sscanf(data, "%d", &(state->pgN));	
	xmlFree(data);
      }
    } else if((!xmlStrcmp(cur->name, (const xmlChar *)"cell"))) {
      if((data = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1))) {
	sscanf(data, "%d", &(state->cellN));	
	xmlFree(data);
      }
    } else if((!xmlStrcmp(cur->name, (const xmlChar *)"block"))) {
      if((data = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1))) {
	sscanf(data, "%d", &(state->blockN));	
	xmlFree(data);
      }
    } else if((!xmlStrcmp(cur->name, (const xmlChar *)"mode"))) {
      if((data = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1))) {
	sscanf(data, "%d", (int *)&(state->mode));	
	xmlFree(data);
      }
    } else if((!xmlStrcmp(cur->name, (const xmlChar *)"rsmvts"))) {
      if((data = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1))) {
	sscanf(data, "%d", &(state->rsm_vtsN));	
	xmlFree(data);
      }
    } else if((!xmlStrcmp(cur->name, (const xmlChar *)"rsmblock"))) {
      if((data = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1))) {
	sscanf(data, "%d", &(state->rsm_blockN));	
	xmlFree(data);
      }
    } else if((!xmlStrcmp(cur->name, (const xmlChar *)"rsmpgc"))) {
      if((data = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1))) {
	sscanf(data, "%d", &(state->rsm_pgcN));	
	xmlFree(data);
      }
    } else if((!xmlStrcmp(cur->name, (const xmlChar *)"rsmcell"))) {
      if((data = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1))) {
	sscanf(data, "%d", &(state->rsm_cellN));	
	xmlFree(data);
      }
    } else     if((!xmlStrcmp(cur->name, (const xmlChar *)"sprm"))) {
      if((data = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1))) {
	int i,c;
	reg = data;
	for(i = 0; i < 5; i++) {
	  c = 0;
	  sscanf(reg, ",%hx%n", &(state->rsm_regs[i]), &c);
	  reg+=c;
	}
	xmlFree(data);
      }
    }
    
    cur = cur->next;
  }
  xmlFreeDoc(doc);

  return 1;
}
