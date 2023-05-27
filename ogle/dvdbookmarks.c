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
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

#include "dvdbookmarks.h"

#define OGLE_RC_DIR ".ogle"
#define OGLE_BOOKMARK_DIR "bookmarks"

struct DVDBookmark_s {
  char *filename;
  xmlDocPtr doc;
};

static xmlDocPtr new_bookmark_doc(const char *dvdid_str)
{
  xmlDocPtr doc;
  xmlNodePtr node;
  
  if((doc = xmlNewDoc("1.0")) == NULL) {
    return NULL;
  }
  if((node = xmlNewDocNode(doc, NULL, "ogle_bookmarks", NULL)) == NULL) {
    xmlFreeDoc(doc);
    return NULL;
  }
  if(xmlDocSetRootElement(doc, node)) {
    xmlFreeNode(node);
    xmlFreeDoc(doc);
    return NULL;
  }
  xmlNewProp(node, "dvddiscid", dvdid_str);
  
  return doc;
}

static xmlNodePtr get_bookmark(xmlDocPtr doc, xmlNodePtr cur, int nr)
{
  int n = 0;
  cur = cur->xmlChildrenNode;
  while(cur != NULL) {
    if((!xmlStrcmp(cur->name, (const xmlChar *)"bookmark"))) {
      if(n++ == nr) {
	return cur;
      }
    }
    cur = cur->next;
  }
  return NULL;  
}


/**
 * Open and and read the file that contains the bookmarks for a dvd.
 * @param dvdid This is the dvddiscid and will be used to locate the correct
 * file to read from.
 * @param dir  This should be NULL to use the standard bookmark directory.
 * If the bookmarks should be read from another directory, name it here.
 * The default directory used when none is specified is
 * $HOME/.ogle/bookmarks
 *
 * @param create Set this to 1 if the bookmark file should be create in case
 * it doesn't exist. 0 if it shouldn't be created.
 * Setting it to 0 can be used if one wants to find out if there are
 * any bookmarks for a dvd. If there are none, NULL will be returned.
 *
 * @return If successful a handle is returned else NULL and errno is set if
 * the failure was file related.
 *
 */
DVDBookmark_t *DVDBookmarkOpen(const unsigned char dvdid[16],
			       const char *dir, int create)
{
  DVDBookmark_t *bm;
  char *home = NULL;
  xmlDocPtr doc = NULL;
  char *filename = NULL;
  int fd;
  char dvdid_str[33];
  int n;

  /* convert dvdid to text format */
  for(n = 0; n < 16; n++) {
    sprintf(&dvdid_str[n*2], "%02x", dvdid[n]);
  }

  if(dir == NULL) {
    home = getenv("HOME");
    if(home == NULL) {
      return NULL;
    } else {
      struct stat buf;
      filename = malloc(strlen(home) + 1 + strlen(OGLE_RC_DIR) + 1 +
			strlen(OGLE_BOOKMARK_DIR) + 1 + strlen(dvdid_str) + 1);
      if(filename == NULL) {
	return NULL;
      }
      strcpy(filename, home);
      strcat(filename, "/");
      strcat(filename, OGLE_RC_DIR);
      if(stat(filename, &buf) == -1) {
	if(errno == ENOENT) {
	  mkdir(filename, 0755);
	}
      }
      strcat(filename, "/");
      strcat(filename, OGLE_BOOKMARK_DIR);
      if(stat(filename, &buf) == -1) {
	if(errno == ENOENT) {
	  mkdir(filename, 0755);
	}
      }
      strcat(filename, "/");
      strcat(filename, dvdid_str);
    }
  } else {
    filename = malloc( strlen(dir) + 1 + strlen(dvdid_str) + 1 );
    if(filename == NULL) {
      return NULL;
    }
    strcpy(filename, dir);
    strcat(filename, "/");
    strcat(filename, dvdid_str);
  }
  
  xmlKeepBlanksDefault(0);

  
  if((fd = open(filename, O_RDONLY)) != -1) {
    close(fd);
  } else {
    if(!create || (errno != ENOENT)) {
      free(filename);
      return NULL;
    } else {
      if((fd = open(filename, O_RDONLY | O_CREAT, 0644)) != -1) {
	close(fd);
	if((doc = new_bookmark_doc(dvdid_str)) == NULL) {
	  free(filename);
	  return NULL;
	}
      } else {
	free(filename);
	return NULL;
      }
    } 
  }
  if(doc == NULL) {
    xmlNodePtr cur;
    xmlChar *prop;

    if((doc = xmlParseFile(filename)) == NULL) {
      free(filename);
      return NULL;
    }
    if((cur = xmlDocGetRootElement(doc)) == NULL) {
      xmlFree(doc);
      free(filename);
      return NULL;
    }
    if((prop = xmlGetProp(cur, "dvddiscid")) == NULL) {
      xmlFree(doc);
      free(filename);
      return NULL;
    }
    if(xmlStrcmp(prop, (const xmlChar *)dvdid_str)) {
      xmlFree(prop);
      xmlFree(doc);
      free(filename);
      return NULL;
    }
    xmlFree(prop);
      
  }

  if((bm = malloc(sizeof(DVDBookmark_t))) == NULL) {
    return NULL;
  }
  
  bm->filename = filename;
  bm->doc = doc;

  return bm;
}


/**
 * Retrieve the number of bookmarks for the current disc
 *
 * @param bm Handle from DVDBookmarkOpen.
 *
 * @return Returns -1 on failure, otherwise the number of bookmarks.
 */
int DVDBookmarkGetNr(DVDBookmark_t *bm)
{
  xmlNodePtr cur;
  int n = 0;

  if(!bm || !(bm->doc)) {
    return -1;
  }
  
  if((cur = xmlDocGetRootElement(bm->doc)) == NULL) {
    return -1;
  }

  cur = cur->xmlChildrenNode;
  while(cur != NULL) {
    if((!xmlStrcmp(cur->name, (const xmlChar *)"bookmark"))) {
      n++;
    }
    cur = cur->next;
  }
  
  return n;  
}


/**
 * Retrieve a bookmark for the current disc.
 *
 * @param bm Handle from DVDBookmarkOpen.
 * @param nr The specific bookmark for this disc.
 * There can be several bookmarks for one disc.
 * Bookmarks are numbered from 0 and up.
 * To read all bookmarks for this disc, call this function
 * with numbers from 0 and up until -1 is returned.
 * @param navstate This is where a pointer to the navstate data
 * will be returned. You need to free() this when you are done with it.
 * If this is NULL the navstate won't be returned.
 * @param usercomment This is where a pointer to the usercomment data
 * will be returned. You need to free() this when you are done with it.
 * If this is NULL the usercommment won't be returned.
 * @param appname If you supply the name of your application here, you will
 * get application specific data back in appinfo if there is a matching
 * entry for your appname.
 * If no data is returned for a char ** it will be set to NULL, unless
 * the call fails in which case it will be undefined.
 * @param appinfo This is where a pointer to the appinfo data
 * will be returned. You need to free() this when you are done with it.
 * If this is NULL the appinfo won't be returned.
 */
int DVDBookmarkGet(DVDBookmark_t *bm, int nr,
		   char **navstate, char **usercomment,
		   const char *appname, char **appinfo)
{
  xmlNodePtr cur;
  xmlChar *data;
  int navstate_read = 0;
  int usercomment_read = 0;
  int appinfo_read = 0;

  if(!bm || !(bm->doc) || (nr < 0)) {
    return -1;
  }
  
  if((cur = xmlDocGetRootElement(bm->doc)) == NULL) {
    return -1;
  }

  cur = get_bookmark(bm->doc, cur, nr);
  if(cur == NULL) {
    return -1;
  }
  if(navstate) {
    *navstate = NULL;
  }
  if(usercomment) {
    *usercomment = NULL;
  }
  if(appinfo) {
    *appinfo = NULL;
  }
  
  cur = cur->xmlChildrenNode;
  while(cur != NULL) {
    if((!xmlStrcmp(cur->name, (const xmlChar *)"navstate"))) {
      xmlBufferPtr xmlbuf;
      if(navstate != NULL && !navstate_read) {
	if((xmlbuf = xmlBufferCreate()) == NULL) {
	  return -1;
	} 
	xmlNodeDump(xmlbuf, bm->doc, cur, 0, 0);
	*navstate = strdup(xmlBufferContent(xmlbuf));
	xmlBufferFree(xmlbuf);
	navstate_read = 1;
      }
    } else if((!xmlStrcmp(cur->name, (const xmlChar *)"usercomment"))) {
      if(usercomment != NULL && !usercomment_read) {
	data = xmlNodeListGetString(bm->doc, cur->xmlChildrenNode, 1);
	*usercomment = strdup(data);
	xmlFree(data);
	usercomment_read = 1;
      }
    } else if((!xmlStrcmp(cur->name, (const xmlChar *)"appinfo"))) {
      if((appname != NULL) && (appinfo != NULL)) {
	xmlChar *prop;
	if((prop = xmlGetProp(cur, "appname")) != NULL) {
	  if(!xmlStrcmp(prop, (const xmlChar *)appname) && !appinfo_read) {
	    data = xmlNodeListGetString(bm->doc, cur->xmlChildrenNode, 1);
	    *appinfo = strdup(data);
	    xmlFree(data);
	    appinfo_read = 1;
	  }
	  xmlFree(prop);
	}
      }
    }
    cur = cur->next;
  }

  return 0;
}
 
/**
 * Add a bookmark for the current disc.
 *
 * @param bm Handle from DVDBookmarkOpen.
 * There can be several bookmarks for one disc.
 * The bookmark will be added at the end of the list of bookmarks
 * for this disc.
 * @param navstate The navstate data for the bookmark.
 * @param usercomment The usercomment data for the bookmark.
 * @param appname If you supply the name of your application here, you will
 * be able to supply application specific data in appinfo.
 * @param appinfo The appinfo data
 *
 * All char * should point to a nullterminated string.
 * The only char * that must be set is navstate, the rest can be NULL.
 *
 * @return 0 on success, -1 on failure.
 */
int DVDBookmarkAdd(DVDBookmark_t *bm,
		   const char *navstate, const char *usercomment,
		   const char *appname, const char *appinfo)
{
  xmlNodePtr cur;

  if(!bm || !navstate) {
    return -1;
  }

  if((cur = xmlDocGetRootElement(bm->doc)) == NULL) {
    return -1;
  }
  
  if((cur = xmlNewChild(cur, NULL, "bookmark", NULL)) == NULL) {
    return -1;
  } else {
    xmlDocPtr navdoc = NULL;
    xmlNodePtr navnode, navcopy;
    
    if((navdoc = xmlParseDoc(navstate)) == NULL) {
      return -1;
    }
   
    if((navnode = xmlDocGetRootElement(navdoc)) == NULL) {
      return -1;
    }
    
    if((navcopy = xmlDocCopyNode(navnode, bm->doc, 1)) == NULL) {
      return -1;
    }
    
    xmlFreeDoc(navdoc);
    
    xmlAddChild(cur, navcopy);
  }

  if(usercomment) {
    if((xmlNewTextChild(cur, NULL, "usercomment", usercomment)) == NULL) {
      return -1;
    }
  }

  if(appname && appinfo) {
    if((cur = xmlNewTextChild(cur, NULL, "appinfo", appinfo)) == NULL) {
      return -1;
    }
    xmlNewProp(cur, "appname", appname);    
  }
  
  return 0;						      
}


/**
 * Set the appinfo in a bookmark.
 *
 * @param bm Handle from DVDBookmarkOpen.
 * @param appname The name of your application here, you will
 * be able to supply application specific data in appinfo.
 * The appname "common" is used for standardized appinfo that all
 * players can read/use.
 * @param appinfo The appinfo data.
 * If there already is appinfo for the corresponding appname
 * it will be replaced.
 * If you pass NULL or a pointer to the empty string ""
 * the previous appinfo, if any, will be removed.
 * All char * should point to a nullterminated string.
 *
 * @return 0 on success, -1 on failure.
 */
int DVDBookmarkSetAppInfo(DVDBookmark_t *bm,
			  int nr,
			  const char *appname, const char *appinfo)
{
  xmlNodePtr cur;
  xmlNodePtr cur_bm;
  
  if(!bm || !appname) {
    return -1;
  }

  if((cur = xmlDocGetRootElement(bm->doc)) == NULL) {
    return -1;
  }

  cur_bm = get_bookmark(bm->doc, cur, nr);
  
  cur = cur_bm;
  if(cur == NULL) {
    return -1;
  }
  
  
  //if there is an appinfo with the same appname already remove it
  cur = cur->xmlChildrenNode;
  while(cur != NULL) {
    xmlNodePtr next = cur->next;
    if((!xmlStrcmp(cur->name, (const xmlChar *)"appinfo"))) {
      xmlChar *prop;
      if((prop = xmlGetProp(cur, "appname")) != NULL) {
	if(!xmlStrcmp(prop, (const xmlChar *)appname)) {
	  xmlFree(prop);
	  xmlUnlinkNode(cur);
	  xmlFreeNode(cur);
	} else {
	  xmlFree(prop);
	}
      }
    }
    cur = next;
  }

  cur = cur_bm;
  
  if(appinfo && (appinfo[0] != '\0')) {
    if((cur = xmlNewTextChild(cur, NULL, "appinfo", appinfo)) == NULL) {
      return -1;
    }
    xmlNewProp(cur, "appname", appname);    
  }

  return 0;						      
}

/**
 * Set the usercomment in a bookmark.
 *
 * @param bm Handle from DVDBookmarkOpen.
 * @param usercomment The comment.
 * If there already is a usercomment, it will be replaced.
 * Setting it to NULL or the empty string "" will remove a previous comment.
 * All char * should point to a nullterminated string.
 *
 * @return 0 on success, -1 on failure.
 */
int DVDBookmarkSetUserComment(DVDBookmark_t *bm, int nr,
			      const char *usercomment)
{
  xmlNodePtr cur;
  xmlNodePtr cur_bm;
  
  if(!bm) {
    return -1;
  }

  if((cur = xmlDocGetRootElement(bm->doc)) == NULL) {
    return -1;
  }

  cur_bm = get_bookmark(bm->doc, cur, nr);
  
  cur = cur_bm;
  if(cur == NULL) {
    return -1;
  }
  
  
  //if there is a usercomment already remove it
  cur = cur->xmlChildrenNode;
  while(cur != NULL) {
    xmlNodePtr next = cur->next;
    if((!xmlStrcmp(cur->name, (const xmlChar *)"usercomment"))) {
      xmlUnlinkNode(cur);
      xmlFreeNode(cur);
    }
    cur = next;
  }

  cur = cur_bm;
  
  if(usercomment && (usercomment[0] != '\0')) {
    if((cur = xmlNewTextChild(cur,NULL, "usercomment", usercomment)) == NULL) {
      return -1;
    }
  }
  
  return 0;						      
}

/**
 * Remove a bookmark for the current disc.
 *
 * @param bm Handle from DVDBookmarkOpen.
 * @param nr The specific bookmark in the list for this disc.
 * The nr is the same as in the DVDBookmarkGet function.
 * The numbering will change once you have removed a bookmark, the numbers
 * after the removed will decrease in value by one so there are no
 * gaps.
 * @return 0 on success, -1 on failure.
 */
int DVDBookmarkRemove(DVDBookmark_t *bm, int nr)
{
  xmlNodePtr cur;

  if(!bm || !(bm->doc) || (nr < 0)) {
    return -1;
  }
  
  if((cur = xmlDocGetRootElement(bm->doc)) == NULL) {
    return -1;
  }
  
  cur = get_bookmark(bm->doc, cur, nr);
  if(cur == NULL) {
    return -1;
  }
  
  xmlUnlinkNode(cur);
  xmlFreeNode(cur);

  return 0;
}


/**
 * Retrieve comment for the current disc.
 *
 * @param bm Handle from DVDBookmarkOpen.
 * @param disccomment This is where a pointer to the disccomment data
 * will be returned. It will be set if the call is succesfull otherwise
 * it will be undefined. You need to free() this when you are done with it.
 * In case nothing is returned it will be set to NULL;
 * @return 0 on success (though no data may be returned) -1 on failure
 */
int DVDBookmarkGetDiscComment(DVDBookmark_t *bm, char **disccomment)
{
  xmlNodePtr cur;
  xmlChar *data;
  if(!bm || !(bm->doc) || !disccomment) {
    return -1;
  }
  
  if((cur = xmlDocGetRootElement(bm->doc)) == NULL) {
    return -1;
  }

  cur = cur->xmlChildrenNode;
  while(cur != NULL) {
    if((!xmlStrcmp(cur->name, (const xmlChar *)"disccomment"))) {
      if(disccomment != NULL) {
	data = xmlNodeListGetString(bm->doc, cur->xmlChildrenNode, 1);
	if(data != NULL) {
	  *disccomment = strdup(data);
	  xmlFree(data);
	  return 0;
	}
      }
    }
    cur = cur->next;
  }
  *disccomment = NULL;
  return 0;
}


/**
 * Set comment for the current disc.
 *
 * @param bm Handle from DVDBookmarkOpen.
 * @param disccomment The disc comment
 */
int DVDBookmarkSetDiscComment(DVDBookmark_t *bm, const char *disccomment)
{
  xmlNodePtr cur, docroot;

  if(!bm || !(bm->doc) || !disccomment) {
    return -1;
  }
  
  if((docroot = xmlDocGetRootElement(bm->doc)) == NULL) {
    return -1;
  }
  cur = docroot;
  cur = cur->xmlChildrenNode;
  while(cur != NULL) {
    xmlNodePtr next = cur->next;
    if((!xmlStrcmp(cur->name, (const xmlChar *)"disccomment"))) {
      xmlUnlinkNode(cur);
      xmlFreeNode(cur);
    }
    cur = next;
  }
  
  cur = docroot;
  cur = cur->xmlChildrenNode;
  
  if(cur != NULL) {  
    xmlNodePtr newnode;
    if(((newnode =  xmlNewTextChild(docroot, NULL, 
				    "disccomment", disccomment))) == NULL) {
      return -1;
    }
    
    xmlGetNodePath(xmlAddPrevSibling(cur, newnode));
  } else {
    if((xmlNewTextChild(docroot, NULL, 
			"disccomment", disccomment)) == NULL) {
      return -1;
    }
  }
  
  return 0;
}


/**
 * Save the bookmark list for the current disc to file.
 * If the list doesn't contain any entries the file will be removed.
 *
 * @param bm Handle from DVDBookmarkOpen.
 * @param compressed If 1 the output file will be compressed and not human
 * readable. If 0 it will be normal indented text.
 * 0 is recommended if not disk space is very limited.
 * DVDBookmarkOpen opens and reads both compressed and uncompressed files.
 *
 * @return 0 on success, -1 on failure.
 */
int DVDBookmarkSave(DVDBookmark_t *bm, int compressed)
{
  xmlNodePtr cur;
  int n;

  if(!bm || !(bm->filename) || !(bm->doc)) {
    return -1;
  }
  if(compressed) {
    xmlSetDocCompressMode(bm->doc, 9);
  } else {
    xmlSetDocCompressMode(bm->doc, 0);
  }
  if(xmlSaveFormatFile(bm->filename, bm->doc, 1) == -1) {
    return -1;
  }

  if((cur = xmlDocGetRootElement(bm->doc)) == NULL) {
    return -1;
  }
  
  n = 0;
  cur = cur->xmlChildrenNode;
  while(cur != NULL) {
    if((!xmlStrcmp(cur->name, (const xmlChar *)"bookmark"))) {
      n++;
    }
    cur = cur->next;
  }
  
  if(n == 0) {
    unlink(bm->filename);
  }

  return 0;
}

/**
 * Close and free memory used for holding the bookmark list.
 * @param bm The handle to be freed. It cannot be used again.
 */
void DVDBookmarkClose(DVDBookmark_t *bm)
{
  if(!bm) {
    return;
  }
  if(bm->filename) {
    free(bm->filename);
    bm->filename = NULL;
  }
  
  if(bm->doc) {
    xmlFreeDoc(bm->doc);
    bm->doc = NULL;
  }
  
  free(bm);
  
  return;
}
