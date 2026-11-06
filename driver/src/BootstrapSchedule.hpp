#pragma once
#include <stdint.h>
namespace dbstate {
// Caller holds hardwareQueue. Only one initial-read visit can be dispatched at
// a time. Cooldowns do not monopolize the turn; another display can run first.
template<unsigned N> struct BootstrapSchedule {
 static constexpr unsigned none=~0U,quantumMs=50;
 struct Visit {bool pending=false;uint64_t dueNs=0;};
 Visit visits[N];unsigned active=none,cursor=0;uint64_t notBeforeNs=0;
 void offer(unsigned i,uint64_t due){
  if(i<N&&(!visits[i].pending||due<visits[i].dueNs))visits[i]={true,due};
 }
 void cancel(unsigned i){if(i<N)visits[i]=Visit{};}
 unsigned spacingMs(uint64_t now)const{
  return now>=notBeforeNs?0:static_cast<unsigned>((notBeforeNs-now+999999ULL)/1000000ULL);
 }
 unsigned delayMs(uint64_t now)const{
  if(active!=none)return none;
  bool found=false;uint64_t due=0;
  for(const auto &v:visits)if(v.pending&&(!found||v.dueNs<due)){found=true;due=v.dueNs;}
  if(!found)return none;
  if(due<notBeforeNs)due=notBeforeNs;
  if(now>=due)return 0;
  uint64_t ms=(due-now+999999ULL)/1000000ULL;
  return ms>=none?none-1:static_cast<unsigned>(ms);
 }
 unsigned take(uint64_t now){
  if(active!=none||now<notBeforeNs)return none;
  for(unsigned offset=0;offset<N;offset++){
   unsigned i=(cursor+offset)%N;
   if(visits[i].pending&&visits[i].dueNs<=now){
    visits[i]=Visit{};active=i;cursor=(i+1)%N;return i;
   }
  }
  return none;
 }
 bool finish(unsigned i,uint64_t now){
  if(active!=i||i>=N)return false;
  active=none;notBeforeNs=now+static_cast<uint64_t>(quantumMs)*1000000ULL;return true;
 }
};
}
