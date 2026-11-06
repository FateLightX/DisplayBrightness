#include "ProbeIdentity.hpp"
#include "HardwarePolicy.hpp"
#include <cassert>
#include <cstdio>
int main(){
 unsigned char e[128]={0,255,255,255,255,255,255,0};e[18]=1;e[19]=4;
 const unsigned products[]={0,1,0x3251,0xffff};unsigned count=0;
 for(unsigned a=1;a<=26;a++)for(unsigned b=1;b<=26;b++)for(unsigned c=1;c<=26;c++)for(unsigned product:products){
  unsigned vendor=(a<<10)|(b<<5)|c;e[8]=vendor>>8;e[9]=vendor;e[10]=product;e[11]=product>>8;
  unsigned sum=0;for(unsigned i=0;i<127;i++)sum+=e[i];e[127]=static_cast<unsigned char>(0-sum);
  assert(isObservedDisplay(e,128));++count;
 }
 // EDID eligibility never authorizes a write; only a valid fresh VCP reply can.
 dbddc::Brightness invalid;unsigned short out=0;assert(!dbhw::target(invalid,50,out));
 const unsigned maxima[]={1U,100U,255U,1000U,65535U};for(unsigned max:maxima){dbddc::Brightness v;v.valid=true;v.current=0;v.maximum=max;assert(dbhw::target(v,100,out)&&out==max);assert(dbhw::target(v,0,out)&&out==0);}
 printf("generic eligibility: %u vendor/product combinations; hardware-derived ranges and invalid-reply write rejection pass\n",count);
}
