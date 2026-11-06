#pragma once
#include "DisplayCompatibility.hpp"
#include "StateSync.hpp"
namespace dbcache {
// One versioned, endian-independent preference value. Full base EDID binding
// avoids model allowlists and fingerprint collisions. This is UI history only.
struct Record { static constexpr size_t size=144; uint8_t bytes[size]={}; };
struct Brightness { uint16_t current=0,maximum=0; unsigned percent=0; };
inline uint32_t checksum(const uint8_t *bytes,size_t length){
 uint32_t crc=0xffffffffU;
 for(size_t i=0;i<length;i++){
  crc^=bytes[i];
  for(unsigned bit=0;bit<8;bit++)crc=(crc>>1)^(0xedb88320U&(0U-(crc&1U)));
 }
 return ~crc;
}
inline void put16(uint8_t *p,uint16_t value){p[0]=static_cast<uint8_t>(value);p[1]=static_cast<uint8_t>(value>>8);}
inline uint16_t get16(const uint8_t *p){return static_cast<uint16_t>(p[0]|(static_cast<unsigned>(p[1])<<8));}
inline bool encode(const uint8_t *edid,size_t length,uint16_t current,uint16_t maximum,Record &out){
 unsigned percent=0;
 if(!dbcompat::validBase(edid,length)||!dbstate::percent(true,current,maximum,percent))return false;
 Record record;
 record.bytes[0]='D';record.bytes[1]='B';record.bytes[2]='V';record.bytes[3]='C';
 put16(record.bytes+4,1); // schema; bytes 6..7 reserved and zero
 for(unsigned i=0;i<128;i++)record.bytes[8+i]=edid[i];
 put16(record.bytes+136,current);put16(record.bytes+138,maximum);
 uint32_t crc=checksum(record.bytes,140);
 for(unsigned i=0;i<4;i++)record.bytes[140+i]=static_cast<uint8_t>(crc>>(8*i));
 out=record;return true;
}
inline bool decode(const void *data,size_t size,const uint8_t *edid,size_t length,Brightness &out){
 if(!data||size!=Record::size||!dbcompat::validBase(edid,length))return false;
 const auto *p=static_cast<const uint8_t *>(data);
 if(p[0]!='D'||p[1]!='B'||p[2]!='V'||p[3]!='C'||get16(p+4)!=1||get16(p+6))return false;
 uint32_t crc=0;for(unsigned i=0;i<4;i++)crc|=static_cast<uint32_t>(p[140+i])<<(8*i);
 if(crc!=checksum(p,140)||!dbcompat::sameBase(p+8,edid,length))return false;
 Brightness value;value.current=get16(p+136);value.maximum=get16(p+138);
 if(!dbstate::percent(true,value.current,value.maximum,value.percent))return false;
 out=value;return true;
}
}
