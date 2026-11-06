// Caller owns the IOGraphics system gate. Native methods use framebuffer as
// their implicit 'this'. Copies are retained; the setter retains its own value.
const char *loadVerifiedPreference(IOService *frame,IOService *display,
                                  const UInt8 *edid,size_t length,dbcache::Brightness &value){
 OSObject *object=copyPreference(frame,display,brightnessPreferenceKey);
 auto *data=OSDynamicCast(OSData,object);
 bool valid=data&&dbcache::decode(data->getBytesNoCopy(),data->getLength(),edid,length,value);
 const char *result=valid?"cache-valid":(object?"cache-invalid":"cache-missing");
 if(object)object->release();return result;
}
const char *storeVerifiedPreference(IOService *frame,IOService *display,
                                   const UInt8 *edid,size_t length,UInt16 current,UInt16 maximum){
 dbcache::Record record;
 if(!dbcache::encode(edid,length,current,maximum,record))return "invalid-record";
 auto *data=OSData::withBytes(record.bytes,sizeof(record.bytes));
 if(!data)return "allocation-failed";
 OSObject *old=copyPreference(frame,display,brightnessPreferenceKey);
 auto *oldData=OSDynamicCast(OSData,old);
 bool unchanged=oldData&&data->isEqualTo(oldData);
 if(old)old->release();
 if(unchanged){data->release();return "unchanged-native";}
 bool accepted=setPreference(frame,display,brightnessPreferenceKey,data);
 // setPreference can report true even when native dictionary allocation fails.
 // Equality here proves only native in-memory acceptance, never disk flush.
 OSObject *stored=accepted?copyPreference(frame,display,brightnessPreferenceKey):nullptr;
 auto *storedData=OSDynamicCast(OSData,stored);
 bool matches=storedData&&data->isEqualTo(storedData);
 if(stored)stored->release();data->release();
 return !accepted?"native-rejected":(matches?"accepted-native":"native-readback-mismatch");
}
