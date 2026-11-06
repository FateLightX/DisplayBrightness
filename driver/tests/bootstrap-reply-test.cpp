#include "BootstrapJournal.hpp"
#include <cassert>
#include <cstdio>
int main(){
 dbstate::BootstrapAttempt a;uint8_t shortReply[3]={0x6e,0x80,0x02};a.captureReply(shortReply,3,20);
 assert(a.reportedReplyBytes==20&&a.capturedReplyBytes==3&&a.reply[0]==0x6e&&a.reply[2]==2&&a.reply[3]==0);
 a.captureReply(shortReply,3,1);assert(a.capturedReplyBytes==1&&a.reply[1]==0);
 a.captureReply(nullptr,0,11);assert(a.reportedReplyBytes==11&&a.capturedReplyBytes==0&&a.reply[0]==0);
 uint8_t big[20];for(unsigned i=0;i<20;i++)big[i]=static_cast<uint8_t>(i);a.captureReply(big,20,20);assert(a.capturedReplyBytes==11&&a.reply[10]==10);
 puts("bootstrap reply: bounded raw snapshot, truncated/oversized/absent input and stale-tail clearing pass");
}
