#include "VerifiedBrightness.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <initializer_list>
static void checksumEDID(uint8_t (&edid)[128]){
 uint8_t sum=0;for(unsigned i=0;i<127;i++)sum=static_cast<uint8_t>(sum+edid[i]);edid[127]=static_cast<uint8_t>(0-sum);
}
static void checksumRecord(dbcache::Record &record){
 auto crc=dbcache::checksum(record.bytes,140);for(unsigned i=0;i<4;i++)record.bytes[140+i]=static_cast<uint8_t>(crc>>(8*i));
}
int main(){
 uint8_t edid[128]={0,255,255,255,255,255,255,0,0x04,0x43,0x21,0x43,1};checksumEDID(edid);
 dbcache::Record record;dbcache::Brightness state;
 assert(dbcache::checksum(reinterpret_cast<const uint8_t *>("123456789"),9)==0xcbf43926U);
 for(unsigned maximum:{1U,100U,255U,65535U}){
  for(unsigned current:{0U,1U,maximum/2,maximum}){
   assert(dbcache::encode(edid,128,current,maximum,record));
   assert(dbcache::decode(record.bytes,sizeof(record.bytes),edid,128,state));
   assert(state.current==current&&state.maximum==maximum);
   assert(state.percent==(current*100+maximum/2)/maximum);
  }
 }
 assert(dbcache::encode(edid,128,80,100,record));
 auto valid=record;
 // Any one-bit corruption, including in the checksum, is rejected.
 for(unsigned i=0;i<sizeof(record.bytes);i++)for(unsigned bit=0;bit<8;bit++){
  record=valid;record.bytes[i]^=1U<<bit;
  assert(!dbcache::decode(record.bytes,sizeof(record.bytes),edid,128,state));
 }
 record=valid;record.bytes[4]=2;checksumRecord(record);
 assert(!dbcache::decode(record.bytes,sizeof(record.bytes),edid,128,state));
 record=valid;record.bytes[6]=1;checksumRecord(record);
 assert(!dbcache::decode(record.bytes,sizeof(record.bytes),edid,128,state));
 record=valid;dbcache::put16(record.bytes+138,0);checksumRecord(record);
 assert(!dbcache::decode(record.bytes,sizeof(record.bytes),edid,128,state));
 record=valid;dbcache::put16(record.bytes+136,101);checksumRecord(record);
 assert(!dbcache::decode(record.bytes,sizeof(record.bytes),edid,128,state));
 state.percent=73;
 assert(!dbcache::decode(nullptr,144,edid,128,state));
 assert(!dbcache::decode(valid.bytes,143,edid,128,state));
 assert(!dbcache::decode(valid.bytes,145,edid,128,state));
 assert(!dbcache::decode(valid.bytes,144,edid,127,state));
 edid[12]^=1;checksumEDID(edid); // Another unit of the same model.
 assert(!dbcache::decode(valid.bytes,144,edid,128,state));
 assert(state.percent==73); // Rejection leaves the caller's value untouched.
 record=valid;
 assert(!dbcache::encode(edid,128,101,100,record));
 assert(!dbcache::encode(edid,128,0,0,record));
 assert(!dbcache::encode(nullptr,128,80,100,record));
 edid[127]^=1;assert(!dbcache::encode(edid,128,80,100,record));
 assert(!memcmp(record.bytes,valid.bytes,sizeof(record.bytes)));
 puts("verified record: zero/full/16-bit ranges, checksum, schema, exact EDID, malformed records and failure immutability");
}
