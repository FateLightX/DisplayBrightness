# Changelog

## 1.1.0

- Import the hardware-side recovery improvements present in upstream DisplayBrightness 0.0.42 while retaining local login gating, verified native preferences and fair startup admission.
- Re-probe an initialized endpoint when the active IOFramebuffer I2C interface or bound native brightness range changes after wake or connection churn. Inactive interfaces are ignored, and an ambiguous active set is rejected.
- Track exact native brightness ownership through `Missing` / `Owned` / `Foreign` states and refuse hardware transport when another provider has taken over.
- Give each brightness target its own bounded automatic retry budget. Repeating the same failed value cannot extend the exhausted cycle; a different target starts a fresh cycle.
- Add a read-only `ExportBootLog.command` exporter with I2C topology, endpoint-refresh decisions, retry state, GPU hang/reset lines and recent kernel panic summaries.
- Validation: 31 C++ tests, two source checks, kext compilation/linking, ad-hoc signature, ZIP integrity and matching dSYM. No live boot, sleep/wake or hot-plug result is claimed.

## 1.0.0

- Renumber the current build from 0.0.33 to 1.0.0, including bundle/module versions, package paths and documentation. Driver behavior and native preference format are unchanged.
- This remains an experimental build; the version change does not establish cold-boot or startup-performance validation.

## 0.0.33

- Replace the fixed boot delay with the first observed completed local console login and a registered target display. Register native notifications before reading the current registry snapshot, including preexisting login and late display publication.
- Keep notification handlers limited to signaling preallocated tasks. Use low-priority, non-reentrant thread calls; no login polling, service restart or timer-based bypass.
- Dispatch one initial-read visit at a time, with a 50 ms scheduling yield between visits. Retry cooldowns remain per display, allowing other displays to progress.
- Reclaim inactive/replaced pending tickets before insertion and on termination notifications, including before login. Park admission during slot retirement and resume on completion.
- Preserve cached early interfaces, rejection of pre-read writes, fresh-read/readback validation and native power checks. Add login observation and DDC framebuffer-lock timing diagnostics.
- Document upgrades from 0.0.31/0.0.32, package verification and loaded-version checks; remove the obsolete fixed-delay policy and its test.
- Validation: 29 C++ tests and two source checks, compilation/linking, signature and matching dSYM. New imported symbols were found in the local boot kernel collection. No live boot or startup-performance comparison of 0.0.33 has been observed.

## 0.0.32

- Replace the fixed 50% startup placeholder and three-device fingerprint list with per-display, hardware-verified history in native IOGraphics preferences.
- Store a versioned, checksummed record containing the exact base EDID and verified current/maximum. Confirm native in-memory acceptance by reading the record back; unchanged values do not request another save.
- Declare an early interface only from a valid record, while the caller already holds the IOGraphics system gate. An unavailable or invalid cache leaves hardware learning enabled, including when native `doUpdate` returns false.
- Preserve the 30-second DDC exclusion window, fresh-read readiness gate and rejection of early requests. The plugin never queues a cached value as a hardware target.
- Commit verified state after releasing bus locks, taking the system gate before the framebuffer gate. Skip obsolete, pending, retired, stopped or replaced endpoints.
- Validation: 26 C++ tests and two source checks, including production preference, publication and commit functions with test doubles; compilation/linking, ad-hoc signature and matching dSYM.
- Later read-only observation of UUID `1DF5F5A8-87EA-3659-93B1-69C6826E29D0`: all three displays declared from native cached records at about 6.73 seconds and completed initial hardware reads after 30.05 seconds. This validates the observed restore path, not repeated-boot reliability or UI appearance. First learning may require another boot for native discovery.

## 0.0.31

- Allow early publication when `IODisplayParameters` does not exist, preserving native parameters and providers.
- Separate early declaration from the native `doUpdate` return value.
- Record first-update state and reasons for declaring or skipping an interface.
- Add a regression test executing the production publication function.
- Consolidate documentation and retain a reusable read-only API diagnostic.
- Validation: 25 C++ tests, two source checks, compilation/linking, ad-hoc signatures and matching dSYM.
- One local startup verified early publication at about 7.02 seconds, working native brightness APIs and 80/100 hardware state on three displays. UI appearance, repeated cold boot, sleep/wake and hot-plug still need validation.

## 0.0.30

- Introduce pre-registration declaration for three verified EDID fingerprints.
- Reject and discard brightness requests before hardware initialization.
- Observed result: DDC initialization succeeded, but no early declaration was recorded and native sliders remained unavailable.

## 0.0.29

- Move added framebuffer locking and DDC out of the display update callback.
- Add a 30-second startup exclusion window without consuming the 60-second probe window.
- Align module/bundle versions; add build and test scripts.
- Observed result: hardware brightness reads and writes worked after native capability discovery was refreshed; automatic discovery remained unresolved.

## 0.0.28

- Initial experimental brightness-only implementation.
- AMD PPLIB panics occurred during startup with the plugin loaded. A causal link to DDC was not established.
