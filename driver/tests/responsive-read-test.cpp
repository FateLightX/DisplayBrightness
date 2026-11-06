#include "ResponsiveRead.hpp"
#include <cassert>
#include <cstdio>
int main(){
 unsigned reads=0,pauses=0;
 auto valid=[](){return true;};auto current=[](){return false;};
 auto r=dbhw::responsiveRead([&](){++reads;return false;},[&](){++pauses;},valid,[](){return true;});
 assert(r==dbhw::ReadResult::Superseded&&reads==1&&pauses==0);
 reads=pauses=0;
 r=dbhw::responsiveRead([&](){++reads;return false;},[&](){++pauses;},valid,[&](){return pauses>=3;});
 assert(r==dbhw::ReadResult::Superseded&&reads==1&&pauses==3);
 reads=pauses=0;
 r=dbhw::responsiveRead([&](){return ++reads==2;},[&](){++pauses;},valid,current);
 assert(r==dbhw::ReadResult::Valid&&reads==2&&pauses==25);
 reads=pauses=0;
 r=dbhw::responsiveRead([&](){++reads;return false;},[&](){++pauses;},valid,current);
 assert(r==dbhw::ReadResult::Failed&&reads==3&&pauses==50);
 reads=pauses=0;
 r=dbhw::responsiveRead([&](){++reads;return true;},[&](){++pauses;},[](){return false;},current);
 assert(r==dbhw::ReadResult::Stopped&&reads==0&&pauses==0);
 bool alive=true;reads=pauses=0;
 r=dbhw::responsiveRead([&](){++reads;alive=false;return false;},[&](){++pauses;},[&](){return alive;},[](){return true;});
 assert(r==dbhw::ReadResult::Stopped&&reads==1&&pauses==0);
 assert(!dbhw::differentPending(true,30,30));assert(!dbhw::differentPending(false,80,30));
 assert(dbhw::differentPending(true,0,30));
 puts("responsive read: supersession interrupts retry; bounded unchanged recovery; stopped endpoint; same-target feedback does not cancel");
}
