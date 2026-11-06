#pragma once
#include <stdint.h>
namespace dbstate {
struct BootstrapWindow {
 static constexpr unsigned intervalMs=1000,maximumRounds=60;
 static constexpr uint64_t durationNs=60000000000ULL;
 uint64_t firstNs=0;unsigned rounds=0;bool begun=false;
 bool within(uint64_t now)const{return begun&&now>=firstNs&&now-firstNs<durationNs;}
 bool begin(uint64_t now){if(!begun){begun=true;firstNs=now;}if(!within(now)||rounds>=maximumRounds)return false;++rounds;return true;}
 bool maySchedule(uint64_t now)const{return within(now)&&rounds<maximumRounds&&durationNs-(now-firstNs)>static_cast<uint64_t>(intervalMs)*1000000ULL;}
};
}
