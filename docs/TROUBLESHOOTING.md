# Troubleshooting

These checks read state. They do not restart services or change brightness.

## Loaded version

```sh
kmutil showloaded | rg 'org.displaybrightness.experimental|as.vit9696.Lilu'
ioreg -lw0 -d 2 | rg DisplayBrightness
```

The source version is 1.0.0. Root routing should be `ready`; `DisplayBrightnessCacheBackend` should be `symbols-resolved`. The latter means symbol resolution, not a loaded or durable cache. `DisplayBrightnessStartupGate` must reach `first-login-observed` before initial hardware probing. Compatibility rejection is expected on unsupported kernels or IOGraphics images; changing an Info.plist version does not fix an ABI mismatch.

Compiling or copying a new kext does not replace the one already loaded in the kernel. If the loaded version does not match 1.0.0 after reboot, verify the actual boot EFI, its enabled OpenCore entry and the bundle at that entry's path. The old `DisplayBrightnessStartupNotBeforeNanoseconds` property belongs to the fixed-delay implementation and is absent in 1.0.0. Match the loaded UUID against the [built binary and dSYM](DEVELOPMENT.md#build-artifacts) when distinguishing rebuilds of the same version.

## Each display

```sh
ioreg -lw0 -r -c AppleDisplay | rg 'DisplayProductID|DisplayBrightness(FirstUpdate|EarlyDeclaration|Cache|InterfaceState|BootstrapState|StateSync|HardwareState|DDCFrame|InitialRead)'
```

| Field | Meaning |
| --- | --- |
| `DisplayBrightnessFirstUpdate*` | First callback time, native result, registration state and parameter-dictionary presence before/after the update |
| `DisplayBrightnessEarlyDeclarationFirstReason` | First outcome; `declared-from-cache` means a validated record supplied the initial parameters |
| `DisplayBrightnessEarlyDeclarationLastReason` | Latest outcome; later `already-registered` does not erase earlier success |
| `DisplayBrightnessEarlyDeclarationUptimeNs` | Successful early-publication time |
| `DisplayBrightnessCachedPercent` | Historical percentage used at early declaration; not a current hardware reading |
| `DisplayBrightnessCacheSaveState` | Latest save outcome; `accepted-native` / `unchanged-native` mean in-memory equality only |
| `DisplayBrightnessCacheCurrent`, `CacheMaximum`, `CacheSequence` | Last record accepted by the native preference store in this boot |
| `DisplayBrightnessInterfaceState` | `pending-hardware`, `hardware-ready` or terminal `hardware-unavailable` |
| `DisplayBrightnessBootstrapState` | `published-from-hardware` indicates valid initial reading and publication |
| `DisplayBrightnessStateSyncMessageResult` | Zero means message dispatch succeeded, not that UI refreshed |
| `DisplayBrightnessDDCFrameWaitMicroseconds` | Latest DDC scope's wait to acquire the framebuffer gate |
| `DisplayBrightnessDDCFrameHeldMicroseconds` | Latest DDC scope's held duration up to the unlock call, including validation and I/O |

Early skip reasons include `cache-missing`, `cache-invalid`, `cache-backend-unavailable`, `cache-display-key-unavailable`, `cache-system-gate-not-held`, `cache-topology-unavailable`, `workers-unavailable`, `already-registered`, `already-owned` and `parameter-publication-failed`. None should create a default value. Valid external-display EDIDs still take the login-gated learning path.

Before initialization, requests are discarded. A cached declaration remains unwritable if hardware initialization fails. A cache is last verified history; monitor-menu changes or another OS can make it differ from current hardware, which the first fresh read corrects.

## Verify login gating and startup performance

Version 1.0.0 is a local test build; no boot or startup-performance result is claimed. Keep the previously working kext available for comparison and rollback.

| Root field | Meaning |
| --- | --- |
| `DisplayBrightnessStartupGate` | `waiting-console-observer`, `waiting-console-state`, `waiting-first-login`, `first-login-observed`, or `notifier-unavailable` |
| `DisplayBrightnessLoginObservedUptimeNs` | Driver observation of the first qualifying login, captured before opening the gate; not necessarily the actual login-transition time |
| `DisplayBrightnessLoginSignalUptimeNs` | Most recent console notification sampled before that observation; zero if none was seen |
| `DisplayBrightnessLoginObservationSource` | `initial-snapshot` or `notification-snapshot`; neither claims a desktop frame has finished rendering |
| `DisplayBrightnessLoginNotificationCount` | Notifications sampled while waiting for the first observed login |
| `DisplayBrightnessPendingCleanup` | `termination-notification` or fallback pruning `on-display-event` |

1. On a cold boot that pauses at the login screen, confirm no initial DDC probe occurs before a completed local login. Cached interfaces may already exist; writes remain blocked. The driver must not need a timeout or user action to let macOS reach the login screen.
2. Log in normally. Confirm `first-login-observed`, then per-display `published-from-hardware` and `hardware-ready`. The initial `DisplayBrightnessBootstrapHistory` entries' `startedUptimeNs` must be at or after `LoginObservedUptimeNs`, with no overlapping initial-read visits. The scheduler yields between visits rather than launching all displays simultaneously.
3. Compare several cold boots against the previous build with the same login mode and attached displays. Record time to the login screen, time from completing login to a usable desktop, visible stutters and the DDC gate/read durations. The asynchronous design has no explicit boot-thread wait, but graphics-lock contention must be measured. Do not interpret the sum of DDC durations as added boot time or a successful build as evidence of zero overhead.
4. Check automatic login separately, plus display arrival after login, unplug/replug while waiting at login, repeated lock/unlock and a monitor with DDC disabled. A failed monitor should cool down while others progress. Lock/unlock must not start a second first-login initialization. Sleep/wake needs its own test; a completed login does not prove wake readiness.

`notifier-unavailable` means the login or display-publication subscription failed and leaves DDC closed with no timed bypass. Missing or non-qualifying console state leaves the driver waiting; merely running WindowServer or loginwindow is insufficient. After login, a display that has not registered remains parked until its publication event, without polling. Once initial hardware probing starts, the existing bounded retry policy applies. These diagnostics neither restart services nor change brightness.

## Verify learning and restoration

Native cached restoration was observed in the specified 0.0.32 sample; 1.0.0 preserves the record format. Existing 0.0.31 installations have no record from this feature, so expect a learning boot after installing the test kext. No service restart is part of this procedure.

1. On the learning boot, confirm the loaded version, complete a local login, then wait until background DDC initialization has run. A first reason of `cache-missing` is expected. Confirm `published-from-hardware`, `hardware-ready` and `accepted-native` for each display. Record its `CacheCurrent` and `CacheMaximum`. Native sliders may still be absent on this boot.
2. Allow normal macOS preference processing before a normal shutdown/reboot; the native setter schedules a notification after about two seconds. That interval and an `accepted-native` result do **not** prove a disk flush.
3. On the next boot with the same test build and display identity, look for `declared-from-cache`, a pre-registration `EarlyDeclarationUptimeNs`, and a `CachedPercent` matching the normalized record. Later verify a fresh hardware read, `hardware-ready`, the native API and the visible controls.
4. Record cold boot, normal reboot, sleep/wake and hot-plug separately. One successful reboot establishes only that observed case.

If `cache-missing` repeats, macOS may not have retained/loaded the record or the native display key may have changed. If `cache-system-gate-not-held` appears, the early callback lacked the required lock; the driver intentionally skips early declaration rather than taking locks in reverse order. `cache-invalid` means record validation failed. Hardware learning can still succeed in each case; repeated missing sliders are not proof of a DDC failure.

Save failures include `backend-unavailable`, `allocation-failed`, `native-rejected` and `native-readback-mismatch`. `skipped-stale-or-stopped` / `skipped-offline-or-replaced` indicate that a completed read was no longer eligible for saving. Inspect the latest save state rather than treating an older `CacheCurrent` diagnostic as a new successful save.

## Native brightness API

The optional diagnostic is a command-line tool, not a runtime dependency:

```sh
mkdir -p build
xcrun clang -Wall -Wextra -Werror tools/display-api-status.c -framework ApplicationServices -o build/display-api-status
./build/display-api-status
```

It reports online displays, `canChangeBrightness` and the read return status. A working native endpoint should report true capability and a successful read. Missing private API symbols are reported separately; private API behavior can change across OS versions.

A successful registry DDC read with native capability false means hardware probing and system discovery disagree. This was observed in 0.0.29 and 0.0.30. No service-restart automation is included.

## Boot black screen

An AMD panic containing `[PPLIB] Failed to send PPLIB IRI to Accelerator` occurred with an earlier build loaded. Similar failures predated the plugin; available logs did not establish that DDC ran before the panic. Neither a delay nor one successful boot proves the failure is fixed.

Recover using a known-working EFI. Restore the previous kext, disable its OpenCore entry, or append `-dboff` to existing boot arguments and reboot. Preserve unrelated configuration.

Reports should include OS build, loaded plugin/Lilu versions, GPU and connection type, relevant fields above, native API output and behavior with the plugin disabled. Report cold boot, sleep/wake and hot-plug separately. Full registry dumps and unrelated logs are unnecessary.
