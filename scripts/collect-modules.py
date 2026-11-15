#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
from pathlib import Path
import shutil,sys
b=Path(sys.argv[1]); k=b/'linux-6.18.56'; dest=b/'artifacts/modules';dest.mkdir(exist_ok=True)
wanted='stk8ba53 hi556 mc mtk-camsv mtk-mipi-csi-trona mtk-seninf ov02b10-trona phy-mtk-mipi-csi-0-5 udmabuf v4l2-async v4l2-fwnode videobuf2-common videobuf2-dma-contig videobuf2-memops videobuf2-v4l2 videodev exfat nls_utf8 mt6779-keypad trona_audio trona_battery trona_btif trona_cpufreq_policy trona_hci trona_poweroff trona_wlan_adaptor wlan_drv_gen3 wmt_drv'.split()
for name in wanted:
 paths=list((b/'external').rglob(name+'.ko')) or list(k.rglob(name+'.ko'))
 if len(paths)!=1:raise SystemExit(f'Expected exactly one {name}.ko, found {paths}')
 shutil.copy2(paths[0],dest/paths[0].name)
print(f'Collected {len(wanted)} modules')
