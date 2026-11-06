#include "ProbeIdentity.hpp"
#include <cassert>
#include <cstdio>
int main(){
 // Deliberately unknown manufacturer ABC/product4321, repeated serial1.
 unsigned char e[128]={0,255,255,255,255,255,255,0,0x04,0x43,0x21,0x43,1};
 e[18]=1;e[19]=4;
 auto checksum=[&](){unsigned sum=0;for(unsigned i=0;i<127;i++)sum+=e[i];e[127]=static_cast<unsigned char>(0-sum);};
 checksum();
 assert(isObservedDisplay(e,128)); // Old vendor allowlist must fail here.
 assert(!isObservedDisplay(e,127));assert(!isObservedDisplay(nullptr,128));
 e[20]^=1;assert(!isObservedDisplay(e,128));e[20]^=1;
 e[0]=1;checksum();assert(!isObservedDisplay(e,128));e[0]=0;
 e[8]=0;e[9]=0;checksum();assert(!isObservedDisplay(e,128));
 puts("unknown manufacturer admitted for probe only; malformed EDID rejected");
}
