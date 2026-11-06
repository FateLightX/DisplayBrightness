#pragma once
// Generic probe eligibility; this is not a claim of brightness capability.
#include "DisplayCompatibility.hpp"
inline bool isObservedDisplay(const unsigned char *b,unsigned long length){
 return dbcompat::validBase(b,length);
}
