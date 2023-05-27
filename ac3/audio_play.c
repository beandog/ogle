/* Ogle - A video player
 * Copyright (C) 2002 Björn Englund, Håkan Hjort
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

#include "common.h"
#include "sync.h"
#include "debug_print.h"

#include "audio_play.h"
#include "decode_private.h" // FIXME

#include "parse_config.h"

extern ctrl_data_t *ctrl_data;
extern ctrl_time_t *ctrl_time;


int play_samples(adec_handle_t *h, int scr_nr, uint64_t PTS, int pts_valid)
{
  static clocktime_t last_sync = { 0, 0 };
  static int last_sync_set = 0;
  static int samples_written;
  static int old_delay = 0;
  clocktime_t odelay_call_time;
  int bytes_to_write;
  int delay;
  int srate;
  static int frqlck_cnt = 0;
  static double frq_correction = 1.0;
  audio_sync_t *s = &(h->config->sync);
  int odelay_fail = 0;
  
  clocktime_get(&odelay_call_time);
  //sync code ...
  if((s->type == SyncType_odelay) &&
     !(odelay_fail = ogle_ao_odelay(h->config->adev_handle, &delay))) {
    
    
    //fprintf(stderr, "(%d)", delay);
    
    if(!s->delay_resolution_set) {
      int delay_diff;
      int delay_step;
      
      delay_diff = delay - s->prev_delay;
      
      if(s->prev_delay != -1) {
	delay_step = s->samples_added - delay_diff;
	
	if((delay_step > 0) && (delay_step < s->delay_resolution)) {
	  if(delay_step < h->config->ainfo->sample_rate/200) {
	    s->delay_resolution = h->config->ainfo->sample_rate/200;
	    s->max_sync_diff = 2 * s->delay_resolution *
	      (CT_FRACTION / h->config->ainfo->sample_rate);
	    NOTE("delay resolution better than: 0.%09d, %d samples, rate: %d\n",
		 s->max_sync_diff/2, s->delay_resolution,
		 h->config->ainfo->sample_rate);
	    s->delay_resolution_set = 1;
	  } else {
	    s->delay_resolution = delay_step;
	    s->max_sync_diff = 2 * s->delay_resolution *
	      (CT_FRACTION / h->config->ainfo->sample_rate);
	    
	    NOTE("delay resolution: 0.%09d, %d samples, rate: %d\n",
		 s->max_sync_diff/2, s->delay_resolution,
		 h->config->ainfo->sample_rate);
	  }
	}
      }
      s->prev_delay = delay;
    }
    
    if(ctrl_data->speed == 1.0) {
      clocktime_t real_time, scr_time, delay_time;
      clocktime_t delta_sync_time;
      clocktime_t sleep_time = { 0, 0 };

      clocktime_get(&real_time);

      timesub(&odelay_call_time, &real_time, &odelay_call_time);
      if(TIME_S(odelay_call_time) > 0 ||
	 TIME_SS(odelay_call_time) > (CT_FRACTION/1000)) {
	//we don't have an accurate time for the odelay call
	// so we skip syncing for this packet
	pts_valid = 0;
	fprintf(stderr, "@");
	/*
	fprintf(stderr, "diff: %ld.%09ld\n",
		TIME_S(test_time), TIME_SS(test_time));
	*/
      }
      
      if(ctrl_time[scr_nr].sync_master <= SYNC_AUDIO) {
	
	ctrl_time[scr_nr].sync_master = SYNC_AUDIO;
	
	if(pts_valid) {
	  int delta_samples;
	  static double prev_df = 1.0;
	  double drift_factor, diff;
	  const clocktime_t buf_time = { 0, CT_FRACTION/5 };
	  
	  srate = h->config->ainfo->sample_rate * frq_correction;
	  //fprintf(stderr, "(%d)", srate);
	  
	  TIME_S(delay_time) =
	    ((int64_t)delay * (int64_t)CT_FRACTION/srate) /
	    CT_FRACTION;
	  
	  TIME_SS(delay_time) = 
	    ((int64_t)delay*(int64_t)CT_FRACTION/srate) %
	    CT_FRACTION;
	  
	  if((TIME_S(delay_time) > 0) ||
	     (TIME_SS(delay_time) > TIME_SS(buf_time))) {
	    timesub(&sleep_time, &delay_time, &buf_time); 
	  }
	  

	  PTS_TO_CLOCKTIME(scr_time, PTS);
	  // add constant offset
	  timeadd(&scr_time, &scr_time, &s->offset);

	  if(ctrl_time[scr_nr].offset_valid == OFFSET_NOT_VALID) {
	    timeadd(&delay_time, &delay_time, &real_time);
	    set_sync_point(&ctrl_time[scr_nr],
			   &delay_time,
			   &scr_time,
			   ctrl_data->speed);

	  } else {
	    clocktime_t t1, t2;
	    calc_realtime_left_to_scrtime(&t1,
					  &real_time,
					  &scr_time,
					  &(ctrl_time[scr_nr].sync_point));
	    /*
	      DNOTE("time left %ld.%+010ld\n", TIME_S(t1), TIME_SS(t1));
	      DNOTE("delay %ld.%+010ld\n", TIME_S(delay_time), TIME_SS(delay_time));
	    */
	  
	    timesub(&t2, &delay_time, &t1);
	    if(TIME_S(t2) > 0 || TIME_SS(t2) > s->max_sync_diff ||
	       TIME_S(t2) < 0 || TIME_SS(t2) < -s->max_sync_diff) {
	      
	      if(TIME_S(t2) > 0 || TIME_SS(t2) > 4*s->max_sync_diff ||
		 TIME_S(t2) < 0 || TIME_SS(t2) < -4*s->max_sync_diff) {
		DNOTE("%ld.%+010ld s off, resyncing\n",
		      TIME_S(t2), TIME_SS(t2));
	      } else { 
		//fprintf(stderr, "(%d)", delay);
		if(TIME_SS(t2) > 0) {
		  fprintf(stderr, "+");
		} else {
		  fprintf(stderr, "-");
		}
		/*
		  {
		  clocktime_t drift;
		  
		  timesub(&drift, &real_time, &last_sync);		
		  fprintf(stderr, "[%+07ld / %+07ld]",
		  TIME_SS(t2), TIME_SS(drift));
		  
		  }
		*/
	      }
	      
	      timeadd(&delay_time, &delay_time, &real_time);
	      
 	      set_sync_point(&ctrl_time[scr_nr],
			     &delay_time,
			     &scr_time,
			     ctrl_data->speed);
	      
	    }
	  }	
	  if(!last_sync_set) {
	    last_sync_set = 1;
	    last_sync = real_time;
	    samples_written = 0;
	  }

	  timesub(&delta_sync_time, &real_time, &last_sync);
	  
	  
	  if(TIME_S(delta_sync_time) > 9) {
	    
	    delta_samples = samples_written - (delay - old_delay);
	    
	    samples_written = 0;
	    last_sync = real_time;
	    old_delay = delay;
	    drift_factor = (double)delta_samples /
	      (((double)(TIME_S(delta_sync_time)) +
		(double)(TIME_SS(delta_sync_time)) / CT_FRACTION) *
	       srate);
	    // fprintf(stderr, "<%ld.%+010ld>", TIME_S(delta_sync_time), TIME_SS(delta_sync_time));
	    //fprintf(stderr, "[%.6f]", drift_factor);
	    
	    diff = drift_factor/prev_df;
	    if(diff > 0.998 && diff < 1.001) {
	      frqlck_cnt++;
	    } else {
	      frqlck_cnt = 0;
	    }
	    
	    {
	      static int avg_cnt = 0;
	      static double avg_drift = 0.0;
	      
	      if(frqlck_cnt > 3) {
		
		if(avg_cnt < 8) {
		  avg_cnt++;
		  avg_drift += drift_factor;
		  
		} else if(avg_cnt == 8) {
		  avg_drift /= avg_cnt;
		  avg_cnt++;
		  
		  diff = frq_correction / avg_drift;
		  fprintf(stderr, "diff: %.6f\n", diff);
		  if(diff < 0.999 || diff > 1.001) {
		    frq_correction = avg_drift;
		    fprintf(stderr, "frq_corr: %.6f\n", frq_correction);
		    fprintf(stderr, "srate: %d\n", 
			    (int)(h->config->ainfo->sample_rate
				  * frq_correction));
		    
		  }
		}
	      } else {
		avg_drift = 0.0;
		avg_cnt = 0;
	      }  
	    }
	    
	    prev_df = drift_factor;
	    
	  }
	}
      }
      
      bytes_to_write = h->output_buf_ptr - h->output_buf;
      
      if(TIME_S(sleep_time) > 0 || TIME_SS(sleep_time) > 0) {
	//sleep so we don't buffer more than buf_time of output
	// needed mostly on solaris which can buffer huge amounts
	/*
	  fprintf(stderr, "sleep: %ld.%09ld\n", 
	      TIME_S(sleep_time),
	      TIME_SS(sleep_time));
	*/
#ifndef HAVE_CLOCK_GETTIME
	struct timespec st;
	st.tv_sec = TIME_S(sleep_time);
	st.tv_nsec = TIME_SS(sleep_time) * 1000;
	
	nanosleep(&st, NULL);
#else
	nanosleep(&sleep_time, NULL);
#endif
      }
      
      ogle_ao_play(h->config->adev_handle, h->output_buf,
		   bytes_to_write);
      
      s->samples_added = bytes_to_write / h->config->ainfo->sample_frame_size;
      samples_written += s->samples_added;
      
    }
  } else if((s->type == SyncType_clock) || odelay_fail) {
    static clocktime_t last_rt = { -1, 0 };
    static clocktime_t in_outputbuf = { 0, 0 };
    static clocktime_t delay = { 0, 0 };
    clocktime_t sleep_time = {0, 0};
    clocktime_t real_time;

    if(odelay_fail) {
      ERROR("%s", "odelay failed, falling back to clock sync\n");
      s->type = SyncType_clock;
    }

    s->max_sync_diff = 20000000;
    /*---------*/
    if(ctrl_data->speed == 1.0) {
      clocktime_t  scr_time, delay_time;
      const clocktime_t buf_time = { 0, CT_FRACTION/5 };
      
      clocktime_get(&real_time);
      
      if(TIME_S(last_rt) != -1) {
	clocktime_t delta_time;

	timesub(&delta_time, &real_time, &last_rt);
	/*
	fprintf(stderr, "delta: %ld.%09ld\n",
		TIME_S(delta_time), TIME_SS(delta_time));
	*/
	timesub(&delay, &delay, &delta_time);
	
	if(TIME_S(delay) < 0 || TIME_SS(delay) < 0) {
	  TIME_S(delay) = 0;
	  TIME_SS(delay) = 0;
	}
	
      } else {
	TIME_S(delay) = 0;
	TIME_SS(delay) = 0;
      }

      delay_time = delay;
      
      if(TIME_S(delay_time) > 0 || TIME_SS(delay_time) > TIME_SS(buf_time)) {
	timesub(&sleep_time, &delay_time, &buf_time); 
      } 
      /*
      fprintf(stderr, "sleep: %ld.%09ld, delay_time: %ld.%09ld\n",
	      TIME_S(sleep_time),
	      TIME_SS(sleep_time),
	      TIME_S(delay_time),
	      TIME_SS(delay_time));
      */
      
      if(pts_valid) {
	clocktime_t t1, t2;
	PTS_TO_CLOCKTIME(scr_time, PTS);

	//add constant offset
	timeadd(&scr_time, &scr_time, &s->offset);
	
	calc_realtime_left_to_scrtime(&t1,
				      &real_time,
				      &scr_time,
				      &(ctrl_time[scr_nr].sync_point));
	/*
	  DNOTE("time left %ld.%+010ld\n", TIME_S(t1), TIME_SS(t1));
	  DNOTE("delay %ld.%+010ld\n", TIME_S(delay_time), TIME_SS(delay_time));
	*/
	
	timesub(&t2, &delay_time, &t1);
	if(TIME_S(t2) > 0 || TIME_SS(t2) > s->max_sync_diff ||
	   TIME_S(t2) < 0 || TIME_SS(t2) < -s->max_sync_diff) {
	  
	  if(TIME_S(t2) > 0 || TIME_SS(t2) > 2*s->max_sync_diff ||
	     TIME_S(t2) < 0 || TIME_SS(t2) < -2*s->max_sync_diff) {
	    DNOTE("%ld.%+010ld s off, resyncing\n",
		  TIME_S(t2), TIME_SS(t2));
	  } else { 
	    //fprintf(stderr, "(%d)", delay);
	    if(TIME_SS(t2) > 0) {
	      fprintf(stderr, "/");
	    } else {
	      fprintf(stderr, "-");
	    }
	    
	  }
	  
	  timeadd(&delay_time, &delay_time, &real_time);

	  set_sync_point(&ctrl_time[scr_nr],
			 &delay_time,
			 &scr_time,
			 ctrl_data->speed);
	  
	}
      }
    }
    
    
    if(ctrl_data->speed == 1.0) {
      if(TIME_S(sleep_time) > 0 || TIME_SS(sleep_time) > 0) {
	//sleep so we don't buffer more than buf_time of output
	// needed mostly on solaris which can buffer huge amounts
	
#ifndef HAVE_CLOCK_GETTIME
	struct timespec st;
	st.tv_sec = TIME_S(sleep_time);
	st.tv_nsec = TIME_SS(sleep_time) * 1000;
	
	nanosleep(&st, NULL);
	
#else
	nanosleep(&sleep_time, NULL);
#endif
      }
      
      last_rt = real_time;

      bytes_to_write = h->output_buf_ptr - h->output_buf;
      ogle_ao_play(h->config->adev_handle, h->output_buf,
		   bytes_to_write);  

      TIME_S(in_outputbuf) =
	((int64_t)(bytes_to_write / h->config->ainfo->sample_frame_size) * 
	 (int64_t)CT_FRACTION / h->config->ainfo->sample_rate) /
	CT_FRACTION;
      
      TIME_SS(in_outputbuf) = 
	((int64_t)(bytes_to_write / h->config->ainfo->sample_frame_size) *
	 (int64_t)CT_FRACTION / h->config->ainfo->sample_rate) %
	CT_FRACTION;
      
      timeadd(&delay, &delay, &in_outputbuf);
      
      /*
      fprintf(stderr, "delay: %ld.%09ld\n",
	      TIME_S(delay), TIME_SS(delay));
      */
    } else {
      TIME_S(delay) = 0;
      TIME_SS(delay) = 0;
    }
    /*--------*/
  }
  
  
  return 0;
}
  
