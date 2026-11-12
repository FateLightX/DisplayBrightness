# DisplayBrightness

[简体中文](README.zh-CN.md)

An experimental Lilu plugin that exposes external-monitor brightness through macOS display parameters and DDC/CI VCP 0x10.

**Current version: 1.1.0, a test build that combines login-gated startup and verified native preferences with the endpoint-recovery work from upstream 0.0.42.** Local tests and build checks pass; this version has not been boot-tested. Login timing, sleep/wake recovery and startup performance need hardware validation.

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
- Uses the display's last hardware-verified brightness to declare its interface before registration, when a valid native preference record is available. There is no fixed initial percentage or model allowlist.
- Saves one versioned record in macOS IOGraphics preferences after a successful hardware read or verified write. The record binds the value and hardware maximum to the exact base EDID and native display preference key.
- With no valid record, learns from hardware after login. **The first learning boot may have no native slider; early discovery can be tested on the next boot after the record is retained.**
- Watches the kernel's console-session notification and checks a current snapshot after subscribing. DDC admission requires both an observed completed local console login and a registered display; there is no elapsed-time override or periodic login polling.
- Probes one display per background visit, yielding between visits. Each display keeps its own bounded retry window; time spent waiting for login does not consume it. A fresh read replaces the historical value before requests are enabled. Earlier requests are discarded; cached values never enter the hardware request queue.
- Rechecks identity, topology and the hardware range before writes, and verifies the result by reading it back.
- Ignores inactive I2C interfaces and requires one unambiguous active path. After wake or connection churn, an initialized endpoint is re-probed before adjustments are accepted.
- Gives each brightness target its own bounded retry budget; repeating the same failed value cannot extend the cycle. A native provider taking over the brightness object retires the synthetic endpoint.
- Preserves an existing native brightness provider. Invalid, unavailable or mismatched records fall back to hardware learning without a fabricated value.

The login callback only signals a preallocated task; it never waits for hardware. Low-priority DDC work still acquires graphics locks, so asynchronous execution does not prove zero startup overhead. Login completion is not proof that the GPU is ready and cannot time out an in-flight AMD call. The cached value is historical: monitor-menu changes or another OS can make it differ from current hardware. No background helper or launch daemon is required. See [startup verification](docs/TROUBLESHOOTING.md#verify-login-gating-and-startup-performance).

## Build

This repository provides source. Install Xcode command-line tools and obtain a [Lilu DEBUG kext](https://github.com/acidanthera/Lilu/releases) containing development headers and [MacKernelSDK](https://github.com/acidanthera/MacKernelSDK). Run from the repository root:

```sh
python3 driver/test.py
python3 driver/build.py --lilu /path/to/Lilu.kext --sdk /path/to/MacKernelSDK
```

Output in `build/1.1.0/`: kext, matching dSYM, `DisplayBrightness-1.1.0-DEBUG.zip` and a build manifest. The ZIP includes both language READMEs, the design/troubleshooting guides and `ExportBootLog.command`. Building does not install or load the driver. See [artifact verification](docs/DEVELOPMENT.md#build-artifacts) for signature, UUID and checksum checks.

## Install or roll back

1. Back up the actual boot EFI and keep a working recovery path.
2. Copy `DisplayBrightness.kext` into `EFI/OC/Kexts`.
3. Enable its OpenCore `Kernel -> Add` entry after Lilu. Existing installations can replace the same bundle without new plugin-specific settings.
4. Enable DDC/CI in the monitor menu if available, then reboot and verify.

To roll back, restore the previous kext or disable its entry. Adding `-dboff` to existing boot arguments also disables it after reboot; preserve unrelated arguments.

### Upgrading to 1.1.0

| Previous version | Expected behavior |
| --- | --- |
| 0.0.31 or earlier | No verified-history record from this feature. Complete a local login to learn the hardware value; native sliders may need the next boot. |
| 0.0.32 | Reuses valid verified-history records. The first login and display registration replace the fixed 30-second DDC gate. |
| 0.0.33 | Version renumbering; retains the same login gating and native preference format. |
| 1.0.0 | Adds active-interface recovery, exact provider ownership and per-target retry budgets; the preference format is unchanged. |

Replace the existing bundle once and confirm the loaded version after reboot. No cache reset, new boot argument or login helper is needed. Before login, a cached interface can exist, but hardware reads and writes remain gated.

A loaded kext or successful DDC read alone does not prove native sliders work. See [troubleshooting](docs/TROUBLESHOOTING.md).

## Development

Current validation covers **31 C++ tests and two source checks**, compilation/linking, ad-hoc signing and matching debug symbols. It does not substitute for a real boot, startup-performance, sleep/wake, hot-plug, persistence or display-control test.

- [Design and test scope](docs/DEVELOPMENT.md)
- [Version history](CHANGELOG.md)
- [Report a reproducible issue](https://github.com/FateLightX/DisplayBrightness/issues)

Lilu's redistribution notice is in [LICENSE-Lilu.txt](LICENSE-Lilu.txt). Thanks to the [Lilu contributors](https://github.com/acidanthera/Lilu/graphs/contributors) and [MacKernelSDK](https://github.com/acidanthera/MacKernelSDK).
