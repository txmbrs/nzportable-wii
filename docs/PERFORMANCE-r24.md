# Single player renderer-memory and machine-texture candidate r24

## Evidence and limits

The user reports a machine that is black on Wii and causes a nearby FPS drop.
The reference image depicts Juggernog. The local assembled NDU entity lump
contains a Mule Kick machine; the exact runtime machine still needs confirmation.
Both Juggernog skin variants and the Mule Kick PCX decode correctly to colored
128x128 images. Their models are small (90-162 triangles), so excessive machine
polygon count alone is not supported by these assets.

Earlier Wii logs contain GL_OUT_OF_MEMORY (0x505) before world VBO upload. r22
fixed attribution of that old error, but did not remove its original cause.
OpenGX allocates texture texels with memalign/free. The single player engine
reserves its 40 MiB hunk directly from MEM2; ordinary libc allocations remain in
MEM1. The logged PCX skins alone account for about 4.45 MB of native RGBA payload;
the six 512x512 NDU sky faces add about 6.29 MB before world, UI and other buffers.
That establishes memory pressure as a concrete suspect, not proof that this
particular machine's upload failed. Lighting/material issues remain possible if
the machine stays black after successful upload.

## Changes

- After the engine-hunk reservation, initialize a separate libogc heap from
  unused MEM2, capped at 8 MiB and leaving at least 2 MiB of the remaining arena
  unreserved. If less than 512 KiB is usable or initialization fails, keep the
  original libc allocation behavior. The engine hunk is not resized or overlapped.
- Route aligned SDK allocations through that heap first, retaining libc fallback.
  Ownership-aware free/realloc handles both MEM2 and ordinary libc pointers,
  including allocations made before pool initialization. Alignment and contents
  survive realloc; failure preserves the original allocation. No logging or FAT
  access occurs inside allocator critical sections.
- The wrappers cover aligned SDK buffers, including OpenGX texture/VBO storage;
  they are not restricted to one named machine. Ordinary malloc stays in libc.
- Check GL object creation and image upload immediately, with the texture name,
  dimensions and error. A genuine unrecoverable upload error is reported as a
  startup error instead of returning an invalid GPU texture as successful.
- Report successful Juggernog/Mule Kick skin uploads and renderer heap statistics.

No textures, colors, model geometry or lighting settings are reduced or replaced.
No map or game-data files are changed. r21-r23 optimizations are retained.

## Validation

Wii cross-compilation passed. Disassembly of the matching glTexImage2D shows
calls to __wrap_memalign/__wrap_free. Host ASan/UBSan tests of the actual wrapper
code cover power-of-two alignment, cold/libc ownership, realloc contents and
failure, libc fallback, NULL/overflow handling, heap initialization failure and
separation from the simulated engine hunk. The SDK heap is mocked in that test;
it is not a Wii execution or allocator-concurrency test.

## Wii check

r24 still requires hardware validation. Look for:

- `Wii renderer memory: MEM2 aligned pool ...`
- `Wii machine skin: ... uploaded ...`
- Any `Wii texture: ... failed ...` message and its memory report.

Approach the same machine and compare its appearance and frame behavior. A
successful skin upload does not by itself prove correct lighting/material state
or an FPS gain. Keep the current game data and configuration; replace boot.dol
only. The online client and published v0.1 full archive are unchanged.
