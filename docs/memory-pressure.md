# Keeping the desktop responsive under memory pressure

## Observed problem

On 2026-11-16 the owner reproduced a previously observed freeze/delayed-touch
problem while opening desktop applications. During launches of Electron-based
apps, load average exceeded 20 and used compressed swap rose from essentially
zero to approximately 950 MiB of the available 1024 MiB. The compositor and
Quickshell shared `wayland-wm@Hyprland.service`; that cgroup had approximately
350 MiB swapped out. The input log reported event-processing delays around
160 ms. There was no recorded OOM kill or GPU reset, and a snapshot of the
sensor showed zero stuck contacts after release.

This establishes desktop memory eviction and delayed input processing as
concrete problems. It does **not** prove that every reported incorrect touch
or freeze has the same cause. A clean sensor snapshot does not rule out an
intermittent input fault. CPU governor changes alone do not address this issue;
the original schedutil governor was temporarily restored to isolate testing.

## Applied mitigation on the reference device

The following settings protect up to 512 MiB of the desktop session's working
set from normal reclamation, limit its *new* swap allocation to 64 MiB, and give
the interactive session twice the CPU weight of the application slice when
both compete. MemoryLow is a best-effort protection, not an allocation or a
hard reservation. The same budget must be propagated through its ancestors;
these repeated 512 MiB values do not reserve five times that amount of RAM.

These are persistent systemd resource-control overrides, independent of the
Trona platform package. Apply only on the native port with user `trona` at UID
1000 and the named Hyprland service. Inspect existing values first if you have
custom resource controls.

As root:

```sh
sudo systemctl set-property user.slice MemoryLow=512M
sudo systemctl set-property user-1000.slice MemoryLow=512M
sudo systemctl set-property user@1000.service MemoryLow=512M
```

In **trona's normal desktop terminal**:

```sh
systemctl --user set-property session.slice MemoryLow=512M CPUWeight=200
systemctl --user set-property wayland-wm@Hyprland.service MemoryLow=512M MemorySwapMax=64M
```

A lowered swap limit does not immediately pull already-swapped pages into RAM.
Expect `memory.swap.current` to remain above the new ceiling until those pages
are accessed or released. Do not force a swapoff during a memory-pressure event.
These controls leave applications running and do not install an automatic
application killer. They cannot make unlimited concurrent apps fit in 3 GiB.

## Extra compressed reserve

The reference device already has a 1 GiB LZ4 zram0 override. A second 1 GiB
LZ4 device was added at lower swap priority, without resetting the existing
live swap. Memory is allocated as pages are stored, not as an immediate 1 GiB
reservation; incompressible data can still exhaust physical memory.

On that layout, with no existing zram1 or conflicting configuration:

```sh
sudo install -m644 examples/zram/trona-headroom.conf /etc/systemd/zram-generator.conf.d/
sudo systemctl daemon-reload
sudo systemctl start dev-zram1.swap
swapon --show
```

zram-generator creates the additional device and its boot dependencies. There
is no internal-flash swap file, partition change or storage reformatting.
Both 1 GiB devices were confirmed active on the reference tablet. The generator
configuration has not yet received a separate cold-boot acceptance test.

## Verification and rollback

Read back `memory.low` along the cgroup hierarchy and `memory.swap.max` on the
compositor cgroup. Check `memory.events`, `memory.swap.current`, `/proc/vmstat`,
`swapon --show`, service failures and compositor input-delay messages while
repeating the problematic launches. Human touch/keyboard acceptance is separate
from successful configuration and must not be inferred from it.

The reference's previous values were MemoryLow=0 at all five levels,
CPUWeight=100 on session.slice and unlimited desktop swap. To restore those
values, repeat the commands above with `MemoryLow=0`, `CPUWeight=100` and
`MemorySwapMax=infinity`. If your previous settings differed, restore your own.

To remove the additional swap on a later boot, remove only
`/etc/systemd/zram-generator.conf.d/trona-headroom.conf` and reboot when ready.
Do not remove the original Trona zram0 override, and do not reset a device that
still contains swapped pages.

Primary references:
[systemd resource control](https://www.freedesktop.org/software/systemd/man/latest/systemd.resource-control.html)
and [zram-generator configuration](https://github.com/systemd/zram-generator/blob/main/zram-generator.conf.example).

## Repeat-test result

The owner repeated application launches/switching, menu and on-screen keyboard
use and explicitly confirmed that operation remained stable. Over an 80-second
parallel sample, the raw touch stream contained 42 contacts and 42 releases,
with no SYN_DROPPED event. Maximum delivery delay to the diagnostic reader was
2.57 ms; this is not compositor latency or touch-to-photon latency. Available
RAM stayed at least about 817 MiB at the sampled points, and the extra swap
reserve was being used. No test input coordinates or typed text were retained.

After this isolated memory test, the previously accepted ondemand CPU policy
was re-enabled. Long-term mixed-app use, battery impact and a fresh cold boot
of the combined settings remain separate qualification items.
