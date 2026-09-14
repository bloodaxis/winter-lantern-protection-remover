#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")"
: "${ZIG:=zig}"
mkdir -p build
for file in hook buffer trampoline hde/hde64; do
    "$ZIG" cc -target x86_64-windows-gnu -O2 -c -Iminhook/include \
        "minhook/src/$file.c" -o "build/${file##*/}.o"
done
"$ZIG" c++ -target x86_64-windows-gnu -std=c++17 -O2 -s -shared \
    -DWIN32_LEAN_AND_MEAN -DNOMINMAX -Iminhook/include \
    runtime.cpp build/{hook,buffer,trampoline,hde64}.o \
    -o build/winter-lantern-protection-remover.dll
