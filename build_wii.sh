#!/usr/bin/env bash
set -euo pipefail
export DEVKITPRO=/opt/devkitpro
export DEVKITPPC=$DEVKITPRO/devkitPPC
bash tools/build_chainstub.sh
build_dir="${NZP_WII_BUILD_DIR:-build/wii}"
cmake -S . -B "$build_dir" -DCMAKE_TOOLCHAIN_FILE=$DEVKITPRO/cmake/Wii.cmake
cmake --build "$build_dir" --parallel 4
mkdir -p dist/apps/nzportable
cp "$build_dir/boot.dol" dist/apps/nzportable/boot.dol
python3 tools/pad_dol.py dist/apps/nzportable/boot.dol
cp platform/wii/meta.xml dist/apps/nzportable/
