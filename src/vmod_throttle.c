#include <stdlib.h>

#include "vrt.h"
#include "bin/varnishd/cache.h"

#include "vcc_if.h"

struct vmodth_call {
  double time;
  struct vmodth_call *next;
  struct vmodth_call *prev;
};

struct vmodth_calls {
  struct vmodth_call* first;
  struct vmodth_call* last;
  int nb_calls_sec;
  struct vmodth_call* last_call_sec;
  int nb_calls_min;
  struct vmodth_call* last_call_min;
  int nb_calls_hour;
  struct vmodth_call* last_call_hour;
};

int
init_function(struct vmod_priv *priv, const struct VCL_conf *conf)
{
	return (0);
}

double
vmod_is_allowed(struct sess *sp, struct vmod_priv *pc, struct sockaddr_storage *ip, const char* key, int call_limit_per_sec, int call_limit_per_min, int call_limit_per_hour)
{
  struct vmodth_calls *calls;
	struct vmodth_call* tmp;

  //Init data structure
  calls = ((struct vmodth_calls*)pc->priv);
  if (!calls) {
    pc->priv = malloc(sizeof(struct vmodth_calls));
    AN(pc->priv);
    pc->free = free;
    calls = ((struct vmodth_calls*)pc->priv);
    calls->first = NULL;
    calls->last = NULL;
    calls->nb_calls_sec = 0;
    calls->last_call_sec = NULL;
    calls->nb_calls_min = 0;
    calls->last_call_min = NULL;
    calls->nb_calls_hour = 0;
    calls->last_call_hour = NULL;
  }

  //Get time
  struct timeval tv;
  gettimeofday(&tv, NULL);

  //Add the call in the call list
  struct vmodth_call* new_call = malloc(sizeof(struct vmodth_call));
  new_call->time = tv.tv_sec+tv.tv_usec/1000000.0;
  new_call->next = calls->first;
  new_call->prev = NULL;
  if(calls->first) {
    calls->first->prev = new_call;
  }
  calls->first = new_call;
  if(calls->last == NULL) {
    calls->last = new_call;
  }

  //Update the windows counters and pointers
  _vmod_update_window_counter(new_call, &calls->last_call_sec, &calls->nb_calls_sec, 1);
  _vmod_update_window_counter(new_call, &calls->last_call_min, &calls->nb_calls_min, 60);
  _vmod_update_window_counter(new_call, &calls->last_call_hour, &calls->nb_calls_hour, 3600);

  //Remove the older entries older than the maximum window we are tracking, 1h
  _vmod_remove_older_entries(calls, 3600);

  //Now lets check if we are allowed
  //If we are not, let's return how much time we should wait before being allowed
  double result = 0;
  if(calls->nb_calls_sec > call_limit_per_sec) {
    result = 1.0 - (new_call->time - calls->last_call_sec->time);
  }

  return result;
}

// Update the window counter (e.g. the last minute calls counter)
void _vmod_update_window_counter(struct vmodth_call* new_call, struct vmodth_call** last_window_call, int* window_counter, int window_length) {
  (*window_counter)++;
  if(*last_window_call == NULL) {
    *last_window_call = new_call;
  }
  while((*last_window_call)->prev) {
    if((*last_window_call)->time < new_call->time - window_length) {
      (*last_window_call) = (*last_window_call)->prev;
      (*window_counter)--;
    }
    else {
      break;
    }
  }  
}

//Remove older entries
void _vmod_remove_older_entries(struct vmodth_calls* calls, int max_window_length) {
  struct vmodth_call *prev;

  while(calls->last->prev) {
    if(calls->last->time < calls->first->time - max_window_length) {
      prev = calls->last->prev;
      free(calls->last);
      calls->last = prev;
      prev->next = NULL;
    }
    else {
      break;
    }
  }
}