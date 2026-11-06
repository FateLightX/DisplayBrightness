#include "ImageGate.hpp"
#include <cassert>
#include <cstdio>
int main(){
 const unsigned char expected[16]={1};
 struct Image{mach_header_64 h;uuid_command u;} image={};
 image.h.magic=MH_MAGIC_64;image.h.ncmds=1;image.h.sizeofcmds=sizeof(image.u);
 image.u.cmd=LC_UUID;image.u.cmdsize=sizeof(image.u);image.u.uuid[0]=1;
 assert(dbImageUUIDMatches(&image,sizeof(image),expected));
 assert(!dbImageUUIDMatches(nullptr,sizeof(image),expected));
 assert(!dbImageUUIDMatches(&image,sizeof(image)-1,expected));
 image.u.uuid[0]=2;assert(!dbImageUUIDMatches(&image,sizeof(image),expected));image.u.uuid[0]=1;
 image.u.cmdsize=0;assert(!dbImageUUIDMatches(&image,sizeof(image),expected));image.u.cmdsize=sizeof(image.u);
 image.h.ncmds=2;assert(!dbImageUUIDMatches(&image,sizeof(image),expected));image.h.ncmds=1;
 image.h.magic=0;assert(!dbImageUUIDMatches(&image,sizeof(image),expected));
 puts("image gate: matching UUID accepted; null/truncation/mismatch/invalid commands rejected");
}
