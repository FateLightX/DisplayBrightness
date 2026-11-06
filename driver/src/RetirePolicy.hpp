#pragma once
namespace dbhw {
inline bool canRetire(bool occupied,bool inactive,bool busy,bool retiring){
 return occupied && inactive && !busy && !retiring;
}
}
