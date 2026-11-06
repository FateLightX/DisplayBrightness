#pragma once
#include <stdint.h>
namespace dbstate {
enum class BootstrapPhase {Interface, Read, Publish, Ready, Exhausted};
inline const char *bootstrapPhaseText(BootstrapPhase p){switch(p){case BootstrapPhase::Interface:return "interface";case BootstrapPhase::Read:return "read";case BootstrapPhase::Publish:return "publish";case BootstrapPhase::Ready:return "ready";case BootstrapPhase::Exhausted:return "window-exhausted";}return "unknown";}
struct BootstrapAttempt {
 uint64_t startedNs=0,finishedNs=0;
 uint32_t reads=0,transport=0,request=0,ordinal=0,reportedReplyBytes=0,capturedReplyBytes=0;
 uint8_t reply[11]={};
 void captureReply(const uint8_t *bytes,unsigned capacity,uint32_t reported){
  reportedReplyBytes=reported;capturedReplyBytes=bytes?(reported<capacity?reported:capacity):0;
  if(capturedReplyBytes>11)capturedReplyBytes=11;
  for(unsigned i=0;i<11;i++)reply[i]=i<capturedReplyBytes?bytes[i]:0;
 }
 BootstrapPhase phase=BootstrapPhase::Interface;
 bool attempted=false,replyValid=false,success=false;
};
struct BootstrapJournal {
 BootstrapAttempt attempts[4];unsigned count=0,dropped=0,total=0;
 bool append(BootstrapAttempt a){a.ordinal=++total;if(count==4){attempts[3]=a;++dropped;}else attempts[count++]=a;return true;}
};
}
