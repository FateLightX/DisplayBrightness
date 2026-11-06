#include <Headers/kern_api.hpp>
#include <Headers/plugin_start.hpp>
#include <libkern/c++/OSData.h>
#include <libkern/c++/OSNumber.h>
#include <libkern/c++/OSSymbol.h>
#include "ProbeIdentity.hpp"
#include "ImageGate.hpp"
#include "DDCProtocol.hpp"
#include "HardwarePolicy.hpp"
#include "LatestRequest.hpp"
#include <libkern/c++/OSArray.h>
#include "ResponsiveRead.hpp"
#include "VerifyTarget.hpp"
#include "RetirePolicy.hpp"
#include "ReadRecovery.hpp"
#include "StateSync.hpp"
#include "Bootstrap.hpp"
#include "BootstrapJournal.hpp"
#include "BootstrapWindow.hpp"
#include "BootstrapPending.hpp"
#include "StartupDelay.hpp"
#include "EarlyDisplayProfiles.hpp"
#include "EndpointRequests.hpp"
#include <IOKit/IOMessage.h>
#include <IOKit/i2c/IOI2CInterface.h>
#include <kern/thread_call.h>
#include <kern/clock.h>
#include "RequestPolicy.hpp"
#include <libkern/c++/OSBoolean.h>
namespace {
using Update=bool (*)(IOService *);
using SetProperties=IOReturn (*)(IOService *,OSObject *);
SetProperties originalProperties=nullptr;
UInt64 eventSequence=0;
using IntegerSet=bool (*)(IOService *,OSDictionary *,const OSSymbol *,UInt32);
using FrameGate=void (*)(IOService *);
Update originalUpdate=nullptr;
IntegerSet originalInteger=nullptr;
FrameGate frameLock=nullptr,frameUnlock=nullptr;
bool routesReady=false,uiRequested=false,edmRequested=false;
using StartIO=IOReturn (*)(IOService *,IOI2CRequest *);
StartIO startIO=nullptr;
IOLock *probeSerial=nullptr;
const char *ownerKey="DisplayBrightnessProbeRange";
const char *simKey="DisplayBrightnessProbeSimulated";
const char *paths[]={"/System/Library/Extensions/IOGraphicsFamily.kext/IOGraphicsFamily"};
KernelPatcher::KextInfo graphics={"com.apple.iokit.IOGraphicsFamily",paths,1,{true},{},0};
const UInt8 uuid[]={0x74,0xaa,0x64,0x83,0x8a,0xb4,0x3a,0x94,0x81,0x43,0x5e,0x30,0x6c,0x16,0xd7,0xda};
bool enabled(){return __atomic_load_n(&routesReady,__ATOMIC_ACQUIRE) && uiRequested;}
bool ownedEndpoint(IOService *);
bool endpointReady(IOService *);
bool hasClass(IORegistryEntry *s,const char *name){
 if(!s)return false;
 for(const OSMetaClass *m=s->getMetaClass();m;m=m->getSuperClass())
  if(!strcmp(m->getClassName(),name))return true;
 return false;
}
bool pilot(IOService *s){
 if(!s || s->isInactive() || strcmp(s->getMetaClass()->getClassName(),"AppleDisplay"))return false;
 OSObject *p=s->copyProperty("IODisplayEDID");
 OSData *e=OSDynamicCast(OSData,p);
 bool ok=e && isObservedDisplay(static_cast<const UInt8 *>(e->getBytesNoCopy()),e->getLength());
 if(p)p->release();return ok;
}
void routeState(const char *state){
 auto *root=IOService::getServiceRoot();
 if(root){root->setProperty("DisplayBrightnessRequestProbeRouting",state);root->setProperty("DisplayBrightnessRequestProbeVersion",xStringify(MODULE_VERSION));}
}
UInt64 nextEvent(){return __atomic_add_fetch(&eventSequence,1ULL,__ATOMIC_RELAXED);}
void record(IOService *s,const char *property,UInt64 seq,const char *stage,const OSSymbol *key,bool hasValue,UInt32 value,UInt32 result){
 if(!pilot(s))return;
 OSDictionary *d=OSDictionary::withCapacity(6);if(!d)return;
 OSNumber *n=OSNumber::withNumber(seq,64);bool ok=n && d->setObject("sequence",n);if(n)n->release();
 OSString *text=OSString::withCString(stage);ok=ok && text && d->setObject("stage",text);if(text)text->release();
 if(key)ok=ok && d->setObject("key",const_cast<OSSymbol *>(key));
 if(hasValue){n=OSNumber::withNumber(value,32);ok=ok && n && d->setObject("value",n);if(n)n->release();}
 n=OSNumber::withNumber(result,32);ok=ok && n && d->setObject("result",n);if(n)n->release();
 if(ok){
  s->setProperty(property,d);
  if(key && key->isEqualTo("brightness"))
   s->setProperty(!strcmp(property,"DisplayBrightnessIntegerEntry")?"DisplayBrightnessBrightnessEntry":"DisplayBrightnessBrightnessExit",d);
 }
 d->release();
}
IOReturn properties(IOService *s,OSObject *input){
 UInt64 seq=nextEvent();
 auto *dict=OSDynamicCast(OSDictionary,input);
 if(dict){if(auto *nested=OSDynamicCast(OSDictionary,dict->getObject("IODisplayParameters")))dict=nested;}
 OSObject *v=dict?dict->getObject("brightness"):nullptr;
 bool brightnessPresent=v!=nullptr;
 if(auto *range=OSDynamicCast(OSDictionary,v))v=range->getObject("value");
 auto *n=OSDynamicCast(OSNumber,v);
 bool hasValue=n!=nullptr;UInt32 value=n?n->unsigned32BitValue():0;
 // Record numeric brightness only, never arbitrary user dictionaries or data.
 record(s,"DisplayBrightnessSetPropertiesEntry",seq,brightnessPresent?"entry-brightness":"entry-no-brightness",nullptr,hasValue,value,0);
 if(brightnessPresent)record(s,"DisplayBrightnessBrightnessPropertyEntry",seq,"entry",nullptr,hasValue,value,0);
 // Reject the entire synthetic brightness transaction before the native setter
 // acquires its framebuffer gate. No pending/default brightness is replayed.
 bool waiting=brightnessPresent && ownedEndpoint(s) && !endpointReady(s);
 IOReturn result=waiting?kIOReturnNotReady:originalProperties(s,input);
 record(s,"DisplayBrightnessSetPropertiesExit",seq,"exit",nullptr,hasValue,value,static_cast<UInt32>(result));
 if(brightnessPresent)record(s,"DisplayBrightnessBrightnessPropertyExit",seq,waiting?"rejected-not-ready":"exit",nullptr,hasValue,value,static_cast<UInt32>(result));
 return result;
}
struct FrameScope {
 IORegistryEntry *connection=nullptr,*frame=nullptr;
 IOService *frameService=nullptr;
 bool locked=false;
 explicit FrameScope(IOService *s){
  connection=s->copyParentEntry(gIOServicePlane);
  if(!hasClass(connection,"IODisplayConnect"))return;
  frame=connection->copyParentEntry(gIOServicePlane);
  if(!hasClass(frame,"IOFramebuffer"))return;
  frameService=OSDynamicCast(IOService,frame);
  if(!frameService || frameService->isInactive())return;
  frameLock(frameService);locked=true;
 }
 bool validTopology(IOService *s){
  if(!locked || !s || s->isInactive() || frameService->isInactive() || strcmp(s->getMetaClass()->getClassName(),"AppleDisplay"))return false;
  auto *c=s->copyParentEntry(gIOServicePlane);
  auto *f=connection->copyParentEntry(gIOServicePlane);
  bool ok=c==connection && f==frame;
  if(c)c->release();if(f)f->release();return ok;
 }
 bool valid(IOService *s){return validTopology(s)&&pilot(s);}
 ~FrameScope(){
  if(locked)frameUnlock(frameService);
  if(frame)frame->release();if(connection)connection->release();
 }
};
bool publish(IOService *,bool,UInt32);
void publishEDMMarker(IOService *);
#include "HardwareWorker.inc"
#include "ParameterPublication.inc"
void publishEDMMarker(IOService *s){
 // Experimental driver-protocol marker, not a changed EDID or hardware identity.
 // Same startup/worker serialization as publish; only our endpoint qualifies.
 if(!edmRequested)return;
 OSObject *property=s->copyProperty("IODisplayParameters");
 OSObject *owned=s->copyProperty(ownerKey);
 auto *params=OSDynamicCast(OSDictionary,property);
 auto *range=OSDynamicCast(OSDictionary,owned);
 bool ours=params && range && params->getObject("brightness")==range && range->getObject("DisplayBrightnessSynthetic")==kOSBooleanTrue;
 if(property)property->release();if(owned)owned->release();
 if(!ours){
  OSObject *marker=s->copyProperty("DisplayBrightnessEDMProbeOwned");
  OSObject *active=s->copyProperty("IODisplayCapabilityString");
  if(marker && active==marker)s->removeProperty("IODisplayCapabilityString");
  if(marker)s->removeProperty("DisplayBrightnessEDMProbeOwned");
  if(active)active->release();if(marker)marker->release();
  // Registry revocation does not guarantee CoreBrightness cleared cached flags.
  s->setProperty("DisplayBrightnessEDMProbeState","revoked-no-synthetic-owner");return;
 }
 OSObject *existing=s->copyProperty("IODisplayCapabilityString");
 if(existing){
  OSObject *marker=s->copyProperty("DisplayBrightnessEDMProbeOwned");
  bool retained=marker && existing==marker;
  if(marker && !retained)s->removeProperty("DisplayBrightnessEDMProbeOwned");
  if(marker)marker->release();existing->release();
  s->setProperty("DisplayBrightnessEDMProbeState",retained?"marker-retained-synthetic":"existing-capability-preserved");return;
 }
 static const char text[]="model(EDM)";
 OSData *data=OSData::withBytes(text,sizeof(text)-1);
 if(!data){s->setProperty("DisplayBrightnessEDMProbeState","allocation-failed");return;}
 bool ok=s->setProperty("DisplayBrightnessEDMProbeOwned",data);
 if(ok)ok=s->setProperty("IODisplayCapabilityString",data);
 if(!ok){
  if(s->getProperty("IODisplayCapabilityString")==data)s->removeProperty("IODisplayCapabilityString");
  if(s->getProperty("DisplayBrightnessEDMProbeOwned")==data)s->removeProperty("DisplayBrightnessEDMProbeOwned");
 }
 data->release();
 s->setProperty("DisplayBrightnessEDMProbeState",ok?"marker-published-synthetic":"publication-failed");
}
void earlyDeclarationReason(IOService *s,const char *reason){
 if(!s->getProperty("DisplayBrightnessEarlyDeclarationFirstReason"))
  s->setProperty("DisplayBrightnessEarlyDeclarationFirstReason",reason);
 s->setProperty("DisplayBrightnessEarlyDeclarationLastReason",reason);
}
void declareEarlyEndpoint(IOService *s){
 // IODisplay::start calls doUpdate before displayOnline/registerService. After
 // registration, all parameter publication remains on the deferred worker.
 if(!hardwareRequested || !startIO || !admissionCall){earlyDeclarationReason(s,"workers-unavailable");return;}
 if(s->getState()&kIOServiceRegisteredState){earlyDeclarationReason(s,"already-registered");return;}
 if(ownedEndpoint(s)){earlyDeclarationReason(s,"already-owned");return;}
 auto *p=s->copyProperty("IODisplayEDID");auto *edid=OSDynamicCast(OSData,p);
 bool known=edid && dbstate::earlyDisplay(static_cast<const UInt8 *>(edid->getBytesNoCopy()),edid->getLength());
 if(p)p->release();if(!known){earlyDeclarationReason(s,"unverified-edid");return;}
 if(!publish(s,true,dbstate::provisionalBrightness)){earlyDeclarationReason(s,"parameter-publication-failed");return;}
 publishEDMMarker(s);
 UInt64 now=0,ns=0;clock_get_uptime(&now);absolutetime_to_nanoseconds(now,&ns);
 s->setProperty("DisplayBrightnessEarlyDeclarationUptimeNs",ns,64);
 s->setProperty("DisplayBrightnessInterfaceState","pending-hardware");
 s->setProperty("DisplayBrightnessProvisionalPercent",dbstate::provisionalBrightness,32);
 earlyDeclarationReason(s,"declared");
}
bool update(IOService *s){
 bool observed=pilot(s);bool parametersBefore=false;
 if(observed){
  auto *p=s->copyProperty("IODisplayParameters");parametersBefore=OSDynamicCast(OSDictionary,p)!=nullptr;
  if(p)p->release();
 }
 bool ok=originalUpdate(s);
 if(pilot(s)){
  if(!s->getProperty("DisplayBrightnessFirstUpdateUptimeNs")){
   UInt64 now=0,ns=0;clock_get_uptime(&now);absolutetime_to_nanoseconds(now,&ns);
   s->setProperty("DisplayBrightnessFirstUpdateUptimeNs",ns,64);
   s->setProperty("DisplayBrightnessFirstUpdateSucceeded",ok);
   s->setProperty("DisplayBrightnessFirstUpdateRegistered",(s->getState()&kIOServiceRegisteredState)!=0);
   s->setProperty("DisplayBrightnessFirstUpdateObservedBefore",observed);
   s->setProperty("DisplayBrightnessFirstUpdateParametersBefore",parametersBefore);
   auto *p=s->copyProperty("IODisplayParameters");
   s->setProperty("DisplayBrightnessFirstUpdateParametersAfter",OSDynamicCast(OSDictionary,p)!=nullptr);
   if(p)p->release();
  }
  s->setProperty("DisplayBrightnessRequestProbeRouteReady",__atomic_load_n(&routesReady,__ATOMIC_ACQUIRE));
  IOLog("DisplayBrightness: update display=%llu original=%d\n",s->getRegistryEntryID(),ok);
  if(enabled()){
   // Only the pre-registration declaration is synchronous. Hardware admission,
   // additional framebuffer gates and all DDC stay on the delayed worker.
   declareEarlyEndpoint(s);
   // Upstream parameter handlers can return false while the service continues
   // to start. That does not prevent our separate, known endpoint declaration.
   if(ok || ownedEndpoint(s))queueBootstrap(s);
  }
 }
 return ok;
}
bool marked(OSDictionary *range){
 return range && range->getObject("DisplayBrightnessSynthetic")==kOSBooleanTrue;
}
bool integer(IOService *s,OSDictionary *p,const OSSymbol *key,UInt32 value){
 UInt64 seq=nextEvent();
 record(s,"DisplayBrightnessIntegerEntry",seq,"entry",key,key!=nullptr,value,0);
 if(!key || !key->isEqualTo("brightness")){
  bool ok=originalInteger(s,p,key,value);
  record(s,"DisplayBrightnessIntegerExit",seq,"forwarded",key,key!=nullptr,value,ok);return ok;
 }
 // The range carries its own marker so stale calls remain identifiable even
 // after native dictionary replacement or removal of service-level ownership.
 auto *suppliedRange=p?OSDynamicCast(OSDictionary,p->getObject("brightness")):nullptr;
 bool synthetic=marked(p) || marked(suppliedRange);
 if(!synthetic){
  bool ok=originalInteger(s,p,key,value);
  record(s,"DisplayBrightnessIntegerExit",seq,"forwarded-native",key,true,value,ok);return ok;
 }
 // Also cover callers that bypass setProperties. Check without a framebuffer
 // gate, then recheck readiness at queue submission under hardwareQueue.
 if(!endpointReady(s)){
  record(s,"DisplayBrightnessIntegerExit",seq,"rejected-not-ready",key,true,value,false);
  return false;
 }
 bool accepted=false;bool queued=false;
 if(enabled() && pilot(s)){
  FrameScope frame(s);
  if(frame.valid(s)){
   OSObject *owned=s->copyProperty(ownerKey);
   OSObject *property=s->copyProperty("IODisplayParameters");
   auto *params=OSDynamicCast(OSDictionary,property);
   bool ours=owned && params && params->getObject("brightness")==owned && (p==owned || p==params);
   accepted=dbRequestAction(true,ours,true,value<=100)==DBRequestAction::Simulate;
   if(accepted && hardwareRequested){queued=queueHardware(s,frame.frameService,value);accepted=queued;}
   if(property)property->release();if(owned)owned->release();
  }
 }
 IOLog("DisplayBrightness: UI request display=%llu value=%u accepted=%d hardwareQueued=%d\n",s?s->getRegistryEntryID():0ULL,value,accepted,queued);
 record(s,"DisplayBrightnessIntegerExit",seq,queued?"hardware-queued":(accepted?"simulated-accepted":"rejected"),key,true,value,accepted);
 return accepted; // Known synthetic requests NEVER enter native handlers.
}

void loaded(void *,KernelPatcher &patcher,size_t id,mach_vm_address_t address,size_t size){
 if(id!=graphics.loadIndex)return;
 routeState("callback-entered");
 if(getKernelVersion()!=KernelVersion::Tahoe || getKernelMinorVersion()!=6 || !dbImageUUIDMatches(reinterpret_cast<const void *>(address),size,uuid)){
  routeState("image-kernel-rejected");
  IOLog("DisplayBrightness: image/kernel gate rejected\n");return;
 }
 frameLock=reinterpret_cast<FrameGate>(patcher.solveSymbol(id,"__ZN13IOFramebuffer6fbLockEv",address,size));
 frameUnlock=reinterpret_cast<FrameGate>(patcher.solveSymbol(id,"__ZN13IOFramebuffer8fbUnlockEv",address,size));
 if(hardwareRequested){
  startIO=reinterpret_cast<StartIO>(patcher.solveSymbol(id,"__ZN25IOFramebufferI2CInterface7startIOEP12IOI2CRequest",address,size));
  if(!startIO){IOLog("DisplayBrightness: DDC read probe symbol unavailable\n");patcher.clearError();}
 }
 if(!frameLock || !frameUnlock){
  routeState("framebuffer-gate-unavailable");
  IOLog("DisplayBrightness: framebuffer gate resolution failed; no routes\n");patcher.clearError();return;
 }
 // Interception first; publication stays disabled unless the entire route set succeeds.
 KernelPatcher::RouteRequest requests[]={
  {"__ZN9IODisplay12doIntegerSetEP12OSDictionaryPK8OSSymbolj",integer,originalInteger},
  {"__ZN9IODisplay13setPropertiesEP8OSObject",properties,originalProperties},
  {"__ZN9IODisplay8doUpdateEv",update,originalUpdate}};
 if(!patcher.routeMultiple(id,requests,3,address,size)){
  routeState("routing-incomplete-no-publication");
  IOLog("DisplayBrightness: routing incomplete error=%u; no publication\n",static_cast<unsigned>(patcher.getError()));patcher.clearError();return;
 }
 __atomic_store_n(&routesReady,true,__ATOMIC_RELEASE);
 routeState("ready");
 IOLog("DisplayBrightness: routes ready uiProbe=%d hardware=%d\n",uiRequested,hardwareRequested);
}
void hardwareInitFailed(const char *reason){
 // No routes registered and no call entered yet; do not fall back to fake UI.
 uiRequested=false;hardwareRequested=false;
 if(retireCall){thread_call_free(retireCall);retireCall=nullptr;}
 if(admissionCall){thread_call_free(admissionCall);admissionCall=nullptr;}
 if(hardwareQueue){IOLockFree(hardwareQueue);hardwareQueue=nullptr;}
 if(probeSerial){IOLockFree(probeSerial);probeSerial=nullptr;}
 routeState(reason);
}
void begin(){
 routeState("bootstrap-entered");
 // Automatic capability probing is the only normal mode; -dboff is handled by Lilu.
 uiRequested=true;edmRequested=true;hardwareRequested=true;
 auto *root=IOService::getServiceRoot();if(root)root->setProperty("DisplayBrightnessStartupMode","automatic-brightness");
 if(root)root->setProperty("DisplayBrightnessStartupNotBeforeNanoseconds",dbstate::StartupDelay::notBeforeNs,64);
 hardwareQueue=IOLockAlloc();if(!hardwareQueue){hardwareInitFailed("hardware-queue-unavailable");return;}
 retireCall=thread_call_allocate_with_priority(retireWorker,nullptr,THREAD_CALL_PRIORITY_LOW);
 admissionCall=thread_call_allocate_with_priority(admissionWorker,nullptr,THREAD_CALL_PRIORITY_LOW);
 if(!retireCall||!admissionCall){hardwareInitFailed("bootstrap-workers-unavailable");return;}
 probeSerial=IOLockAlloc();if(!probeSerial){hardwareInitFailed("hardware-serial-unavailable");return;}
 routeState("registering-callback");
 auto e=lilu.onKextLoad(&graphics,1,loaded);
 if(e!=LiluAPI::Error::NoError)routeState("callback-registration-failed");
 IOLog("DisplayBrightness: callback registration=%u uiProbe=%d\n",static_cast<unsigned>(e),uiRequested);
}
const char *disableArgs[]={"-dboff"};
}
PluginConfiguration ADDPR(config)={xStringify(PRODUCT_NAME),parseModuleVersion(xStringify(MODULE_VERSION)),LiluAPI::AllowNormal,disableArgs,1,nullptr,0,nullptr,0,KernelVersion::Tahoe,KernelVersion::Tahoe,begin};
