#include "LatestRequest.hpp"
#include <cassert>
#include <cstdio>
int main(){
 dbhw::LatestRequest q;bool start=false;
 assert(q.submit(10,start)&&start);assert(q.take()==10);
 assert(q.submit(30,start)&&!start);
 assert(q.complete(false)); // failure must schedule automatic recovery
 assert(q.failed&&q.busy);assert(q.submit(80,start)&&!start);
 assert(q.take()==80);assert(!q.complete(true));assert(!q.failed);
 for(unsigned i=0;i<1000;i++){
  unsigned v=i%101;assert(q.submit(v,start)&&start);assert(q.take()==v);assert(!q.complete(true));
 }
 assert(!q.submit(101,start));
 assert(q.submit(70,start));assert(q.take()==70);
 for(unsigned i=0;i<10;i++){assert(q.complete(false));assert(q.take()==70);}
 assert(!q.complete(true));
 puts("unlimited normal requests; automatic failure recovery; latest target preserved; bounds pass");
}
