#!/bin/bash
# SPDX-License-Identifier: MIT
# Produces host files only. Does not contact or write a tablet.
set -euo pipefail
src=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo=$(cd "$src/../../.." && pwd)
build=${BUILD_DIR:-$repo/build/scp-proxy}
ld=${ARM_LD:-arm-none-eabi-ld}
objcopy=${ARM_OBJCOPY:-arm-none-eabi-objcopy}
mkdir -p "$build"
clang --target=arm-none-eabi -mcpu=cortex-m4 -mthumb -mfloat-abi=soft \
 -ffreestanding -fno-builtin -O2 -c "$src/firmware/proxy.c" -o "$build/proxy.o"
for name in entry hook; do
 clang --target=arm-none-eabi -mcpu=cortex-m4 -mthumb \
  -c "$src/firmware/$name.S" -o "$build/$name.o"
done
"$ld" -m armelf -T "$src/firmware/proxy.ld" "$build/entry.o" "$build/proxy.o" -o "$build/proxy.elf"
"$objcopy" -O binary "$build/proxy.elf" "$build/proxy.bin"
"$ld" -m armelf -T "$src/firmware/hook.ld" "$build/hook.o" -o "$build/hook.elf"
"$objcopy" -O binary "$build/hook.elf" "$build/hook.bin"
printf 'Helper artifacts: %s\n' "$build"
