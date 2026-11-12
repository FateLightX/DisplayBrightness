#pragma once
namespace dbhw {
// Enumerates only active services. A stale interface must never make an
// otherwise unique replacement look ambiguous.
template<class Service> struct ActiveInterfaces {
 unsigned count=0;
 Service *first=nullptr;
 void observe(Service *service,bool inactive){
  if(!service||inactive)return;
  if(count++==0)first=service;
 }
 Service *unique()const{return count==1?first:nullptr;}
};

inline bool interfaceUsable(unsigned count,bool present,bool inactive,bool simple){
 return count==1 && present && !inactive && simple;
}

enum class Refresh { Keep, Retire, Reprobe };
// Caller holds the native framebuffer gate and queue lock. The disabled path
// is the wake/interface recovery path; the enabled path also handles provider
// ownership and display-mode replacement.
inline Refresh endpointRefresh(bool initialized,bool stopped,bool sameBinding,
 bool sameInterface,bool sameRange,bool foreignBrightness,bool enabled,
 bool bootstrapPending,bool usableInterface){
 if(!enabled){
  if(!initialized || bootstrapPending)return Refresh::Keep;
  return stopped||!sameBinding||!sameInterface||!sameRange||!usableInterface
   ?Refresh::Reprobe:Refresh::Keep;
 }
 if(foreignBrightness)return Refresh::Retire;
 if(!sameBinding || !sameInterface || (initialized && (stopped || !sameRange)))
  return Refresh::Reprobe;
 return Refresh::Keep;
}
}
