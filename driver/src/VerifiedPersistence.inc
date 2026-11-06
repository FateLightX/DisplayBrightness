struct PreferenceCommit {HardwareSlot *slot;UInt64 sequence;};
IOReturn commitVerifiedPreference(OSObject *,void *arg,void *,void *,void *){
 auto *commit=static_cast<PreferenceCommit *>(arg);auto *c=commit->slot;
 auto *loop=preferenceWorkLoop();
 if(!loop||!loop->inGate()||!preferenceWriterReady())return kIOReturnNotReady;
 // Legal order: system gate -> framebuffer gate -> queue. The caller released
 // the bus first. Retirement waits for this worker before reusing its slot.
 FrameScope gate(c->display);
 if(!hardwareLive(c,gate)){
  c->display->setProperty("DisplayBrightnessCacheSaveState","skipped-offline-or-replaced");return kIOReturnNotReady;
 }
 IOLockLock(hardwareQueue);
 bool eligible=!c->retiring&&!c->terminal&&c->requests.writable()&&
  dbstate::mayPublish(commit->sequence,c->sequence,c->requests.pending,true);
 IOLockUnlock(hardwareQueue);
 if(!eligible){c->display->setProperty("DisplayBrightnessCacheSaveState","skipped-stale-or-stopped");return kIOReturnNotReady;}
 // The framebuffer gate excludes new submissions for this display. No queue
 // lock remains held while the native preference setter schedules its timer.
 const char *result=storeVerifiedPreference(c->frame,c->display,c->boundEDID,sizeof(c->boundEDID),c->verifiedCurrent,c->verifiedMaximum);
 c->display->setProperty("DisplayBrightnessCacheSaveState",result);
 if(!strcmp(result,"accepted-native")||!strcmp(result,"unchanged-native")){
  c->display->setProperty("DisplayBrightnessCacheCurrent",c->verifiedCurrent,32);
  c->display->setProperty("DisplayBrightnessCacheMaximum",c->verifiedMaximum,32);
  c->display->setProperty("DisplayBrightnessCacheSequence",commit->sequence,64);
 }
 return kIOReturnSuccess;
}
void persistVerifiedState(HardwareSlot *c,UInt64 sequence){
 // Only called by a successful read/verified cycle, with every native, bus and
 // queue lock released. The cached value is never fed into EndpointRequests.
 auto *loop=preferenceWorkLoop();
 if(!loop||!preferenceWriterReady()){
  c->display->setProperty("DisplayBrightnessCacheSaveState","backend-unavailable");return;
 }
 PreferenceCommit commit{c,sequence};
 loop->runAction(commitVerifiedPreference,nullptr,&commit);
}
