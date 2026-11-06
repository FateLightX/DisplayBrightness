#pragma once
#include "DDCProtocol.hpp"
#include "StateSync.hpp"
namespace dbstate {
// No write callback exists here: bootstrap reads, validates, then publishes.
template<class Read,class Publish> bool bootstrap(Read read,Publish publish){
 auto raw=read();unsigned value=0;
 if(!percent(raw.valid,raw.current,raw.maximum,value))return false;
 return publish(value,raw);
}
}
