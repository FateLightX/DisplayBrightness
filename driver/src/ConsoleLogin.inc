dbstate::LoginGate loginGate;
thread_call_t loginCall=nullptr;
IONotifier *loginNotifier=nullptr,*displayPublishNotifier=nullptr,*displayTerminationNotifier=nullptr;
bool loginRegistrationAttempted=false;
UInt64 loginLastSignalTicks=0,loginSignalCount=0;
void scheduleAdmission();
void requestPendingCleanup();
UInt64 uptimeNanoseconds(){UInt64 ticks=0,ns=0;clock_get_uptime(&ticks);absolutetime_to_nanoseconds(ticks,&ns);return ns;}
void requestLoginCheck(){if(loginCall&&!loginGate.allowed())thread_call_enter(loginCall);}
IOReturn consoleSecurityChanged(void *,void *,UInt32 type,IOService *,void *,vm_size_t){
 // Synchronously called by IOService. No allocation, registry traversal, logging,
 // framebuffer/queue lock, wait or hardware access belongs on this path.
 if(type==kIOMessageConsoleSecurityChange&&!loginGate.allowed()){
  UInt64 ticks=0;clock_get_uptime(&ticks);
  __atomic_store_n(&loginLastSignalTicks,ticks,__ATOMIC_RELAXED);
  __atomic_add_fetch(&loginSignalCount,1ULL,__ATOMIC_RELAXED);
  thread_call_enter(loginCall);
 }
 return kIOReturnSuccess;
}
bool pendingDisplayChanged(void *,void *,IOService *,IONotifier *){
 requestPendingCleanup();return true;
}
void loginWorker(thread_call_param_t,thread_call_param_t){
 // THREAD_CALL_OPTIONS_ONCE serializes registration and snapshots. Notifications
 // received during this visit arrange another visit; none is waited on here.
 if(loginGate.allowed())return;
 auto *serviceRoot=IOService::getServiceRoot();
 if(!serviceRoot)return; // A later display/route event retries; never wait in boot.
 if(!loginRegistrationAttempted){
  loginRegistrationAttempted=true;
  loginNotifier=serviceRoot->registerInterest(gIOConsoleSecurityInterest,consoleSecurityChanged,nullptr,nullptr);
  auto *matching=IOService::serviceMatching("AppleDisplay");
  if(matching){
   displayPublishNotifier=IOService::addMatchingNotification(gIOFirstPublishNotification,matching,pendingDisplayChanged,nullptr,nullptr);
   displayTerminationNotifier=IOService::addMatchingNotification(gIOTerminatedNotification,matching,pendingDisplayChanged,nullptr,nullptr);
   matching->release();
  }
  serviceRoot->setProperty("DisplayBrightnessPendingCleanup",displayTerminationNotifier?"termination-notification":"on-display-event");
 }
 if(!loginNotifier||!displayPublishNotifier){serviceRoot->setProperty("DisplayBrightnessStartupGate","notifier-unavailable");return;}
 // The notification is installed on the IOService root, but IOConsoleUsers
 // belongs to the IORegistry root. Inspect a retained, current snapshot AFTER
 // registration, including when login happened before this plugin subscribed.
 auto *registryRoot=IORegistryEntry::getRegistryRoot();
 auto *object=registryRoot?registryRoot->copyProperty("IOConsoleUsers"):nullptr;
 auto *users=OSDynamicCast(OSArray,object);
 bool loggedIn=false;
 if(users)for(unsigned i=0;i<users->getCount();i++){
  auto *user=OSDynamicCast(OSDictionary,users->getObject(i));
  if(user&&user->getObject("kCGSSessionOnConsoleKey")==kOSBooleanTrue&&
     user->getObject("kCGSessionLoginDoneKey")==kOSBooleanTrue){loggedIn=true;break;}
 }
 bool haveSnapshot=users!=nullptr;
 if(object)object->release();
 UInt64 signal=__atomic_load_n(&loginLastSignalTicks,__ATOMIC_RELAXED),signalNs=0;
 UInt64 count=__atomic_load_n(&loginSignalCount,__ATOMIC_RELAXED);
 absolutetime_to_nanoseconds(signal,&signalNs);
 UInt64 observedNs=uptimeNanoseconds();
 if(!loginGate.observe(loggedIn,loggedIn)){
  serviceRoot->setProperty("DisplayBrightnessStartupGate",haveSnapshot?"waiting-first-login":"waiting-console-state");return;
 }
 serviceRoot->setProperty("DisplayBrightnessLoginObservedUptimeNs",observedNs,64);
 serviceRoot->setProperty("DisplayBrightnessLoginSignalUptimeNs",signalNs,64);
 serviceRoot->setProperty("DisplayBrightnessLoginNotificationCount",count,64);
 serviceRoot->setProperty("DisplayBrightnessLoginObservationSource",signal?"notification-snapshot":"initial-snapshot");
 serviceRoot->setProperty("DisplayBrightnessStartupGate","first-login-observed");
 scheduleAdmission();
}
