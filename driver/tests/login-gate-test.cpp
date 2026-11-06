#include "LoginGate.hpp"
#include "BootstrapWindow.hpp"
#include "EndpointRequests.hpp"
#include <cassert>
#include <cstdio>
int main(){
 dbstate::LoginGate gate;dbstate::BootstrapWindow window;
 dbhw::EndpointRequests requests;bool schedule=false;
 assert(requests.submitBootstrap(schedule)&&schedule);
 for(unsigned i=0;i<100000;i++){
  assert(!gate.observe(false,true)&&!gate.observe(true,false));
  assert(!gate.allowed()&&requests.pending&&!window.begun&&window.rounds==0);
 }
 assert(gate.observe(true,true)&&gate.allowed());
 assert(!gate.observe(true,true));
 assert(!gate.observe(false,false)&&gate.allowed());
 assert(window.begin(3600000000000ULL)&&window.rounds==1); // Hours at loginwindow cost no probe budget.
 assert(requests.take()==0&&!requests.writable()); // Permission is not hardware readiness.
 assert(!requests.submitBrightness(80,schedule));
 puts("login gate: exact flags, one-way latch, no fabricated readiness or pre-login budget consumption");
}
