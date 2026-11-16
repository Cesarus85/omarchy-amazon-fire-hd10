# Next work on the reference tablet

Priorities agreed on 2026-11-16. These are planned work, not completed features
or a promise of support on other Fire models.

## Investigated: video decoding; hardware path still blocked

Measure the browser playback path and investigate the MT8183 hardware decoder.
Panfrost compositing alone does not establish hardware video decoding. Require
an operational decoder, actual decoded frames and a measured playback comparison
before enabling a browser acceleration path by default. Preserve the working
boot, camera, CPU DVFS, standby and wake behavior. The first kernel/firmware
bring-up reached the decoder but failed on every frame; the working system was
restored. See [the measurements and next driver investigation](video-decoding.md).

## Later: further performance optimization

- Measure app cold/warm starts and switching under realistic memory pressure.
- Investigate storage, compositor/frame-copy cost and browser resource use,
  selecting changes from measurements rather than increasing clocks again.
- Compare frame latency, memory use, temperature and battery drain; retain
  existing zram/memory protection and thermal controls.

## Later: further tablet usability

- Exercise app dialogs, window sizing and keyboard show/hide in all orientations.
- Check touch scrolling, focus, text selection, clipboard, emojis and Super
  shortcuts across native apps and Chromium web apps.
- Check bar/dock overlap and usable touch targets with different running apps.
- Repeat lock/unlock, rotation and standby/wake during normal use.

## Qualification still outstanding

Long-duration battery and stability measurements, additional USB-C chargers,
headphone-jack/Bluetooth-audio testing, and a clean installation on a second
compatible tablet remain separate acceptance tasks. Firmware deep sleep,
USB-A charging and further microphone work are deferred by the owner.
