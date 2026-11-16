# Performance investigation and optional responsive CPU policy

Measured on the reference 32 GB Trona on 2026-11-16, running
`6.18.56-trona-arch12`, Hyprland 0.57.2 and Mesa 26.2.7. These results describe
one physical tablet, not a performance guarantee for other devices.

## Findings

- The port still uses the conservative 793–1248 MHz CPU table at a fixed
  800 mV on both clusters. Higher OPPs are intentionally disabled. Increasing
  the frequency requires a separate voltage/SRAM/thermal qualification, not
  simply changing a maximum-frequency setting.
- Hyprland and a fresh Google Chrome test profile both reported Mali-G72 MC3
  through Panfrost. Chrome compositing, rasterization and WebGL were enabled.
  Its hardware video-decoding profile list was empty. No GPU blocklist bypass,
  sandbox changes or browser-profile changes were applied.
- Display output still uses the boot framebuffer through simpledrm. A native
  MediaTek display pipeline is a separate potential improvement; no gain from
  replacing it has been measured yet.
- Desktop blur, shadows and animations were already disabled. Initially about
  1 GiB memory was available without swap use; disk space was not a bottleneck
  (about 17 GiB free). This is not a measurement of memory pressure with many
  browser tabs open.

## Measured governor comparison

Forty serial CPU bursts of two million xorshift iterations, separated by
100 ms sleeps, pinned to one core. Two alternating runs per governor:

| Workload | Existing schedutil | Default ondemand |
| --- | --- | --- |
| CPU4 burst median | 15.20 ms / 15.20 ms | 12.82 ms / 12.65 ms |
| CPU0 burst median | 15.47 ms / 15.35 ms | 13.16 ms / 13.28 ms |
| Warm foot launch to compositor openwindow event | 270.86 ms / 264.90 ms | 257.49 ms / 255.45 ms |

The CPU4 burst improvement is about 16–17% in elapsed time. The warm application
launch improvement was only about 4–5%; neither result is a desktop FPS number
or a promise of the same improvement for all apps. The first baseline foot
launch was 491 ms, so cold-cache effects are larger than this governor change.
The app comparison used four launches per run and does not measure the time
until all application content is ready or the normal launcher's overhead.

For reference, the performance governor produced 9.64–9.65 ms CPU4 bursts.
It was **not** retained because it requests maximum frequency continuously.
Lower ondemand thresholds did not provide a convincing additional improvement,
so the default parameters were retained (8 ms sampling, threshold 95 on this
kernel).

In separate 12-second idle samples, ondemand spent about 94% of the small
cluster's and over 99% of the large cluster's time at 793 MHz. Temperature was
about 36–40 C during these short checks. This is not a battery-life or sustained
thermal qualification. The CPU burst source is `tests/bench-cpu-burst.c`; build
it on Linux with `cc -O2 -Wall tests/bench-cpu-burst.c -o /tmp/cpu-burst` and run
`/tmp/cpu-burst 4` or `/tmp/cpu-burst 0`. It never changes a CPU setting itself.

## Optional installation on an already-working native Trona

This is deliberately opt-in and not yet the guided installer's default.
Run these commands **on the tablet**, from this source checkout:

```sh
sudo install -m755 tools/trona-responsive-cpu.py /usr/local/libexec/trona-responsive-cpu
sudo install -m644 examples/systemd/trona-responsive-cpu.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now trona-responsive-cpu.service
sudo /usr/local/libexec/trona-responsive-cpu status
```

The oneshot service selects ondemand after the existing Trona CPU policy has
loaded. It checks the board, driver, both frequency tables and hardware limits
before making changes. It never writes voltage, min/max frequency, cooling or
CPU-online controls. Original governors are saved privately under `/run` and
restored when the service stops. A later explicit governor change by another
tool is preserved. No polling daemon is added and no boot image is modified.

To revert immediately and on future boots:

```sh
sudo systemctl disable --now trona-responsive-cpu.service
sudo /usr/local/libexec/trona-responsive-cpu status
```

The reference tablet passed service stop/start restoration, a CPU4–7 offline /
online round trip retaining ondemand, idle downclocking, unchanged frequency
limits and `trona-update-check check`. All eight CPUs were online afterward.
Four isolated unit tests cover wrong-board/table rejection, partial-change
rollback, restoration and preservation of subsequent external changes.
A full reboot, complete suspend/resume cycle and long-term battery comparison
with this new policy have not yet been repeated. Subjective app switching is
a separate user acceptance check.

## Next substantial work

1. Qualify normal CPU voltage/frequency scaling above the provisional 1.248 GHz
   ceiling, including SRAM tracking, both clusters, temperature, suspend and
   repeated cold boots. This is the most relevant next investigation for app
   startup and CPU-heavy web apps.
2. Profile native display scanout versus the present simpledrm path before
   undertaking the display-driver port.
3. Investigate the MediaTek video decoder and browser integration separately
   for YouTube/video playback.

The standard governor and policy lifecycle are documented in the
[Linux CPU performance-scaling documentation](https://docs.kernel.org/admin-guide/pm/cpufreq.html).
The browser backend can be checked against the
[Chromium Ozone documentation](https://chromium.googlesource.com/chromium/src/+/main/docs/ozone_overview.md).
