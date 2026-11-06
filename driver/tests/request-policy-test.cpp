#include "RequestPolicy.hpp"
#include <cassert>
#include <cstdio>
int main(){
 for(unsigned flags=0;flags<16;flags++){
  bool synthetic=flags&1,active=flags&2,live=flags&4,valid=flags&8;
  auto a=dbRequestAction(synthetic,active,live,valid);
  if(!synthetic)assert(a==DBRequestAction::Forward);
  else {assert(a!=DBRequestAction::Forward);assert((a==DBRequestAction::Simulate)==(active&&live&&valid));}
 }
 assert(dbRequestAction(true,true,false,true)==DBRequestAction::Reject); // disconnect/identity failure
 assert(dbRequestAction(true,false,true,true)==DBRequestAction::Reject); // native replacement/stale request
 puts("request isolation: synthetic never forwards; inactive/stale rejected; native preserved");
}
