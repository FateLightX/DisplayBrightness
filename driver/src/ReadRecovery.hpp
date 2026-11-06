#pragma once
namespace dbhw {
// Only read attempts belong here. Never wrap SetVCP in this helper.
template<class Read,class Pause,class Continue> bool readRecovery(Read read,Pause pause,Continue canContinue){
 for(unsigned attempt=0;attempt<3;attempt++){
  if(read())return true;
  if(!canContinue())return false;
  if(attempt<2)pause();
 }
 return false;
}
template<class Read,class Pause> bool readRecovery(Read read,Pause pause){return readRecovery(read,pause,[](){return true;});}
}
