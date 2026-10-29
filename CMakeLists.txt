cmake_minimum_required(VERSION 3.16)
project(nzportable_wii C)
set(CMAKE_C_STANDARD 99)
file(GLOB ENGINE source/*.c source/menu/*.c source/qcvm/*.c source/render/*.c source/tests/*.c)
file(GLOB PLATFORM source/platform/sdl/*.c source/platform/sdl/gl/*.c source/platform/wii/*.c)
list(FILTER PLATFORM EXCLUDE REGEX "(in_sdl|compat_sdl).c$")
file(WRITE ${CMAKE_BINARY_DIR}/_build_info.h "#define GIT_HASH \"wii-port\"\n#define GIT_BRANCH \"wii\"\n#define BUILD_DATE \"2026-10-25\"\n")
add_executable(boot ${ENGINE} ${PLATFORM})
target_include_directories(boot PRIVATE source ${CMAKE_BINARY_DIR} /opt/devkitpro/portlibs/wii/include /opt/devkitpro/portlibs/wii/include/SDL2 /opt/devkitpro/portlibs/ppc/include)
target_compile_definitions(boot PRIVATE GLQUAKE PLATFORM_SDL PLATFORM_WII PLATFORM_DIRECTORY=sdl PLATFORM_RENDERER=gl MAX_AI_COUNT=24 PLATFORM_CONFIRM_IS_ENTER PLATFORM_USES_GENERIC_GLYPHS)
target_compile_options(boot PRIVATE -O2 -g -fcommon -fsigned-char -ffunction-sections -fdata-sections -Wno-implicit-function-declaration -Wno-incompatible-pointer-types)

# Port glue must not silently accept ABI mismatches or misleading control flow.
set_source_files_properties(source/images.c source/snd_music.c source/platform/sdl/music_sdl.c
    source/platform/sdl/snd_sdl.c source/platform/sdl/sys_sdl.c source/platform/sdl/common.c
    source/platform/sdl/gl/gl_vidsdl.c source/platform/sdl/net_udp_sdl.c
    source/platform/wii/boot_wii.c source/platform/wii/in_wii.c PROPERTIES
    COMPILE_OPTIONS "-Wall;-Wextra;-Werror=implicit-function-declaration;-Werror=incompatible-pointer-types;-Werror=misleading-indentation")
target_link_directories(boot PRIVATE /opt/devkitpro/portlibs/wii/lib /opt/devkitpro/portlibs/ppc/lib)
target_link_options(boot PRIVATE -Wl,--gc-sections)
target_link_libraries(boot PRIVATE SDL2_mixer SDL2main SDL2 modplug vorbisidec mpg123 opusfile opus FLAC ogg pthread aesnd wiikeyboard opengx png z fat wiiuse bte ogc m stdc++)
ogc_create_dol(boot)
