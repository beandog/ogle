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

#include "timemath.h"


#ifndef HAVE_CLOCK_GETTIME
#define tv_nsec tv_usec
#endif


#ifdef HAVE_CLOCK_GETTIME
void clocktime_get(struct timespec *d) {
  clock_gettime(CLOCK_REALTIME, d);
}
#else
void clocktime_get(struct timeval *d) {
  gettimeofday(d, NULL);
}
#endif


// d = s1-s2
void timesub(clocktime_t *d,
	     const clocktime_t *s1, 
	     const clocktime_t *s2)
{
  long tv_sec  = s1->tv_sec - s2->tv_sec;
  long tv_nsec = s1->tv_nsec - s2->tv_nsec;
  
  if(tv_nsec >= CT_FRACTION) {
    tv_sec += 1;
    tv_nsec -= CT_FRACTION;
  } else if(tv_nsec <= -CT_FRACTION) {
    tv_sec -= 1;
    tv_nsec += CT_FRACTION;
  }

  if((tv_sec > 0) && (tv_nsec < 0)) {
    tv_sec -= 1;
    tv_nsec += CT_FRACTION;
  } else if((tv_sec < 0) && (tv_nsec > 0)) {
    tv_sec += 1;
    tv_nsec -= CT_FRACTION;
  }
  
  d->tv_nsec = tv_nsec;
  d->tv_sec = tv_sec;
}  


// d = s1+s2
void timeadd(clocktime_t *d,
	     const clocktime_t *s1, 
	     const clocktime_t *s2)
{
  long tv_sec = s1->tv_sec + s2->tv_sec;
  long tv_nsec = s1->tv_nsec + s2->tv_nsec;
  
  if(tv_nsec >= CT_FRACTION) {
    tv_sec +=1;
    tv_nsec -= CT_FRACTION;
  } else if(tv_nsec <= -CT_FRACTION) {
    tv_sec -=1;
    tv_nsec += CT_FRACTION;
  }

  if((tv_sec > 0) && (tv_nsec < 0)) {
    tv_sec -= 1;
    tv_nsec += CT_FRACTION;
  } else if((tv_sec < 0) && (tv_nsec > 0)) {
    tv_sec += 1;
    tv_nsec -= CT_FRACTION;
  }
  
  d->tv_nsec = tv_nsec;
  d->tv_sec = tv_sec;
}  

int timecompare(const clocktime_t *s1, 
		const clocktime_t *s2)
{
  if(s1->tv_sec > s2->tv_sec)
    return 1;
  if(s1->tv_sec < s2->tv_sec)
    return -1;

  if(s1->tv_nsec > s2->tv_nsec)
    return 1;
  if(s1->tv_nsec < s2->tv_nsec)
    return -1;
  
  return 0;
}


void timemul(clocktime_t *d, const clocktime_t *s1, double mult)
{

  double tv = (double)s1->tv_sec*CT_FRACTION + (double)s1->tv_nsec;
  
  tv *= mult;
  
  d->tv_nsec = ((long long int)tv)%CT_FRACTION;
  d->tv_sec = ((long long int)tv)/CT_FRACTION;
}

