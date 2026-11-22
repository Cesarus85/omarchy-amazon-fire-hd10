#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-only
# Run natively in an AArch64 Arch build environment with the listed dependencies.
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
out=${1:?Usage: build.sh EMPTY_OUTPUT_DIRECTORY}
[[ ! -e $out ]] || { echo 'Output already exists' >&2; exit 1; }
git clone --branch v0.16 --depth 1 https://git.sr.ht/~proycon/wvkbd "$out"
cd "$out"
patch --batch -p1 < "$here/wvkbd-trona.patch"
patch --batch -p1 < "$here/wvkbd-ime-keymap.patch"
patch --batch -p1 < "$here/wvkbd-symbols.patch"
patch --batch -p1 < "$here/wvkbd-text-edit.patch"
make -j"${JOBS:-4}" LAYOUT=mobintl
printf 'Built %s/wvkbd-mobintl; not installed.\n' "$out"
