# Single player model optimization r21

This candidate preserves the original models, interpolation formula, UV values,
textures, lighting and rendering order. Visual equivalence and FPS gains still
need verification on a physical Wii. The online client is unchanged.

## Changes

- Immutable model UV arrays are uploaded once through OpenGX VBOs instead of
  submitting the same float coordinates at every model draw. At most 32 optional
  UV objects and 128 KiB of UV payload are retained per map.
- At most 128 KiB of final short vertex positions are cached per map. Reuse
  requires the exact same pose pointers and interpolation factor. There is no
  animation-rate reduction or interpolation approximation. Different animations
  keep their original calculation, using a reusable scratch buffer if necessary.
- Allocation/upload failure falls back to the existing draw path. UV staging is
  padded to 32 bytes for OpenGX's rounded upload read. Cached GPU UV data is
  immutable; dynamic positions stay on the existing client-array draw path.
- Commands/model relocation and map teardown release cached buffers. Scratch
  positions are also released on Wii map teardown.

The 256 KiB payload ceiling excludes small buffer/topology metadata and padding.
Existing topology UV/index arrays remain as before. Animated zombies with
unique interpolation phases may not hit the pose cache; UV reuse still applies.

## Profiling

`wii_perf` defaults to 1. It records CPU wall time and emits one `WII PERF` line
per 600 active frames into `wii-startup.txt`. `wii_perf 0` disables it. It does
not insert GPU drains or change graphics settings. Logging is infrequent but
can itself cause an occasional storage-related hitch.

- `work_ms`: time spent processing the accepted frame, including presentation;
  excludes the frame limiter. This is not a direct FPS measurement.
- `server`: local server work. `physics` is a nested subset containing physics
  and game think/AI; it is not an isolated AI benchmark.
- `client`: reading local/server state.
- `screen`: full screen update, including nested `entities` and `present`.
- `present`: swap/presentation wait, potentially including GPU work and VSync.
- `alias_build` / `alias_draw`: model geometry preparation / draw submission.
- `models/frame`, `verts/frame`, `pose_hits`: all alias-model draws, including
  weapons, decorations and zombie parts; these are not zombie counts.

Nested timings must not be added together. Compare the same map and view with
few/many zombies, looking toward/away from them. Compare against the previous
boot.dol; retain the same data and settings.

## Validation

Wii cross-compilation passed. The host regression test compiles the actual
r_mdl.c cache/batching logic with a stub graphics backend and ASan/UBSan. It
passed 2048 draws covering exact interpolated positions, index winding, immutable
UV reuse, allocation fallback, memory/object caps, relocation and map teardown.
This is not a GX rendering test or hardware performance measurement.
