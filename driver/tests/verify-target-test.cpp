#include "VerifyTarget.hpp"
#include <cassert>
#include <cstdio>
int main(){
 dbddc::Brightness before;before.valid=true;before.current=100;before.maximum=100;
 unsigned reads=0,recorded=999;
 auto record=[&](dbddc::Brightness b){recorded=b.current;};
 assert(dbhw::verifyTarget(before,100,[&](dbddc::Brightness &){++reads;return false;},record));
 assert(reads==0&&recorded==100);
 auto after=before;after.current=30;
 assert(dbhw::verifyTarget(before,30,[&](dbddc::Brightness &b){++reads;b=after;return true;},record));
 assert(reads==1&&recorded==30);
 recorded=999;after.current=31;
 assert(!dbhw::verifyTarget(before,30,[&](dbddc::Brightness &b){b=after;return true;},record));assert(recorded==999);
 after.current=30;after.maximum=200;
 assert(!dbhw::verifyTarget(before,30,[&](dbddc::Brightness &b){b=after;return true;},record));
 assert(!dbhw::verifyTarget(before,30,[](dbddc::Brightness &){return false;},record));
 before.valid=false;
 assert(!dbhw::verifyTarget(before,100,[](dbddc::Brightness &){assert(false);return true;},record));
 before.valid=true;before.current=0;
 assert(dbhw::verifyTarget(before,0,[](dbddc::Brightness &){assert(false);return true;},record));assert(recorded==0);
 puts("verification: fresh already-target uses one read; changed target requires valid exact readback; no publication on error/mismatch");
}
