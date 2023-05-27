#ifndef TIMEMATH_H_INCLUDED
#define TIMEMATH_H_INCLUDED

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

#include <time.h>
#include <sys/types.h>
#include <sys/time.h>
#include <inttypes.h>

#ifdef HAVE_CLOCK_GETTIME
#define clocktime_t struct timespec
#else
#define clocktime_t struct timeval
#endif

#ifdef HAVE_CLOCK_GETTIME
#define CT_FRACTION 1000000000
#else
#define CT_FRACTION 1000000
#endif

#ifdef HAVE_CLOCK_GETTIME
#define TIME_S(t)  ((t).tv_sec)
#define TIME_SS(t) ((t).tv_nsec)
#else
#define TIME_S(t)  ((t).tv_sec)
#define TIME_SS(t) ((t).tv_usec)
#endif

#define PTS_BASE 90000

#define PTS_TO_CLOCKTIME(xtime, PTS) { \
  TIME_S(xtime) = PTS/PTS_BASE; \
  TIME_SS(xtime) = (PTS%PTS_BASE)*(CT_FRACTION/PTS_BASE); \
}

void clocktime_get(clocktime_t *d);

void timesub(clocktime_t *d, const clocktime_t *s1, const clocktime_t *s2);

void timeadd(clocktime_t *d, const clocktime_t *s1, const clocktime_t *s2);

int timecompare(const clocktime_t *s1, const clocktime_t *s2); 

void timemul(clocktime_t *d, const clocktime_t *s1, double mult);

#endif /* TIMEMATH_H_INCLUDED */
