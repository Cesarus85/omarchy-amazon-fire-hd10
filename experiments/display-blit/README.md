# Opaque framebuffer copy on Fire HD 10 (2021, Trona)

The bootloader leaves OVL0 layer 0 scanning a 1200 × 1920 framebuffer with a 4864-byte row stride. The live layer configuration was 0x21ff, with alpha processing enabled. The device tree specifies ARGB8888 and the primary DRM plane accepts XRGB8888. The patch writes alpha 255 while copying each damaged rectangle directly from the shadow buffer to the write-combined framebuffer. It retains DRM synchronization, per-buffer pitches and clipping.

Two runs per mode in A/B/B/A order used the same kernel, foreground Chrome window and four synthetic local tabs. Each run scrolled the same document twice at each size. At 99 on-CPU samples per second per busy core, the display-update stacks cost the equivalent of 46–49% of one core for the larger area on the generic path and about 18% on the direct path. For the smaller area the estimates were 14–16% and 6–7%. Across four five-second large-area scrolls per mode, 41 browser animation callback intervals exceeded 25 ms with the generic copy and zero did with the direct copy. Panel presentation latency and battery life still require separate measurements. [Per-run results](../../docs/evidence/display-blit-2026-11-17.json)

## Build and compare

Apply this optional patch on top of the prepared ARM64 Trona 6.18.56 arch14 tree before rebuilding Image.gz. Use the same device tree, ramdisk and guarded boot packaging as the running installation. Compare the rebuilt Module.symvers with that of the existing tree. Each tablet needs its own matching boot hash in the platform package.

    patch -p1 < /path/to/experiments/display-blit/trona-opaque-blit.patch
    make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- Image.gz

The root-only parameter starts disabled. To try the direct copy and then switch back for comparison on the updated kernel:

    sudo sh -c 'echo Y > /sys/module/drm_sysfb_helper/parameters/trona_opaque_blit'
    sudo sh -c 'echo N > /sys/module/drm_sysfb_helper/parameters/trona_opaque_blit'

The reference tablet's platform package includes the adjacent tmpfiles rule under /etc/tmpfiles.d and records it in the integrity manifest. That enables the direct path at boot. For a full rollback use the previous matched boot image and platform package. The standard installer continues to build its existing configuration.

The on-device reference test compared 4,800 clipping and alignment cases byte for byte, including row padding. After a second boot the parameter read Y, the guarded platform check passed, all 206 package files passed verification and the H.264 decoder self-test produced the reference frame hash.
