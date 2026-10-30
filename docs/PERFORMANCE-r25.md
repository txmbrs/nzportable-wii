# Single player memory and double-tap sprint candidate r25

## Hardware evidence from r24

r24 fails during startup at `weapon_test` (512x512) with GL_OUT_OF_MEMORY.
The aligned MEM2 heap reports 7,528,216 bytes used and 860,384 bytes free;
libc reports 1,502,552 free bytes in total, which does not establish a large
enough contiguous block. A 512x512 native RGBA8 texture needs 1,048,576 bytes,
before allocator overhead. The former 8 MiB pool cap prevented the renderer
from using the rest of the available MEM2 arena.

## Memory correction

Use the remaining MEM2 arena after the engine hunk, retaining the 2 MiB reserve
and minimum viable heap size. There is no fixed 8 MiB cap. The 40 MiB engine
hunk, texture sizes, precision and colors are unchanged. Existing ownership,
alignment, overflow and libc fallback handling remain. Actual pool capacity is
reported at startup and depends on available memory, including a custom `-heap`.
This removes the artificial cap; it does not guarantee every map fits in RAM.
The explicit texture allocation failure report is retained.

## Sprint gesture

Quickly push the main stick forward twice, returning to neutral between pushes;
hold it forward after the second push to run. One push and a held stick walk
normally. Each push must reach 80 percent forward within 180 ms of leaving the
neutral zone. The second push must reach that point within 400 ms of the first.
Strong sideways or backward motion cancels the pending gesture.

Supported movement sticks: Nunchuk, Classic Controller and GameCube controller.
The gesture sends the existing `impulse 23` sprint command once; it does not
change movement multipliers, stamina, weapon logic or the game's stopping rules.
It is disabled in menus, while paused, before signon, when dead or aiming.
Disconnecting or changing the stick source clears the gesture, and a new source
must first return to neutral. Console option `wii_flick_sprint 0` disables the
shortcut; its setting is saved to the normal configuration.

## Verification and installation

Cross-compiled for Wii. Host ASan/UBSan tests run the actual allocation wrappers
and gesture detector. The memory test now verifies use beyond the old 8 MiB cap,
resident allocations plus a 512x512 upload with libc unavailable, the arena
reserve, hunk separation, ownership and failure cases. The input test covers
double taps, single held walking, timeout, slow pushes, blocked input, source
changes, reverse/sideways movement, jitter, disconnect and tick-counter wrap.
The allocator SDK is mocked; these are not hardware runtime tests.

Extract the update's `nzportable` folder into `apps`, replacing boot.dol.
Existing game data and online client are retained. This candidate needs Wii
verification: successful startup, map load, machine texture, and both walking
and double-tap sprint. Online gameplay is still unfinished. The v0.1 full
release archive has not been replaced with this unverified candidate.
