// Experimental, bounded boot-lifetime endpoints. Lilu plugin is not unloadable.
// Never hold hardwareQueue while acquiring native framebuffer or bus gates.
struct HardwareSlot {
 IOService *display=nullptr,*frame=nullptr,*interface=nullptr;
 IORegistryEntry *connection=nullptr;
 thread_call_t call=nullptr;
 UInt8 boundEDID[128]={};
 dbstate::BootstrapWindow bootstrapWindow;
 dbstate::BootstrapAttempt bootAttempt;dbstate::BootstrapJournal bootJournal;
 bool superseded=false;
 bool haveOriginal=false,retiring=false,reclaiming=false,terminal=false;
 dbhw::EndpointRequests requests;
 UInt64 sequence=0;UInt16 verifiedCurrent=0,verifiedMaximum=0;
 bool haveNotified=false;UInt32 lastNotifiedPercent=0;
};
HardwareSlot hardwareSlots[dbcompat::endpointCapacity];
IOLock *hardwareQueue=nullptr;
bool hardwareRequested=false;
thread_call_t retireCall=nullptr,admissionCall=nullptr;
dbstate::BootstrapPending<IOService,dbcompat::endpointCapacity> pendingBootstrap;
bool queueHardware(IOService *,IOService *,UInt32,bool);
unsigned startupRemainingMs(){
 UInt64 now=0,ns=0;clock_get_uptime(&now);absolutetime_to_nanoseconds(now,&ns);
 return dbstate::StartupDelay::remainingMs(ns);
}
void scheduleAdmission(){
 unsigned delay=startupRemainingMs();if(delay<50)delay=50;
 uint64_t deadline;clock_interval_to_deadline(delay,kMillisecondScale,&deadline);
 thread_call_enter_delayed(admissionCall,deadline);
}
void admissionWorker(thread_call_param_t,thread_call_param_t){
 // Retirement retries also use this worker. Check before taking tickets or
 // acquiring any framebuffer gate, even if another path scheduled us early.
 if(startupRemainingMs()){scheduleAdmission();return;}
 for(unsigned i=0;i<dbcompat::endpointCapacity;i++){
  IOLockLock(hardwareQueue);auto ticket=pendingBootstrap.take(i);IOLockUnlock(hardwareQueue);
  if(!ticket.display)continue;
  {
   FrameScope gate(ticket.display);
   if(gate.valid(ticket.display)&&gate.frameService==ticket.frame){
    publish(ticket.display,false,0);publishEDMMarker(ticket.display);
    queueHardware(ticket.display,ticket.frame,0,true);
   }
  }
  ticket.frame->release();ticket.display->release();
 }
}
// hardwareQueue held. Existing tickets already own their reference pair.
bool rememberBootstrap(IOService *s,IOService *fb){
 auto result=pendingBootstrap.offer(s,fb);
 if(result==dbstate::Offer::Inserted){s->retain();fb->retain();}
 return result!=dbstate::Offer::Full;
}
bool queueBootstrap(IOService *s){
 if(!hardwareRequested || !hardwareQueue || !admissionCall || !startIO)return false;
 // Copy topology references without acquiring a framebuffer gate in doUpdate.
 auto *connection=s->copyParentEntry(gIOServicePlane);
 auto *parent=connection?connection->copyParentEntry(gIOServicePlane):nullptr;
 auto *fb=OSDynamicCast(IOService,parent);
 bool queued=false;
 if(hasClass(connection,"IODisplayConnect") && hasClass(fb,"IOFramebuffer") && !fb->isInactive()){
  IOLockLock(hardwareQueue);queued=rememberBootstrap(s,fb);IOLockUnlock(hardwareQueue);
 }
 if(parent)parent->release();if(connection)connection->release();
 if(queued)scheduleAdmission();
 else s->setProperty("DisplayBrightnessBootstrapState","admission-unavailable");
 return queued;
}

void retireWorker(thread_call_param_t,thread_call_param_t){
 // Runs independently of framebuffer gates. Retiring slots cannot accept work.
 for(auto &c:hardwareSlots){
  IOLockLock(hardwareQueue);
  bool retire=c.retiring && !c.reclaiming;
  if(retire)c.reclaiming=true;
  thread_call_t call=c.call;
  IOLockUnlock(hardwareQueue);
  if(!retire)continue;
  // No queue, bus or native framebuffer gate held while waiting.
  if(call)thread_call_cancel_wait(call);
  IOLockLock(hardwareQueue);
  IOService *display=c.display,*frame=c.frame,*interface=c.interface;
  IORegistryEntry *connection=c.connection;c.connection=nullptr;
  c.display=nullptr;c.frame=nullptr;c.interface=nullptr;
  c.bootstrapWindow=dbstate::BootstrapWindow{};
  c.bootAttempt=dbstate::BootstrapAttempt{};c.bootJournal=dbstate::BootstrapJournal{};
  c.requests=dbhw::EndpointRequests{};c.haveOriginal=false;c.terminal=false;
  c.sequence=0;c.verifiedCurrent=0;c.verifiedMaximum=0;
  c.haveNotified=false;c.lastNotifiedPercent=0;c.superseded=false;
  // Keep the completed thread_call object stable for reuse; never free in flight.
  c.retiring=false;c.reclaiming=false;
  IOLockUnlock(hardwareQueue);
  if(interface)interface->release();if(connection)connection->release();if(frame)frame->release();if(display)display->release();
 }
}

bool ownedEndpoint(IOService *s){
 OSObject *o=s->copyProperty(ownerKey),*p=s->copyProperty("IODisplayParameters");
 auto *params=OSDynamicCast(OSDictionary,p);
 auto *range=OSDynamicCast(OSDictionary,o);
 bool ok=range && params && params->getObject("brightness")==range && range->getObject("DisplayBrightnessSynthetic")==kOSBooleanTrue;
 if(o)o->release();if(p)p->release();return ok;
}
bool endpointReady(IOService *s){
 if(!hardwareQueue)return false;
 bool ready=false;
 IOLockLock(hardwareQueue);
 for(const auto &c:hardwareSlots)
  if(c.display==s && !c.retiring && c.requests.writable()){ready=true;break;}
 IOLockUnlock(hardwareQueue);
 return ready;
}
bool hardwarePilot(IOService *s);
bool endpointEDIDMatches(HardwareSlot *c){
 auto *o=c->display->copyProperty("IODisplayEDID");auto *e=OSDynamicCast(OSData,o);
 bool ok=e && dbcompat::sameBase(c->boundEDID,static_cast<const UInt8 *>(e->getBytesNoCopy()),e->getLength());
 if(o)o->release();return ok;
}
bool captureEndpointEDID(IOService *s,UInt8 (&bound)[128]){
 auto *o=s->copyProperty("IODisplayEDID");auto *e=OSDynamicCast(OSData,o);
 bool ok=e && dbcompat::validBase(static_cast<const UInt8 *>(e->getBytesNoCopy()),e->getLength());
 if(ok)memcpy(bound,e->getBytesNoCopy(),128);
 if(o)o->release();return ok;
}
bool hardwareLive(HardwareSlot *c,FrameScope &gate,bool bootstrapRead=false){
 if(!c->interface || !gate.valid(c->display))return false;
 // Re-enumerate under this framebuffer's native gate; no stale interface cache.
 unsigned count=0;bool selected=false;
 auto *it=c->frame->getChildIterator(gIOServicePlane);
 if(it){while(auto *o=it->getNextObject()){
  auto *candidate=OSDynamicCast(IOService,o);
  if(candidate && hasClass(candidate,"IOFramebufferI2CInterface")){
   ++count;if(candidate==c->interface)selected=true;
  }
 }it->release();}
 if(!dbcompat::sameTopology(c->connection,gate.connection,c->frame,gate.frameService,count,selected))return false;
 auto *parent=c->interface->copyParentEntry(gIOServicePlane);
 bool ok=hardwarePilot(c->display) && endpointEDIDMatches(c) && gate.valid(c->display) && gate.frameService==c->frame && parent==c->frame && !c->interface->isInactive() && (ownedEndpoint(c->display) || (bootstrapRead && !c->requests.initialized));
 if(parent)parent->release();return ok;
}
// The caller holds bus serialization; scope releases native gate after each IO.
bool hardwareIO(HardwareSlot *c,IOI2CRequest &q,const char *operation){
 FrameScope gate(c->display);
 if(!hardwareLive(c,gate,!strcmp(operation,"get-initial-brightness"))){
  c->terminal=true;
  c->display->setProperty("DisplayBrightnessHardwareState","offline-or-replaced");return false;
 }
 if(!c->requests.initialized){
  UInt64 now=0,ns=0;clock_get_uptime(&now);absolutetime_to_nanoseconds(now,&ns);
  if(!c->bootstrapWindow.within(ns)){
   c->terminal=true;c->bootAttempt.phase=dbstate::BootstrapPhase::Exhausted;
   c->display->setProperty("DisplayBrightnessBootstrapState","probe-window-exhausted-reconnect-required");return false;
  }
 }
 IOReturn status=startIO(c->interface,&q);
 if(!c->requests.initialized){c->bootAttempt.attempted=true;c->bootAttempt.transport=static_cast<UInt32>(status);c->bootAttempt.request=static_cast<UInt32>(q.result);}
 c->display->setProperty("DisplayBrightnessHardwareOperation",operation);
 c->display->setProperty("DisplayBrightnessHardwareTransportResult",static_cast<UInt32>(status),32);
 c->display->setProperty("DisplayBrightnessHardwareRequestResult",static_cast<UInt32>(q.result),32);
 if(status!=kIOReturnSuccess || q.result!=kIOReturnSuccess){
  c->display->setProperty("DisplayBrightnessHardwareState","transport-error-stopped");return false;
 }
 return true;
}
bool hardwareReadOnce(HardwareSlot *c,dbddc::Brightness &b,const char *operation){
 if(!c->requests.initialized){++c->bootAttempt.reads;c->bootAttempt.replyValid=false;}
 UInt8 send[5],reply[11]={0};dbddc::getBrightness(send);
 IOI2CRequest q={};q.sendAddress=0x6e;q.replyAddress=0x6f;q.replySubAddress=0x51;
 q.sendTransactionType=kIOI2CSimpleTransactionType;q.replyTransactionType=kIOI2CSimpleTransactionType;
 q.sendBuffer=reinterpret_cast<vm_address_t>(send);q.sendBytes=sizeof(send);
 q.replyBuffer=reinterpret_cast<vm_address_t>(reply);q.replyBytes=sizeof(reply);
 clock_interval_to_absolutetime_interval(40,1000000,&q.minReplyDelay);
 bool ioOK=hardwareIO(c,q,operation);
 if(!c->requests.initialized)c->bootAttempt.captureReply(reply,sizeof(reply),q.replyBytes);
 if(!ioOK)return false;
 b=dbddc::decodeBrightness(reply,q.replyBytes);
 if(!c->requests.initialized)c->bootAttempt.replyValid=b.valid;
 c->display->setProperty("DisplayBrightnessHardwareReply",reply,sizeof(reply));
 if(!b.valid){c->display->setProperty("DisplayBrightnessHardwareState","invalid-reply-stopped");return false;}
 c->display->setProperty("DisplayBrightnessHardwareActual",b.current,32);
 c->display->setProperty("DisplayBrightnessHardwareMaximum",b.maximum,32);
 return true;
}
bool hardwarePilot(IOService *s){return pilot(s);}
// This measures the full read phase, including retry waits and framebuffer gate.
bool hardwareRead(HardwareSlot *c,dbddc::Brightness &b,const char *operation,bool responsive=false,UInt32 captured=0){
 uint64_t begin=0,end=0,ns=0;clock_get_uptime(&begin);unsigned attempts=0;
 auto read=[&](){++attempts;b=dbddc::Brightness{};return hardwareReadOnce(c,b,operation);};
 bool ok=false;
 if(responsive){
  auto result=dbhw::responsiveRead(read,[](){IOSleep(10);},[&](){return !c->terminal;},[&](){
   IOLockLock(hardwareQueue);
   bool obsolete=dbhw::differentPending(c->requests.pending,c->requests.desired,captured);
   IOLockUnlock(hardwareQueue);return obsolete;
  });
  c->superseded=result==dbhw::ReadResult::Superseded;ok=result==dbhw::ReadResult::Valid;
 }else{
  ok=dbhw::readRecovery(read,[](){IOSleep(250);},[&](){return !c->terminal;});
 }
 clock_get_uptime(&end);absolutetime_to_nanoseconds(end-begin,&ns);
 bool before=!strcmp(operation,"get-before-write"),after=!strcmp(operation,"get-after-write");
 c->display->setProperty(before?"DisplayBrightnessBeforeReadMicroseconds":(after?"DisplayBrightnessAfterReadMicroseconds":"DisplayBrightnessInitialReadMicroseconds"),ns/1000,64);
 c->display->setProperty(before?"DisplayBrightnessBeforeReadAttempts":(after?"DisplayBrightnessAfterReadAttempts":"DisplayBrightnessInitialReadAttempts"),attempts,32);
 return ok;
}
bool prepareBootstrapInterface(HardwareSlot *c){
 FrameScope gate(c->display);
 if(!gate.valid(c->display)||!endpointEDIDMatches(c)||gate.connection!=c->connection||gate.frameService!=c->frame){c->terminal=true;return false;}
 OSObject *paramsObject=c->display->copyProperty("IODisplayParameters");
 auto *params=OSDynamicCast(OSDictionary,paramsObject);
 bool nativeBrightness=params && params->getObject("brightness") && !ownedEndpoint(c->display);
 if(paramsObject)paramsObject->release();
 if(nativeBrightness){c->terminal=true;c->display->setProperty("DisplayBrightnessBootstrapState","native-provider-preserved");return false;}
 if(c->interface)return true;
 IOService *iface=nullptr;unsigned count=0;
 auto *it=c->frame->getChildIterator(gIOServicePlane);
 if(it){while(auto *o=it->getNextObject()){
  auto *candidate=OSDynamicCast(IOService,o);
  if(candidate && hasClass(candidate,"IOFramebufferI2CInterface")){++count;if(!iface){iface=candidate;iface->retain();}}
 }it->release();}
 auto *o=iface?iface->copyProperty("IOI2CTransactionTypes"):nullptr;
 auto *types=OSDynamicCast(OSNumber,o);
 bool supported=count==1 && types && (types->unsigned32BitValue()&(1U<<kIOI2CSimpleTransactionType));
 if(o)o->release();
 if(supported){IOLockLock(hardwareQueue);c->interface=iface;IOLockUnlock(hardwareQueue);return true;}
 if(iface)iface->release();
 if(count>1)c->terminal=true;
 c->display->setProperty("DisplayBrightnessBootstrapState",count>1?"ambiguous-interface":"waiting-interface");
 return false;
}
bool initializeEndpoint(HardwareSlot *c){
 c->bootAttempt.phase=dbstate::BootstrapPhase::Interface;
 if(!prepareBootstrapInterface(c))return false;
 c->bootAttempt.phase=dbstate::BootstrapPhase::Read;
 return dbstate::bootstrap([&](){
  // One read per startup visit. Release the bus before the one-second retry.
  dbddc::Brightness raw;UInt64 begin=0,end=0,ns=0;clock_get_uptime(&begin);
  hardwareReadOnce(c,raw,"get-initial-brightness");clock_get_uptime(&end);absolutetime_to_nanoseconds(end-begin,&ns);
  c->display->setProperty("DisplayBrightnessInitialReadMicroseconds",ns/1000,64);
  c->display->setProperty("DisplayBrightnessInitialReadAttempts",static_cast<UInt32>(1),32);return raw;
 },[&](unsigned value,dbddc::Brightness raw){
  c->bootAttempt.phase=dbstate::BootstrapPhase::Publish;
  FrameScope gate(c->display);
  if(!hardwareLive(c,gate,true)){c->terminal=true;return false;}
  bool published=publish(c->display,true,value);publishEDMMarker(c->display);
  if(!published || !ownedEndpoint(c->display)){
   OSObject *p=c->display->copyProperty("IODisplayParameters");auto *d=OSDynamicCast(OSDictionary,p);
   if(d && d->getObject("brightness"))c->terminal=true;
   if(p)p->release();return false;
  }
  c->verifiedCurrent=raw.current;c->verifiedMaximum=raw.maximum;
  c->display->setProperty("DisplayBrightnessHardwareOriginal",raw.current,32);
  c->display->setProperty("DisplayBrightnessHardwareOriginalMaximum",raw.maximum,32);c->haveOriginal=true;
  c->display->setProperty("DisplayBrightnessBootstrapState","published-from-hardware");
  c->display->setProperty("DisplayBrightnessInterfaceState","hardware-ready");
  IOLockLock(hardwareQueue);c->requests.initialized=true;IOLockUnlock(hardwareQueue);
  c->bootAttempt.phase=dbstate::BootstrapPhase::Ready;
  return true;
 });
}
bool hardwareCycle(HardwareSlot *c,UInt32 &desired,UInt64 &sequence){
 if(!c->requests.initialized)return false;
 // Re-read on every cycle. A previous reply/cache never authorizes this write.
 dbddc::Brightness before;
 if(!hardwareRead(c,before,"get-before-write",true,desired))return false;
 // Fresh read completed. Adopt the latest input before choosing this write.
 // No native framebuffer or bus operation while holding hardwareQueue.
 IOLockLock(hardwareQueue);
 bool selected=c->requests.refreshAfterRead(c->sequence,desired,sequence);
 IOLockUnlock(hardwareQueue);
 if(!selected)return false;
 c->display->setProperty("DisplayBrightnessHardwareDesired",desired,32);
 if(!c->haveOriginal){
  c->display->setProperty("DisplayBrightnessHardwareOriginal",before.current,32);
  c->display->setProperty("DisplayBrightnessHardwareOriginalMaximum",before.maximum,32);
  c->haveOriginal=true;
 }
 UInt16 target=0;
 if(!dbhw::target(before,desired,target)){
  c->display->setProperty("DisplayBrightnessHardwareState","invalid-target-stopped");return false;
 }
 c->display->setProperty("DisplayBrightnessHardwareTarget",target,32);
 if(before.current!=target){
  UInt8 packet[7];if(!dbddc::setBrightness(before,target,packet))return false;
  IOI2CRequest q={};q.sendAddress=0x6e;q.sendTransactionType=kIOI2CSimpleTransactionType;
  q.replyTransactionType=kIOI2CNoTransactionType;
  q.sendBuffer=reinterpret_cast<vm_address_t>(packet);q.sendBytes=sizeof(packet);
  uint64_t begin=0,end=0,ns=0;clock_get_uptime(&begin);
  bool wrote=hardwareIO(c,q,"set-brightness");
  clock_get_uptime(&end);absolutetime_to_nanoseconds(end-begin,&ns);
  c->display->setProperty("DisplayBrightnessSetMicroseconds",ns/1000,64);
  if(!wrote)return false;
  // No framebuffer gate is held during monitor settling. Bus serialization stays.
  IOSleep(50);
 }
 bool readOK=true;
 bool verified=dbhw::verifyTarget(before,target,[&](dbddc::Brightness &after){
  readOK=hardwareRead(c,after,"get-after-write",true,desired);return readOK;
 },[&](dbddc::Brightness after){
  c->verifiedCurrent=after.current;c->verifiedMaximum=after.maximum;
  c->display->setProperty("DisplayBrightnessHardwareVerified",after.current,32);
  c->display->setProperty("DisplayBrightnessHardwareState",before.current==target?"verified-already-target":"verified-readback");
 });
 if(!verified&&readOK)c->display->setProperty("DisplayBrightnessHardwareState","readback-mismatch-stopped");
 return verified;
}

void syncVerifiedState(HardwareSlot *c,UInt64 sequence){
 unsigned value=0;
 if(!dbstate::percent(true,c->verifiedCurrent,c->verifiedMaximum,value))return;
 bool changed=false;
 {
  FrameScope gate(c->display);
  if(!hardwareLive(c,gate))return;
  // Same lock order as submission: native framebuffer gate, then queue lock.
  IOLockLock(hardwareQueue);
  if(dbstate::mayPublish(sequence,c->sequence,c->requests.pending,true)){
   OSObject *o=c->display->copyProperty(ownerKey);
   auto *range=OSDynamicCast(OSDictionary,o);
   auto *n=range?OSDynamicCast(OSNumber,range->getObject("value")):nullptr;
   if(n){n->setValue(value);changed=true;}
   if(o)o->release();
  }
  IOLockUnlock(hardwareQueue);
 }
 if(changed && dbstate::shouldNotify(c->haveNotified,c->lastNotifiedPercent,value)){
  // Current AppleMCCS producer sends the changed key, zero padded to 16 bytes.
  char key[16]="brightness";
  IOReturn status=c->display->messageClients(kIOMessageServicePropertyChange,key,sizeof(key));
  if(status==kIOReturnSuccess){c->haveNotified=true;c->lastNotifiedPercent=value;}
  c->display->setProperty("DisplayBrightnessStateSyncPercent",value,32);
  c->display->setProperty("DisplayBrightnessStateSyncSequence",sequence,64);
  c->display->setProperty("DisplayBrightnessStateSyncMessageResult",static_cast<UInt32>(status),32);
 }
}
// Startup-only diagnostics: bounded history, emitted after releasing the bus.
void recordBootstrapAttempt(HardwareSlot *c,bool ok,UInt64 started,UInt64 finished){
 absolutetime_to_nanoseconds(started,&c->bootAttempt.startedNs);
 absolutetime_to_nanoseconds(finished,&c->bootAttempt.finishedNs);c->bootAttempt.success=ok;
 bool recorded=c->bootJournal.append(c->bootAttempt);
 if(!recorded)return;
 IOLog("DisplayBrightness: bootstrap display=%llu round=%u phase=%s ok=%u reads=%u transport=0x%x request=0x%x uptimeNs=%llu..%llu\n",c->display->getRegistryEntryID(),c->bootJournal.total,dbstate::bootstrapPhaseText(c->bootAttempt.phase),ok,c->bootAttempt.reads,c->bootAttempt.transport,c->bootAttempt.request,c->bootAttempt.startedNs,c->bootAttempt.finishedNs);
 c->display->setProperty("DisplayBrightnessBootstrapAttemptCount",c->bootJournal.total,32);
 c->display->setProperty("DisplayBrightnessBootstrapOmittedCount",c->bootJournal.dropped,32);
 auto *array=OSArray::withCapacity(c->bootJournal.count);if(!array)return;bool valid=true;
 auto put=[](OSDictionary *d,const char *key,UInt64 value){auto *n=OSNumber::withNumber(value,64);bool ok=n&&d->setObject(key,n);if(n)n->release();return ok;};
 for(unsigned i=0;i<c->bootJournal.count&&valid;i++){
  const auto &a=c->bootJournal.attempts[i];auto *d=OSDictionary::withCapacity(14);if(!d){valid=false;break;}
  auto *phase=OSString::withCString(dbstate::bootstrapPhaseText(a.phase));
  valid=phase&&d->setObject("phase",phase)&&put(d,"round",a.ordinal)&&put(d,"startedUptimeNs",a.startedNs)&&put(d,"finishedUptimeNs",a.finishedNs)&&put(d,"reads",a.reads)&&put(d,"transport",a.transport)&&put(d,"request",a.request)&&d->setObject("transportAttempted",a.attempted?kOSBooleanTrue:kOSBooleanFalse)&&d->setObject("replyValid",a.replyValid?kOSBooleanTrue:kOSBooleanFalse)&&d->setObject("success",a.success?kOSBooleanTrue:kOSBooleanFalse)&&put(d,"reportedReplyBytes",a.reportedReplyBytes)&&put(d,"capturedReplyBytes",a.capturedReplyBytes);
  auto *raw=OSData::withBytes(a.reply,a.capturedReplyBytes);
  valid=valid&&raw&&d->setObject("reply",raw)&&array->setObject(d);if(raw)raw->release();
  if(phase)phase->release();d->release();
 }
 if(valid)c->display->setProperty("DisplayBrightnessBootstrapHistory",array);array->release();
}
void hardwareWorker(thread_call_param_t arg,thread_call_param_t){
 auto *c=static_cast<HardwareSlot *>(arg);
 // Preserve the pending request and the full probe budget while deferred.
 // No native gate, bus lock or synchronous AMD call before the boot deadline.
 if(unsigned delay=startupRemainingMs()){
  uint64_t deadline;clock_interval_to_deadline(delay,kMillisecondScale,&deadline);
  thread_call_enter_delayed(c->call,deadline);return;
 }
 IOLockLock(hardwareQueue);
 UInt32 desired=c->requests.take();UInt64 sequence=c->sequence;bool initialized=c->requests.initialized;
 IOLockUnlock(hardwareQueue);
 if(initialized)c->display->setProperty("DisplayBrightnessHardwareDesired",desired,32);
 // This is NOT a hard timeout. The underlying AMD synchronous call may stall.
 c->terminal=false;c->superseded=false;
 const char *metrics[]={"DisplayBrightnessBeforeReadMicroseconds","DisplayBrightnessAfterReadMicroseconds","DisplayBrightnessBeforeReadAttempts","DisplayBrightnessAfterReadAttempts","DisplayBrightnessSetMicroseconds"};
 for(const char *key:metrics)c->display->setProperty(key,static_cast<UInt64>(0),64);
 if(!initialized)c->bootAttempt=dbstate::BootstrapAttempt{};
 uint64_t started=0,finished=0,elapsed=0,startedNs=0;clock_get_uptime(&started);
 absolutetime_to_nanoseconds(started,&startedNs);
 bool bootAllowed=initialized||c->bootstrapWindow.begin(startedNs);
 IOLockLock(probeSerial);uint64_t acquired=0,waitNS=0;clock_get_uptime(&acquired);
 absolutetime_to_nanoseconds(acquired-started,&waitNS);
 c->display->setProperty("DisplayBrightnessBusWaitMicroseconds",waitNS/1000,64);
 uint64_t acquiredNs=0;absolutetime_to_nanoseconds(acquired,&acquiredNs);
 bool ok=false;
 if(!initialized&&(!bootAllowed||!c->bootstrapWindow.within(acquiredNs))){
  c->terminal=true;c->bootAttempt.phase=dbstate::BootstrapPhase::Exhausted;
  c->display->setProperty("DisplayBrightnessBootstrapState","probe-window-exhausted-reconnect-required");
 }else ok=initialized?hardwareCycle(c,desired,sequence):initializeEndpoint(c);
 IOLockUnlock(probeSerial);
 clock_get_uptime(&finished);absolutetime_to_nanoseconds(finished-started,&elapsed);
 if(!initialized&&bootAllowed)recordBootstrapAttempt(c,ok,started,finished);
 c->display->setProperty("DisplayBrightnessHardwareCycleMicroseconds",elapsed/1000,64);
 c->display->setProperty("DisplayBrightnessHardwareCycleSequence",sequence,64);
 if(ok)syncVerifiedState(c,sequence);
 // Deadline bounds new startup attempts, not an in-flight synchronous AMD call.
 UInt64 finishedNs=0;absolutetime_to_nanoseconds(finished,&finishedNs);
 if(!initialized&&!ok&&!c->terminal&&!c->bootstrapWindow.maySchedule(finishedNs)){
  c->terminal=true;
  c->display->setProperty("DisplayBrightnessBootstrapState","probe-window-exhausted-reconnect-required");
 }
 bool live=!c->terminal && hardwarePilot(c->display);
 if(!live)c->display->setProperty("DisplayBrightnessInterfaceState","hardware-unavailable");
 IOLockLock(hardwareQueue);
 if(!live)c->requests.stop();
 bool yielded=c->superseded&&live;
 if(yielded)c->display->setProperty("DisplayBrightnessHardwareState","superseded-unverified");
 if(live && c->requests.complete(ok||yielded)){
  uint64_t deadline;clock_interval_to_deadline((ok||yielded)?50:(initialized?2000:dbstate::BootstrapWindow::intervalMs),kMillisecondScale,&deadline);
  if(!ok&&!yielded)c->display->setProperty("DisplayBrightnessHardwareState",initialized?"recovering-read-before-write":"probe-cooldown");
  thread_call_enter_delayed(c->call,deadline);
 }
 IOLockUnlock(hardwareQueue);
}
void endpointSnapshot(IOService *s,const char *reason,unsigned interfaces){
 // Caller holds hardwareQueue; snapshots carry registry IDs, never kernel pointers.
 auto *d=OSDictionary::withCapacity(5);if(!d)return;
 auto *why=OSString::withCString(reason);
 if(why){d->setObject("reason",why);why->release();}
 auto put=[](OSDictionary *dict,const char *key,UInt64 value){
  auto *n=OSNumber::withNumber(value,64);if(n){dict->setObject(key,n);n->release();}
 };
 put(d,"event",nextEvent());put(d,"interfaceCount",interfaces);
 put(d,"requestDisplay",s->getRegistryEntryID());
 for(unsigned i=0;i<dbcompat::endpointCapacity;i++){
  auto &c=hardwareSlots[i];auto *item=OSDictionary::withCapacity(9);if(!item)continue;
  put(item,"display",c.display?c.display->getRegistryEntryID():0);
  put(item,"frame",c.frame?c.frame->getRegistryEntryID():0);
  put(item,"interface",c.interface?c.interface->getRegistryEntryID():0);
  item->setObject("retiring",c.retiring?kOSBooleanTrue:kOSBooleanFalse);
  item->setObject("inactive",c.display && c.display->isInactive()?kOSBooleanTrue:kOSBooleanFalse);
  item->setObject("busy",c.requests.busy?kOSBooleanTrue:kOSBooleanFalse);
  item->setObject("pending",c.requests.pending?kOSBooleanTrue:kOSBooleanFalse);
  item->setObject("failed",c.requests.failed?kOSBooleanTrue:kOSBooleanFalse);
  put(item,"desired",c.requests.desired);
  char key[16];snprintf(key,sizeof(key),"slot%u",i);
  d->setObject(key,item);item->release();
 }
 s->setProperty("DisplayBrightnessEndpointSnapshot",d);d->release();
}
bool queueHardware(IOService *s,IOService *fb,UInt32 desired,bool bootstrap=false){
 // Called inside the native gate; no bus operations here.
 if(!hardwarePilot(s))return false;
 if(!hardwareRequested || !startIO || !probeSerial || !hardwareQueue || desired>100)return false;
 IOLockLock(hardwareQueue);
 HardwareSlot *slot=nullptr,*empty=nullptr;
 const char *reason="slots-full";unsigned interfaceCount=0;bool retireNeeded=false;
 for(auto &candidate:hardwareSlots){
  if(retireCall && dbhw::canRetire(candidate.display!=nullptr,candidate.display && candidate.display->isInactive(),candidate.requests.busy,candidate.retiring)){
   candidate.retiring=true;retireNeeded=true;
  }
  if(!candidate.retiring && candidate.display==s)slot=&candidate;
  if(!candidate.retiring && !candidate.display && !empty)empty=&candidate;
 }
 if(!slot && empty && bootstrap){
  IOService *iface=nullptr;unsigned count=0;
  reason="interface-iterator-unavailable";
  OSIterator *it=fb->getChildIterator(gIOServicePlane);
  if(it){while(auto *o=it->getNextObject()){
   auto *p=OSDynamicCast(IOService,o);
   if(p && hasClass(p,"IOFramebufferI2CInterface")){++count;if(!iface){iface=p;iface->retain();}}
  }it->release();}
  interfaceCount=count;
  if(it)reason=count==0?"interface-not-ready":(count!=1?"ambiguous-interface":"missing-transaction-types");
  OSObject *types=iface?iface->copyProperty("IOI2CTransactionTypes"):nullptr;
  auto *n=OSDynamicCast(OSNumber,types);
  bool supported=count==1 && n && (n->unsigned32BitValue()&(1U<<kIOI2CSimpleTransactionType));
  if(n && count==1)reason=supported?"thread-call-allocation-failed":"unsupported-transaction";
  if(types)types->release();
  if(supported || bootstrap){
   if(!empty->call)empty->call=thread_call_allocate_with_priority(hardwareWorker,empty,THREAD_CALL_PRIORITY_LOW);
   if(empty->call && captureEndpointEDID(s,empty->boundEDID)){
    auto *connection=s->copyParentEntry(gIOServicePlane);
    auto *parent=connection?connection->copyParentEntry(gIOServicePlane):nullptr;
    if(hasClass(connection,"IODisplayConnect") && parent==fb){
     empty->connection=connection;connection=nullptr;
     empty->display=s;empty->frame=fb;empty->interface=supported?iface:nullptr;
     s->retain();fb->retain();slot=empty;if(supported)iface=nullptr;
    }
    if(parent)parent->release();if(connection)connection->release();
   }
  }
  if(iface)iface->release();
 }
 if(slot)reason=slot->frame!=fb?"frame-mismatch":(slot->requests.stopped?"endpoint-invalid-stopped":(slot->requests.failed?"recovering-latest-target":"queued"));
 if(slot && bootstrap && slot->frame==fb && (slot->requests.initialized || slot->requests.busy)){
  bool ok=!slot->requests.stopped;
  IOLockUnlock(hardwareQueue);if(retireNeeded)thread_call_enter(retireCall);return ok;
 }
 bool schedule=false;
 bool ok=slot && slot->frame==fb && (bootstrap?slot->requests.submitBootstrap(schedule):slot->requests.submitBrightness(desired,schedule));
 if(ok){
  slot->sequence=nextEvent();
  if(schedule){
   uint64_t deadline;clock_interval_to_deadline(50,kMillisecondScale,&deadline);
   thread_call_enter_delayed(slot->call,deadline);
  }
 }
 bool retryAdmission=false;
 // A full set of active slots must not create an endless admission poll.
 if(!slot && bootstrap && admissionCall && retireNeeded){
  retryAdmission=rememberBootstrap(s,fb);
  reason=retryAdmission?"bootstrap-admission-queued":"bootstrap-pending-full";
 }
 if(!slot && retireNeeded && !bootstrap)reason="retirement-queued-retry-request";
 endpointSnapshot(s,reason,interfaceCount);
 if(!ok)s->setProperty("DisplayBrightnessEndpointFailure",reason);
 IOLockUnlock(hardwareQueue);
 if(retireNeeded)thread_call_enter(retireCall);
 if(retryAdmission){uint64_t deadline;clock_interval_to_deadline(2000,kMillisecondScale,&deadline);thread_call_enter_delayed(admissionCall,deadline);}
 if(!slot)s->setProperty("DisplayBrightnessHardwareState","no-unambiguous-endpoint-or-slot");
 return ok;
}
