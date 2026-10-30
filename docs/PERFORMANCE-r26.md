# Single player menu picture residency correction r26

## Confirmed failure and cause

r25 stops during `Menu_Main_Set` / `Menu_LoadPics`, after the model-preload
message, on the `wahnsinn` 512x512 picture. It reports GL_OUT_OF_MEMORY,
618,352 free MEM2 pool bytes and 1,931,952 free libc bytes in total. Total free
libc bytes do not establish a large contiguous block. This is a menu map
picture, not evidence that the Wahnsinn BSP or a weapon model failed to load.

`Menu_Preload_Custom_Images` eagerly uploads every enabled map thumbnail.
The assembled game data includes 19 PNG files in `gfx/menu/custom`. Their
power-of-two RGBA8 payloads sum to 17,956,864 bytes, before allocator overhead
and other menu/model/SDK textures. Runtime metadata determines which are enabled.
The failing weapon_test and wahnsinn files are each 480x272 and become 512x512
uploads (1,048,576 GPU bytes each). r25's roughly 10 MB aligned pool plus a
small remaining libc heap cannot sustain an unbounded menu picture preload.

## Correction

On Wii, preload only records thumbnail availability. Load map pictures when
they are actually needed: one rotating menu background and one selected map
preview. These are two independent, replaceable slots with unique texture
identifiers. Browsing many maps or rotating backgrounds cannot accumulate all
their pictures. Selecting a cached image reuses it without loading again.

Free the replaced slot before loading its successor. Wait for GX to finish
sampling old texels before deletion, then invalidate the texture cache. This
wait is performed when releasing a resident picture, not on every draw. Check
registry identifiers rather than trusting numeric IDs that may be reused after
map-load cleanup. Missing pictures are cached as missing until the slot changes
or resets; a missing background uses the usual default menu background.

The main menu reset clears both slots and the background timer. All Wii callers
that display map thumbnails, including the selected map's lobby, use the bounded
loader. Other platforms retain the existing eager preload path.

Picture files, dimensions, filtering, precision and colors are unchanged. The
engine hunk remains 40 MiB, r25's MEM2 allocation correction is retained, and
the double forward-tap sprint shortcut is retained. Single push still walks.
No online files or game data are changed.

## Verification and limits

Wii cross-compilation passed. ASan/UBSan host tests exercise the actual picture
cache with mocked GPU/texture APIs: 1000 repeated draws without extra loads,
1000 selected-preview changes with at most two live pictures, independent
background rotation, fences before deletion, external map cleanup and reused
IDs, missing files, invalid indices and reset. Memory allocator and double-tap
sprint regression tests also pass. These do not constitute a Wii runtime test.

Extract the update's `nzportable` folder into `apps`, replacing boot.dol.
Keep current game data. Check successful startup, map browsing, map loading and
the previously black machine on Wii. Startup should report `Wii menu images:
... loading background and selected preview on demand`. Machine appearance,
remaining map memory limits and any FPS gain still need hardware verification.
Online gameplay remains unfinished; the published v0.1 full archive is unchanged.
