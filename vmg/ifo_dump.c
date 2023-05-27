/* Ogle - A video player
 * Copyright (C) 2000, 2001 Bj�rn Englund, H�kan Hjort
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
#include <inttypes.h>

#include <dvdread/ifo_types.h>
#include <dvdread/ifo_read.h>
#include <dvdread/ifo_print.h>

static char *program_name;

static void print_ifo(char *path, int title);

void usage(void)
{
  fprintf(stderr, "Usage: %s <dvd path> <title number>\n", 
	  program_name);
}

int main(int argc, char *argv[])
{
  program_name = argv[0];

  if(argc != 3) {
    usage();
    return 1;
  }
  
  print_ifo(argv[1], atoi(argv[2]));
  exit(0);
}


static void print_ifo(char *path, int title) {
  dvd_reader_t *dvd;
  ifo_handle_t *h;
  
  dvd = DVDOpen(path);
  if(!dvd) {
    fprintf(stderr, "Can't open disc %s!\n", path);
    return;
  }

  if((h = ifoOpen(dvd, title)) == NULL)
    return;
    
  if(h->vmgi_mat != NULL) {
    
    printf("VMG top level\n-------------\n");
    ifoPrint_VMGI_MAT(h->vmgi_mat);
      
    printf("\nFirst Play PGC\n--------------\n");
    ifoPrint_PGC(h->first_play_pgc);
      
    printf("\nTitle Track search pointer table\n");
    printf(  "------------------------------------------------\n");
    ifoPrint_TT_SRPT(h->tt_srpt);
      
    printf("\nMenu PGCI Unit table\n");
    printf(  "--------------------\n");
    if(h->vmgi_mat->vmgm_pgci_ut != 0) {
      ifoPrint_PGCI_UT(h->pgci_ut);
    } else 
      printf("No Menu PGCI Unit table present\n");
      
    printf("\nParental Manegment Information table\n");
    printf(  "------------------------------------\n");
    if(h->vmgi_mat->ptl_mait != 0) {
      ifoPrint_PTL_MAIT(h->ptl_mait);
    } else
      printf("No Parental Management Information present\n");
      
    printf("\nVideo Title Set Attribute Table\n");
    printf(  "-------------------------------\n");
    ifoPrint_VTS_ATRT(h->vts_atrt);

      
    printf("\nText Data Manager Information\n");
    printf(  "-----------------------------\n");
    if(h->vmgi_mat->txtdt_mgi != 0) {
      //ifoPrint_TXTDT_MGI(h->txtdt_mgi);
      printf("Can't print Text Data Manager Information yet\n");
    } else
      printf("No Text Data Manager Information present\n");
      
    if(1) {
      
      printf("\nCell Address table\n");
      printf(  "-----------------\n");
      if(h->vmgi_mat->vmgm_c_adt != 0) {
	ifoPrint_C_ADT(h->menu_c_adt);
      } else
	printf("No Cell Address table present\n");
      
      printf("\nVideo Title set Menu VOBU address map\n");
      printf(  "-----------------\n");
      if(h->vmgi_mat->vmgm_vobu_admap != 0) {
	ifoPrint_VOBU_ADMAP(h->menu_vobu_admap);
      } else
	printf("No Menu VOBU address map present\n");
    }
  }

  if(h->vtsi_mat != NULL) {
      
    printf("VTS top level\n-------------\n");
    ifoPrint_VTSI_MAT(h->vtsi_mat);
      
    printf("\nPart of title search pointer table information\n");
    printf(  "----------------------------------------------\n");
    ifoPrint_VTS_PTT_SRPT(h->vts_ptt_srpt);
       
    printf("\nPGCI Unit table\n");
    printf(  "--------------------\n");
    ifoPrint_PGCIT(h->vts_pgcit);
      
    printf("\nMenu PGCI Unit table\n");
    printf(  "--------------------\n");
    if(h->vtsi_mat->vtsm_pgci_ut != 0) {
      ifoPrint_PGCI_UT(h->pgci_ut);
    } else
      printf("No Menu PGCI Unit table present\n");
      
    if(1) {
      
      printf("\nTime Map table\n");
      printf(  "-----------------\n");
      if(h->vtsi_mat->vts_tmapt != 0) {
	ifoPrint_VTS_TMAPT(h->vts_tmapt);
      } else
	printf("No Time Map table present\n");
      
      printf("\nMenu Cell Address table\n");
      printf(  "-----------------\n");
      if(h->vtsi_mat->vtsm_c_adt != 0) {
	ifoPrint_C_ADT(h->menu_c_adt);
      } else
	printf("No Cell Address table present\n");
      
      printf("\nVideo Title Set Menu VOBU address map\n");
      printf(  "-----------------\n");
      if(h->vtsi_mat->vtsm_vobu_admap != 0) {
	ifoPrint_VOBU_ADMAP(h->menu_vobu_admap);
      } else
	printf("No Menu VOBU address map present\n");
      
      printf("\nCell Address table\n");
      printf(  "-----------------\n");
      ifoPrint_C_ADT(h->vts_c_adt);
      
      printf("\nVideo Title Set VOBU address map\n");
      printf(  "-----------------\n");
      ifoPrint_VOBU_ADMAP(h->vts_vobu_admap);
      
    }
  }


  /* Vob */
  
}




