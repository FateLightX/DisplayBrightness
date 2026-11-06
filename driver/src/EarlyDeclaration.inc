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
 dbcache::Brightness cached;
 const char *result=readCachedBrightness(s,cached);
 if(strcmp(result,"cache-valid")){earlyDeclarationReason(s,result);return;}
 if(!publish(s,true,cached.percent)){earlyDeclarationReason(s,"parameter-publication-failed");return;}
 publishEDMMarker(s);
 UInt64 now=0,ns=0;clock_get_uptime(&now);absolutetime_to_nanoseconds(now,&ns);
 s->setProperty("DisplayBrightnessEarlyDeclarationUptimeNs",ns,64);
 s->setProperty("DisplayBrightnessInterfaceState","pending-hardware");
 s->setProperty("DisplayBrightnessCachedPercent",cached.percent,32);
 earlyDeclarationReason(s,"declared-from-cache");
}
