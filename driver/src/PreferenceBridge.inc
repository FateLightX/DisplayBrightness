using CopyPreference=OSObject *(*)(IOService *,IOService *,const OSSymbol *);
using SetPreference=bool (*)(IOService *,IOService *,const OSSymbol *,OSObject *);
CopyPreference copyPreference=nullptr;
SetPreference setPreference=nullptr;
IOWorkLoop **graphicsSystemWorkLoop=nullptr;
IOTimerEventSource **graphicsPrefsTimer=nullptr;
const OSSymbol **graphicsPrefsVersionKey=nullptr;
OSNumber **graphicsPrefsVersionValue=nullptr;
const OSSymbol *brightnessPreferenceKey=nullptr;
bool preferenceSymbolsReady(){
 return copyPreference&&setPreference&&graphicsSystemWorkLoop&&graphicsPrefsTimer&&
        graphicsPrefsVersionKey&&graphicsPrefsVersionValue&&brightnessPreferenceKey;
}
IOWorkLoop *preferenceWorkLoop(){return preferenceSymbolsReady()?*graphicsSystemWorkLoop:nullptr;}
bool preferenceWriterReady(){
 return preferenceSymbolsReady()&&*graphicsPrefsTimer&&*graphicsPrefsVersionKey&&*graphicsPrefsVersionValue;
}
#include "PreferenceStore.inc"
const char *readCachedBrightness(IOService *s,dbcache::Brightness &value){
 auto *loop=preferenceWorkLoop();
 if(!loop)return "cache-backend-unavailable";
 // Never take the system gate from doUpdate: its caller may hold a controller
 // gate. Startup normally holds both; fail closed if that is not true here.
 if(!loop->inGate())return "cache-system-gate-not-held";
 auto *key=s->copyProperty("IODisplayPrefsKey");
 bool keyed=OSDynamicCast(OSSymbol,key)!=nullptr;
 if(key)key->release();if(!keyed)return "cache-display-key-unavailable";
 auto *connection=s->copyParentEntry(gIOServicePlane);
 auto *parent=connection?connection->copyParentEntry(gIOServicePlane):nullptr;
 auto *frame=OSDynamicCast(IOService,parent);
 const char *result="cache-topology-unavailable";
 if(pilot(s)&&hasClass(connection,"IODisplayConnect")&&hasClass(frame,"IOFramebuffer")&&!frame->isInactive()){
  auto *object=s->copyProperty("IODisplayEDID");auto *edid=OSDynamicCast(OSData,object);
  if(edid)result=loadVerifiedPreference(frame,s,static_cast<const UInt8 *>(edid->getBytesNoCopy()),edid->getLength(),value);
  if(object)object->release();
 }
 if(parent)parent->release();if(connection)connection->release();return result;
}
void resolvePreferenceBackend(KernelPatcher &patcher,size_t id,mach_vm_address_t address,size_t size){
 copyPreference=reinterpret_cast<CopyPreference>(patcher.solveSymbol(id,"__ZN13IOFramebuffer14copyPreferenceEP9IODisplayPK8OSSymbol",address,size));
 setPreference=reinterpret_cast<SetPreference>(patcher.solveSymbol(id,"__ZN13IOFramebuffer13setPreferenceEP9IODisplayPK8OSSymbolP8OSObject",address,size));
 graphicsSystemWorkLoop=reinterpret_cast<IOWorkLoop **>(patcher.solveSymbol(id,"__ZL19gIOFBSystemWorkLoop",address,size));
 graphicsPrefsTimer=reinterpret_cast<IOTimerEventSource **>(patcher.solveSymbol(id,"__ZL22gIOFBDelayedPrefsEvent",address,size));
 graphicsPrefsVersionKey=reinterpret_cast<const OSSymbol **>(patcher.solveSymbol(id,"__ZL26gIOGraphicsPrefsVersionKey",address,size));
 graphicsPrefsVersionValue=reinterpret_cast<OSNumber **>(patcher.solveSymbol(id,"__ZL28gIOGraphicsPrefsVersionValue",address,size));
 brightnessPreferenceKey=OSSymbol::withCStringNoCopy("DisplayBrightnessVerifiedState");
 // An optional backend failure must not leave an error that disables routing.
 patcher.clearError();
 auto *root=IOService::getServiceRoot();
 if(root)root->setProperty("DisplayBrightnessCacheBackend",preferenceSymbolsReady()?"symbols-resolved":"symbols-unavailable");
}
