#pragma once
#include <stdint.h>
namespace dbstate {
// A boot-time exclusion window, not proof that the GPU is ready and not an
// I/O timeout. Never sleep in IODisplay callbacks to wait for this deadline.
struct StartupDelay {
 static constexpr uint64_t notBeforeNs=30000000000ULL;
 static unsigned remainingMs(uint64_t uptimeNs){
  if(uptimeNs>=notBeforeNs)return 0;
  return static_cast<unsigned>((notBeforeNs-uptimeNs+999999ULL)/1000000ULL);
 }
};
}
