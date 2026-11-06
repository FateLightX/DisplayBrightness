// Executes production serialization and early publication against a native
// preference double. Native persistence across a reboot is not simulated.
#include "RegistryDoubles.hpp"
#include "VerifiedBrightness.hpp"
#include "EndpointRequests.hpp"
const char *ownerKey="DisplayBrightnessProbeRange",*simKey="DisplayBrightnessProbeSimulated";
const char *brightnessPreferenceKey="DisplayBrightnessVerifiedState";
IOService framebuffer;
enum class Setter {Store,Reject,Drop,WrongValue};
Setter setter=Setter::Store;
unsigned writes=0;
bool checkLocks=false,systemGate=false,frameGate=false,queueLock=false;
OSObject *copyPreference(IOService *frame,IOService *display,const char *key){
 if(checkLocks)assert(systemGate&&frameGate&&!queueLock);
 assert(frame==&framebuffer);return display->copyProperty(key);
}
bool setPreference(IOService *frame,IOService *display,const char *key,OSObject *value){
 if(checkLocks)assert(systemGate&&frameGate&&!queueLock);
 assert(frame==&framebuffer);++writes;
 if(setter==Setter::Reject)return false;
 if(setter==Setter::Drop)return true;
 return display->setProperty(key,setter==Setter::WrongValue?kOSBooleanTrue:value);
}
#include "../src/PreferenceStore.inc"
#include "../src/ParameterPublication.inc"
bool hardwareRequested=true,startIO=true,admissionCall=true;
constexpr unsigned kIOServiceRegisteredState=1;
void clock_get_uptime(UInt64 *value){*value=7000000000ULL;}
void absolutetime_to_nanoseconds(UInt64 value,UInt64 *out){*out=value;}
bool ownedEndpoint(IOService *s){
 auto *params=OSDynamicCast(OSDictionary,s->getProperty("IODisplayParameters"));
 auto *range=OSDynamicCast(OSDictionary,s->getProperty(ownerKey));
 return params&&range&&params->getObject("brightness")==range&&range->getObject("DisplayBrightnessSynthetic")==kOSBooleanTrue;
}
unsigned edm=0;
void publishEDMMarker(IOService *s){assert(ownedEndpoint(s));++edm;}
UInt8 edid[128]={0,255,255,255,255,255,255,0,0x04,0x43,0x21,0x43,1};
const char *readCachedBrightness(IOService *s,dbcache::Brightness &value){
 return loadVerifiedPreference(&framebuffer,s,edid,sizeof(edid),value);
}
#include "../src/EarlyDeclaration.inc"
using IOReturn=int;
constexpr int kIOReturnNotReady=-1,kIOReturnSuccess=0;
struct IOWorkLoop {
 bool inGate()const{return systemGate;}
 void runAction(IOReturn (*action)(OSObject *,void *,void *,void *,void *),OSObject *target,void *arg){
  assert(!systemGate&&!frameGate&&!queueLock);systemGate=true;
  action(target,arg,nullptr,nullptr,nullptr);systemGate=false;
 }
} loop;
bool writerReady=true,loopReady=true;
IOWorkLoop *preferenceWorkLoop(){return loopReady?&loop:nullptr;}
bool preferenceWriterReady(){return writerReady;}
struct FrameScope {
 explicit FrameScope(IOService *){assert(systemGate&&!queueLock&&!frameGate);frameGate=true;}
 ~FrameScope(){assert(!queueLock);frameGate=false;}
};
int hardwareQueue=1;
void IOLockLock(int){assert(systemGate&&frameGate&&!queueLock);queueLock=true;}
void IOLockUnlock(int){assert(queueLock);queueLock=false;}
struct HardwareSlot {
 IOService *display=nullptr,*frame=&framebuffer;
 UInt8 boundEDID[128]={};
 bool retiring=false,terminal=false,live=true;
 dbhw::EndpointRequests requests;
 UInt64 sequence=1;
 UInt16 verifiedCurrent=0,verifiedMaximum=255;
};
bool hardwareLive(HardwareSlot *c,FrameScope &){assert(systemGate&&frameGate&&!queueLock);return c->live&&ownedEndpoint(c->display);}
#include "../src/VerifiedPersistence.inc"
static std::string reason(IOService &s){return OSDynamicCast(OSString,s.getProperty("DisplayBrightnessEarlyDeclarationLastReason"))->value;}
static unsigned value(IOService &s){
 auto *range=OSDynamicCast(OSDictionary,s.getProperty(ownerKey));
 return OSDynamicCast(OSNumber,range->getObject("value"))->unsigned32BitValue();
}
int main(){
 UInt8 sum=0;for(unsigned i=0;i<127;i++)sum=static_cast<UInt8>(sum+edid[i]);edid[127]=static_cast<UInt8>(0-sum);
 const unsigned baseline=OSObject::live;
 {
  IOService s;dbcache::Brightness cached;cached.percent=61;
  assert(!strcmp(readCachedBrightness(&s,cached),"cache-missing")&&cached.percent==61);
  declareEarlyEndpoint(&s);
  assert(reason(s)=="cache-missing"&&!s.getProperty("IODisplayParameters")&&edm==0&&writes==0);
  s.setProperty(brightnessPreferenceKey,kOSBooleanTrue);
  declareEarlyEndpoint(&s);assert(reason(s)=="cache-invalid"&&!ownedEndpoint(&s)&&writes==0);
  s.removeProperty(brightnessPreferenceKey);
  OSData::failAllocation=true;
  assert(!strcmp(storeVerifiedPreference(&framebuffer,&s,edid,128,80,100),"allocation-failed")&&writes==0);
  assert(!strcmp(storeVerifiedPreference(&framebuffer,&s,edid,128,101,100),"invalid-record")&&writes==0);
  setter=Setter::Reject;assert(!strcmp(storeVerifiedPreference(&framebuffer,&s,edid,128,80,100),"native-rejected"));
  setter=Setter::Drop;assert(!strcmp(storeVerifiedPreference(&framebuffer,&s,edid,128,80,100),"native-readback-mismatch"));
  setter=Setter::WrongValue;assert(!strcmp(storeVerifiedPreference(&framebuffer,&s,edid,128,80,100),"native-readback-mismatch"));
  setter=Setter::Store;assert(!strcmp(storeVerifiedPreference(&framebuffer,&s,edid,128,0,255),"accepted-native"));
  const unsigned before=writes;
  assert(!strcmp(storeVerifiedPreference(&framebuffer,&s,edid,128,0,255),"unchanged-native")&&writes==before);
  assert(!strcmp(readCachedBrightness(&s,cached),"cache-valid")&&cached.current==0&&cached.maximum==255&&cached.percent==0);
  dbhw::EndpointRequests requests;
  declareEarlyEndpoint(&s);assert(reason(s)=="declared-from-cache"&&value(s)==0&&edm==1);
  assert(!requests.initialized&&!requests.pending&&!requests.busy);
  bool schedule=true;assert(!requests.submitBrightness(cached.percent,schedule)&&!schedule);
  assert(requests.submitBootstrap(schedule)&&schedule&&requests.take()==0);
  assert(publish(&s,true,80));requests.initialized=true;assert(!requests.complete(true));
  assert(value(s)==80&&requests.writable()&&!requests.pending); // Fresh read wins, no cache replay.
  declareEarlyEndpoint(&s);assert(reason(s)=="already-owned"&&value(s)==80);
  assert(writes==before); // Restoring/publishing never saves or writes hardware.
 }
 {
  IOService a,b;
  assert(!strcmp(storeVerifiedPreference(&framebuffer,&a,edid,128,35,100),"accepted-native"));
  declareEarlyEndpoint(&b);assert(reason(b)=="cache-missing"&&!ownedEndpoint(&b));
  a.state=kIOServiceRegisteredState;declareEarlyEndpoint(&a);
  assert(reason(a)=="already-registered"&&!ownedEndpoint(&a));
  a.state=0;
  auto *params=OSDictionary::withCapacity(1);auto *native=OSDictionary::withCapacity(1);
  number(native,"value",61);params->setObject("brightness",native);a.setProperty("IODisplayParameters",params);
  native->release();params->release();
  declareEarlyEndpoint(&a);assert(reason(a)=="parameter-publication-failed"&&!ownedEndpoint(&a));
  assert(params->getObject("brightness")==native);
 }
 {
  IOService s;HardwareSlot slot;slot.display=&s;memcpy(slot.boundEDID,edid,128);
  assert(publish(&s,true,80));slot.requests.initialized=true;
  const unsigned before=writes;checkLocks=true;
  // Each exclusion is exercised independently against the real callback.
  slot.retiring=true;persistVerifiedState(&slot,1);slot.retiring=false;
  slot.terminal=true;persistVerifiedState(&slot,1);slot.terminal=false;
  slot.live=false;persistVerifiedState(&slot,1);slot.live=true;
  slot.requests.initialized=false;persistVerifiedState(&slot,1);slot.requests.initialized=true;
  slot.requests.stopped=true;persistVerifiedState(&slot,1);slot.requests.stopped=false;
  slot.requests.pending=true;persistVerifiedState(&slot,1);slot.requests.pending=false;
  persistVerifiedState(&slot,0); // Obsolete completion.
  writerReady=false;persistVerifiedState(&slot,1);writerReady=true;
  loopReady=false;persistVerifiedState(&slot,1);loopReady=true;
  assert(writes==before);
  persistVerifiedState(&slot,1);assert(writes==before+1);
  assert(OSDynamicCast(OSString,s.getProperty("DisplayBrightnessCacheSaveState"))->value=="accepted-native");
  assert(OSDynamicCast(OSNumber,s.getProperty("DisplayBrightnessCacheCurrent"))->unsigned32BitValue()==0);
  persistVerifiedState(&slot,1);assert(writes==before+1); // Duplicate saves are inert.
  // A replaced native owner must prevent another commit as well.
  s.removeProperty(ownerKey);slot.verifiedCurrent=127;
  persistVerifiedState(&slot,1);assert(writes==before+1);
  assert(!systemGate&&!frameGate&&!queueLock);checkLocks=false;
 }
 assert(OSObject::live==baseline);
 puts("native preference/early declaration: no fabricated value or replay; acceptance readback; per-display isolation; zero; native ownership; stale/retired/stopped exclusion; commit lock order");
}
