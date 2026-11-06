#include "RegistryDoubles.hpp"
#include "BootstrapPending.hpp"
#include "BootstrapSchedule.hpp"
#include "EndpointRequests.hpp"
#include "LoginGate.hpp"
namespace dbcompat {constexpr unsigned endpointCapacity=2;}
using thread_call_param_t=void *;
struct Call {bool pending=false;UInt64 due=0;};
using thread_call_t=Call *;
UInt64 now=1000000000ULL;
bool locked=false;unsigned frameDepth=0,frameEntries=0,hardwareDispatches=0,loginChecks=0;
int hardwareQueue=1,gIOServicePlane=0;
constexpr unsigned kIOServiceRegisteredState=1;
void IOLockLock(int){assert(!locked);locked=true;}
void IOLockUnlock(int){assert(locked);locked=false;}
Call admission,work[dbcompat::endpointCapacity];
thread_call_t admissionCall=&admission;
void thread_call_enter(Call *call){if(call!=admissionCall){assert(locked&&!frameDepth);++hardwareDispatches;}call->pending=true;call->due=now;}
void thread_call_enter_delayed(Call *call,UInt64 deadline){assert(locked);call->pending=true;call->due=deadline;}
constexpr int kMillisecondScale=1;
void clock_interval_to_deadline(unsigned ms,int,UInt64 *out){*out=now+static_cast<UInt64>(ms)*1000000ULL;}
UInt64 uptimeNanoseconds(){return now;}
dbstate::LoginGate loginGate;
void requestLoginCheck(){if(!loginGate.allowed())++loginChecks;}
bool hardwareRequested=true,startIO=true;
bool hasClass(IOService *s,const char *name){return s&&!strcmp(s->className,name);}
struct FrameScope {
 IOService *frameService=nullptr;
 explicit FrameScope(IOService *s){assert(loginGate.allowed()&&!locked);++frameDepth;++frameEntries;frameService=s->parent?s->parent->parent:nullptr;}
 bool valid(IOService *s){return !s->inactive&&frameService&&!frameService->inactive;}
 ~FrameScope(){assert(!locked);--frameDepth;}
};
bool publish(IOService *,bool,unsigned){assert(frameDepth&&loginGate.allowed());return false;}
void publishEDMMarker(IOService *){assert(frameDepth);}
struct HardwareSlot {IOService *display=nullptr;Call *call=nullptr;bool retiring=false;dbhw::EndpointRequests requests;};
HardwareSlot hardwareSlots[dbcompat::endpointCapacity];
bool queueHardware(IOService *,IOService *,UInt32,bool);
#include "../src/BootstrapAdmission.inc"
bool queueHardware(IOService *s,IOService *,UInt32 value,bool bootstrap){
 assert(frameDepth&&!locked&&loginGate.allowed()&&bootstrap&&value==0);
 IOLockLock(hardwareQueue);
 unsigned i=0;
 for(;i<dbcompat::endpointCapacity;i++)if(hardwareSlots[i].display==s)break;
 if(i==dbcompat::endpointCapacity)for(i=0;i<dbcompat::endpointCapacity;i++)if(!hardwareSlots[i].display)break;
 assert(i<dbcompat::endpointCapacity);auto &slot=hardwareSlots[i];slot.display=s;slot.call=&work[i];
 bool schedule=false;assert(slot.requests.submitBootstrap(schedule));
 if(schedule)bootstrapTurns.offer(i,now);
 IOLockUnlock(hardwareQueue);scheduleAdmission();return true;
}
void run(){
 assert(admission.pending);if(now<admission.due)now=admission.due;admission.pending=false;
 admissionWorker(nullptr,nullptr);assert(!locked&&!frameDepth);
}
void finish(bool success){
 unsigned i=bootstrapTurns.active;assert(i!=BootstrapTurns::none&&work[i].pending);
 work[i].pending=false;now+=47000000ULL;
 auto &slot=hardwareSlots[i];slot.requests.take();
 if(success)slot.requests.initialized=true;
 bool again=slot.requests.complete(success);
 if(again)bootstrapTurns.offer(i,now+1000000000ULL);
 assert(bootstrapTurns.finish(i,now));scheduleAdmission();
}
struct Display {
 IOService display,connection,frame;
 Display(){display.state=kIOServiceRegisteredState;connection.className="IODisplayConnect";frame.className="IOFramebuffer";display.parent=&connection;connection.parent=&frame;}
 void unreferenced(){assert(display.refs==1&&connection.refs==1&&frame.refs==1);}
};
int main(){
 const auto baseline=OSObject::live;
 {
  Display a,b,c,d;
  assert(queueBootstrap(&a.display)&&queueBootstrap(&b.display));
  assert(!admission.pending&&!frameEntries&&!hardwareDispatches&&loginChecks==2);
  a.display.inactive=true;
  assert(queueBootstrap(&c.display));a.unreferenced(); // Full queue reclaims stale tickets before insertion.
  c.display.inactive=true;requestPendingCleanup();run();c.unreferenced();
  assert(!admission.pending&&!frameEntries&&!hardwareDispatches&&pendingBootstrap.any());
  b.display.state=0;loginGate.observe(true,true);scheduleAdmission();
  assert(!admission.pending&&!frameEntries); // Fast login cannot probe an unregistered display.
  b.display.state=kIOServiceRegisteredState;requestPendingCleanup();run(); // First-publish event admits b.
  assert(frameEntries==1&&hardwareDispatches==0);b.unreferenced();
  run();assert(hardwareDispatches==1&&bootstrapTurns.active!=BootstrapTurns::none);
  assert(queueBootstrap(&d.display));assert(!admission.pending); // Arrival while another visit runs.
  requestPendingCleanup();run();assert(hardwareDispatches==1&&frameEntries==1);
  finish(false);run();assert(frameEntries==2);run();assert(hardwareDispatches==2); // d passes b's cooldown.
  assert(hardwareSlots[bootstrapTurns.active].display==&d.display);
  finish(true);run();assert(hardwareDispatches==3); // b's retry, without an admission busy loop.
  assert(hardwareSlots[bootstrapTurns.active].display==&b.display);finish(true);
  assert(!admission.pending&&!pendingBootstrap.any());
  const auto dispatched=hardwareDispatches;
  assert(queueBootstrap(&b.display));run();assert(!admission.pending&&hardwareDispatches==dispatched);
  // A changed parent replaces an active old ticket before a full queue rejects it.
  admissionWaitingForRetirement=true;
  assert(queueBootstrap(&b.display)&&queueBootstrap(&d.display));assert(!admission.pending);
  IOService replacement;replacement.className="IOFramebuffer";
  b.connection.parent=&replacement;
  assert(queueBootstrap(&b.display));assert(b.frame.refs==1&&replacement.refs==2);
  b.display.inactive=true;d.display.inactive=true;requestPendingCleanup();run();
  assert(!pendingBootstrap.any()&&!admission.pending&&replacement.refs==1);
  admissionWaitingForRetirement=false;scheduleAdmission();assert(!admission.pending);
  a.unreferenced();b.unreferenced();c.unreferenced();d.unreferenced();
 }
 assert(OSObject::live==baseline);
 puts("production admission: pre-login cleanup without gates/timers, full-queue replacement, one visit per turn, arrivals while active, cooldown fairness, retirement parking and balanced references");
}
