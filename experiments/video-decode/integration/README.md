# Guarded browser integration candidate

This optional payload has passed boot self-test, browser decoder/fallback
selection and one owner-confirmed standby/video/touch cycle on the reference Trona.
It remains experimental rather than a general compatibility claim. It is not
included in the normal installer merely by building the base platform package.
Use the exact arch14 kernel, the pinned SCP firmware and the
[decoder proxy](../scp-proxy/README.md). No secure-firmware policy is changed.

The SCP driver is built with `scp-dependency.patch`; its symbol dependency pins
the proxy module for the entire SCP lifetime. Build the proxy first and supply
its `Module.symvers` to the SCP module build. The decoder common module already
depends on SCP. Unloading the worker while SCP remains loaded must be rejected.

The DT selects a separate `mediatek/mt8183/scp-trona-h264.img` file, leaving the
distribution's official firmware untouched. The patched image must hash to
`058d707c53ba0bc2c66ffd67b36f1a0fcbad3ab29e7489ad681af640f9d89616`.
The bridge is built locally from commit
`7e5d1dd49e37e9b99521d6ea63da59c6e1a2bfec` of
[shelterx/vaapi-v4l2-bridge-mt8183](https://github.com/shelterx/vaapi-v4l2-bridge-mt8183).
No bridge or firmware binary is redistributed here.

## Reproduce the artifacts

Start with a separate prepared arch14 kernel build as described in the parent
experiment, including its camera media sources and symbol files. In the cross
build container, run:

```sh
bash experiments/video-decode/integration/build-modules.sh \
  /build/separate/linux-6.18.56 /build/video-output
```

This builds the pinned module dependency and optional DTB without replacing
`Image.gz`. Build and patch the exact official SCP image with the proxy's
existing helpers. Build the unmodified pinned bridge on AArch64 with the libva,
libdrm, GBM, EGL and GLES development headers. Its shared library must export
`__vaDriverInit_1_20`, as in the tested manual build:

```sh
gcc -O3 -DNDEBUG -std=c11 -D_GNU_SOURCE \
  -DVA_DRIVER_INIT_FUNC_NAME=__vaDriverInit_1_20 -fPIC -shared src/*.c -I src \
  $(pkg-config --cflags --libs libva libva-drm libdrm gbm egl glesv2) \
  -pthread -o v4l2stateless_drv_video.so
```

Prepare the normal platform stage with the current source and the correct
per-device profile. Its boot hash must match the boot image built from the
qualified kernel plus this optional DTB. Then, on Linux with `modinfo`, FFmpeg
and Python installed, overlay the optional files before running `makepkg`:

```sh
python3 experiments/video-decode/integration/stage-payload.py \
  --platform /build/platform-stage --modules /build/video-output/modules \
  --firmware /build/scp-trona-h264.img --bridge /build/v4l2stateless_drv_video.so \
  --sample /build/selftest-480.mp4
```

The self-test clip must be a synthetic 60-frame, 640×480 H.264 stream. The helper
computes its software reference hash, validates firmware and module ABI/dependency,
and regenerates the platform manifest. Neither script installs or flashes.
The reference build reproduced the candidate DTB and the staged package manifest.
Normal per-device boot/partition checks remain mandatory when installing.

For a complete rollback, restore the matched previous boot **and** platform
package, disable `trona-video.service`, and restore the Chrome wrapper's original
`exec /opt/google/chrome/google-chrome $CHROME_USER_FLAGS "$@"` launch line.
Replacing only the boot image or removing only the wrapper is incomplete.

## Selection and fallback

`trona-video.service` performs a bounded hardware-only decode of a local synthetic
clip and checks its complete NV12 SHA256 before creating a root-owned readiness
file in `/run`. Failure leaves software playback available and does not prevent
the desktop from starting. No network or user media is part of the self-test.

The Chrome wrapper is repaired through `trona-update-check`, including after
package updates. The launcher checks the current boot ID, proxy state/errors
and running SCP before adding the H.264 VA-API environment and feature flag.
Other browser arguments and feature flags are preserved. Other codecs are not
enabled by this integration. Existing Chrome processes retain their old decoder
environment; fully exit and relaunch Chrome when changing the selection.

`TRONA_VIDEO=software google-chrome-stable` explicitly disables accelerated
decoding for a diagnostic launch. Creating `/etc/trona-video-disabled` and
restarting `trona-video.service` disables readiness across subsequent boots;
remove the file and restart the service to run the self-test again. Neither
operation force-unloads a decoder that may still be in use.

This protects new browser launches when the backend is unavailable. It is not
a claim that any possible kernel/firmware hang can be recovered inside an
already-playing browser process. The decoder remains an experimental port.

The platform standby helper reinitializes the board's FT8201 input device after
full sleep, before making the display available. USB-powered screen blanking
does not reset it. Recovery intent remains recorded if reset fails, and other
restoration steps still run. The camera sensor's system sleep callbacks balance
runtime power and restore an active stream rather than disabling already-off
supplies twice.

Validation and remaining limits are recorded in
[video decoding](../../../docs/video-decoding.md). Test results must distinguish
successful hardware decode from physical touch/unlock and browser acceptance.
