# App opening, switching and desktop memory — 2026-11-16

On the reference 3 GiB Trona, platform `2026.11.16-2` adds three measured
optimizations on top of arch14 CPU DVFS. Tests used normal app/window activity,
without an artificial CPU stress load. The owner confirmed smoother operation,
working apps/menu and the on-screen keyboard.

| Measurement | Before | After |
|---|---:|---:|
| Dock focus request to compositor focus event, median of 16 switches | 84.58 ms | 3.99 ms |
| Quickshell RSS after the same menu/window sequence | 457.7 MiB | 304.8 MiB |
| Quickshell proportional memory (PSS), same point | 399.3 MiB | 246.5 MiB |
| Warm Apps-grid readiness, median excluding first open | 77 ms | 87 ms |

The focus measurement ends at the compositor event: it does not measure screen
scanout or touch-to-photon latency. The grid did **not** demonstrate a clear
opening-time improvement; its change avoids redundant work after opening.
The memory saving was approximately 153 MiB in these short comparisons. Caches
can grow with use, and this is not a long-term memory-leak or battery study.
Full samples are in the [measurement record](evidence/app-responsiveness-2026-11-16.json).

## Changes

### Launch only the required processes

AppLibrary launches UWSM and gtk-launch with an argument vector, inheriting the
established GUI environment. It avoids starting `bash -lc` first. Terminal
entries still go through Omarchy's TUI launcher. UWSM application scopes,
desktop-entry resolution, argument boundaries and session memory protection
are retained. Other shell commands and user shell configuration are untouched.

The isolated terminal-launch comparison measured these medians twice:

| Path to first mapped window | Run 1 | Run 2 |
|---|---:|---:|
| Direct foot process (diagnostic reference) | 149.07 ms | 161.60 ms |
| UWSM + foot | 247.03 ms | 254.34 ms |
| Login shell + UWSM + foot | 290.25 ms | 315.38 ms |

The retained UWSM path removes roughly 43–61 ms of overhead in that comparison.
This is not a claim that a browser's network content loads in these times.
The plain-shell comparison was 14.875 ms versus 62.18 ms for a login shell.

### Focus windows through the compositor connection

The common, non-minimized Dock path uses Quickshell's existing
[Hyprland dispatch API](https://quickshell.org/docs/v0.3.1/types/Quickshell.Hyprland/Hyprland/)
for focus and stacking. It avoids the login shell and two hyprctl processes.
The existing minimized-window restore/animation sequence is retained.

### Reuse icons and avoid inflating sparse GUI heaps

Opening the grid previously scanned 6901 icon paths (about 198 ms for the
filesystem traversal alone), then reassigned the icon map even if unchanged.
The patch reuses a recent index for up to 60 seconds; desktop-entry changes
still schedule a scan immediately through the existing debounce. A successful
unchanged scan no longer invalidates every icon binding. Failed scans do not
replace the existing map.

`/etc/tmpfiles.d/trona-memory.conf` selects `madvise` for transparent hugepages
at boot. Programs can still explicitly request large pages; sparse GUI heaps
no longer receive them indiscriminately. See the
[kernel THP documentation](https://www.kernel.org/doc/html/v6.12/admin-guide/mm/transhuge.html).
Defragmentation policy, swap settings, CPU governor and memory protection are
not changed by this package. A separate madvise-only run reproduced the memory
reduction while the old focus path remained about 85 ms, isolating the two gains.

## Persistence and checks

`trona-fast-apps` validates both source files before writing either, refuses
changed upstream anchors, and is idempotent. The platform update repair hook
reapplies it and verifies both integration markers. The normal guarded package
transaction retains the same arch14 kernel/module ABI. Portable tests cover
argument-vector scope preservation, the non-minimized focus branch and refusal
to partially patch when the second upstream file is incompatible.

Temporary QML timing logs and the test-only IPC method are removed from the
installed production files. The desktop shell is restarted through the official
helper, which refuses to interrupt a secure lock. No browser profile, saved tab,
user document or application installation is removed by this optimization.

The guarded upgrade and production-shell restart passed; `pacman -Qkk` reports
181 package entries and zero altered files. The tmpfiles rule was applied and
read back as `madvise`. A separate full tablet reboot was not performed in this
app-responsiveness pass.
