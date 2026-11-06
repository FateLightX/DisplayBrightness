#pragma once
namespace dbstate {
enum class Offer { Inserted,Existing,Full };
// Caller serializes access. Insertion acquires refs externally, take transfers them.
template<class T,unsigned N> struct BootstrapPending {
 struct Ticket {T *display=nullptr,*frame=nullptr;};
 Ticket entries[N];
 Offer offer(T *d,T *f){
  if(!d||!f)return Offer::Full;
  for(auto &e:entries)if(e.display==d&&e.frame==f)return Offer::Existing;
  for(auto &e:entries)if(!e.display){e.display=d;e.frame=f;return Offer::Inserted;}
  return Offer::Full;
 }
 Ticket take(unsigned i){if(i>=N)return Ticket{};auto t=entries[i];entries[i]=Ticket{};return t;}
};
}
