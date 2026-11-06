#include "BootstrapSchedule.hpp"
#include <cassert>
#include <cstdio>
int main(){
 using Turns=dbstate::BootstrapSchedule<3>;Turns t;
 assert(t.delayMs(0)==Turns::none);
 t.offer(0,10);t.offer(1,10);t.offer(2,10);
 t.offer(0,100); // Duplicate wake cannot postpone existing work.
 assert(t.take(9)==Turns::none&&t.take(10)==0);
 assert(t.take(10)==Turns::none&&t.delayMs(1000000)==Turns::none);
 assert(!t.finish(1,100));
 t.offer(0,1000000100ULL); // Failed monitor cools down while the others run.
 assert(t.finish(0,100));assert(t.spacingMs(100)==50);
 assert(t.take(50000099)==Turns::none&&t.take(50000100)==1);
 assert(t.finish(1,60000000));
 assert(t.take(110000000)==2&&t.finish(2,120000000));
 assert(t.delayMs(170000000)==831);
 assert(t.take(1000000100ULL)==0&&t.finish(0,1000000200ULL));
 assert(t.delayMs(1100000000ULL)==Turns::none);
 t.offer(2,0);t.cancel(2);assert(t.delayMs(1200000000ULL)==Turns::none);
 t.offer(10,0);t.cancel(10);assert(t.take(1200000000ULL)==Turns::none);
 assert(!t.finish(Turns::none,0));
 puts("bootstrap schedule: one active visit, yield spacing, fair order, independent cooldowns, cancellation and no idle timer");
}
