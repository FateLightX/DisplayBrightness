# Changelog

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
