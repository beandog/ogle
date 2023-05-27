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

#include <stdio.h>

#include "sync.h"
#include "common.h"
#include "timemath.h"


void calc_realtime_from_scrtime(clocktime_t *rt, clocktime_t *scrtime,
				sync_point_t *sp)
{

  double inv_speed = 1.0/sp->speed;

  timesub(rt, scrtime, &sp->scr);
  timemul(rt, rt, inv_speed);
  timeadd(rt, rt, &sp->rt);

}

void calc_realtime_left_to_scrtime(clocktime_t *time_left,
				   clocktime_t *rt,
				   clocktime_t *scrtime,
				   sync_point_t *sp)
{

  calc_realtime_from_scrtime(time_left, scrtime, sp);
  timesub(time_left, time_left, rt);

}


void set_sync_point(ctrl_time_t *ctrl_time, clocktime_t *rt,
		    clocktime_t *scrtime, double speed)
{
  ctrl_time->sync_point.rt = *rt;
  ctrl_time->sync_point.scr = *scrtime;
  ctrl_time->sync_point.speed = speed;
  ctrl_time->offset_valid = OFFSET_VALID;

}

void set_speed(sync_point_t *sp, double speed)
{
  clocktime_t cur_scrtime;
  clocktime_t cur_rt;
  
  clocktime_get(&cur_rt);
  timesub(&cur_scrtime, &cur_rt, &sp->rt); 
  timemul(&cur_scrtime, &cur_scrtime, sp->speed);
  timeadd(&cur_scrtime, &cur_scrtime, &sp->scr);

  
  sp->speed = speed;
  sp->rt = cur_rt;
  sp->scr = cur_scrtime;
  
}
