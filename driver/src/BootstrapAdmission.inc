using PendingBootstrap=dbstate::BootstrapPending<IOService,dbcompat::endpointCapacity>;
using BootstrapTurns=dbstate::BootstrapSchedule<dbcompat::endpointCapacity>;
PendingBootstrap pendingBootstrap;
BootstrapTurns bootstrapTurns;
bool admissionScheduled=false;
bool admissionWaitingForRetirement=false;
UInt64 admissionDueNs=0;
unsigned pendingCursor=0;
unsigned slotIndex(const HardwareSlot *slot){return static_cast<unsigned>(slot-hardwareSlots);}
void releaseTickets(PendingBootstrap::Ticket *tickets,unsigned count){
 for(unsigned i=0;i<count;i++){tickets[i].frame->release();tickets[i].display->release();}
}
// hardwareQueue held. isInactive is a state check; no native framebuffer gate.
unsigned prunePendingLocked(PendingBootstrap::Ticket (&removed)[dbcompat::endpointCapacity],IOService *current=nullptr,IOService *frame=nullptr){
 return pendingBootstrap.prune([&](const PendingBootstrap::Ticket &ticket){
  return ticket.display->isInactive()||ticket.frame->isInactive()||
         (ticket.display==current&&ticket.frame!=frame);
 },removed);
}
void requestPendingCleanup(){thread_call_enter(admissionCall);}
bool registeredPending(){
 for(const auto &ticket:pendingBootstrap.entries)
  if(ticket.display&&(ticket.display->getState()&kIOServiceRegisteredState))return true;
 return false;
}
void scheduleAdmission(){
 // Event driven before login: never arm a repeating timer to wait for a user.
 if(!loginGate.allowed())return;
 UInt64 now=uptimeNanoseconds();
 IOLockLock(hardwareQueue);
 unsigned delay=bootstrapTurns.delayMs(now);
 if(bootstrapTurns.active==BootstrapTurns::none&&registeredPending()&&!admissionWaitingForRetirement){
  unsigned spacing=bootstrapTurns.spacingMs(now);if(spacing<delay)delay=spacing;
 }
 if(delay!=BootstrapTurns::none){
  if(delay<BootstrapTurns::quantumMs)delay=BootstrapTurns::quantumMs;
  UInt64 due=now+static_cast<UInt64>(delay)*1000000ULL;
  // Repeated display updates may bring work forward, never postpone it forever.
  if(!admissionScheduled||due<admissionDueNs){
   admissionScheduled=true;admissionDueNs=due;
   uint64_t deadline;clock_interval_to_deadline(delay,kMillisecondScale,&deadline);
   thread_call_enter_delayed(admissionCall,deadline);
  }
 }
 IOLockUnlock(hardwareQueue);
}
void admissionWorker(thread_call_param_t,thread_call_param_t){
 PendingBootstrap::Ticket removed[dbcompat::endpointCapacity]={};
 IOLockLock(hardwareQueue);
 admissionScheduled=false;
 unsigned pruned=prunePendingLocked(removed);
 IOLockUnlock(hardwareQueue);
 releaseTickets(removed,pruned);
 if(!loginGate.allowed())return; // Cleanup is permitted before hardware admission.
 UInt64 now=uptimeNanoseconds();
 PendingBootstrap::Ticket ticket;
 IOLockLock(hardwareQueue);
 unsigned next=bootstrapTurns.take(now);
 if(next!=BootstrapTurns::none){
  auto &slot=hardwareSlots[next];
  bool eligible=slot.display&&slot.call&&!slot.retiring&&!slot.requests.stopped&&
                !slot.requests.initialized&&slot.requests.busy;
  if(eligible)thread_call_enter(slot.call);
  else bootstrapTurns.finish(next,now);
  IOLockUnlock(hardwareQueue);
  if(!eligible)scheduleAdmission();
  return; // Exactly one initial-read visit is dispatched per turn.
 }
 if(bootstrapTurns.active==BootstrapTurns::none&&!bootstrapTurns.spacingMs(now)&&!admissionWaitingForRetirement){
  for(unsigned offset=0;offset<dbcompat::endpointCapacity;offset++){
   unsigned i=(pendingCursor+offset)%dbcompat::endpointCapacity;
   auto *display=pendingBootstrap.entries[i].display;
   if(!display||!(display->getState()&kIOServiceRegisteredState))continue;
   ticket=pendingBootstrap.take(i);
   if(ticket.display){pendingCursor=(i+1)%dbcompat::endpointCapacity;break;}
  }
 }
 IOLockUnlock(hardwareQueue);
 if(ticket.display){
  {
   FrameScope gate(ticket.display);
   if(gate.valid(ticket.display)&&gate.frameService==ticket.frame){
    publish(ticket.display,false,0);publishEDMMarker(ticket.display);
    queueHardware(ticket.display,ticket.frame,0,true);
   }
  }
  releaseTickets(&ticket,1);
 }
 scheduleAdmission(); // One display per visit; yield with all native locks released.
}
// hardwareQueue held. Existing tickets already own their reference pair.
bool rememberBootstrap(IOService *s,IOService *fb){
 auto result=pendingBootstrap.offer(s,fb);
 if(result==dbstate::Offer::Inserted){s->retain();fb->retain();}
 return result!=dbstate::Offer::Full;
}
bool queueBootstrap(IOService *s){
 if(!hardwareRequested||!hardwareQueue||!admissionCall||!startIO)return false;
 auto *connection=s->copyParentEntry(gIOServicePlane);
 auto *parent=connection?connection->copyParentEntry(gIOServicePlane):nullptr;
 auto *fb=OSDynamicCast(IOService,parent);
 bool queued=false;
 PendingBootstrap::Ticket removed[dbcompat::endpointCapacity]={};unsigned pruned=0;
 if(hasClass(connection,"IODisplayConnect")&&hasClass(fb,"IOFramebuffer")&&!fb->isInactive()){
  IOLockLock(hardwareQueue);
  pruned=prunePendingLocked(removed,s,fb);
  queued=rememberBootstrap(s,fb);
  IOLockUnlock(hardwareQueue);
 }
 releaseTickets(removed,pruned);
 if(parent)parent->release();if(connection)connection->release();
 if(queued){requestLoginCheck();scheduleAdmission();}
 else s->setProperty("DisplayBrightnessBootstrapState","admission-unavailable");
 return queued;
}
