#include "EndpointRefresh.hpp"
#include <cassert>
#include <cstdio>
int main(){
 dbhw::ActiveInterfaces<int> active;
 int a=1,b=2;
 active.observe(&a,true);assert(active.count==0&&!active.unique());
 active.observe(nullptr,false);assert(active.count==0);
 active.observe(&a,false);assert(active.count==1&&active.unique()==&a);
 active.observe(&b,false);assert(active.count==2&&!active.unique());
 dbhw::ActiveInterfaces<int> replacement;
 replacement.observe(&b,false);assert(replacement.unique()==&b);

 assert(!dbhw::interfaceUsable(0,false,false,false));
 assert(dbhw::interfaceUsable(1,true,false,true));
 assert(!dbhw::interfaceUsable(1,true,true,true));
 assert(!dbhw::interfaceUsable(1,false,false,true));
 assert(!dbhw::interfaceUsable(1,true,false,false));
 assert(!dbhw::interfaceUsable(2,true,false,true));

 using R=dbhw::Refresh;
 // Wake path: initialized, same binding, no pending ticket, changed active interface.
 assert(dbhw::endpointRefresh(true,false,true,false,true,false,false,false,true)==R::Reprobe);
 assert(dbhw::endpointRefresh(true,true,true,true,true,false,false,false,true)==R::Reprobe);
 assert(dbhw::endpointRefresh(true,false,true,true,true,false,false,false,true)==R::Keep);
 assert(dbhw::endpointRefresh(true,false,false,true,true,false,false,false,true)==R::Reprobe);
 assert(dbhw::endpointRefresh(true,false,true,true,true,false,false,true,true)==R::Keep);
 assert(dbhw::endpointRefresh(true,false,true,true,true,false,false,false,false)==R::Reprobe);
 // Enabled path: provider handoff retires; binding/interface/range changes reprobe.
 assert(dbhw::endpointRefresh(true,false,true,true,true,true,true,false,true)==R::Retire);
 assert(dbhw::endpointRefresh(true,false,false,true,true,false,true,false,true)==R::Reprobe);
 assert(dbhw::endpointRefresh(true,false,true,false,true,false,true,false,true)==R::Reprobe);
 assert(dbhw::endpointRefresh(true,false,true,true,false,false,true,false,true)==R::Reprobe);
 assert(dbhw::endpointRefresh(true,false,true,true,true,false,true,false,true)==R::Keep);
 puts("endpoint refresh: inactive interfaces ignored, unique replacement selected, wake/provider/binding transitions classified");
}
