# Full CPU and CCI voltage/frequency scaling

The `arch14` platform replaces the provisional fixed-800-mV, 1.248-GHz CPU
setup with the MT8183's 16 nominal operating points, 793–1989 MHz on both
clusters. 1989 MHz is the kernel's nominal 2 GHz point, not an overclock.

## Implementation

- Enable the upstream MediaTek CCI devfreq driver. Its passive governor follows
  CPU demand, including the required-OPP links and shared Vproc12 rail votes.
- Use the upstream MT8183 CPU and CCI voltage tables, with a conservative
  700 mV floor for the two lowest A53/CCI points. Maximum Vproc is 1.05 V.
- Keep MT6358 hardware SRAM tracking, as in the upstream MT8183 Kukui wiring.
  Do not add a competing software `sram-supply` consumer. Before publishing
  the regulators, the Trona-only probe validates both bootloader tracking
  configurations and reads back the updated upper clamp. SRAM follows Vproc
  +100 mV within 900–1150 mV. The old bootloader clamp was 1118.75 mV.
- Leave the flat-voltage energy-model workaround inactive on the full-DVFS
  device tree. The optional ondemand helper accepts either exact qualified
  table and continues to reject unknown frequencies.
- Start thermal power allocation at 60 C, target 70 C and use a 2500 mW
  sustainable-power estimate. Existing CPU/GPU cooling maps and critical
  protection remain enabled. These are die temperatures, not case readings.
- Preserve the existing s2idle path and CPU hotplug restoration. Firmware
  deep sleep is not enabled. Modules and update checks use the arch14 ABI.

The peak frequency is a ceiling, not a sustained eight-core performance
promise. A passive tablet must reduce clocks under prolonged load. Raising
minimum frequency or disabling cooling defeats this design.

## Qualification

Hardware qualification is recorded separately from source/build checks. The
reference tablet is the 32 GB Fire HD 10 (2021), Trona, with 3 GB RAM.

The test boot starts with the powersave governor and applies a 1625 MHz cap
before systemd. Only after that stage passes is 1989 MHz tested. The previous
boot image and module tree remain available during qualification; root,
partition layout, recovery and firmware partitions are not modified.

`tests/verify-cpu.c` runs eight pinned workers with independently precomputed
integer and exact floating-point results and memory readback. It deliberately
saturates all CPU cores: application responsiveness during this test is not
representative. Build it with `cc -O2 tests/verify-cpu.c -o verify-cpu`; an
argument selects the duration in seconds. Run only while monitoring thermal
state. Clock changes use cpufreq and regulator drivers, never raw PLL/PMIC writes.

The qualification includes constant load, repeated low/high clock requests,
CPU hotplug, reboot, and a physical Power-key suspend/resume check. It is not
a multi-day soak test or qualification of another tablet revision.

### Reference-device results, 2026-11-16

- 1625 MHz: 90 seconds / eight workers, no verification failures; maximum
  sampled die temperature 70.75 C with the revised thermal policy.
- 1989 MHz: 90 seconds / eight workers, no verification failures; both clusters
  observed at 1989 MHz, maximum sampled temperature 72.375 C.
- Low/high requests every approximately 0.5 seconds for 60 seconds: no failures,
  both clusters reached 1989 MHz; maximum sampled temperature 72 C.
- Three CPU hotplug cycles restored all eight cores and the ondemand governor.
- The initial generic 80 C thermal target was not retained: the first stress
  attempt was stopped at the test's conservative 75 C cutoff. Revised tests
  used a 78 C abort threshold, which was not reached.
- Two alternating short-burst comparisons on CPU4: 1248-MHz cap medians
  12.871 / 13.288 ms; 1989-MHz cap 11.877 / 11.8165 ms (about 8–11% shorter).
  This isolates the cap on the same new kernel and is not an application-start
  or whole-system performance measurement.
- A real guarded package upgrade to `trona-platform 2026.11.16-1` completed;
  boot/recovery readback, all module ABIs, platform-file integrity and package
  hooks passed. The final boot uses the ordinary ondemand user policy with
  both ceilings at 1989 MHz and the thermal policy in the device tree.

Constant eight-core load can make the desktop temporarily slow because it
competes for execution time and consumes the shared thermal budget. The
qualification workload is stopped after each test and is not a background
service on the installed tablet.

The physical USB-disconnected Power-key suspend/resume check for arch14 is
still awaiting the owner response. Earlier standby acceptance was on arch12;
reboot and hotplug tests alone do not close this remaining gate.
