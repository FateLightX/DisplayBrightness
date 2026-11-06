# DisplayBrightness

[简体中文](README.zh-CN.md)

An experimental Lilu plugin that exposes external-monitor brightness through macOS display parameters and DDC/CI VCP 0x10.

**Current version: 0.0.31. One local startup has verified early declaration, native brightness APIs and hardware brightness state on three DisplayPort monitors.** UI appearance, repeated cold-boot stability, sleep/wake and hot-plug still need validation. See the [runtime sample](docs/DEVELOPMENT.md#observed-runtime-sample) for the exact tested build.

## Requirements

| Component | Requirement |
| --- | --- |
| Architecture | x86_64 |
| Kernel | Darwin 25.6 |
| IOGraphics UUID | `74AA6483-8AB4-3A94-8143-5E306C16D7DA` |
| Lilu | 1.7.3 or newer |
| Display connection | Working IOFramebuffer I2C path and DDC/CI VCP 0x10 |

These are intentional ABI limits. Other OS builds, GPUs, adapters, sleep/wake and hot-plug behavior are not established as compatible.

## Behavior

- Starts automatically with Lilu; `-dboff` disables the plugin.
- Declares an interface before display registration for three verified **EDID base-block fingerprints**, belonging to U2790B, U28P2G6B and U2701B units. Another unit of the same model may not match.
- Uses 50% only as a provisional UI value. Writes are rejected and discarded until the first valid hardware read.
- Defers DDC until at least 30 seconds of uptime, then probes for up to 60 seconds. The first valid read replaces the provisional value and enables requests.
- Rechecks identity, topology and the hardware range before writes, and verifies the result by reading it back.
- Probes other valid external-display EDIDs without a model allowlist, publishing only after a valid read. Native slider discovery after that late publication remains unresolved.

The delay is a startup exclusion window, not proof that the GPU is ready or a timeout for an in-flight AMD call. No background helper or launch daemon is required.

## Build

This repository provides source. Install Xcode command-line tools and obtain a [Lilu DEBUG kext](https://github.com/acidanthera/Lilu/releases) containing development headers and [MacKernelSDK](https://github.com/acidanthera/MacKernelSDK). Run from the repository root:

```sh
python3 driver/test.py
python3 driver/build.py --lilu /path/to/Lilu.kext --sdk /path/to/MacKernelSDK
```

Output in `build/0.0.31/`: kext, matching dSYM, `DisplayBrightness-0.0.31-DEBUG.zip` and a build manifest. Building does not install or load the driver.

## Install or roll back

1. Back up the actual boot EFI and keep a working recovery path.
2. Copy `DisplayBrightness.kext` into `EFI/OC/Kexts`.
3. Enable its OpenCore `Kernel -> Add` entry after Lilu. Existing installations can replace the same bundle without new plugin-specific settings.
4. Enable DDC/CI in the monitor menu if available, then reboot and verify.

To roll back, restore the previous kext or disable its entry. Adding `-dboff` to existing boot arguments also disables it after reboot; preserve unrelated arguments.

A loaded kext or successful DDC read alone does not prove native sliders work. See [troubleshooting](docs/TROUBLESHOOTING.md).

## Development

Current validation covers **25 C++ tests and two source checks**, compilation/linking, ad-hoc signing and matching debug symbols. It does not substitute for a real boot or display-control test.

- [Design and test scope](docs/DEVELOPMENT.md)
- [Version history](CHANGELOG.md)
- [Report a reproducible issue](https://github.com/FateLightX/DisplayBrightness/issues)

Lilu's redistribution notice is in [LICENSE-Lilu.txt](LICENSE-Lilu.txt). Thanks to the [Lilu contributors](https://github.com/acidanthera/Lilu/graphs/contributors) and [MacKernelSDK](https://github.com/acidanthera/MacKernelSDK).
