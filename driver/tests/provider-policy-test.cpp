#include "ProviderPolicy.hpp"
#include <cassert>
#include <cstdio>
int main(){
 using dbhw::BrightnessProvider;
 assert(dbhw::brightnessProvider(false,false,false)==BrightnessProvider::Missing);
 assert(dbhw::brightnessProvider(true,true,true)==BrightnessProvider::Owned);
 assert(dbhw::brightnessProvider(true,false,true)==BrightnessProvider::Foreign);
 assert(dbhw::brightnessProvider(true,true,false)==BrightnessProvider::Foreign);
 assert(!dbhw::allowsTransport(BrightnessProvider::Foreign,false,false,true));
 assert(!dbhw::allowsTransport(BrightnessProvider::Foreign,true,true,false));
 assert(dbhw::allowsTransport(BrightnessProvider::Missing,false,false,true));
 assert(!dbhw::allowsTransport(BrightnessProvider::Missing,false,false,false));
 assert(dbhw::allowsTransport(BrightnessProvider::Owned,true,true,false));
 assert(!dbhw::allowsTransport(BrightnessProvider::Owned,true,false,false));
 puts("provider policy: native owner never transported; bootstrap read requires missing/owned path; initialized writes require exact range");
}
