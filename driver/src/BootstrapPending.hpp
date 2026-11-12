#pragma once
namespace dbstate {
enum class Offer { Inserted,Existing,Full };
// Caller serializes access. Insertion acquires refs externally, take transfers them.
template<class T,unsigned N> struct BootstrapPending {
 struct Ticket {T *display=nullptr,*frame=nullptr;bool recovery=false;};
 Ticket entries[N];
 Offer offer(T *d,T *f,bool recovery=false){
  if(!d||!f)return Offer::Full;
  for(auto &e:entries)if(e.display==d&&e.frame==f){
   e.recovery=e.recovery||recovery;return Offer::Existing;
  }
  for(auto &e:entries)if(!e.display){e.display=d;e.frame=f;e.recovery=recovery;return Offer::Inserted;}
  return Offer::Full;
 }
 bool recoveryPending(T *d,T *f)const{
  if(!d||!f)return false;
  for(const auto &e:entries)if(e.display==d&&e.frame==f)return e.recovery;
  return false;
 }
 Ticket take(unsigned i){if(i>=N)return Ticket{};auto t=entries[i];entries[i]=Ticket{};return t;}
 Ticket takePair(T *d,T *f){
  if(!d||!f)return Ticket{};
  for(unsigned i=0;i<N;i++)if(entries[i].display==d&&entries[i].frame==f)return take(i);
  return Ticket{};
 }
 bool any()const{for(const auto &e:entries)if(e.display)return true;return false;}
 // Move ownership out under the queue lock; release objects after unlocking.
 template<class Stale> unsigned prune(Stale stale,Ticket (&removed)[N]){
  unsigned count=0;
  for(unsigned i=0;i<N;i++)if(entries[i].display&&stale(entries[i]))removed[count++]=take(i);
  return count;
 }
};
}
