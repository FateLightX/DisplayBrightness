#include "DDCProtocol.hpp"
#include <cassert>
#include <cstring>
#include <cstdio>
int main(){
 unsigned char raw[]={0x6e,0x88,2,0,0x10,0,0,0x64,0,0x64,0xa4};
 auto c=dbddc::decodeBrightness(raw,11);assert(c.valid&&c.current==100&&c.maximum==100);
 for(unsigned n=0;n<11;n++)assert(!dbddc::decodeBrightness(raw,n).valid);
 assert(!dbddc::decodeBrightness(nullptr,11).valid);
 for(unsigned i=0;i<11;i++){raw[i]^=1;assert(!dbddc::decodeBrightness(raw,11).valid);raw[i]^=1;}
 auto fix=[&](){raw[10]=0x50;for(unsigned i=0;i<10;i++)raw[10]^=raw[i];};
 raw[4]=0x12;fix();assert(!dbddc::decodeBrightness(raw,11).valid);raw[4]=0x10;
 raw[3]=1;fix();assert(!dbddc::decodeBrightness(raw,11).valid);raw[3]=0;
 raw[7]=0;fix();assert(!dbddc::decodeBrightness(raw,11).valid);raw[7]=99;fix();assert(!dbddc::decodeBrightness(raw,11).valid);
 unsigned char get[5];dbddc::getBrightness(get);const unsigned char expectedGet[]={0x51,0x82,1,0x10,0xac};assert(!memcmp(get,expectedGet,5));
 unsigned char set[7];assert(dbddc::setBrightness(c,75,set));const unsigned char expectedSet[]={0x51,0x84,3,0x10,0,75,0xe3};assert(!memcmp(set,expectedSet,7));
 assert(dbddc::setBrightness(c,0,set));assert(dbddc::setBrightness(c,100,set));assert(!dbddc::setBrightness(c,101,set));assert(!dbddc::setBrightness({},1,set));assert(!dbddc::setBrightness(c,1,nullptr));
 puts("PASS: real T1901W reply; corruption/length/opcode/status/range rejection; Get/Set packet fixtures; unprobed writes rejected");
}
