#!/usr/bin/env bash
# Compile only. This script never launches FTE, Dolphin, or tests.
set -euo pipefail
export DEVKITPRO=/opt/devkitpro
export DEVKITPPC=$DEVKITPRO/devkitPPC
export PATH="$DEVKITPPC/bin:$PATH"
bash "$(dirname "$0")/build_tls.sh"
bash "$(dirname "$0")/../tools/build_chainstub.sh"
python3 "$(dirname "$0")/prepare_nettrace.py"
cd "$(dirname "$0")/fteqw-master/engine"
# Custom Make objects do not track the included shared loader/header reliably.
touch client/wii_online_chainload.c
touch client/wii_online_network.c
# mbedTLS config toggles can change public context sizes. Custom FTE objects
# do not track these external headers, so rebuild all consumers together.
touch client/wii_online_tls.c client/wii_online_dtls_mbedtls.c client/wii_online_rng.c
flags='-mrvl -mcpu=750 -meabi -mhard-float -DGEKKO -DHW_RVL -DPLATFORM_WII -I/opt/devkitpro/libogc/include -I/opt/devkitpro/portlibs/wii/include -I/opt/devkitpro/portlibs/ppc/include -I/src/fte-online/dependencies/mbedtls-3.6.7/include -DMBEDTLS_CONFIG_FILE=\"/src/fte-online/mbedtls_wii_config.h\" -ffunction-sections -fdata-sections -Wno-error=implicit-function-declaration -Wno-error=incompatible-pointer-types'
make -j4 glcl-rel FTE_TARGET=SDL2 FTE_FULLTARGET=wii FTE_CONFIG=wii \
    CC=powerpc-eabi-gcc CXX=powerpc-eabi-g++ STRIP=SKIP \
    SDLCONFIG="$PWD/../../sdl2-wii-config.sh" \
    CFLAGS="$flags -I/opt/devkitpro/libogc/include/ogc -I/opt/devkitpro/libogc/include/ogc/machine" LTO= PRECOMPHEADERS= \
    RELEASE_CFLAGS=-O2 CPUOPTIMIZATIONS= \
    BASELDFLAGS='-L/opt/devkitpro/libogc/lib/wii -L/opt/devkitpro/portlibs/wii/lib -L/opt/devkitpro/portlibs/ppc/lib -Wl,--gc-sections' \
    IMAGELDFLAGS= OGGVORBISLDFLAGS= CLIENTLDDEPS='/src/build/wii-mbedtls/library/libmbedtls.a /src/build/wii-mbedtls/library/libmbedx509.a /src/build/wii-mbedtls/library/libmbedcrypto.a -lpng -lz -lm' \
    GLLDFLAGS='-Wl,--wrap=SDL_main,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=free,--wrap=memalign' \
    CUSTOMOBJS="wii_online_platform.o wii_online_input.o wii_online_rng.o wii_online_tls.o wii_online_dtls_mbedtls.o wii_online_chainload.o wii_online_network.o wii_online_nettrace.o wii_online_gl_sync.o" PKGCONFIG=false NATIVE_PLUGINS=

# Keep the in-progress module outside the released solo distribution.
python3 - <<'PY'
import subprocess
lines=subprocess.check_output(['/opt/devkitpro/devkitPPC/bin/powerpc-eabi-nm','release/wiicl-glwii'],text=True).splitlines()
symbols={p[2]:p[1] for line in lines if len(p:=line.split())==3}
for name in ('ogx_get_proc_address','ogx_initialize','glGetString'):
    if symbols.get(name)!='T':
        raise SystemExit(f'Wrong OpenGX link: {name} is {symbols.get(name)}, expected a strong implementation (T).')
print('OpenGX link verified: resolver, initializer and glGetString use strong implementations.')
PY
mkdir -p ../../../build/wii-online
/opt/devkitpro/devkitPPC/bin/powerpc-eabi-size release/wiicl-glwii
/opt/devkitpro/tools/bin/elf2dol release/wiicl-glwii ../../../build/wii-online/client.dol
python3 ../../../tools/pad_dol.py ../../../build/wii-online/client.dol
