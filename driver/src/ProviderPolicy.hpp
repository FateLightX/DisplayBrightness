#pragma once
namespace dbhw {
enum class BrightnessProvider { Missing, Owned, Foreign };

inline BrightnessProvider brightnessProvider(bool present,bool sameOwner,bool marked){
 if(!present)return BrightnessProvider::Missing;
 return sameOwner&&marked?BrightnessProvider::Owned:BrightnessProvider::Foreign;
}

inline bool allowsTransport(BrightnessProvider provider,bool initialized,
 bool boundRangeMatches,bool bootstrapRead){
 if(provider==BrightnessProvider::Foreign)return false;
 if(!initialized)return bootstrapRead;
 return provider==BrightnessProvider::Owned&&boundRangeMatches;
}
}
