#pragma once
#include "DisplayCompatibility.hpp"
namespace dbstate {
// Local opt-in for three EDID base blocks with verified VCP 0x10 read/write.
// This fingerprint is an identity hint, never permission to issue a write.
// Unknown displays retain the generic read-before-publication path.
inline uint64_t displayFingerprint(const uint8_t *base){
 uint64_t value=14695981039346656037ULL;
 for(unsigned i=0;i<128;i++)value=(value^base[i])*1099511628211ULL;
 return value;
}
inline bool knownEarlyFingerprint(uint64_t fingerprint){
 switch(fingerprint){
  case 0x24ccf51437e8e58bULL: // U2790B
  case 0x0d2e6e803e5c4f53ULL: // U28P2G6B
  case 0x28b3a6a8891dfaffULL: // U2701B
   return true;
  default:return false;
 }
}
inline bool earlyDisplay(const uint8_t *base,size_t length){
 return dbcompat::validBase(base,length) && knownEarlyFingerprint(displayFingerprint(base));
}
// UI-only placeholder. Requests are rejected until the first hardware read has
// replaced this value; it must never seed the hardware request queue.
constexpr unsigned provisionalBrightness=50;
}
