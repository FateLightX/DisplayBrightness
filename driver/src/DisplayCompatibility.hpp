#pragma once
#include <stddef.h>
#include <stdint.h>
namespace dbcompat {
constexpr unsigned endpointCapacity=8;
// EDID admits a read-only probe. It never authorizes a SetVCP by itself.
inline bool validBase(const uint8_t *b,size_t length){
 if(!b || length<128)return false;
 const uint8_t header[]={0,255,255,255,255,255,255,0};
 for(unsigned i=0;i<8;i++)if(b[i]!=header[i])return false;
 uint8_t sum=0;for(unsigned i=0;i<128;i++)sum=static_cast<uint8_t>(sum+b[i]);
 if(sum)return false;
 const unsigned vendor=(static_cast<unsigned>(b[8])<<8)|b[9];
 if(vendor&0x8000)return false;
 for(unsigned shift=0;shift<=10;shift+=5){unsigned letter=(vendor>>shift)&31;if(!letter||letter>26)return false;}
 return true;
}
inline bool sameBase(const uint8_t *bound,const uint8_t *current,size_t length){
 if(!bound || !validBase(current,length))return false;
 for(unsigned i=0;i<128;i++)if(bound[i]!=current[i])return false;
 return true;
}
inline bool sameTopology(const void *boundConnection,const void *currentConnection,
                         const void *boundFrame,const void *currentFrame,
                         unsigned interfaceCount,bool selectedPresent){
 return boundConnection && boundFrame && boundConnection==currentConnection &&
        boundFrame==currentFrame && interfaceCount==1 && selectedPresent;
}
struct ProbeBudget {
 unsigned failures=0;
 bool canProbe()const{return failures<3;}
 void failed(){if(canProbe())++failures;}
};
}
