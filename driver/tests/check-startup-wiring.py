"""Prevent the deferred policy from being bypassed by its kernel callers."""
from pathlib import Path
root = Path(__file__).resolve().parents[1] / 'src'
plugin = (root / 'Plugin.cpp').read_text()
worker = (root / 'HardwareWorker.inc').read_text()
admission_source = (root / 'BootstrapAdmission.inc').read_text()
console = (root / 'ConsoleLogin.inc').read_text()
publication_source = (root / 'ParameterPublication.inc').read_text()
early_source = (root / 'EarlyDeclaration.inc').read_text()
preferences = (root / 'PreferenceBridge.inc').read_text()
persistence = (root / 'VerifiedPersistence.inc').read_text()

def body(text, signature):
    start = text.index('{', text.index(signature))
    depth = 1
    end = start + 1
    while depth:
        if text[end] == '{':
            depth += 1
        elif text[end] == '}':
            depth -= 1
        end += 1
    return text[start:end]

update = body(plugin, 'bool update(IOService *s)')
assert update.index('originalUpdate(s)') < update.index('declareEarlyEndpoint(s)') < update.index('queueBootstrap(s)')
assert 'if(ok && enabled())' not in update
assert 'if(enabled())' in update and '\n   queueBootstrap(s);' in update
assert 'if(ok' not in update, 'uncached displays must learn even if native doUpdate returns false'
for forbidden in ('FrameScope', 'queueHardware(', 'publish(', 'IOSleep(', 'startIO('):
    assert forbidden not in update, f'doUpdate must defer {forbidden}'
early = body(early_source, 'void declareEarlyEndpoint(')
assert early.index('kIOServiceRegisteredState') < early.index('publish(s,true,')
assert early.index('readCachedBrightness(s,cached)') < early.index('if(strcmp(result,"cache-valid"))') < early.index('publish(s,true,cached.percent)')
assert early.index('publish(s,true,') < early.index('publishEDMMarker(s)')
for forbidden in ('FrameScope', 'queueHardware(', 'IOSleep(', 'startIO(', 'IOLockLock(', 'requests.', 'runAction(', 'storeVerifiedPreference('):
    assert forbidden not in early, f'early declaration must not perform {forbidden}'
properties = body(plugin, 'IOReturn properties(IOService *s,')
assert properties.index('!endpointReady(s)') < properties.index('originalProperties(s,input)')
assert 'waiting?kIOReturnNotReady:originalProperties(s,input)' in properties
integer = body(plugin, 'bool integer(IOService *s,')
assert integer.index('if(!endpointReady(s))') < integer.index('FrameScope frame(s)')
readiness = body(worker, 'bool endpointReady(')
assert 'c.requests.writable()' in readiness and 'FrameScope' not in readiness
submission = body(worker, 'bool queueHardware(IOService *s,')
assert 'submitBrightness(desired,schedule)' in submission
assert 'submitBootstrap(schedule)' in submission
assert 'requests.submit(' not in worker
initialize = body(worker, 'bool initializeEndpoint(')
assert initialize.index('publish(c->display,true,value)') < initialize.index('requests.initialized=true')
publication = body(publication_source, 'bool publish(IOService *s,')
assert 'if(ok && haveSeed){ok=seed<=100;if(ok)n->setValue(seed);}' in publication
queue = body(admission_source, 'bool queueBootstrap(IOService *s)')
assert 'rememberBootstrap(s,fb)' in queue and 'scheduleAdmission()' in queue
assert 'FrameScope' not in queue and 'startIO(' not in queue
admission = body(admission_source, 'void admissionWorker(')
assert admission.index('if(!loginGate.allowed())return;') < admission.index('bootstrapTurns.take(') < admission.index('FrameScope')
assert admission.index('IOLockUnlock(hardwareQueue)') < admission.index('releaseTickets(removed,pruned)') < admission.index('if(!loginGate.allowed())return;')
assert 'Exactly one initial-read visit' in admission
schedule = body(admission_source, 'void scheduleAdmission(')
assert schedule.index('if(!loginGate.allowed())return;') < schedule.index('IOLockLock(')
assert 'due<admissionDueNs' in schedule and 'admissionWaitingForRetirement' in schedule
assert 'registeredPending()' in schedule
assert admission.index('display->getState()&kIOServiceRegisteredState') < admission.index('FrameScope')
assert queue.index('prunePendingLocked(removed,s,fb)') < queue.index('rememberBootstrap(s,fb)')
assert queue.index('IOLockUnlock(hardwareQueue)') < queue.index('releaseTickets(removed,pruned)')
hardware = body(worker, 'void hardwareWorker(')
assert hardware.index('if(!loginGate.allowed())return;') < hardware.index('requests.take()') < hardware.index('bootstrapWindow.begin(')
assert hardware.index('bootstrapTurns.active!=slotIndex(c)') < hardware.index('requests.take()')
assert 'if(!initialized)bootstrapTurns.finish(slotIndex(c),uptimeNanoseconds())' in hardware
assert hardware.rindex('IOLockUnlock(hardwareQueue)') < hardware.rindex('scheduleAdmission()')
io = body(worker, 'bool hardwareIO(')
assert io.index('if(!loginGate.allowed())return false;') < io.index('FrameScope') < io.index('startIO(')
observer = body(console, 'IOReturn consoleSecurityChanged(')
assert 'thread_call_enter(loginCall)' in observer
for forbidden in ('IOLockLock(', 'setProperty(', 'copyProperty(', 'FrameScope', 'IOSleep(', 'startIO(', 'IOLog(', 'withCapacity', 'waitFor'):
    assert forbidden not in observer, f'console callback must only signal work: {forbidden}'
assert body(console, 'bool pendingDisplayChanged(').strip() == '{\n requestPendingCleanup();return true;\n}'
assert body(admission_source, 'void requestPendingCleanup(') == '{thread_call_enter(admissionCall);}'
login = body(console, 'void loginWorker(')
assert login.index('serviceRoot->registerInterest(') < login.index('IORegistryEntry::getRegistryRoot()') < login.index('copyProperty("IOConsoleUsers")')
assert '"kCGSSessionOnConsoleKey"' in login and '"kCGSessionLoginDoneKey"' in login
assert '"kCGSSessionLoginDoneKey"' not in login
assert login.index('if(!loginNotifier||!displayPublishNotifier)') < login.index('loginGate.observe(') < login.index('scheduleAdmission()')
assert 'gIOFirstPublishNotification' in login and 'gIOTerminatedNotification' in login
assert 'thread_call_enter_delayed' not in console
assert 'thread_call_allocate_with_priority' not in plugin+worker
assert (plugin+worker).count('THREAD_CALL_PRIORITY_LOW,THREAD_CALL_OPTIONS_ONCE') == 4
retire = body(worker, 'void retireWorker(')
assert retire.index('thread_call_cancel_wait(call)') < retire.index('bootstrapTurns.cancel(') < retire.index('scheduleAdmission()')
assert 'admissionWaitingForRetirement=false' in retire
assert hardware.index('IOLockUnlock(probeSerial)') < hardware.index('if(ok){syncVerifiedState(c,sequence);persistVerifiedState(c,sequence);}')
assert hardware.index('persistVerifiedState(c,sequence)') < hardware.rindex('IOLockLock(hardwareQueue)')
restore = body(preferences, 'const char *readCachedBrightness(')
assert restore.index('!loop->inGate()') < restore.index('loadVerifiedPreference(')
for forbidden in ('runAction(', 'FrameScope', 'IOLockLock(', 'startIO('):
    assert forbidden not in restore, f'early preference lookup must not acquire {forbidden}'
persist = body(persistence, 'void persistVerifiedState(')
assert 'runAction(commitVerifiedPreference,' in persist
for forbidden in ('FrameScope', 'IOLockLock(', 'startIO('):
    assert forbidden not in persist, f'system gate must be acquired without {forbidden}'
commit = body(persistence, 'IOReturn commitVerifiedPreference(')
assert commit.index('loop->inGate()') < commit.index('FrameScope') < commit.index('hardwareLive(') < commit.index('IOLockLock(hardwareQueue)')
assert '!c->retiring&&!c->terminal&&c->requests.writable()' in commit
assert 'dbstate::mayPublish(commit->sequence,c->sequence,c->requests.pending,true)' in commit
assert commit.index('IOLockUnlock(hardwareQueue)') < commit.index('if(!eligible)') < commit.index('storeVerifiedPreference(')
assert 'c->verifiedCurrent,c->verifiedMaximum' in commit
assert persistence.count('storeVerifiedPreference(') == 1 and 'storeVerifiedPreference(' not in worker
assert '#include "VerifiedPersistence.inc"' in worker
cycle = body(worker, 'bool hardwareCycle(')
assert cycle.index('dbhw::verifyTarget(') < cycle.index('c->verifiedCurrent=after.current')
loaded = body(plugin, 'void loaded(')
assert loaded.index('dbImageUUIDMatches(') < loaded.index('resolvePreferenceBackend(')
assert '#include "PreferenceBridge.inc"' in plugin and '#include "EarlyDeclaration.inc"' in plugin
for source in root.iterdir():
    if source.is_file():
        for removed in ('provisionalBrightness', 'EarlyDisplayProfiles', 'knownEarlyFingerprint', 'DisplayBrightnessProvisionalPercent', 'StartupDelay', 'startupRemainingMs', '30000000000ULL'):
            assert removed not in source.read_text(), f'{removed} must not return'
assert worker.count('startIO(c->interface,&q)') == 1
print('startup/cache wiring: login-only admission and I/O; signal-only observer; serialized workers; fair bootstrap turns; cache-only early UI; verified persistence and lock order')
