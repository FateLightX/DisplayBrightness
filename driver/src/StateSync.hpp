#pragma once
#include <stdint.h>
namespace dbstate {
inline bool shouldNotify(bool havePrevious,unsigned previous,unsigned value){return !havePrevious || previous!=value;}
inline bool percent(bool valid,uint16_t current,uint16_t maximum,unsigned &out){
 if(!valid||!maximum||current>maximum)return false;
 out=(static_cast<unsigned>(current)*100+maximum/2)/maximum;return true;
}
inline bool mayPublish(uint64_t completed,uint64_t latest,bool pending,bool verified){
 return verified && completed==latest && !pending;
}
}
