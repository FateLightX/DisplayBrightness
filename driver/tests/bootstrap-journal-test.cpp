#include "BootstrapJournal.hpp"
#include <cassert>
#include <cstdio>
int main(){
 dbstate::BootstrapJournal j;
 dbstate::BootstrapAttempt a{};a.startedNs=10;a.finishedNs=20;a.phase=dbstate::BootstrapPhase::Read;a.reads=3;
 assert(j.append(a));a.startedNs=30;a.finishedNs=50;a.phase=dbstate::BootstrapPhase::Ready;a.success=true;a.reads=1;assert(j.append(a));
 assert(j.count==2&&j.attempts[0].phase==dbstate::BootstrapPhase::Read&&!j.attempts[0].success&&j.attempts[0].reads==3);
 assert(j.attempts[1].success&&j.attempts[1].startedNs==30);
 assert(j.append(a));a.startedNs=100;assert(j.append(a));a.startedNs=200;assert(j.append(a));assert(j.count==4&&j.dropped==1&&j.total==5&&j.attempts[2].startedNs==30&&j.attempts[3].startedNs==200);
 j=dbstate::BootstrapJournal{};assert(j.count==0&&j.dropped==0&&j.total==0);
 puts("bootstrap journal: ordered outcomes, fixed bound, first-three preservation, latest snapshot and endpoint reset pass");
}
