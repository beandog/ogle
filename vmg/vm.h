#ifndef VM_H_INCLUDED
#define VM_H_INCLUDED

/* Ogle - A video player
 * Copyright (C) 2000, 2001 Håkan Hjort
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

#include <inttypes.h>
#include <dvdread/ifo_types.h>
#include <dvdread/nav_types.h>
/* TODO: Remove the dependency on ogle/dvd.h */
#include <ogle/dvd.h>
#include "decoder.h"


typedef enum {
  FP_DOMAIN   = DVD_DOMAIN_FirstPlay,
  VMGM_DOMAIN = DVD_DOMAIN_VMG,
  VTSM_DOMAIN = DVD_DOMAIN_VTSMenu,
  VTS_DOMAIN  = DVD_DOMAIN_VTSTitle
} domain_t;


typedef enum {
  RESET_MODE = 1,
  PLAY_MODE  = 2,
  STILL_MODE = 4,
  PAUSE_MODE = 8
} player_mode_t;  

/**
 * State: SPRM, GPRM, Domain, pgc, pgN, cellN, ?
 */
typedef struct {
  registers_t registers;
  
  pgc_t *pgc; // either this or *pgc is enough?
  
  domain_t domain;
  int vtsN; // 0 is vmgm?
  int pgcN; // either this or *pgc is enough. Which to use?
  int pgN;  // is this needed? can allways fid pgN from cellN?
  int cellN; // current cell
  int blockN; // block offset to the VOPBU within the current cell
  
  player_mode_t mode;
  
  /* Resume info */
  int rsm_vtsN;
  int rsm_blockN; /* of nav_packet */
  uint16_t rsm_regs[5]; /* system registers 4-8 */
  int rsm_pgcN;
  int rsm_cellN;
} dvd_state_t;


// Audio stream number
#define AST_REG      registers.SPRM[1]
// Subpicture stream number
#define SPST_REG     registers.SPRM[2]
// Angle number
#define AGL_REG      registers.SPRM[3]
// Title Track Number
#define TTN_REG     registers.SPRM[4]
// VTS Title Track Number
#define VTS_TTN_REG  registers.SPRM[5]
// PGC Number for this Title Track
#define TT_PGCN_REG  registers.SPRM[6]
// Current Part of Title (PTT) number for (One_Sequential_PGC_Title)
#define PTTN_REG     registers.SPRM[7]
// Highlighted Button Number (btn nr 1 == value 1024)
#define HL_BTNN_REG  registers.SPRM[8]
// Parental Level
#define PTL_REG      registers.SPRM[13]

int set_sprm(unsigned int nr, uint16_t val);
int vm_reset(void);
int vm_init(char *dvdroot);
int vm_start(void);
int vm_eval_cmd(vm_cmd_t *cmd);
int vm_get_next_cell();
int vm_menu_call(DVDMenuID_t menuid, int block);
int vm_resume(void);
int vm_top_pg(void);
int vm_next_pg(void);
int vm_prev_pg(void);
int vm_goup_pgc(void);
int vm_jump_ptt(int pttN);
int vm_jump_title_ptt(int titleN, int pttN);
int vm_jump_title(int titleN);
int vm_time_play(dvd_time_t *time, unsigned int offset);
int vm_get_audio_stream(int audioN);
int vm_get_subp_stream(int subpN);
int vm_get_subp_active_stream(void);
void vm_get_angle_info(int *num_avail, int *current);
void vm_get_audio_info(int *num_avail, int *current);
void vm_get_subp_info(int *num_avail, int *current);
int vm_get_domain(void);
void vm_get_volume_info(int *nrofvolumes, int *volume, 
			int *side, int *nroftitles);
int vm_get_titles(void);
int vm_get_ptts_for_title(DVDTitle_t titleN);
subp_attr_t vm_get_subp_attr(int streamN);
user_ops_t vm_get_uops(void);
audio_attr_t vm_get_audio_attr(int streamN);
video_attr_t vm_get_video_attr(void);
void vm_get_video_res(int *width, int *height);
void vm_get_total_time(dvd_time_t *current_time);
void vm_get_current_time(dvd_time_t *current_time, dvd_time_t *cell_elapsed);
void vm_get_cell_start_time(dvd_time_t *current_time, int cellN);

unsigned int bcd2int(unsigned int bcd);
unsigned int int2bcd(unsigned int number);

char *vm_get_state_str(int blockN);
int vm_set_state_str(char *state_str);

int vm_get_udf_volids(char *volid, unsigned int volidlen,
		      unsigned char *volsetid, unsigned int volsetidlen);
int vm_get_iso_volids(char *volid, unsigned int volidlen,
		      unsigned char *volsetid, unsigned int volsetidlen);

#endif /* VM_H_INCLUDED */
