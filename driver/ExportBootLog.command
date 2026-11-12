#!/bin/bash
# Read-only compatibility survey. Output is limited to selected fields.
set -u
umask 077
export LC_ALL=C
show_finder=true
oc_file=
while (( $# )); do
    case "$1" in
        --no-open) show_finder=false; shift ;;
        --opencore)
            if (( $# < 2 )) || [[ -z "$2" ]]; then
                printf 'Usage: %s --opencore /path/to/opencore-log.txt\n' "$0" >&2
                exit 2
            fi
            oc_file=$2
            [[ "$oc_file" = /* ]] || oc_file="$PWD/$oc_file"
            [[ -f "$oc_file" && -r "$oc_file" ]] || { echo 'Cannot read the OpenCore log.' >&2; exit 2; }
            shift 2
            ;;
        *) printf 'Unknown option %s\n' "$1" >&2; exit 2 ;;
    esac
done
cd "$(dirname "$0")" || exit 1
report_dir="$(/usr/bin/mktemp -d "$PWD/bootlog-XXXXXX")" || exit 1
report="$report_dir/DisplayBrightness-boot.txt"
graphics=/System/Library/Extensions/IOGraphicsFamily.kext
plist="$graphics/Info.plist"
binary="$graphics/IOGraphicsFamily"
[[ -f "$plist" ]] || plist="$graphics/Contents/Info.plist"
[[ -f "$binary" ]] || binary="$graphics/Contents/MacOS/IOGraphicsFamily"
{
    echo '[System]'
    /usr/bin/sw_vers
    /usr/bin/uname -m
    /usr/bin/uname -r
    /usr/sbin/sysctl kern.boottime
    echo
    echo '[IOGraphics disk image]'
    if [[ -f "$plist" ]]; then
        /usr/libexec/PlistBuddy -c 'Print :CFBundleVersion' "$plist"
    fi
    if [[ -f "$binary" ]]; then
        /usr/bin/shasum -a 256 "$binary"
        if /usr/bin/xcode-select -p >/dev/null 2>&1; then
            /usr/bin/xcrun dwarfdump --uuid "$binary" 2>/dev/null
            /usr/bin/xcrun nm -arch x86_64 -j "$binary" 2>/dev/null |
                /usr/bin/awk '/^__ZN(13IOFramebuffer(6fbLock|8fbUnlock)Ev|25IOFramebufferI2CInterface7startIOEP12IOI2CRequest|9IODisplay(12doIntegerSetEP12OSDictionaryPK8OSSymbolj|13setPropertiesEP8OSObject|8doUpdateEv))$/'
        else
            echo 'UUID/symbols skipped: developer tools not installed.'
        fi
    else
        echo 'IOGraphics executable unavailable at standard paths.'
    fi
    echo
    echo '[Selected loaded drivers: identifier, version, UUID]'
    if loaded="$(/usr/bin/kmutil showloaded --list-only 2>/dev/null)"; then
        printf '%s\n' "$loaded" | /usr/bin/awk '
            {for(i=1;i<=NF;i++) {
                if(tolower($i) ~ /^(org\.displaybrightness|as\.vit9696\.(lilu|whatevergreen)|[^ ]+\.(nootrx|nootedred)$|com\.apple\.[^ ]*(iographicsfamily|applebacklight|applemccs|amdradeon|appleintel.*(graphics|framebuffer)|geforce|nvda))/) {
                    print $i, $(i+1), $(i+2)
                    break
                }
            }}'
    else
        echo 'Loaded-driver query unavailable in this session.'
    fi
    echo
    echo '[DisplayBrightness plugin state]'
    /usr/sbin/ioreg -r -c IOPlatformExpertDevice -d 1 -l -w 0 2>/dev/null |
        /usr/bin/awk '/"DisplayBrightness(RequestProbeRouting|RequestProbeVersion|GraphicsProfile|StartupMode|StartupGate|HDRRecoveryEnabled|CacheBackend)" =/'
    echo
    echo '[Display class counts; system-wide]'
    for display_class in AppleDisplay AppleBacklightDisplay IOFramebufferI2CInterface; do
        if entries="$(/usr/sbin/ioreg -r -c "$display_class" -d 0 2>/dev/null)"; then
            count="$(printf '%s\n' "$entries" | /usr/bin/awk '/\+-o /{n++} END{print n+0}')"
            printf '%s: %s\n' "$display_class" "$count"
        else
            printf '%s: unavailable\n' "$display_class"
        fi
    done
    echo
    echo '[Current framebuffer topology and selected display state]'
    /usr/sbin/ioreg -r -c IOFramebuffer -d 4 -l -w 0 |
        /usr/bin/awk '
            /^\+-o |\+-o .*<class (IOFramebufferI2CInterface|IODisplayConnect|AppleBacklightDisplay|AppleDisplay),/ { print; next }
            /"(IOI2CTransactionTypes|DisplayBrightness(I2CActiveCount|InterfaceState|EndpointRefresh|RequestRetries|BootstrapState|BootstrapAttemptCount|HardwareState|HardwareOperation|HardwareReplyType|HardwareActual|HardwareMaximum|HardwareTransportResult|HardwareRequestResult|HardwareVerified|CacheSaveState|CacheCurrent|CacheMaximum|EarlyDeclarationFirstReason|EarlyDeclarationLastReason))" =/ { print }
        '
    echo
    echo '[DisplayBrightness kernel logs from the past 24 hours]'
    /usr/bin/log show --last 24h --style compact --info \
        --predicate '(process == "kernel" OR process == "kernelmanagerd") AND eventMessage CONTAINS[c] "DisplayBrightness"' ||
        echo 'Unable to read macOS logs. See the error above.'
    echo 'Missing DBBOOT entries may mean that logging was disabled, the kext entry did not run, or the system did not save the records.'
    echo
    echo '[GPU hang and reset log lines from the past 24 hours]'
    /usr/bin/log show --last 24h --style compact --info \
        --predicate '(eventMessage CONTAINS[c] "GPU hang" OR eventMessage CONTAINS[c] "GPU reset" OR eventMessage CONTAINS[c] "PPLIB IRI" OR eventMessage CONTAINS[c] "SW_IP_CLIENT_ID__BGM" OR eventMessage CONTAINS[c] "gfx reset" OR eventMessage CONTAINS[c] "GPU restart")' 2>/dev/null |
        /usr/bin/awk '/GPU hang|GPU reset|PPLIB IRI|SW_IP_CLIENT_ID__BGM|gfx reset|GPU restart/ { print }' |
        /usr/bin/tail -n 200 ||
        echo 'Unable to read GPU event logs.'
    echo
    echo '[Recent kernel panic summaries]'
    for panic in $(/bin/ls -t /Library/Logs/DiagnosticReports/Kernel-*.panic 2>/dev/null | /usr/bin/head -n 5); do
        printf '%s\n' "--- $panic"
        /usr/bin/grep -Eo 'panic\([^\"]{0,220}|Panicked task[^\"]{0,160}|Boot args:[^\"]{0,120}' "$panic" 2>/dev/null | /usr/bin/head -n 8
    done
    if [[ -n "$oc_file" ]]; then
        echo
        echo '[OpenCore entries for DisplayBrightness and Lilu]'
        /usr/bin/awk 'tolower($0) ~ /displaybrightness|lilu/ { print }' "$oc_file" ||
            echo 'Unable to read OpenCore log.'
    fi
} > "$report" 2>&1 || exit 1
printf '%s\n' "$report"
if "$show_finder"; then /usr/bin/open -R "$report"; fi
