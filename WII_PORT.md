# NZPortable Wii development port

## Startup revision 6: resource fallback and port review

The PCX logging branch now encloses both log and loader call. Missing PCX files
fall through to TGA/PNG/JPEG again instead of calling fread on a null FILE.
The PCX entry point also refuses null streams. Fatal screens identify this
revision explicitly, allowing an old launch to be distinguished from a new build.

File handles are range/null checked. COM_FindFile no longer closes handle -1
after a failed reopen; unreadable files can fall through to another search path.
File lengths reject seek errors and lengths beyond the engine's int interface.

Input now merges held buttons across controllers before emitting Key_Event,
so releasing a Wiimote button cannot cancel the same held GameCube action.
Classic Controller buttons and both calibrated sticks are mapped; A jumps,
B changes stance, X reloads/uses, Y changes weapon, L/ZL aims, R/ZR fires,
minus throws a grenade, plus pauses and HOME exits. Connection reporting reflects
the observed devices. These mappings have not been tested on hardware.

Host frame limiting recognizes PLATFORM_WII with a 60 FPS default and retains
the SDL menu's configurable cl_maxfps setting. Saved settings can override this
default; this is not a measured frame rate. Legacy __WII__ input/view branches are not
enabled globally: the SDL/native input backend owns this port's controller path.
Strict prototype/pointer/control-flow checks cover the changed port modules.
The audit's claim of an absent music_last_error prototype was checked and ruled
out: platform/sdl/music.h declares it and nzportable_def.h includes that header.

Merge NZPortable-Wii-startup-r6-resource.zip into an existing install, retaining
nzp. Fully stop the previous executable and launch the replaced boot.dol;
continuing a paused emulator session does not reload it. Compilation only;
no game execution, emulator run or tests were performed for this revision.

## Startup revision 5: shared audio

The Wii backend now uses one SDL_mixer output device for both original game
sound effects and MP3 music. SNDDMA's ring buffer is added to decoded music in
a native-endian, signed 16-bit stereo postmix callback with SDL's saturating mix.
Music initialization reuses this output rather than opening a second SDL device.
Callback removal precedes PCM buffer release; music only closes a device it
opened itself (e.g. when effects are disabled). Desktop audio remains separate.

MP3 initialization remains required, and the actual SDL_mixer error is included
in fatal diagnostics. Music has not been disabled as a workaround. Merge
NZPortable-Wii-startup-r5-audio.zip into the existing SD install, retaining nzp.
This revision was compiled only; playback has not been tested.

## Startup revision 4: PCX

PCX header fields now decode explicitly from little-endian bytes on every
platform, independently of the __WII__ macro. This removes the endian-dependent
header rejection at LoadPCX on PowerPC. Both PCX loaders share this header reader
and bounded RLE decoding, honor bytes_per_line padding and use the fixed 128-byte
data offset rather than C structure size. Truncation, palette marker and run
boundaries are checked. Startup logging names each PCX image before decoding it.

Merge NZPortable-Wii-startup-r4-pcx.zip into the existing SD install, replacing
boot.dol and meta.xml while retaining nzp. This revision was compiled, not run.

## Startup revision 3

Startup now initializes a visible libogc console before mounting storage or
initializing SDL/the engine. It checks mount success and selects the application
directory using a valid device path from argv, the initial working directory,
or the conventional SD/USB app folders. The nzp directory must be present.
It no longer calls freopen on unchecked relative paths or setvbuf on a stream
whose reopen may have failed. Engine messages and startup stages are flushed to
wii-startup.txt beside boot.dol. Fatal errors restore a readable console and
wait for HOME/START rather than relying on an SDL message box.

Network configuration runs in UDP_Init after storage/log initialization and
uses explicit address buffers. Network failure retains the local game path.
The MEM2 arena allocation checks arena ordering before subtracting addresses.
Earlier initialization failure/black screen does not establish that these were
the only problems. Runtime execution remains untested at the user's request.

For an existing installation, merge NZPortable-Wii-startup-r3.zip into SD root.
It does not include game data: retain apps/nzportable/nzp. For a fresh install,
use NZPortable-Wii.zip. Look for "startup revision 3" at launch. If it stops,
provide apps/nzportable/wii-startup.txt or the visible error/stage.

## Loader fix, revision 2

The original generated DOL ended 12 bytes before the end of the final section's
32-byte-aligned read range. Dolphin's loader rejected the executable before any
game code ran. The packaged DOL now includes those trailing zero bytes. The build
script applies this padding automatically, preserving section addresses, sizes,
entry point and executable code. This fixes the identified file bounds defect;
runtime execution remains untested at the user's request.

This is a native build of the original nzp-team/vril-engine, not a replacement game.
Target: retain the upstream game mechanics and content. Full functional parity has
not been established. No runtime tests were performed, at the user's request.

## Install

Extract NZPortable-Wii.zip into the SD root. Keep the entire apps/nzportable/nzp
directory beside boot.dol. Start NZPortable Wii through Homebrew Channel.
The data is from the official NZPortable nightly Linux package, including progs.dat,
maps, models, sounds and textures. Its licenses are in nzp/licenses.

## Controls

Wiimote + Nunchuk: stick moves, IR displacement turns/aims, B fires, Z aims,
A jumps, minus reloads (hold to use), 1 changes weapons, 2 changes stance,
C throws a grenade, plus pauses, HOME exits. D-pad up sprints, down melees,
left performs impulse 33, right shows scores. These use the original game actions.
GameCube: stick moves, C-stick looks, A jumps, B changes stance, X reloads/uses,
Y changes weapons, L aims, R fires, Z throws a grenade, START pauses.

## Build

From PowerShell:

    wsl -d Ubuntu -- bash --noprofile --norc -lc 'docker run --rm -v /mnt/c/Users/HARDPC/Desktop/googlecloud/nzportable-wii:/src -w /src devkitpro/devkitppc:latest bash build_wii.sh'

Output: dist/apps/nzportable/boot.dol. Uses the existing devkitPPC container,
SDL2, OpenGX, libogc and SDL2_mixer. The engine heap reserves 40 MiB of MEM2.
OpenGX and remaining allocations use their existing allocators.

## Current limits

Compilation is not evidence of a successful boot or gameplay. No Dolphin or
physical-console validation has been performed. Rendering, memory fit for every
map, networking, controls and complete parity still require runtime validation.
Classic Controller is mapped in revision 6; physical validation remains pending.
Gyroscope and lightbar callbacks have no implementation on this target.
Diagnostics are written to wii-startup.txt beside boot.dol.
The official desktop config is adapted for Wii controls and 640x480 output.

Port changes: Wii CMake build, native WPAD/PAD input, MEM2 engine heap,
big-endian file/protocol conversion and palette byte order, IOS network adapter,
OpenGX video setup and a local perspective calculation instead of GLU.
Upstream gameplay and QuakeC bytecode are retained.


## Render r7

- Apply the weapon/muzzle-flash depth range through GX_SetViewport as well
  as glDepthRange. The installed OpenGX glDepthRange only stores two floats;
  its 3D viewport always uses 0..1, losing the original weapon/wall separation.
- Restore the scene camera matrix and opaque depth-tested state for the
  viewmodel pass. Keep the existing close near plane for aiming down sights.
- Validate the incoming alias frame before a model-change reset, initialize
  both poses from the new model, and reject stale out-of-range blend poses.
  Model scale and ADS offsets from the original assets are preserved.
- Configure VI output to 720 pixels wide without allocating a wider GX EFB
  or changing the preferred interlaced/progressive TV mode.
- Camera/HUD use an anamorphic logical canvas matching the console aspect.
  wii_widescreen -1 follows Wii settings (default), 0 forces 4:3, 1 forces 16:9.
  Screenshots still read the actual physical framebuffer dimensions.
- This revision is built and statically inspected only. No game, Dolphin,
  automated gameplay tests, or real Wii validation were run.

## Font r8

Wii text uses a 512x512 RGBA atlas baked from Liberation Sans Regular, with linear filtering, antialiased alpha and matching proportional text metrics. Existing controller icons and special symbols are preserved. Red alternate text remains red. This applies to engine-rendered text, not lettering baked into game textures. The original font is retained as a fallback if the new atlas is absent. Font artwork, metrics and OFL license must be copied alongside the new boot.dol. No configuration replacement is needed.

Regenerate with Python + Pillow: python tools/build_smooth_font.py. Bundled font source and license are in tools/fonts. Build and static asset preview only; no gameplay/emulator tests were run.

## Smoothing and settings r9

Video Options includes IMAGE SMOOTHING: OFF / LOW, default LOW. This uses a
mild GX display-copy vertical filter (8/48/8 weights) at the current resolution.
It is not MSAA, does not reduce EFB height, and adds no extra scene render or
framebuffer copy. OFF restores the unfiltered display copy.

APPLY now saves settings instead of only playing a sound. Wii also saves one
second after the last archived cvar change, debouncing sliders. Failed writes
are reported and retried after five seconds. cl_maxfps, r_retro and the new
smoothing choice are archived. A normal exit saves before network-list shutdown.
The writer checks flush/close failures and records its config.cfg path in the
startup log. Update ZIPs exclude config.cfg, preserving personal settings.

Build/static inspection only. No emulator/game tests were run. Image softness,
actual performance, SD writes and persistence across Wii restarts still need
confirmation on the user's console.

## Exit and hit marker r10

Wii END GAME now queues a return-to-menu command. At the command boundary it
fully disconnects, drains pending rendering and releases the map/model caches
and level hunk before loading menu pictures. Previously menu input deleted
textures and reloaded menu pictures before its queued disconnect had run, with
scene memory still occupied and queued draws potentially holding old textures.

Unscaled non-mipmapped textures upload directly from their decoded pixels.
This removes a redundant full-size RGBA preparation allocation, including the
buffer whose allocation failure reports r_texture.c:286 in r9. Scaled/mipmapped
uploads retain their existing preparation path. Failure diagnostics now include
the texture identifier, source size and output size. The user's exact error/log
was unavailable, so the reported exit failure is not conclusively reproduced.

The Wii hit marker loads a new antialiased 64x64 atlas and explicitly uses linear
2D filtering independently of the world texture setting. Its display size,
normal/kill colors and fade timing are unchanged. Original artwork is retained
as a fallback. Regenerate with python tools/build_hitmarker.py.

Build/static inspection only; no emulator or console gameplay tests were run.

## Floor shimmer r11

Generate complete mip chains for Wii BSP world textures loaded from embedded
Quake textures or WAD3 fallback, previously uploaded without mipmaps. External
world replacements already requested mipmaps. LINEAR now uses trilinear minification;
NEAREST retains sharp magnification but samples the nearest mip when distant.
HUD/font textures remain unchanged. Mip chains add roughly one third to base
world texture storage (plus native alignment), so runtime memory/performance
still require console confirmation.

Wii's second world-lighting pass uses LEQUAL instead of exact EQUAL, without
depth writes. World coordinates remain floats and OpenGX's RGB8/Z24 format and
4-unit world near plane are preserved. No blanket depth bias or geometry
rounding workaround is introduced. The reported white speckles were not shown
in an actual gameplay capture and were not reproduced; texture aliasing and
lightmap depth equality are the two concrete code issues addressed here.
Build/static inspection only; no game/emulator tests were run.

The persistent corner round indicator is also about 20 percent smaller on Wii,
with linear 2D filtering for both tally marks and numeric rounds. Its bottom
margin is preserved and the large round-start animation stays the same size.

## Full quit and Hangar performance r12

The user clarified the texture failure happens on HOME/QUIT, not END GAME.
Wii quit now blocks screen updates immediately, returns before other input or
queued commands can load assets, and performs client/server disconnect from
the outer SDL loop. The last frame's pending draw batch is drained before
context deletion. QUIT phase markers are written to wii-startup.txt. The exact
texture error text is still unavailable; this hardens a concrete shutdown path
but does not constitute reproduction or confirmation of the reported failure.

LINEAR returns to bilinear filtering with nearest mip selection, retaining the
complete mip chains from r11. Trilinear filtering is no longer the Wii default.
A short read-only Hangar audit found 44 small world textures (max128x128), 128
brushmodels, many placed models and 1279 faces with animated styles1/10 (478
world faces). Animated lighting can trigger repeated CPU rebuilds/lightmap
uploads. World PVS is425 leaves, average115 visible; an earlier preliminary
PVS estimate based on submodel leaves was discarded. No gameplay logic, model
counts or animated lighting were reduced in this patch, so 60FPS is not promised.
Build/static inspection only; no game/emulator tests were run.

## Real-hardware lightmap coherency r13

OpenGX's update_texture converts offset lightmap subrectangles into a tiled
atlas but calls DCFlushRange at the atlas/mip base with a length calculated
from the update rectangle rather than its offset and atlas pitch. Consequently
CPU-written lightmap texels can remain dirty in cache on real Wii while a
coherent emulator renders updated texels. Wii lightmap updates now submit full
pages through the existing GL API, ensuring OpenGX flushes the complete atlas.

Full-page updates skip the redundant region copy, and Wii reuses a conversion
scratch allocation instead of allocating/freeing one for every lightmap update.
Light animation cadence, colors and resolution are unchanged. Full conversion
can cost more than a tiny subrectangle; it is a correctness workaround, not a
promise of higher FPS. Filtering choices are unchanged from r12. The earlier
trilinear rollback was a quality/performance tradeoff, not an equal-quality
renderer optimization.

Build/static inspection only. Actual Wii lighting and performance still need
comparison against the user's Dolphin reference with the same DOL/data/config.

## Partial lightmap coherency r14

Replace r13's full-page conversion/upload on current SDKs with dirty-rectangle
updates plus DCFlushRange over affected tiled atlas rows. Use OpenGX's exported
_ogx_texture_get_texobj accessor (verified in the actual SDK archive), obtain
native address/width/format through GX and calculate row strides with
GX_GetTexBufferSize. CPU flushing covers full tile rows, respecting Y offsets
and atlas stride; GX texture cache is invalidated after the flush. No extra
pixel staging buffer or whole-page conversion is needed. Reused conversion
scratch remains. Filtering, light animation, texture resolution and colors
are unchanged. A weak accessor reference retains the r13 correctness fallback
on older SDKs lacking this internal OpenGX export.

The user's 30FPS report followed r13; the full-page workaround is a plausible
regression but no console profile was captured. This restores partial upload
work while retaining the cache coherency fix; recovery to60FPS is unverified.
Build/static inspection only; no gameplay/emulator tests were run.

## Cutout fences r15

Restore exact EQUAL depth comparison for the second world/lightmap pass. The
r11 LEQUAL change was incorrect for alpha-tested surfaces: rejected holes do
not write base-pass depth, so a less-or-equal lighting pass can multiply a
background object behind a hole, creating an apparent solid/ghost rectangle.
Keep r14's real-hardware texture-cache coherency fix.

Wii textures named with the Half-Life cutout prefix { no longer receive box
averaged alpha mipmaps, which can erase narrow opaque bars below the alpha
cutoff. This applies to external replacements and WAD/embedded fallbacks;
ordinary opaque world textures keep mipmaps. A later coverage-preserving
mipmap generator could improve distant cutout stability without erasing bars.
Collision and gameplay remain unchanged. Build/static inspection only; actual
fence appearance requires console validation.

## Game-over camera and health review r16

Wii game-over rendering (health<=0, outside explicit intermissions) reuses the
last living camera origin/angles. The original QC EndGameSetup switches players
to MOVETYPE_TOSS and clears velocity; continuing to follow the simulated entity
can drag the results background below the map. The snapshot invalidates on
world/view entity changes, disconnect or client-time reset. Living scoreboard
views, explicit intermission cameras, simulation and damage are unchanged.

Packaged nzp/progs.dat is byte-for-byte identical to official-data.zip:
SHA256 2e14abce3a19be1559f0d12ff01f2376cb46a264edf6a0f1ae701b7b9ae36e1a.
Original QC source player_core.qc defines Normal100, Easy125, Hard75,
Nightmare25 HP; player spawn applies game-mode/perk and map modifier logic.
Packaged config selects sv_difficulty0 and sv_gamemode0. No health/damage
balance change was made. Sources inspected: nzp-team/quakec main,
source/server/player/player_core.qc and source/server/damage.qc.
Build/static inspection only; no gameplay/emulator tests were run.

## World submission and lightmap CPU work r17

Immutable Wii world cache arrays now use OpenGX VBOs. In the inspected OpenGX
implementation, client arrays send vertex components through CPU readers,
whereas compatible VBO attributes use DirectVboReader/GX_INDEX16 and GX_SetArray.
Both diffuse and lightmap passes reuse the same immutable buffer, with their
existing float position/UV offsets. CPU staging storage is released after the
upload; streamed visibility indices remain client-side. Bindings are cleared
before raw client pointers can be used. An upload error retains the client array.
This removes repeated component submission for cached world/eligible brush faces;
it does not accelerate every alias model, simulation, or pixel fill.

Lightmap accumulation seeds the first sampled style directly instead of clearing
the accumulator and then adding to zero. Later styles and dynamic lights keep
the original integer sums, clamp and RGBA output. No lighting cadence, texture
filter, mipmap policy, resolution, alpha/depth rule or gameplay data was reduced.
The r14 partial upload/cache flush remains intact.

Read-only packaged BSP inventory (tools/inspect_bsp_costs.py):

| Map | World/all faces | Brushmodels | Placed model entities | Animated world/all faces | World leaves | PVS faces avg/max |
| --- | --- | --- | --- | --- | --- | --- |
| nzp_warehouse | 570/1390 | 53 | 71 | 52/87 | 131 | 138.54/330 |
| nzp_warehouse2 | 729/2814 | 87 | 143 | 573/1960 | 220 | 250.05/496 |

PVS counts precede frustum/backface rejection and exclude runtime visibility of
separate model entities. Warehouse has 22 embedded textures (~0.32 MiB base
RGBA); Warehouse 2 has 38 (~0.70 MiB). These inventories are not timing profiles
and do not establish the cause of a particular 50 FPS view. Warehouse 2 has much
more animated-light work; that explanation cannot simply be applied to Warehouse.

Wii VIDEO OPTIONS now has PERFORMANCE LOG (off by default, session-only).
It selects r_speeds 2 and writes one PERF line to wii-startup.txt per 120 rendered
views: average/max scene submission time, world/entity submission time, lightmap
build/upload time and counts, updated texels, and the last view's batch/entity
counts. It does not force glFinish. Submission can include FIFO stalls and is
not total GPU time, frame time, or measured FPS; world/entity timings include
their lightmap work, so columns must not be added together. Logging itself can
perturb performance. Disable it after capturing the slow area. r_speeds 1 retains
the original verbose diagnostic behavior.

### Cooperation findings

COOPERATIVE is disabled in the shared menu_main.c, not by a Wii-only restriction.
The retained PRE-GAME screen is a local map/settings/countdown screen. There is
no implemented create/join screen, server browser, remote player readiness,
shared countdown or configuration synchronization. Stock/custom map back paths
also only implement the solo destination. Menu_LoadMap does not configure coop,
maxplayers or listen; startup normally creates one active client slot (capacity
for four is allocated). Merely linking the grey button to map selection would
therefore still start the existing single-player path.

The SDL target does compile loopback + datagram + UDP, including Wii IOS network
setup, discovery, connect and listen/maxplayers commands. Transport handshake
version is 3 and gameplay protocol is NZP's extended version 15; the numeric
version alone does not establish compatibility with arbitrary Quake clients or
different NZP snapshots. Payload read/write pairs exist for multiplayer state;
transport being present does not establish complete cooperative gameplay.

Concrete remote-client defect fixed: client message effect deadlines, view spread
and HUD fade/hitmarker checks now use cl.time, rather than the local server's
sv.time (which does not advance on a remote-only client). This also makes the
max-ammo expiry check use the same clock as its existing cl.time deadline.

An additional gameplay gap is outside the menu: the retained original QC
PutClientInServer only calls PlayerSpawn during the initial spawn window or
before round one. Its late-join SpectatorSpawn branch is guarded by FTE, while
this port uses Vril. Read-only inspection of packaged progs.dat's function calls
also finds PlayerSpawn but no SpectatorSpawn call in PutClientInServer.
Exposing an in-progress LAN server is consequently insufficient to guarantee a
usable late-joining player. A synchronized pre-match connection/ready flow or
an explicitly implemented QC late-join policy is still required. The official
progs.dat remains unchanged, and the button remains disabled rather than claiming
that Cooperation is functional. LAN gameplay remains unvalidated.

Local multiplayer is a separate missing implementation: this Wii input backend
merges controllers into one player's commands, and there is one local client,
camera and HUD. There is no split-screen or independent local-player input path.

r17 was compiled and statically inspected only. No game, Dolphin or automated
tests were run. Neither 60 FPS recovery nor multiplayer functionality has been
confirmed on physical hardware. Update ZIP excludes config.cfg; r16 DOL backup:
tools/boot-before-world-vbo-r17.dol.

