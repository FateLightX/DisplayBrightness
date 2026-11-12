#include "RetirePolicy.hpp"
#include <cassert>
#include <cstdio>
int main(){
 for(unsigned bits=0;bits<32;bits++){
  bool occupied=bits&1,inactive=bits&2,busy=bits&4,retiring=bits&8,parked=bits&16;
  assert(dbhw::canRetire(occupied,inactive,busy,retiring,parked)==
   (occupied&&inactive&&(!busy||parked)&&!retiring));
 }
 puts("retirement: empty/active/retiring slots rejected; parked busy endpoints reclaimable");
}
