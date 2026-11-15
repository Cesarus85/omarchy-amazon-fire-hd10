# Sources and credits

This repository combines new Trona integration with adaptations of substantial
existing work. Copyright and SPDX notices in copied files remain authoritative.

| Component | Source / revision | Use here |
|---|---|---|
| Linux | [kernel.org Linux 6.18.56](https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-6.18.56.tar.xz), SHA256 4633bbda2ab95eaf2a7af4041236451d6759c72e6946265531af4b3aacefbf89 | Core patch, standard driver frameworks and adapted sensor/PHY drivers |
| Amazon MT8183 kernel | [amazon-mt8183-devs/android_kernel_amazon_mt8183](https://github.com/amazon-mt8183-devs/android_kernel_amazon_mt8183), 7ec747b431c61293a00ed6a01f2a01fa50c6afde | Board wiring, GPIO/power sequences, sensor tables and hardware definitions |
| Amazon GPL release | [vendor-mtk-sources/Amazon-Fire_HD10_11th_Gen](https://github.com/vendor-mtk-sources/Amazon-Fire_HD10_11th_Gen), a7bf7858ae80a6986a3c63f0186610b1d6478c4f | WMT, BTIF and WLAN sources |
| Motorola connectivity common | [MotorolaMobilityLLC/vendor-mediatek-kernel_modules-connectivity-common](https://github.com/MotorolaMobilityLLC/vendor-mediatek-kernel_modules-connectivity-common), 038e72f831e7af86b23d6e0ab9bdeb894f7caf21 | Missing GPL build/debug support files |
| Motorola WLAN gen3 | [MotorolaMobilityLLC/vendor-mediatek-kernel_modules-connectivity-wlan-core-gen3](https://github.com/MotorolaMobilityLLC/vendor-mediatek-kernel_modules-connectivity-wlan-core-gen3), 2c22688f6a5e27b766a21eb630e2b68bd80434f2 | Missing GPL build/header/qosmap files |
| TECHO5 | [HuskerMinion/techo5](https://github.com/HuskerMinion/techo5), 75538f1fa03d94fce4606afe00d21810efb1d30e | MT8183 baseline, USB and camera pipeline porting references |
| MT8183 cameras | [vpavlin/mt8183-camera](https://github.com/vpavlin/mt8183-camera), c00ad163b8fd65ce7a8f1bf241587923b1a90ead | Camera pipeline and bring-up reference |
| MediaTek ISP 3.0 | BayLibre MT8365 v8 work, MediaTek SENINF/CAMSV | Imported through the TECHO5 camera sources; retained MediaTek/BayLibre headers |
| Omarchy | [omacom/omarchy](https://github.com/omacom/omarchy), runtime 4.5.0-3 | Desktop, shell and patch targets; retained MIT notice |
| Hyprland | [hyprwm/Hyprland](https://github.com/hyprwm/Hyprland), runtime 0.57.2 | Wayland compositor; no vendored full compositor |
| Quickshell | [quickshell-mirror/quickshell](https://github.com/quickshell-mirror/quickshell), runtime 0.3.2.r7.g2b4501a-1 | Real session-lock surfaces and PAM integration |
| Mesa/Panfrost | [Mesa](https://docs.mesa3d.org/drivers/panfrost.html) | GPU rendering; no vendored Mesa binaries |
| Arch Linux ARM | [archlinuxarm.org](https://archlinuxarm.org/) | Root filesystem and packages |
| wvkbd | [proycon/wvkbd](https://git.sr.ht/~proycon/wvkbd), v0.16 | Touch keyboard plus local Super/Alt layout patch |
| Android mkbootimg | Android Open Source Project; file retains 2015 Apache-2.0 header | Boot container packing |
| amonet-koboreru | [R0rt1z2/amonet-koboreru](https://github.com/R0rt1z2/amonet-koboreru) | Existing unlock/recovery prerequisite; no code copied or modified here |

Camera upstream authors include Louis Kuo and Phi-bang Nguyen (MediaTek), and
Julien Stephan, Florian Sylvestre, Paul Elder and Laurent Pinchart in the
BayLibre camera work. The HI556 driver retains Intel's copyright. The
accelerometer adapts the Linux STK8BA50 driver, retaining Intel's copyright.
See each file for its exact authors and licensing.

The reference hardware was configured and physically tested by Stefan Maier.
Porting, scripts and documentation were developed with OpenAI Codex assistance.
This source publication is not an upstream endorsement or a claim of sole
authorship. Please submit issues about this adaptation here; do not send
automatically generated reports to upstream projects.
