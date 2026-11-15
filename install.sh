#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-only
set -eu
cd "$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)"
exec python3 installer/trona-installer.py "$@"
