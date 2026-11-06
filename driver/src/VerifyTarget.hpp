#pragma once
#include "DDCProtocol.hpp"
namespace dbhw {
template<class Read,class Record>
bool verifyTarget(const dbddc::Brightness &before,unsigned short target,Read read,Record record){
 if(!before.valid||!before.maximum||before.current>before.maximum||target>before.maximum)return false;
 dbddc::Brightness after=before;
 if(before.current!=target&&!read(after))return false;
 if(!after.valid||after.maximum!=before.maximum||after.current!=target)return false;
 record(after);return true;
}
}
