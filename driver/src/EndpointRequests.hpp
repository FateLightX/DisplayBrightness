#pragma once
#include "LatestRequest.hpp"
namespace dbhw {
// Bootstrap tickets carry no user intent. A pre-initialization write must not
// replace their dummy target or survive as a pending request after the read.
struct EndpointRequests:LatestRequest {
 bool initialized=false;
 bool writable()const{return initialized&&!stopped;}
 bool submitBrightness(unsigned value,bool &schedule){
  schedule=false;
  return writable()&&LatestRequest::submit(value,schedule);
 }
 bool submitBootstrap(bool &schedule){
  schedule=false;
  if(stopped)return false;
  if(initialized||busy)return true;
  return LatestRequest::submit(0,schedule);
 }
private:
 using LatestRequest::submit;
};
}
