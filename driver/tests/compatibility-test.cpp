#include "DisplayCompatibility.hpp"
#include "HardwarePolicy.hpp"
#include "StateSync.hpp"
#include <cassert>
#include <cstdio>
int main(){
 unsigned char a[128]={0,255,255,255,255,255,255,0,0x04,0x43,0x21,0x43,1};a[18]=1;a[19]=4;
 unsigned sum=0;for(unsigned i=0;i<127;i++)sum+=a[i];a[127]=static_cast<unsigned char>(0-sum);
 unsigned char b[128];for(unsigned i=0;i<128;i++)b[i]=a[i];
 assert(dbcompat::sameBase(a,b,128));
 // Same product and duplicate serial must not hide a changed EDID.
 b[20]=1;b[127]--;assert(!dbcompat::sameBase(a,b,128));
 assert(!dbcompat::sameBase(a,b,127));assert(!dbcompat::sameBase(nullptr,a,128));
 dbcompat::ProbeBudget budget;assert(budget.canProbe());
 budget.failed();assert(budget.canProbe());budget.failed();assert(budget.canProbe());
 budget.failed();assert(!budget.canProbe());budget.failed();assert(!budget.canProbe());
 dbddc::Brightness raw;raw.valid=true;raw.current=1;raw.maximum=65535;unsigned short out=0;
 assert(dbhw::target(raw,100,out)&&out==65535);
 assert(dbhw::target(raw,50,out)&&out==32768);
 assert(dbhw::target(raw,0,out)&&out==0);
 unsigned percent=0;assert(dbstate::percent(true,32768,65535,percent)&&percent==50);
 raw.maximum=255;assert(dbhw::target(raw,50,out)&&out==128);
 raw.current=256;assert(!dbhw::target(raw,50,out));
 // independent budgets/endpoints must not consume each other's admission.
 dbcompat::ProbeBudget other;assert(other.canProbe());
 puts("identity replacement, bounded admission, independent state, full uint16 ranges pass");
}
