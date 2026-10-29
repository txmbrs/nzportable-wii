#!/usr/bin/env bash
# Dependency compilation only. No programs or tests are built or run.
set -euo pipefail
cd "$(dirname "$0")"
if [ ! -d dependencies/mbedtls-3.6.7 ]; then
    tar -xjf dependencies/mbedtls-3.6.7.tar.bz2 -C dependencies
fi
cmake -S dependencies/mbedtls-3.6.7 -B ../build/wii-mbedtls \
    -DCMAKE_TOOLCHAIN_FILE="$PWD/wii-toolchain.cmake" \
    -DCMAKE_BUILD_TYPE=Release -DENABLE_PROGRAMS=OFF -DENABLE_TESTING=OFF \
    -DUSE_SHARED_MBEDTLS_LIBRARY=OFF -DUSE_STATIC_MBEDTLS_LIBRARY=ON \
    -DMBEDTLS_CONFIG_FILE="$PWD/mbedtls_wii_config.h"
cmake --build ../build/wii-mbedtls --parallel 4
