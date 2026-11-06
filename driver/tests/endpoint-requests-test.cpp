#include "EndpointRequests.hpp"
#include <cassert>
#include <cstdio>
#include <initializer_list>
int main(){
 dbhw::EndpointRequests r;bool schedule=true;
 // A visible cached slider can cause an automatic zero restore. Neither
 // that nor an early drag may start work or mutate the bootstrap target.
 for(unsigned value:{0U,20U,50U,100U}){
  assert(!r.submitBrightness(value,schedule));assert(!schedule);
  assert(!r.busy&&!r.pending&&!r.writable());
 }
 assert(r.submitBootstrap(schedule)&&schedule);assert(r.busy&&r.pending);
 assert(!r.submitBrightness(100,schedule)&&!schedule);assert(r.desired==0);
 assert(r.take()==0&&!r.pending);
 assert(!r.submitBrightness(0,schedule)&&!schedule&&!r.pending);
 assert(r.complete(false)); // Failed read retries without adopting early input.
 assert(!r.submitBrightness(75,schedule)&&!schedule&&r.desired==0);
 assert(r.submitBootstrap(schedule)&&!schedule); // Repeated doUpdate is a no-op.
 assert(r.take()==0);
 r.initialized=true; // Only after a validated read and registry publication.
 assert(!r.complete(true));assert(!r.busy&&!r.pending&&r.writable());
 // There is no delayed default write. The first post-read user input starts it.
 assert(r.submitBootstrap(schedule)&&!schedule&&!r.pending);
 assert(r.submitBrightness(37,schedule)&&schedule);
 assert(r.take()==37);
 assert(r.submitBrightness(63,schedule)&&!schedule);
 assert(r.submitBootstrap(schedule)&&!schedule&&r.desired==63);
 unsigned desired=37;uint64_t sequence=1;
 assert(r.refreshAfterRead(2,desired,sequence)&&desired==63&&sequence==2);
 assert(!r.complete(true));
 // A terminal endpoint stays closed even if it was initialized in the past.
 r.stop();assert(!r.writable());
 assert(!r.submitBrightness(50,schedule)&&!schedule);
 assert(!r.submitBootstrap(schedule)&&!schedule);
 // Retirement creates a fresh endpoint; it cannot inherit the previous ready bit.
 r=dbhw::EndpointRequests{};
 assert(!r.submitBrightness(50,schedule)&&!schedule);
 assert(r.submitBootstrap(schedule)&&schedule);
 puts("endpoint requests: early writes discarded, bootstrap never replays defaults, ready writes coalesce, stopped/reused endpoints stay closed");
}
