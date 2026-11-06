#include <ApplicationServices/ApplicationServices.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdbool.h>
int main(void) {
 void *h=dlopen("/System/Library/PrivateFrameworks/DisplayServices.framework/DisplayServices",RTLD_LAZY|RTLD_LOCAL);
 bool (*canChange)(CGDirectDisplayID)=h?dlsym(h,"DisplayServicesCanChangeBrightness"):NULL;
 int (*getBrightness)(CGDirectDisplayID,float*)=h?dlsym(h,"DisplayServicesGetBrightness"):NULL;
 CGDirectDisplayID displays[16];uint32_t count=0;
 CGError err=CGGetOnlineDisplayList(16,displays,&count);
 printf("online_list_status=%d count=%u canChange_symbol=%d getBrightness_symbol=%d\n",err,count,canChange!=NULL,getBrightness!=NULL);
 for(unsigned i=0;i<count;i++){
  float value=-1;int status=getBrightness?getBrightness(displays[i],&value):-1;
  printf("display=%u vendor=%u product=%u builtin=%d canChangeBrightness=%d getBrightness_status=%d brightness=%.3f\n",displays[i],CGDisplayVendorNumber(displays[i]),CGDisplayModelNumber(displays[i]),CGDisplayIsBuiltin(displays[i]),canChange?canChange(displays[i]):-1,status,value);
 }
 if(h)dlclose(h);return 0;
}
