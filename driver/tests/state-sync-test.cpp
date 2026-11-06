#include "StateSync.hpp"
#include <cassert>
#include <cstdio>
int main(){unsigned out=999;
 assert(!dbstate::percent(false,0,100,out)&&out==999);
 assert(!dbstate::percent(true,0,0,out));assert(!dbstate::percent(true,101,100,out));
 assert(dbstate::percent(true,0,100,out)&&out==0);
 assert(dbstate::percent(true,100,100,out)&&out==100);
 assert(dbstate::percent(true,77,255,out)&&out==30);
 assert(dbstate::percent(true,65535,65535,out)&&out==100);
 assert(dbstate::mayPublish(7,7,false,true));
 assert(!dbstate::mayPublish(7,8,false,true));assert(!dbstate::mayPublish(7,7,true,true));
 assert(!dbstate::mayPublish(7,7,false,false));
 assert(dbstate::shouldNotify(false,0,0));
 assert(!dbstate::shouldNotify(true,70,70));
 assert(dbstate::shouldNotify(true,70,30));
 puts("state sync: valid hardware normalization, stale/pending/failure suppression pass");}
