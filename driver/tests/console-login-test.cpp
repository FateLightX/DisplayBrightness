#include "RegistryDoubles.hpp"
#include "LoginGate.hpp"
using IOReturn=int;using vm_size_t=size_t;using thread_call_param_t=void *;
constexpr IOReturn kIOReturnSuccess=0;
constexpr UInt32 kIOMessageConsoleSecurityChange=0x128;
const char *gIOConsoleSecurityInterest="console-security",*gIOTerminatedNotification="terminated";
const char *gIOFirstPublishNotification="first-publish";
struct Call {bool pending=false;unsigned wakes=0;};
using thread_call_t=Call *;
void thread_call_enter(Call *call){assert(call);call->pending=true;++call->wakes;}
UInt64 now=100;
void clock_get_uptime(UInt64 *ticks){*ticks=now;}
void absolutetime_to_nanoseconds(UInt64 ticks,UInt64 *ns){*ns=ticks;}
struct IORegistryEntry {static IOService *root;static IOService *getRegistryRoot(){return root;}};
IOService *IORegistryEntry::root=nullptr;
unsigned admissions=0,cleanups=0,registrations=0;
void scheduleAdmission(){++admissions;}
void requestPendingCleanup(){++cleanups;}
InterestHandler observer=nullptr;
MatchingHandler terminated=nullptr,published=nullptr;
bool rejectRegistration=false,rejectPublish=false,rejectTermination=false,notifyWhileRegistering=false;
IONotifier *IOService::registerInterest(const char *kind,InterestHandler handler,void *,void *){
 assert(this==serviceRoot&&this!=IORegistryEntry::root&&kind==gIOConsoleSecurityInterest);
 ++registrations;observer=handler;
 if(rejectRegistration)return nullptr;
 if(notifyWhileRegistering)observer(nullptr,nullptr,kIOMessageConsoleSecurityChange,this,nullptr,0);
 return new IONotifier;
}
OSDictionary *IOService::serviceMatching(const char *name){assert(!strcmp(name,"AppleDisplay"));return OSDictionary::withCapacity(1);}
IONotifier *IOService::addMatchingNotification(const char *kind,OSDictionary *,MatchingHandler handler,void *,void *){
 if(kind==gIOTerminatedNotification){if(rejectTermination)return nullptr;terminated=handler;}
 else{assert(kind==gIOFirstPublishNotification);if(rejectPublish)return nullptr;published=handler;}
 return new IONotifier;
}
#include "../src/ConsoleLogin.inc"
void users(IOService &root,bool onConsole,bool done,bool wrongKey=false){
 auto *a=new OSArray;auto *d=OSDictionary::withCapacity(2);
 if(onConsole)d->setObject("kCGSSessionOnConsoleKey",kOSBooleanTrue);
 if(done)d->setObject(wrongKey?"kCGSSessionLoginDoneKey":"kCGSessionLoginDoneKey",kOSBooleanTrue);
 a->setObject(d);root.setProperty("IOConsoleUsers",a);d->release();a->release();
}
void reset(){
 if(loginNotifier)loginNotifier->release();if(displayPublishNotifier)displayPublishNotifier->release();if(displayTerminationNotifier)displayTerminationNotifier->release();
 loginNotifier=nullptr;displayPublishNotifier=nullptr;displayTerminationNotifier=nullptr;loginRegistrationAttempted=false;
 loginGate=dbstate::LoginGate{};loginLastSignalTicks=0;loginSignalCount=0;
 admissions=0;cleanups=0;registrations=0;rejectRegistration=false;rejectPublish=false;rejectTermination=false;notifyWhileRegistering=false;
}
int main(){
 const auto baseline=OSObject::live;
 {
  IOService service,registry;IOService::serviceRoot=&service;IORegistryEntry::root=&registry;
  Call call;loginCall=&call;
  users(service,true,true); // Wrong root must never grant permission.
  requestLoginCheck();assert(call.pending&&!loginGate.allowed());
  call.pending=false;loginWorker(nullptr,nullptr);assert(!loginGate.allowed()&&registrations==1);
  users(registry,true,true,true);loginWorker(nullptr,nullptr);assert(!loginGate.allowed());
  users(registry,false,true);loginWorker(nullptr,nullptr);assert(!loginGate.allowed());
  users(registry,true,false);loginWorker(nullptr,nullptr);assert(!loginGate.allowed());
  users(registry,true,true);now=500;
  const auto objects=OSObject::live,wakes=call.wakes;
  observer(nullptr,nullptr,0,&service,nullptr,0);assert(call.wakes==wakes);
  observer(nullptr,nullptr,kIOMessageConsoleSecurityChange,&service,nullptr,0);
  assert(OSObject::live==objects&&!loginGate.allowed()&&admissions==0&&call.pending);
  now=600;call.pending=false;loginWorker(nullptr,nullptr);
  assert(loginGate.allowed()&&admissions==1&&registrations==1);
  assert(OSDynamicCast(OSNumber,service.getProperty("DisplayBrightnessLoginObservedUptimeNs"))->value==600);
  assert(OSDynamicCast(OSNumber,service.getProperty("DisplayBrightnessLoginSignalUptimeNs"))->value==500);
  users(registry,false,false);const auto after=call.wakes;
  observer(nullptr,nullptr,kIOMessageConsoleSecurityChange,&service,nullptr,0);
  requestLoginCheck();loginWorker(nullptr,nullptr);assert(call.wakes==after&&admissions==1);
  terminated(nullptr,nullptr,&registry,nullptr);published(nullptr,nullptr,&registry,nullptr);assert(cleanups==2);
  reset();
  users(registry,true,true);notifyWhileRegistering=true;
  loginWorker(nullptr,nullptr);assert(loginGate.allowed()&&admissions==1&&call.pending);
  loginWorker(nullptr,nullptr);assert(admissions==1&&registrations==1); // Coalesced follow-up is inert.
  reset();
  loginWorker(nullptr,nullptr);assert(loginGate.allowed()&&admissions==1&&loginSignalCount==0);
  assert(OSDynamicCast(OSString,service.getProperty("DisplayBrightnessLoginObservationSource"))->value=="initial-snapshot");
  reset();rejectRegistration=true;
  loginWorker(nullptr,nullptr);assert(!loginGate.allowed()&&admissions==0);
  assert(OSDynamicCast(OSString,service.getProperty("DisplayBrightnessStartupGate"))->value=="notifier-unavailable");
  reset();rejectPublish=true;
  loginWorker(nullptr,nullptr);assert(!loginGate.allowed()&&admissions==0);
  reset();rejectTermination=true;
  loginWorker(nullptr,nullptr);assert(loginGate.allowed()&&admissions==1);
  assert(OSDynamicCast(OSString,service.getProperty("DisplayBrightnessPendingCleanup"))->value=="on-display-event");
  reset();IOService::serviceRoot=nullptr;
  loginWorker(nullptr,nullptr);assert(!loginRegistrationAttempted&&!loginGate.allowed());
  IOService::serviceRoot=&service;loginWorker(nullptr,nullptr);assert(loginGate.allowed());
  reset();IOService::serviceRoot=nullptr;IORegistryEntry::root=nullptr;loginCall=nullptr;
 }
 assert(OSObject::live==baseline);
 puts("production console observer: correct root/flags, callback-only wake, register-then-snapshot, preexisting login, reentrant notification, late root, failure and cleanup");
}
