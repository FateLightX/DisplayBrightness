#pragma once
namespace dbhw {
enum class ReadResult {Valid,Failed,Superseded,Stopped};
inline bool differentPending(bool pending,unsigned latest,unsigned captured){return pending&&latest!=captured;}
template<class Read,class Pause,class Alive,class Obsolete>
ReadResult responsiveRead(Read read,Pause pause,Alive alive,Obsolete obsolete){
 for(unsigned i=0;i<3;i++){
  if(!alive())return ReadResult::Stopped;
  if(read())return ReadResult::Valid;
  if(!alive())return ReadResult::Stopped;
  if(obsolete())return ReadResult::Superseded;
  if(i<2)for(unsigned slice=0;slice<25;slice++){
   if(!alive())return ReadResult::Stopped;
   if(obsolete())return ReadResult::Superseded;
   pause();
  }
 }
 return ReadResult::Failed;
}
}
