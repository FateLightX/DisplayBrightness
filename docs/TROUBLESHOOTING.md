# Troubleshooting

These checks read state. They do not restart services or change brightness.

## Loaded version

```sh
kmutil showloaded | grep -E 'org.displaybrightness.experimental|as.vit9696.Lilu'
ioreg -lw0 -d 1 | grep DisplayBrightness
```

The source version is 0.0.31. Root routing should be `ready`. Compatibility rejection is expected on unsupported kernels or IOGraphics images; changing an Info.plist version does not fix an ABI mismatch.

## Each display

```sh
ioreg -lw0 -r -c AppleDisplay | grep -E 'DisplayProductID|DisplayBrightness(FirstUpdate|EarlyDeclaration|InterfaceState|BootstrapState|StateSync|HardwareState)'
```

| Field | Meaning |
| --- | --- |
| `DisplayBrightnessFirstUpdate*` | First callback time, native result, registration state and parameter-dictionary presence before/after the update |
| `DisplayBrightnessEarlyDeclarationFirstReason` | First outcome; `declared` means parameters were published |
| `DisplayBrightnessEarlyDeclarationLastReason` | Latest outcome; later `already-registered` does not erase earlier success |
| `DisplayBrightnessEarlyDeclarationUptimeNs` | Successful early-publication time |
| `DisplayBrightnessInterfaceState` | `pending-hardware`, `hardware-ready` or terminal `hardware-unavailable` |
| `DisplayBrightnessBootstrapState` | `published-from-hardware` indicates valid initial reading and publication |
| `DisplayBrightnessStateSyncMessageResult` | Zero means message dispatch succeeded, not that UI refreshed |

Skip reasons include `workers-unavailable`, `already-registered`, `already-owned`, `unverified-edid` and `parameter-publication-failed`. Unverified EDIDs can still use the generic delayed-probe path.

The provisional 50% is not a hardware reading. Before initialization, requests are discarded. On initialization failure, a declared control may remain visible while writes stay blocked.

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
