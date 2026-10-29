#!/usr/bin/env bash
case "$1" in
    --cflags) echo '-I/opt/devkitpro/portlibs/wii/include/SDL2 -D_REENTRANT' ;;
    # SDL defines weak OpenGX placeholders. Resolve FTE's explicit OpenGX
    # references first so the real resolver/initializer enter the static link.
    --libs|--static-libs) echo '-lopengx -lSDL2 -laesnd -lopengx -lwiikeyboard -lfat -lwiiuse -lbte -logc -lpthread -lstdc++ -lm -lz' ;;
    --version) echo '2.30.0' ;;
    *) exit 1 ;;
esac
