// Executes the actual kernel publication function with small reference-counted
// registry doubles. No native kernel API, I/O or service is invoked.
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <map>
#include <string>
using UInt32=uint32_t;
struct OSObject {
 static unsigned live;
 unsigned refs=1;
 OSObject(){++live;}
 virtual ~OSObject(){--live;}
 void retain(){++refs;}
 void release(){assert(refs);if(!--refs)delete this;}
};
unsigned OSObject::live=0;
OSObject *kOSBooleanTrue=new OSObject;
#define OSDynamicCast(type, object) dynamic_cast<type *>(object)
#define IOLog(...) ((void)0)
struct OSNumber:OSObject {
 static bool failAllocation;
 UInt32 value;
 explicit OSNumber(UInt32 v):value(v){}
 static OSNumber *withNumber(UInt32 v,unsigned){
  if(failAllocation){failAllocation=false;return nullptr;}return new OSNumber(v);
 }
 UInt32 unsigned32BitValue()const{return value;}
 void setValue(UInt32 v){value=v;}
};
bool OSNumber::failAllocation=false;
struct OSDictionary:OSObject {
 std::map<std::string,OSObject *> objects;
 ~OSDictionary(){for(auto &entry:objects)entry.second->release();}
 static OSDictionary *withCapacity(unsigned){return new OSDictionary;}
 static OSDictionary *withDictionary(OSDictionary *from){
  auto *d=new OSDictionary;for(auto &entry:from->objects)d->setObject(entry.first.c_str(),entry.second);return d;
 }
 OSObject *getObject(const char *key){auto i=objects.find(key);return i==objects.end()?nullptr:i->second;}
 bool setObject(const char *key,OSObject *value){
  if(!value)return false;value->retain();auto *old=getObject(key);
  objects[key]=value;if(old)old->release();return true;
 }
 void removeObject(const char *key){auto *old=getObject(key);objects.erase(key);if(old)old->release();}
};
struct IOService:OSObject {
 OSDictionary *properties=OSDictionary::withCapacity(1);
 std::string failProperty;
 ~IOService(){properties->release();}
 OSObject *getProperty(const char *key){return properties->getObject(key);}
 OSObject *copyProperty(const char *key){auto *p=getProperty(key);if(p)p->retain();return p;}
 bool setProperty(const char *key,OSObject *p){return failProperty!=key&&properties->setObject(key,p);}
 bool setProperty(const char *key,bool value){assert(value);return setProperty(key,kOSBooleanTrue);}
 void removeProperty(const char *key){properties->removeObject(key);}
};
const char *ownerKey="DisplayBrightnessProbeRange";
const char *simKey="DisplayBrightnessProbeSimulated";
#include "../src/ParameterPublication.inc"
static OSDictionary *range(IOService &s){
 auto *d=OSDynamicCast(OSDictionary,s.getProperty("IODisplayParameters"));
 return d?OSDynamicCast(OSDictionary,d->getObject("brightness")):nullptr;
}
static unsigned value(IOService &s){return OSDynamicCast(OSNumber,range(s)->getObject("value"))->unsigned32BitValue();}
int main(){
 {
  IOService s;
  // Regression: 0.0.30 refused publication when the first native update had not
  // created IODisplayParameters yet, silently losing the registration window.
  assert(!s.getProperty("IODisplayParameters"));
  assert(publish(&s,true,50));assert(range(s)&&value(s)==50);
  auto *identity=range(s);assert(s.getProperty(ownerKey)==identity);
  assert(publish(&s,true,0));assert(range(s)==identity&&value(s)==0);
  assert(publish(&s,true,100));assert(range(s)==identity&&value(s)==100);
 }
 {
  IOService s;
  assert(!publish(&s,false,0));assert(!s.getProperty("IODisplayParameters"));
  assert(!publish(&s,true,101));assert(!s.getProperty(ownerKey));
  // A malformed existing property belongs to the system, not to this plugin.
  auto *native=OSNumber::withNumber(42,32);s.setProperty("IODisplayParameters",native);native->release();
  assert(!publish(&s,true,50));assert(s.getProperty("IODisplayParameters")==native);
 }
 {
  IOService s;auto *params=OSDictionary::withCapacity(1);
  number(params,"contrast",77);s.setProperty("IODisplayParameters",params);params->release();
  assert(publish(&s,true,50));
  auto *updated=OSDynamicCast(OSDictionary,s.getProperty("IODisplayParameters"));
  assert(OSDynamicCast(OSNumber,updated->getObject("contrast"))->unsigned32BitValue()==77);
  // Native dictionary replacement drops brightness but retains the owner.
  auto *replacement=OSDictionary::withCapacity(1);
  s.setProperty("IODisplayParameters",replacement);replacement->release();
  assert(publish(&s,false,0));assert(value(s)==50);
  replacement=OSDictionary::withCapacity(1);
  s.setProperty("IODisplayParameters",replacement);replacement->release();
  assert(publish(&s,true,19));assert(value(s)==19); // Fresh read overrides old owner.
  // A real native provider replaces our range; it must never be overwritten.
  updated=OSDynamicCast(OSDictionary,s.getProperty("IODisplayParameters"));
  auto *native=OSDictionary::withCapacity(3);number(native,"value",61);
  updated->setObject("brightness",native);native->release();
  assert(!publish(&s,true,50));assert(range(s)==native&&value(s)==61);
  assert(!s.getProperty(ownerKey)&&!s.getProperty(simKey));
 }
 {
  IOService s;OSNumber::failAllocation=true;
  assert(!publish(&s,true,50));assert(!s.getProperty("IODisplayParameters")&&!s.getProperty(ownerKey));
  s.failProperty="IODisplayParameters";
  assert(!publish(&s,true,50));assert(!s.getProperty(ownerKey)&&!s.getProperty(simKey));
 }
 assert(OSObject::live==1); // Only the process-lifetime Boolean remains.
 puts("actual publication: absent dictionary created; initial value replaced; native data preserved; allocation/property failures clean up references");
}
