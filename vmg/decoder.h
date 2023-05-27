#ifndef DECODER_H_INCLUDED
#define DECODER_H_INCLUDED

/* Ogle - A video player
 * Copyright (C) 2000, 2001 Martin Norb�ck, H�kan Hjort
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
#include <dvdread/ifo_types.h> // vm_cmd_t

typedef enum {
  LinkNoLink  = 0,

  LinkTopC    = 1,
  LinkNextC   = 2,
  LinkPrevC   = 3,

  LinkTopPG   = 5,
  LinkNextPG  = 6,
  LinkPrevPG  = 7,

  LinkTopPGC  = 9,
  LinkNextPGC = 10,
  LinkPrevPGC = 11,
  LinkGoUpPGC = 12,
  LinkTailPGC = 13,

  LinkRSM     = 16,

  LinkPGCN,
  LinkPTTN,
  LinkPGN,
  LinkCN,

  Exit,

  JumpTT, /* 22 */
  JumpVTS_TT,
  JumpVTS_PTT,

  JumpSS_FP,
  JumpSS_VMGM_MENU,
  JumpSS_VTSM,
  JumpSS_VMGM_PGC,

  CallSS_FP, /* 29 */
  CallSS_VMGM_MENU,
  CallSS_VTSM,
  CallSS_VMGM_PGC,

  PlayThis
} link_cmd_t;

typedef struct {
  link_cmd_t command;
  uint16_t   data1;
  uint16_t   data2;
  uint16_t   data3;
} link_t;

typedef struct {
  uint16_t SPRM[24];
  uint16_t GPRM[16];
  /* Need to have some thing to indicate normal/counter mode for every GPRM */
  /* int GPRM_mode[16]; */
} registers_t;

int vmEval_CMD(vm_cmd_t commands[], int num_commands, 
	       registers_t *registers, link_t *return_values);

void vmPrint_LINK(link_t value);

#endif /* DECODER_H_INCLUDED */
