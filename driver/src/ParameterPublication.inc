bool number(OSDictionary *d,const char *key,UInt32 value){
 OSNumber *n=OSNumber::withNumber(value,32);if(!n)return false;
 bool ok=d->setObject(key,n);n->release();return ok;
}
bool publish(IOService *s,bool haveSeed,UInt32 seed){
 // Serialized by display startup before registration, or by this display's
 // existing framebuffer gate on the worker. No additional startup gate.
 OSObject *property=s->copyProperty("IODisplayParameters");
 OSDictionary *old=OSDynamicCast(OSDictionary,property);
 OSObject *owned=s->copyProperty(ownerKey);
 if(!old){
  // At the first doUpdate, IOFramebufferParameterHandler may not have run its
  // later displayModeChange yet. A missing dictionary is normal during start.
  // Create it locally, while preserving an existing property of another type.
  if(!property)old=OSDictionary::withCapacity(1);
  if(!old){if(property)property->release();if(owned)owned->release();return false;}
 }
 OSObject *current=old->getObject("brightness");
 if(current){
  // A real first read must replace our cached value. Never touch a native
  // provider, even when a stale synthetic owner remains in the registry.
  auto *range=OSDynamicCast(OSDictionary,owned);
  auto *n=range?OSDynamicCast(OSNumber,range->getObject("value")):nullptr;
  bool ok=current==owned && range && range->getObject("DisplayBrightnessSynthetic")==kOSBooleanTrue && n;
  if(ok && haveSeed){ok=seed<=100;if(ok)n->setValue(seed);}
  if(current!=owned){s->removeProperty(ownerKey);s->removeProperty(simKey);}
  old->release();if(owned)owned->release();return ok;
 }
 UInt32 value=seed;
 if(!haveSeed){
  auto *range=OSDynamicCast(OSDictionary,owned);
  auto *n=range?OSDynamicCast(OSNumber,range->getObject("value")):nullptr;
  if(!n){old->release();if(owned)owned->release();return false;}
  value=n->unsigned32BitValue();
 }
 if(value>100){old->release();if(owned)owned->release();return false;}
 OSDictionary *params=OSDictionary::withDictionary(old);
 OSDictionary *range=OSDictionary::withCapacity(3);
 bool ok=params && range && number(range,"min",0) && number(range,"max",100) &&
  number(range,"value",value) && range->setObject("DisplayBrightnessSynthetic",kOSBooleanTrue) && params->setObject("brightness",range);
 if(ok)ok=s->setProperty(ownerKey,range);
 if(ok)ok=s->setProperty(simKey,true);
 if(ok)ok=s->setProperty("IODisplayParameters",params);
 if(!ok){s->removeProperty(ownerKey);s->removeProperty(simKey);}
 if(range)range->release();if(params)params->release();old->release();if(owned)owned->release();
 IOLog("DisplayBrightness: UI probe display=%llu published=%d initialPercent=%u\n",s->getRegistryEntryID(),ok,value);
 return ok;
}
