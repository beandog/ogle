#ifndef DVDBOOKMARKS_H_INCLUDED
#define DVDBOOKMARKS_H_INCLUDED

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

/* helper functions */
typedef struct DVDBookmark_s DVDBookmark_t;

DVDBookmark_t *DVDBookmarkOpen(const unsigned char dvdid[16],
			       const char *file, int create);
int DVDBookmarkGetNr(DVDBookmark_t *bm);
int DVDBookmarkGet(DVDBookmark_t *bm, int nr,
		   char **navstate, char **usercomment,
		   const char *appname, char **appinfo);
int DVDBookmarkAdd(DVDBookmark_t *bm,
		   const char *navstate, const char *usercomment,
		   const char *appname, const char *appinfo);
int DVDBookmarkSetAppInfo(DVDBookmark_t *bm, int nr,
			  const char *appname, const char *appinfo);
int DVDBookmarkSetUserComment(DVDBookmark_t *bm, int nr,
			      const char *usercomment);
int DVDBookmarkRemove(DVDBookmark_t *bm, int nr);
int DVDBookmarkGetDiscComment(DVDBookmark_t *bm, char **disccomment);
int DVDBookmarkSetDiscComment(DVDBookmark_t *bm, const char *disccomment);
int DVDBookmarkSave(DVDBookmark_t *bm, int compressed);
void DVDBookmarkClose(DVDBookmark_t *bm);
/* end helper functions */

#endif /* DVDBOOKMARKS_H_INCLUDED */
