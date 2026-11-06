#pragma once
#include "DDCProtocol.hpp"
namespace dbhw {
// No range or hardware capability may come from the simulated UI dictionary.
inline bool target(const dbddc::Brightness &fresh,unsigned percent,unsigned short &out) {
 if(!fresh.valid || !fresh.maximum || fresh.current>fresh.maximum || percent>100)return false;
 out=static_cast<unsigned short>((static_cast<unsigned>(fresh.maximum)*percent+50)/100);
 return true;
}
}
