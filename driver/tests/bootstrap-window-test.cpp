#include "BootstrapWindow.hpp"
#include <cassert>
#include <cstdio>
using namespace dbstate;
int main(){
 assert(BootstrapWindow::intervalMs==1000);
 BootstrapWindow p;assert(!p.within(0)&&!p.maySchedule(0));
 assert(p.begin(0)&&p.rounds==1&&p.within(0)&&p.maySchedule(0));
 for(unsigned i=1;i<60;i++)assert(p.begin(static_cast<uint64_t>(i)*1000000000ULL));
 assert(p.rounds==60&&!p.begin(59000000000ULL)&&!p.maySchedule(59000000000ULL));
 p=BootstrapWindow{};assert(p.begin(100));assert(!p.begin(60000000100ULL));assert(!p.within(99));
 p=BootstrapWindow{};assert(p.begin(100));assert(p.maySchedule(58000000100ULL));assert(!p.maySchedule(59000000100ULL));
 assert(!p.begin(50));
 p=BootstrapWindow{};assert(p.begin(0));assert(p.within(59000000000ULL));assert(!p.within(60000000000ULL));
 puts("startup window: one-second cadence, 60-round cap, elapsed deadline, zero origin and backward-clock rejection pass");
}
