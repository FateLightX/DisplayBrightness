"""Prevent the deferred policy from being bypassed by its kernel callers."""
from pathlib import Path
root = Path(__file__).resolve().parents[1] / 'src'
plugin = (root / 'Plugin.cpp').read_text()
worker = (root / 'HardwareWorker.inc').read_text()
publication_source = (root / 'ParameterPublication.inc').read_text()

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
assert 'if(enabled())' in update and 'if(ok || ownedEndpoint(s))queueBootstrap(s)' in update
for forbidden in ('FrameScope', 'queueHardware(', 'publish(', 'IOSleep(', 'startIO('):
    assert forbidden not in update, f'doUpdate must defer {forbidden}'
early = body(plugin, 'void declareEarlyEndpoint(')
assert early.index('kIOServiceRegisteredState') < early.index('publish(s,true,')
assert early.index('dbstate::earlyDisplay(') < early.index('publish(s,true,')
assert early.index('publish(s,true,') < early.index('publishEDMMarker(s)')
for forbidden in ('FrameScope', 'queueHardware(', 'IOSleep(', 'startIO(', 'IOLockLock('):
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
queue = body(worker, 'bool queueBootstrap(IOService *s)')
assert 'rememberBootstrap(s,fb)' in queue and 'scheduleAdmission()' in queue
assert 'FrameScope' not in queue and 'startIO(' not in queue
admission = body(worker, 'void admissionWorker(')
assert admission.index('if(startupRemainingMs())') < admission.index('pendingBootstrap.take(')
assert admission.index('scheduleAdmission();return;') < admission.index('FrameScope')
hardware = body(worker, 'void hardwareWorker(')
assert hardware.index('if(unsigned delay=startupRemainingMs())') < hardware.index('requests.take()')
assert hardware.index('thread_call_enter_delayed(c->call,deadline);return;') < hardware.index('bootstrapWindow.begin(')
assert hardware.index('thread_call_enter_delayed(c->call,deadline);return;') < hardware.index('IOLockLock(probeSerial)')
assert worker.count('startIO(c->interface,&q)') == 1
print('startup wiring: early declaration only before registration; pending writes fail before native gates; verified value precedes readiness; DDC remains deferred')
