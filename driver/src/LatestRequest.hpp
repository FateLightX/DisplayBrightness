#pragma once
#include <stdint.h>
namespace dbhw {
struct LatestRequest {
 bool busy=false,pending=false,failed=false,stopped=false;
 static constexpr unsigned maxAutomaticFailureRetries=2;
 unsigned desired=0;
 unsigned activeTarget=0;
 unsigned automaticFailureRetries=0;
 bool submit(unsigned value,bool &schedule){
  schedule=false;if(stopped || value>100)return false;
  desired=value;pending=true;
  if(!busy){automaticFailureRetries=0;busy=true;schedule=true;}
  return true;
 }
 bool refreshAfterRead(uint64_t latest,unsigned &target,uint64_t &sequence){
  if(stopped || !busy)return false;
  if(pending){target=take();sequence=latest;}
  return true;
 }
 bool hasNewTarget()const{return pending&&desired!=activeTarget;}
 unsigned take(){
  if(hasNewTarget())automaticFailureRetries=0;
  pending=false;activeTarget=desired;return activeTarget;
 }
 bool complete(bool success,bool boundFailureRetries=false,unsigned retryLimit=maxAutomaticFailureRetries){
  failed=!success;
  if(success)automaticFailureRetries=0;
  else if(!boundFailureRetries)pending=true; // startup recovery is bounded by BootstrapWindow
  else if(!hasNewTarget()){
   // A different target starts a fresh budget when take() observes it. The
   // same target cannot extend an exhausted cycle by repeated submissions.
   pending=automaticFailureRetries<retryLimit;
   if(pending)++automaticFailureRetries;
  }
  if(pending && !stopped)return true;
  busy=false;return false;
 }
 void stop(){stopped=true;pending=false;busy=false;failed=true;}
};
}
