#include "Bootstrap.hpp"
#include <cassert>
#include <cstdio>
int main(){
 unsigned step=0,published=999;
 dbddc::Brightness b;b.valid=true;b.current=83;b.maximum=100;
 bool ok=dbstate::bootstrap([&](){assert(step++==0);return b;},[&](unsigned value,dbddc::Brightness raw){assert(step++==1);published=value;assert(raw.current==83);return true;});
 assert(ok&&published==83&&step==2);
 step=0;b.valid=false;ok=dbstate::bootstrap([&](){++step;return b;},[&](unsigned,dbddc::Brightness){assert(false);return false;});assert(!ok&&step==1);
 b.valid=true;b.current=0;ok=dbstate::bootstrap([&](){return b;},[&](unsigned v,dbddc::Brightness){published=v;return true;});assert(ok&&published==0);
 b.current=100;ok=dbstate::bootstrap([&](){return b;},[&](unsigned v,dbddc::Brightness){published=v;return false;});assert(!ok&&published==100);
 puts("bootstrap: fresh read precedes publication; invalid reply never publishes; real zero retained; publish failure propagated");
}
