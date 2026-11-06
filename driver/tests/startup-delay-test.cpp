#include "StartupDelay.hpp"
#include "BootstrapWindow.hpp"
#include "LatestRequest.hpp"
#include <cassert>
#include <cstdio>
#include <stdint.h>
int main(){
 using dbstate::StartupDelay;
 assert(StartupDelay::remainingMs(0)==30000);
 // The observed AMD panics occurred at 5.35--5.42 seconds after boot.
 assert(StartupDelay::remainingMs(5349140666ULL)==24651);
 assert(StartupDelay::remainingMs(5377497462ULL)==24623);
 assert(StartupDelay::remainingMs(5420995332ULL)==24580);
 assert(StartupDelay::remainingMs(StartupDelay::notBeforeNs-1000001)==2);
 assert(StartupDelay::remainingMs(StartupDelay::notBeforeNs-1)==1);
 assert(StartupDelay::remainingMs(StartupDelay::notBeforeNs)==0);
 assert(StartupDelay::remainingMs(UINT64_MAX)==0);
 // Deferral leaves the pending target and the entire 60-second probe window
 // intact; a late-running worker starts its budget when it actually runs.
 dbhw::LatestRequest request;dbstate::BootstrapWindow window;bool schedule=false;
 assert(request.submit(75,schedule)&&schedule);
 for(uint64_t now=0;now<StartupDelay::notBeforeNs;now+=1000000000ULL){
  assert(StartupDelay::remainingMs(now)>0);
  assert(request.pending&&request.busy&&!window.begun&&window.rounds==0);
 }
 assert(window.begin(45000000000ULL));assert(request.take()==75);
 assert(window.maySchedule(103000000000ULL));assert(!window.within(105000000000ULL));
 puts("startup exclusion: panic window, millisecond rounding, late callback and preserved probe budget pass");
}
