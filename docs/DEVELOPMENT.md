# Driver design

## Startup and requests

`Plugin.cpp` routes IODisplay parameter updates and brightness setters through Lilu. Routing is restricted to Darwin 25.6 and the IOGraphics UUID in the README. Failed compatibility checks leave publication disabled.

For a verified EDID fingerprint, the first pre-registration update declares synthetic `brightness` parameters and the EDM marker. `ParameterPublication.inc` can create a missing dictionary, preserves unrelated parameters and refuses to replace a native brightness provider. The provisional value is 50%.

The property setter returns `kIOReturnNotReady` for synthetic brightness until initialization. Direct integer-set calls are also rejected before taking an additional framebuffer gate. `EndpointRequests.hpp` prevents rejected input from replacing a bootstrap ticket or becoming a later write.

`HardwareWorker.inc` checks the 30-second uptime deadline before framebuffer/bus work. A separate 60-second probe window then governs retries. The first valid VCP 0x10 read replaces the provisional value before the endpoint becomes writable. A brightness property-change message follows; success proves dispatch, not that CoreBrightness refreshed its UI.

Normal requests coalesce to the latest target. Writes use fresh hardware reads and readback verification. Identity and connection topology are rechecked before I/O. Native framebuffer and queue locking retain their established order.

## Compatibility scope

Generic read-only probing accepts valid external-display EDIDs. Early declaration is narrower: `EarlyDisplayProfiles.hpp` contains three previously verified base-block fingerprints. A fingerprint is an identity hint, not permission to write; hardware validation remains mandatory. Another unit of the same model may not match.

The startup delay is not a GPU-ready signal or a hard timeout. A synchronous AMD I2C request can still block.

CoreBrightness may cache capabilities during device construction. Ordinary value-change notifications are not established as capability rediscovery. Initialization errors may affect whether the system retains a control; later system retries cannot reliably be distinguished from new user input.

## Validation

Run `python3 driver/test.py` from the repository root.

The suite contains 25 C++ programs and two source checks. It covers packets, read recovery, coalescing, endpoint retirement/reuse, identity/topology, startup timing, state synchronization and publication ownership.

The publication regression harness executes the same implementation included by the kext, using reference-counted registry doubles. It tests missing dictionaries, true zero, native data preservation and failure cleanup. It reproduces the missing-dictionary rejection in 0.0.30, but does not prove which branch ran during that historical boot.

Public fixtures use synthetic or serial-redacted EDIDs. Known-profile membership and fingerprint calculation are tested separately so raw device serial data need not be published.

The build script compiles and links x86_64 kernel code, generates dSYM, applies an ad-hoc signature and validates the bundle and ZIP. The local manifest records source/archive hashes and toolchain paths. Build output is ignored by Git.

These checks do not exercise a running kernel, DDC transport, native UI, cold boot, sleep/wake or hot-plug. See [runtime checks](TROUBLESHOOTING.md).

## Observed runtime sample

A local 0.0.31 build, UUID `774A9B08-C844-3059-854C-6A95B93A1F61`, was inspected on Darwin 25.6 with Lilu 1.7.3. Match UUIDs as well as versions when comparing builds; a rebuilt package is not automatically the same tested binary.

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
