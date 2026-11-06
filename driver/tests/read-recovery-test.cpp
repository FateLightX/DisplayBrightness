#include "ReadRecovery.hpp"
#include <cassert>
#include <cstdio>
int main(){
 unsigned reads=0,pauses=0;
 bool ok=dbhw::readRecovery([&](){return ++reads==2;},[&](){++pauses;});
 assert(ok&&reads==2&&pauses==1);
 reads=pauses=0;ok=dbhw::readRecovery([&](){++reads;return false;},[&](){++pauses;});
 assert(!ok&&reads==3&&pauses==2);
 reads=pauses=0;ok=dbhw::readRecovery([&](){++reads;return true;},[&](){++pauses;});
 assert(ok&&reads==1&&pauses==0);
 reads=pauses=0;ok=dbhw::readRecovery([&](){++reads;return false;},[&](){++pauses;},[](){return false;});
 assert(!ok&&reads==1&&pauses==0);
 puts("read recovery: transient failure recovers, permanent failure bounded, success has no retry");
}
