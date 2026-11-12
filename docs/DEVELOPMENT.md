# Driver design

## Startup and requests

`Plugin.cpp` routes IODisplay parameter updates and brightness setters through Lilu. Routing is restricted to Darwin 25.6 and the IOGraphics UUID in the README. Failed compatibility checks leave publication disabled.

When the display has a valid verified-state record, `EarlyDeclaration.inc` declares synthetic `brightness` parameters and the EDM marker before registration. Its initial percentage is derived from that record's current/maximum, including a genuine zero. `ParameterPublication.inc` can create a missing dictionary, preserves unrelated parameters and refuses to replace a native brightness provider. There is no fixed percentage or hardcoded device list.

The property setter returns `kIOReturnNotReady` for synthetic brightness until initialization. Direct integer-set calls are also rejected before taking an additional framebuffer gate. `EndpointRequests.hpp` prevents rejected input from replacing a bootstrap ticket or becoming a later write.

Every eligible update queues hardware learning even if the native update returned false or no cache was available. Admission, hardware workers and the I/O boundary require the first-login permission before additional framebuffer/bus work. Each display's 60-second probe window starts when its worker begins initialization, not while waiting for login. The first valid VCP 0x10 read replaces the historical value before the endpoint becomes writable. A brightness property-change message follows; success proves dispatch, not that CoreBrightness refreshed its UI.

Normal requests coalesce to the latest target. Writes use fresh hardware reads and readback verification. Identity and connection topology are rechecked before I/O. Native framebuffer and queue locking retain their established order.

## Login events and scheduling

`ConsoleLogin.inc` subscribes to `gIOConsoleSecurityInterest` on `IOService::getServiceRoot()` from a background task. Only `kIOMessageConsoleSecurityChange` signals a login check. Because this callback is synchronous in XNU, it only records an atomic notification timestamp/count and signals a preallocated task: no allocations, property access, logging, queue/framebuffer locks or hardware I/O.

The worker reads a retained `IOConsoleUsers` snapshot from **`IORegistryEntry::getRegistryRoot()`**, which differs from the notification provider. A dictionary must contain both `kCGSSessionOnConsoleKey == true` and `kCGSessionLoginDoneKey == true` (the latter has a single `S` in `Session`). Process existence, unlocked-screen state and remote sessions alone do not open the gate. Subscription precedes the first snapshot; display arrivals also request a check while waiting. A missing root is retried on a subsequent route/display event. Failed login or first-publication subscription leaves hardware admission closed with an explicit diagnostic; there is no timed fallback.

`LoginGate.hpp` publishes a one-way atomic permission bit. Lock/unlock notifications cannot restart initialization after the first observed login. This bit does not imply that a display is hardware-ready, powered, connected or safe across sleep/wake. The native I2C path retains its powered/inactive checks; identity and readback validation still run per operation.

`BootstrapAdmission.inc` admits at most one pending display per task invocation. `BootstrapSchedule.hpp` grants only one initial-read visit at a time; retries rejoin the schedule with their own due time, so a failing monitor does not hold a turn throughout its cooldown. Tasks yield with all native locks released; a 50 ms scheduling quantum spaces visits, not a boot-uptime threshold. Repeated update events may advance an existing deadline but cannot postpone it indefinitely. All four thread-call allocation paths use `THREAD_CALL_PRIORITY_LOW` and `THREAD_CALL_OPTIONS_ONCE`, including per-display hardware calls, to prevent concurrent invocation of the same task.

Admission also checks `kIOServiceRegisteredState` before taking a display's framebuffer gate. A pre-registration ticket stays parked without a timer, even if automatic login has already completed. The first-publication notification wakes admission; its handler only signals the existing task. If publication preceded the ticket, the current registration state handles that ordering instead.

Pending tickets retain their display/framebuffer pair. Inactive entries and a display's superseded framebuffer are removed before insertion; references are released after dropping the queue lock. A termination callback signals cleanup, which can run before login without taking a framebuffer gate. If that optional notifier is unavailable, pruning still runs on display arrivals and admission. Admission waits for retiring slots to finish rather than polling for them; retirement cancels queued visits before reuse and wakes admission after freeing a slot.

Nothing waits for a user login on a boot, registration or notification thread. Waiting does not consume retry rounds or arm periodic login timers. DDC work is still synchronous while holding a framebuffer gate, however: lower CPU priority and deferred execution do not prove zero impact on desktop rendering. Optional timing in the DDC `FrameScope` records gate wait and held duration; these properties are published after the gate is released. The measurements do not impose a timeout on an AMD call. A live cold-boot comparison remains required.

Native console notification behavior was checked against [XNU IOService.cpp](https://github.com/apple-oss-distributions/xnu/blob/main/iokit/Kernel/IOService.cpp) (`updateConsoleUsers`, `registerInterest`, `applyToInterestNotifiers`). New imported symbols were also found as external definitions in the local boot kernel collection; this is not a load test of 1.1.0.

## Verified history in native preferences

`PreferenceBridge.inc` resolves `IOFramebuffer::copyPreference`, `setPreference` and the native system-workloop/preference readiness globals only after the exact kernel and IOGraphics UUID check. These are private interfaces, so missing symbols disable the cache without enabling a fabricated interface or disabling ordinary hardware learning.

The preference name is `DisplayBrightnessVerifiedState`. Native IOGraphics chooses the per-display bucket through `IODisplayPrefsKey`, which incorporates GPU/display identity and can fall back to a connection-specific key for duplicate EDIDs. The plugin does not build its own paths or write files, NVRAM or user settings. Changed records use IOGraphics' existing delayed preference notification and system persistence path.

`VerifiedBrightness.hpp` defines a single 144-byte `OSData` record: magic, schema version 1, reserved zero bytes, the complete 128-byte EDID base block, 16-bit current and maximum, and CRC32. Multi-byte fields are little-endian. A record must have the exact length/version, valid checksum, matching valid EDID and a nonzero maximum with current at or below it. CRC detects accidental corruption; the record never authorizes a hardware write.

Early reads only proceed when `IOGraphicsWorkLoop::inGate()` says the current thread already owns the system gate. They do not acquire another gate, wait or access DDC. A caller without that gate gets `cache-system-gate-not-held` and continues through login-gated learning. The specified 0.0.32 sample below observed this pre-registration path; 1.1.0 remains untested in a live boot.

After a successful initial read or verified write cycle, `VerifiedPersistence.inc` runs a synchronous action under the system workloop gate. The bus and all framebuffer/queue locks have already been released. The action takes the framebuffer gate, rechecks live topology, EDID and synthetic ownership, then checks readiness, retirement and request sequence under the queue lock. It releases the queue lock before calling the native setter, while the framebuffer gate excludes new submissions for that display. The lock order is system → framebuffer → queue; no system-gate acquisition occurs while a controller or bus lock is held. Retirement waits for the worker before clearing or reusing its slot.

`PreferenceStore.inc` encodes only the worker's verified current/maximum, skips an identical stored record and reads back changed records after native acceptance. A successful setter return alone is insufficient because the native implementation can return true on allocation failure. `accepted-native` and `unchanged-native` prove in-memory equality, not that macOS flushed the record to disk. There is no persistence retry daemon or new polling loop; another verified hardware cycle can attempt a failed save again.

The cache seeds the interface only. It does not mark an endpoint writable, change a bootstrap ticket, enqueue a brightness request or restore monitor hardware. A monitor-menu change or another OS may make the historical value stale; the next hardware read corrects it. Without a valid record, the first learning boot may still miss native capability discovery. The next boot can use the learned record only if macOS retained it and loaded preferences before discovery. See the [two-boot procedure](TROUBLESHOOTING.md#verify-learning-and-restoration).

Native implementation references: Apple's [IOFramebuffer.cpp](https://github.com/apple-oss-distributions/IOGraphics/blob/main/IOGraphicsFamily/IOFramebuffer.cpp) (`copyPreference`, `setPreference`, `writePrefs`, `IOFramebufferLockedSerialize`, `IOGraphicsWorkLoop::closeGate`) and [IODisplay.cpp](https://github.com/apple-oss-distributions/IOGraphics/blob/main/IOGraphicsFamily/IODisplay.cpp) (`loadPrefs`, `start`). Required symbols were also found in the UUID-matched local kernel collection; this does not substitute for running the new integration.

## Endpoint recovery and retry budgets

`EndpointRefresh.hpp` ignores inactive I2C services and accepts a path only when exactly one active service is visible. `HardwareWorker.inc` re-enumerates that set under the framebuffer gate before I/O and while classifying an initialized endpoint. A replaced or stopped interface, a changed framebuffer binding or a changed bound brightness object retires the old slot and queues a recovery ticket; the replacement performs a fresh hardware read before requests become writable again. Uninitialized slots keep their existing bounded startup window rather than being retired by an interface that is not bound yet.

`ProviderPolicy.hpp` distinguishes a missing brightness object, the exact object owned by this endpoint, and a foreign native provider. Initialized writes require exact ownership plus the bound range; a foreign provider is never transported through. `LatestRequest.hpp` stores the active target and a per-target automatic retry counter. A different target starts a new budget when taken, while repeating the same failed target cannot extend an exhausted cycle.

`ExportBootLog.command` is a read-only companion for field reports. It records I2C topology and active counts, endpoint-refresh decisions, retry state, GPU hang/reset lines and summaries from recent kernel panic files. It does not change services, NVRAM or display state.

## Compatibility scope

Generic read-only probing accepts valid external-display EDIDs. Early declaration requires a valid record for that native display preference bucket and exact base EDID. A new monitor, changed identity or a changed native preference key may need another learning boot. Hardware validation remains mandatory on every boot.

The first-login permission is not a GPU-ready signal or a hard timeout. A synchronous AMD I2C request can still block. Before the first login, controls declared from history remain unwritable. Automatic login may occur early, so its timing and performance require separate measurement.

CoreBrightness may cache capabilities during device construction. Ordinary value-change notifications are not established as capability rediscovery. Initialization errors may affect whether the system retains a control; later system retries cannot reliably be distinguished from new user input.

## Validation

Run `python3 driver/test.py` from the repository root.

The suite contains 31 C++ programs and two source checks. It covers packets, read recovery, coalescing, endpoint retirement/reuse, active-interface selection, endpoint refresh, provider ownership, per-target retry budgets, identity/topology, login admission, scheduling, state synchronization, publication ownership and verified history.

The publication and preference harnesses execute the implementations included by the kext, using reference-counted registry and native-preference doubles. They test missing dictionaries/cache records, real zero, unchanged saves, allocation failures, false native acceptance, native owner preservation and no early cache replay. The actual commit callback is exercised with gate assertions and stale, pending, retired, stopped and replaced endpoints. Codec tests reject bit corruption, unknown schemas, bad ranges and changed EDID identity. Source checks verify the kernel wiring and lock acquisition order; they do not exercise real kernel scheduling.

The console and admission harnesses also execute production functions. They cover the two distinct roots, exact session flags, existing login, notification during registration, late root availability, failed subscription, signal-only callbacks, pre-login cleanup, full-queue replacement, arrivals while a visit runs, independent cooldowns, retirement parking and balanced references. The kernel source checks enforce login gating at each boundary and non-reentrant task allocation. Test doubles do not reproduce real notification scheduling or graphics-lock contention.

Public fixtures use synthetic or serial-redacted EDIDs. Actual cache records contain the device's base EDID locally; reports need only the diagnostic fields, not raw preference records or full registry dumps.

These checks do not exercise a running kernel, DDC transport, native persistence, UI, cold boot, sleep/wake or hot-plug. No live boot of 1.1.0 has been observed. See [runtime checks](TROUBLESHOOTING.md).

### Build artifacts

The build script compiles and links x86_64 kernel code, generates dSYM, applies an ad-hoc signature and validates the bundle and ZIP. Rebuilding replaces the current version's bundle, symbols, ZIP and manifest. Compiler intermediates use a temporary staging directory that is removed when the build exits.

From the repository root, check the resulting package:

```sh
codesign --verify --strict build/1.1.0/DisplayBrightness.kext
xcrun dwarfdump --uuid build/1.1.0/DisplayBrightness.kext/Contents/MacOS/DisplayBrightness build/1.1.0/DisplayBrightness.kext.dSYM
shasum -a 256 build/1.1.0/DisplayBrightness-1.1.0-DEBUG.zip
```

The binary and dSYM must have the same x86_64 UUID. The ZIP SHA256 must match `archive_sha256` in `build/1.1.0/build-manifest.json`. This local manifest also records source hashes and toolchain paths; it is not included in the ZIP. The ZIP contains the kext, matching symbols, both READMEs, changelog, license, design/troubleshooting guides and the read-only boot-log exporter. Build output is ignored by Git.

Old version directories and the optional compiled `build/display-api-status` tool can be removed after preserving any needed rollback packages outside the repository. Keep symbols with their matching binary when archiving a build. Git tracks the diagnostic source, not its executable or local logs.

## Observed runtime sample

A read-only inspection of 0.0.32, UUID `1DF5F5A8-87EA-3659-93B1-69C6826E29D0`, found all three displays declared from valid native preference records at approximately 6.729 seconds of uptime. The first DDC visits began at 30.052–30.055 seconds and completed by 30.192 seconds. Individual initial reads took 46.433, 45.886 and 47.274 ms. All three endpoints were `hardware-ready`, with hardware and cached current/maximum at 100/100. This is evidence for cached early restoration in that boot, not the new login gate, visible controls or startup-performance equivalence. The original login transition time was not captured.

### Earlier 0.0.31 sample

A local 0.0.31 build, UUID `774A9B08-C844-3059-854C-6A95B93A1F61`, was inspected on Darwin 25.6 with Lilu 1.7.3. Match UUIDs as well as versions when comparing builds; a rebuilt package is not automatically the same tested binary.

This is historical evidence for 0.0.31's publication path. That version used a fixed placeholder and did not create verified-state cache records; the observations below do not validate 0.0.32's preference integration.

| Display | Early declaration | Native API | Hardware state |
| --- | --- | --- | --- |
| U2790B | 7.0175 s, before registration | Capability true; read succeeded at 0.8 | 80/100, verified already at target |
| U28P2G6B | 7.0180 s, before registration | Capability true; read succeeded at 0.8 | 80/100, verified readback |
| U2701B | 7.0183 s, before registration | Capability true; read succeeded at 0.8 | 80/100, verified already at target |

All three first-update records showed no parameter dictionary before or after the native update, followed by successful early declaration. This confirms that allowing a missing dictionary addresses an actual startup state. Each endpoint later reported `hardware-ready`.

The inspected corebrightnessd process had started about one second after boot. Its process start time was consistent with discovery in that boot session without a later service restart. The check itself was read-only.

This is one startup/runtime observation, not repeated cold-boot testing. The visible controls were not directly inspected, and sleep/wake and hot-plug remain unverified.

## Layout

| Path | Purpose |
| --- | --- |
| `driver/src/` | Kext code and portable policies |
| `driver/tests/` | Policy, publication and startup-order checks |
| `driver/build.py` | Build/package; never installs or loads |
| `driver/test.py` | Run local tests |
| `tools/display-api-status.c` | Optional read-only native API diagnostic |
| `build/` | Ignored local binaries and manifests |
