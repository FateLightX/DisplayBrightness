#include "HardwarePolicy.hpp"
#include <cassert>
#include <cstdio>
int main(){
 unsigned short out=1234;
 dbddc::Brightness absent; assert(!dbhw::target(absent,50,out));assert(out==1234);
 dbddc::Brightness a;a.valid=true;a.maximum=100;a.current=100;
 assert(dbhw::target(a,70,out)&&out==70);
 assert(dbhw::target(a,0,out)&&out==0);assert(dbhw::target(a,100,out)&&out==100);
 assert(!dbhw::target(a,101,out));
 dbddc::Brightness b;b.valid=true;b.maximum=255;b.current=50;
 assert(dbhw::target(b,30,out)&&out==77);
 assert(dbhw::target(a,30,out)&&out==30); // capabilities stay per endpoint
 b.maximum=65535;b.current=65535;assert(dbhw::target(b,100,out)&&out==65535);
 b.maximum=0;assert(!dbhw::target(b,10,out));
 b.maximum=100;b.current=101;assert(!dbhw::target(b,10,out));
 puts("hardware write policy: fresh valid capability required; per-device scale and bounds pass");
}
