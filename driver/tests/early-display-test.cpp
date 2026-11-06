#include "EarlyDisplayProfiles.hpp"
#include "ProbeIdentity.hpp"
#include <cassert>
#include <cstdio>
#include <initializer_list>
static void checksum(unsigned char (&e)[128]){
 unsigned sum=0;for(unsigned i=0;i<127;i++)sum+=e[i];e[127]=static_cast<unsigned char>(0-sum);
}
int main(){
 // Profile membership is tested without publishing the devices' raw serials.
 for(uint64_t fingerprint:{0x24ccf51437e8e58bULL,0x0d2e6e803e5c4f53ULL,0x28b3a6a8891dfaffULL}){
  assert(dbstate::knownEarlyFingerprint(fingerprint));
  assert(!dbstate::knownEarlyFingerprint(fingerprint^1));
 }
 assert(!dbstate::knownEarlyFingerprint(0));
 assert(!dbstate::earlyDisplay(nullptr,128));
 unsigned char e[128]={0,255,255,255,255,255,255,0,0x04,0x43,0x21,0x43,1};
 e[18]=1;e[19]=4;checksum(e);
 assert(dbstate::displayFingerprint(e)==0x4c3124433074bdb1ULL);
 assert(isObservedDisplay(e,128)&&!dbstate::earlyDisplay(e,128));
 assert(!dbstate::earlyDisplay(e,127));
 const auto original=dbstate::displayFingerprint(e);
 e[12]^=1;checksum(e);
 assert(dbstate::displayFingerprint(e)!=original);
 assert(isObservedDisplay(e,128)&&!dbstate::earlyDisplay(e,128));
 e[127]^=1;assert(!isObservedDisplay(e,128)&&!dbstate::earlyDisplay(e,128));
 puts("early profiles: exact fingerprint membership; synthetic EDID hashing, serial changes and generic fallback verified");
}
