#include "ProbeIdentity.hpp"
#include "DisplayFixtures.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
int main(){
 assert(isObservedDisplay(dell,sizeof(dell)));
 assert(isObservedDisplay(t1901,sizeof(t1901)));
 assert(!isObservedDisplay(nullptr,128));
 assert(!isObservedDisplay(dell,127));
 unsigned char e[128];std::memcpy(e,dell,sizeof(e));
 e[20]^=1;assert(!isObservedDisplay(e,128));e[20]^=1;
 e[8]=0;unsigned sum=0;for(unsigned i=0;i<127;i++)sum+=e[i];e[127]=static_cast<unsigned char>(0-sum);
 assert(!isObservedDisplay(e,128));
 puts("identity: serial-redacted samples accepted; null, truncated, corrupt and malformed vendor rejected");
}
