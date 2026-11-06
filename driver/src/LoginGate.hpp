#pragma once
namespace dbstate {
// Permission to begin first-boot probing, not hardware readiness. Notifications
// and workers may run on different CPUs. Once opened, lock/unlock cannot close it.
struct LoginGate {
 bool opened=false;
 bool allowed()const{return __atomic_load_n(&opened,__ATOMIC_ACQUIRE);}
 bool observe(bool onConsole,bool loginDone){
  if(!onConsole||!loginDone)return false;
  bool expected=false;
  return __atomic_compare_exchange_n(&opened,&expected,true,false,__ATOMIC_RELEASE,__ATOMIC_RELAXED);
 }
};
}
