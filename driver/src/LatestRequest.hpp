#pragma once
#include <stdint.h>
namespace dbhw {
struct LatestRequest {
 bool busy=false,pending=false,failed=false,stopped=false;
 unsigned desired=0;
 bool submit(unsigned value,bool &schedule){
  schedule=false;if(stopped || value>100)return false;
  desired=value;pending=true;if(!busy){busy=true;schedule=true;}return true;
 }
 bool refreshAfterRead(uint64_t latest,unsigned &target,uint64_t &sequence){
  if(stopped || !busy)return false;
  if(pending){target=take();sequence=latest;}
  return true;
 }
 unsigned take(){pending=false;return desired;}
 bool complete(bool success){
  failed=!success;
  if(!success)pending=true; // recover automatically, keep latest desired
  if(pending && !stopped)return true;
  busy=false;return false;
 }
 void stop(){stopped=true;pending=false;busy=false;failed=true;}
};
}
