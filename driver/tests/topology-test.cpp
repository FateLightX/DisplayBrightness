#include "DisplayCompatibility.hpp"
#include <cassert>
#include <cstdio>
int main(){
 int connection1=0,connection2=0,frame1=0,frame2=0;
 assert(dbcompat::sameTopology(&connection1,&connection1,&frame1,&frame1,1,true));
 assert(!dbcompat::sameTopology(&connection1,&connection2,&frame1,&frame1,1,true));
 assert(!dbcompat::sameTopology(&connection1,&connection1,&frame1,&frame2,1,true));
 assert(!dbcompat::sameTopology(&connection1,&connection1,&frame1,&frame1,2,true));
 assert(!dbcompat::sameTopology(&connection1,&connection1,&frame1,&frame1,0,false));
 assert(!dbcompat::sameTopology(&connection1,&connection1,&frame1,&frame1,1,false));
 assert(!dbcompat::sameTopology(nullptr,nullptr,&frame1,&frame1,1,true));
 puts("connection replacement and interface ambiguity reject same-EDID binding");
}
