#ifndef COMMON_H_INCLUDED
#define COMMON_H_INCLUDED

/* Ogle - A video player
 * Copyright (C) 2000, 2001 Björn Englund, Håkan Hjort
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

#include "timemath.h"
#include "queue.h"


#define PICTURES_READY_TO_DISPLAY 0
#define PICTURES_DISPLAYED 1

typedef enum {
  MODE_STOP,
  MODE_PLAY,
  MODE_PAUSE,
  MODE_SPEED
} playmode_t;

typedef enum {
  OFFSET_NOT_VALID,
  OFFSET_VALID
} offset_valid_t;

typedef struct {
  clocktime_t rt;
  clocktime_t scr;
  double speed;
  //  int speed_frac_n;
  //  int speed_frac_d; 
} sync_point_t;


typedef struct {
  int scr_id;
  offset_valid_t offset_valid;
  int sync_master;
  sync_point_t sync_point;
} ctrl_time_t;


#define SYNC_NONE 0
#define SYNC_VIDEO 1
#define SYNC_AUDIO 2

typedef struct {
  playmode_t mode;
  double speed;
  int sync_master;
} ctrl_data_t;

typedef struct {
  int shmid;
  int size;
} shm_bufinfo_t;

#endif /* COMMON_H_INCLUDED */
