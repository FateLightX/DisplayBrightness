#pragma once
namespace dbhw {
inline bool canRetire(bool occupied,bool inactive,bool busy,bool retiring,bool parked=false){
 return occupied && inactive && (!busy || parked) && !retiring;
}
}
