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

`trona-fast-apps` validates its shell source files before writing any of them, refuses
changed upstream anchors, and is idempotent. The platform update repair hook
reapplies it and verifies the integration markers. The normal guarded package
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

## Chrome and web-app launches on November 17

Platform `2026.11.17-11` enables Omarchy's existing Chrome process handoff
with the tablet's two startup flags, `--no-first-run` and
`--force-renderer-accessibility=form-controls`. The latter supports the
automatic on-screen keyboard. The guard allows these exact settings for
Google Chrome. The existing profile selection, socket ownership, and browser
acknowledgement checks still apply. A stopped browser takes the normal launch
path. The platform update hook reapplies the change.

On the reference tablet, a running Chrome opened a new window on a local HTML
page in a median **510 ms** through the installed helper versus **967 ms**
through the previous helper. Four runs per route alternated within an isolated
profile. Timing ends when Hyprland reports the expected page title. The saved
test profile also opened in about **2 s** with its browser process stopped.
The normal Omarchy web-app command opened the local page successfully.
These measurements exclude network content and do not time display scanout.

A separate 16-switch test measured **1.5 ms** median from the compositor
focus request to the focus event. For ordinary desktop entries, a trial
of GIO inside the existing UWSM launch scope reduced median time to the first
mapped terminal window from 458 to 334 ms. The general launcher remains on
gtk-launch pending checks of desktop-entry selection and foreground activation.

An optional Omarchy Bluetooth agent retried every two seconds when its binary
was missing. A systemd unit condition skips this agent until the executable is
available. On the tablet the restart counter stopped at 723, with bluetoothd
and the Blueman applet still active.

## Dock and Apps grid on November 22

Platform `2026.11.22-2` uses a small GIO/GDK launcher for graphical desktop
entries. Dock and Apps grid pass the full desktop ID as one argument inside
the existing UWSM application scope. GLib resolves the entry and handles
D-Bus activation or its Exec line. The GDK launch context carries display
startup information. Terminal-only entries keep Omarchy's TUI launcher.

| App | Previous gtk-launch | Installed launch path |
|---|---:|---:|
| Files (Nautilus) | 488 ms | 353 ms |
| Foot via a desktop entry with spaces in its ID and display name | 402 ms | 341 ms |

Each median covers six starts, three per surface. Measurements ran on the
reference tablet with a competing window in front. Timing starts in the
shared launcher and ends at Hyprland's window-open event. The first Files
sample included application startup (1639 ms on the previous route). These
figures characterize this test sequence and do not estimate cold-start
savings. All 24 starts brought the intended window to the active workspace;
it retained focus at the second check half a second later.
[Individual samples](evidence/app-launch-2026-11-22.json)

In the installed package, a second test used different names containing
spaces for its desktop file and visible label. The grid launched it, then
a simulated touch on the Files dock icon opened Nautilus in front of it.
Returning to an already-open, maximized Files window exposed a separate
ordering issue with a floating window. The Dock now raises the selected
window before assigning focus. Three consecutive touch selections focused
the same Files window according to Hyprland. The scanout measurement below
checks what was visible on the panel path.

The package transaction verified the platform and the shell was restarted
without rebooting the tablet. The update hook reapplies both launch and
focus integration. The installed package passed its 221-entry integrity
check. Temporary test desktop entries and shell instrumentation were removed.

## Switching between open apps from the Dock

On the Fire HD 10, switching from a floating Foot terminal to maximized
Files (Nautilus) assigned keyboard focus but left the terminal covering
Files in all eight baseline trials. It was still covering Files at the
1.5-second timeout. After each timeout we exposed the reference window
again before testing the reverse direction. Version 2026.11.22-3 lowers
eligible workspace-managed floating windows when selecting a maximized
tiled app on the tablet, then assigns focus in the same Hyprland dispatch.
All 16 switches with the fix passed in the comparison session.

The following medians come from a second run with the installed Dock and
its normal render settings. Each row includes eight switches. Both apps
were already open.

| Target | Release to focus | Release to all 528 marker pixels | Release to full pixel match | Touch-down to full pixel match |
| --- | ---: | ---: | ---: | ---: |
| Foot terminal | 5.2 ms | 42.7 ms | 94.2 ms | 174.3 ms |
| Files | 5.4 ms | 34.0 ms | 124.6 ms | 204.7 ms |

A virtual touchscreen tapped the actual Dock icons with an 80 ms hold.
We captured an unobscured reference for each app, sampled changed pixels
across the client including the last differing scanline, then compared
the complete 1164 × 1730 pixel client area against that reference in the
display scanout buffer. A mismatch triggered another read. Full verification
times include readback and are upper bounds for the complete digital image.
Optical LCD response was outside the measurement.

Hyprland's alternative render scheduling passed all 16 switches, but the
median full verification took 122 ms versus 112 ms with normal scheduling.
The tablet retains normal scheduling. The clearest improvement here is the
reliable transition to a previously obscured app.

[Individual timing samples](evidence/app-switch-2026-11-22.json)
