#pragma once
#include <mach-o/loader.h>
#include <stddef.h>
#include <string.h>
inline bool dbImageUUIDMatches(const void *image,size_t length,const unsigned char expected[16]){
    if(!image || length<sizeof(mach_header_64))return false;
    const auto *h=static_cast<const mach_header_64 *>(image);
    if(h->magic!=MH_MAGIC_64 || h->sizeofcmds>length-sizeof(*h))return false;
    const unsigned char *cursor=reinterpret_cast<const unsigned char *>(h+1);
    const unsigned char *end=cursor+h->sizeofcmds;
    bool matched=false;
    for(unsigned i=0;i<h->ncmds;i++){
        if(static_cast<size_t>(end-cursor)<sizeof(load_command))return false;
        const auto *cmd=reinterpret_cast<const load_command *>(cursor);
        if(cmd->cmdsize<sizeof(*cmd) || cmd->cmdsize>static_cast<size_t>(end-cursor))return false;
        if(cmd->cmd==LC_UUID){
            if(matched || cmd->cmdsize<sizeof(uuid_command))return false;
            const auto *uuid=reinterpret_cast<const uuid_command *>(cursor);
            if(memcmp(uuid->uuid,expected,16))return false;
            matched=true;
        }
        cursor+=cmd->cmdsize;
    }
    return matched && cursor==end;
}
