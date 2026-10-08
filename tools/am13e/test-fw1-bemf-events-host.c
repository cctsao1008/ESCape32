#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "hw_bemf_am13e_events.h"
typedef struct {unsigned arms,cancels,commutes;uint32_t ticks;} fixture_t;
static void arm(void *p,uint32_t t){fixture_t *f=p;f->arms++;f->ticks=t;}
static void cancel(void *p){((fixture_t*)p)->cancels++;}
static void commute(void *p){((fixture_t*)p)->commutes++;}
int main(void){
 fixture_t f={0};am13e_bemf_event_engine_t e;
 am13e_bemf_event_ops_t ops={arm,cancel,commute,&f};
 am13e_bemf_state_t initial={.interval=1000,.electrical_time=5000};
 assert(am13e_bemf_event_init(&e,&ops,initial,8));
 assert(am13e_bemf_event_capture(&e,499)==AM13E_BEMF_IGNORED);
 assert(f.arms==0 && !e.pending);
 assert(am13e_bemf_event_capture(&e,1000)==AM13E_BEMF_ACCEPTED);
 assert(f.arms==1 && f.ticks==375 && e.pending);
 assert(am13e_bemf_event_capture(&e,1000)==AM13E_BEMF_IGNORED);
 am13e_bemf_event_delay_elapsed(&e);
 assert(f.commutes==1 && !e.pending);
 am13e_bemf_event_delay_elapsed(&e);
 assert(f.commutes==1);
 assert(am13e_bemf_event_capture(&e,1000)==AM13E_BEMF_ACCEPTED);
 am13e_bemf_event_timeout(&e,0);
 assert(!e.pending && f.cancels==1 && e.policy.sync==0);
 am13e_bemf_event_delay_elapsed(&e);
 assert(f.commutes==1);
 puts("[PASS] BEMF capture -> delay -> commutation callback, timeout/cancel");
 return 0;
}
