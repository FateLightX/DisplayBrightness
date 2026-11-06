#include "LatestRequest.hpp"
#include <cassert>
#include <cstdint>
#include <cstdio>
int main(){
 dbhw::LatestRequest q;bool schedule=false;
 assert(q.submit(10,schedule)&&schedule);
 unsigned desired=q.take();uint64_t completed=1;
 // Hardware fresh Get is in progress while these input events arrive.
 assert(q.submit(30,schedule)&&!schedule);assert(q.submit(80,schedule)&&!schedule);
 assert(q.refreshAfterRead(3,desired,completed));
 assert(desired==80&&completed==3&&!q.pending);
 // A later input arriving after target selection must remain for another cycle.
 assert(q.submit(20,schedule)&&!schedule);assert(q.complete(true));
 assert(q.take()==20);assert(!q.complete(true));
 assert(q.submit(70,schedule)&&schedule);desired=q.take();completed=5;
 assert(q.refreshAfterRead(5,desired,completed));assert(desired==70&&completed==5);
 assert(!q.submit(101,schedule));assert(q.refreshAfterRead(5,desired,completed));assert(desired==70);
 // Failure recovery preserves the newest pending target, not the captured one.
 assert(q.submit(0,schedule)&&!schedule);assert(q.complete(false));
 assert(q.refreshAfterRead(6,desired,completed));assert(desired==0&&completed==6);
 q.stop();assert(!q.refreshAfterRead(7,desired,completed));
 puts("fresh-read latest selection; matching sequence; subsequent input preserved; failed-cycle recovery; zero and stopped endpoint pass");
}
