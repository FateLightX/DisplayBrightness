#include "RetirePolicy.hpp"
#include <cassert>
#include <cstdio>
int main(){
 for(unsigned bits=0;bits<16;bits++){
  bool occupied=bits&1,inactive=bits&2,busy=bits&4,retiring=bits&8;
  assert(dbhw::canRetire(occupied,inactive,busy,retiring)==(occupied&&inactive&&!busy&&!retiring));
 }
 puts("retirement: active, busy, empty or already retiring slots cannot be reclaimed");
}
