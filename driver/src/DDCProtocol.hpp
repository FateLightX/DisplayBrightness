#pragma once
#include <stddef.h>
#include <stdint.h>
// Protocol only: no I/O, no identity binding, no timers or kernel dependencies.
namespace dbddc {
struct Brightness { bool valid=false; uint16_t current=0,maximum=0; };
inline Brightness decodeBrightness(const uint8_t *p,size_t size){
 Brightness b;
 if(!p || size!=11 || p[0]!=0x6e || p[1]!=0x88 || p[2]!=0x02 || p[3]!=0 || p[4]!=0x10 || p[5]!=0)return b;
 uint8_t sum=0x50;for(size_t i=0;i<size;i++)sum^=p[i];if(sum)return b;
 uint16_t maximum=static_cast<uint16_t>((static_cast<unsigned>(p[6])<<8)|p[7]);
 uint16_t current=static_cast<uint16_t>((static_cast<unsigned>(p[8])<<8)|p[9]);
 if(!maximum || current>maximum)return b;
 b.valid=true;b.current=current;b.maximum=maximum;return b;
}
inline void getBrightness(uint8_t (&p)[5]){
 p[0]=0x51;p[1]=0x82;p[2]=0x01;p[3]=0x10;p[4]=0x6e;
 for(unsigned i=0;i<4;i++)p[4]^=p[i];
}
inline bool setBrightness(const Brightness &capability,uint16_t value,uint8_t *p){
 if(!p || !capability.valid || !capability.maximum || capability.current>capability.maximum || value>capability.maximum)return false;
 p[0]=0x51;p[1]=0x84;p[2]=0x03;p[3]=0x10;p[4]=static_cast<uint8_t>(value>>8);p[5]=static_cast<uint8_t>(value);p[6]=0x6e;
 for(unsigned i=0;i<6;i++)p[6]^=p[i];return true;
}
}
