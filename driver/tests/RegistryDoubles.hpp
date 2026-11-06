#pragma once
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <map>
#include <string>
#include <vector>
#include <cstring>
using UInt8=uint8_t;
using UInt16=uint16_t;
using UInt32=uint32_t;
using UInt64=uint64_t;
struct OSObject {
 static unsigned live;
 unsigned refs=1;
 OSObject(){++live;}
 virtual ~OSObject(){--live;}
 virtual bool isEqualTo(const OSObject *other)const{return this==other;}
 void retain(){++refs;}
 void release(){assert(refs);if(!--refs)delete this;}
};
unsigned OSObject::live=0;
OSObject *kOSBooleanTrue=new OSObject;
#define OSDynamicCast(type, object) dynamic_cast<type *>(object)
#define IOLog(...) ((void)0)
struct OSData:OSObject {
 static bool failAllocation;
 std::vector<UInt8> bytes;
 static OSData *withBytes(const void *data,size_t length){
  if(failAllocation){failAllocation=false;return nullptr;}
  auto *d=new OSData;const auto *p=static_cast<const UInt8 *>(data);d->bytes.assign(p,p+length);return d;
 }
 const void *getBytesNoCopy()const{return bytes.data();}
 size_t getLength()const{return bytes.size();}
 bool isEqualTo(const OSObject *other)const override{
  auto *d=dynamic_cast<const OSData *>(other);return d&&bytes==d->bytes;
 }
};
bool OSData::failAllocation=false;
struct OSString:OSObject {
 std::string value;
 explicit OSString(const char *s):value(s){}
};
struct OSNumber:OSObject {
 static bool failAllocation;
 UInt64 value;
 explicit OSNumber(UInt64 v):value(v){}
 static OSNumber *withNumber(UInt64 v,unsigned){
  if(failAllocation){failAllocation=false;return nullptr;}return new OSNumber(v);
 }
 UInt32 unsigned32BitValue()const{return static_cast<UInt32>(value);}
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
struct OSArray:OSObject {
 std::vector<OSObject *> objects;
 ~OSArray(){for(auto *o:objects)o->release();}
 unsigned getCount()const{return static_cast<unsigned>(objects.size());}
 OSObject *getObject(unsigned i){return i<objects.size()?objects[i]:nullptr;}
 void setObject(OSObject *o){o->retain();objects.push_back(o);}
};
struct IONotifier:OSObject {};
struct IOService;
using InterestHandler=int (*)(void *,void *,UInt32,IOService *,void *,size_t);
using MatchingHandler=bool (*)(void *,void *,IOService *,IONotifier *);
struct IOService:OSObject {
 OSDictionary *properties=OSDictionary::withCapacity(1);
 UInt32 state=0;
 bool inactive=false;
 const char *className="AppleDisplay";
 IOService *parent=nullptr;
 static IOService *serviceRoot;
 static IOService *getServiceRoot(){return serviceRoot;}
 bool isInactive()const{return inactive;}
 IOService *copyParentEntry(int){if(parent)parent->retain();return parent;}
 IONotifier *registerInterest(const char *,InterestHandler,void *,void *);
 static OSDictionary *serviceMatching(const char *);
 static IONotifier *addMatchingNotification(const char *,OSDictionary *,MatchingHandler,void *,void *);
 std::string failProperty;
 UInt32 getState()const{return state;}
 ~IOService(){properties->release();}
 OSObject *getProperty(const char *key){return properties->getObject(key);}
 OSObject *copyProperty(const char *key){auto *p=getProperty(key);if(p)p->retain();return p;}
 bool setProperty(const char *key,OSObject *p){return failProperty!=key&&properties->setObject(key,p);}
 bool setProperty(const char *key,bool value){assert(value);return setProperty(key,kOSBooleanTrue);}
 bool setProperty(const char *key,const char *value){auto *s=new OSString(value);bool ok=setProperty(key,s);s->release();return ok;}
 bool setProperty(const char *key,UInt64 value,unsigned bits){auto *n=OSNumber::withNumber(value,bits);bool ok=setProperty(key,n);if(n)n->release();return ok;}
 void removeProperty(const char *key){properties->removeObject(key);}
};
IOService *IOService::serviceRoot=nullptr;
