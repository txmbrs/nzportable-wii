# Single player lightmap upload optimization r23

## Problem and scope

The r22 Wii log confirms successful world VBO upload. A substantial remaining
part of world submission is lightmap upload: roughly 2-3 ms in many windows,
occasionally close to 4 ms. These are observations from that session, not a
matched FPS comparison between versions.

The old update path copies the dirty rectangle into an allocated RGBA region,
packs it to RGBA5551, then asks OpenGX to decode the packed pixels into its
native RGBA8 texture. OpenGX provides a specialized fast RGBA-byte converter,
whereas packed formats use the generic per-pixel reader.

## Change

For supported existing RGBA8 lightmap textures, r23 copies directly from atlas
rows into one reusable 64 KiB RGBA staging area. Compact lightmaps keep the exact
old RGBA5551 quantization/expansion result. Full-precision lightmaps keep their
original bytes. This avoids the per-update region allocation, separate packing
pass and generic SDK packed-pixel decode. Initial allocation/replacement still
uses the original path, as do luminance, larger rectangles, unsupported native
formats, mipmapped data and non-default pixel-store row/skip state.

Uploads still use the same glTexSubImage2D entry point and existing tile-row CPU
cache flush. No native texture-memory writes, extra GPU waits, reduced texture
resolution, reduced update frequency or lighting/fog changes are introduced.
Filtering and logical texture-record precision remain as before.

Before enabling the fast path, the client compares old packed and new byte
conversion results through its actually linked `_ogx_bytes_to_texture` function.
Calibration covers all channel values, restores unpack state, and falls back
if the SDK output differs. It preserves the SDK's existing alpha expansion too;
it does not assume that the nominal alpha bit expands to 255.

## Validation

- Wii cross-compilation passed; both required SDK symbols are strong in the ELF.
- Host ASan/UBSan test compares all 65536 RGBA5551 combinations against OpenGX's
  actual CompoundPixelStream implementation.
- 1000 strided partial/tiled atlas updates match the old output byte for byte,
  including unchanged pixels outside each rectangle; both precision modes pass.
- Bounds/capacity, filtering, pixel-store restoration and fallback guards pass.
- Deliberate converter mismatch disables the new path.

Reference sources and their original copyright/license notices are retained in
`tools/opengx-pixels.cpp`, `tools/opengx-pixel_stream.cpp`,
`tools/opengx-pixel_stream.h` and `tools/opengx-texel.h`.
Upstream: https://github.com/devkitPro/opengx/tree/master/src

## Wii validation still needed

r23 has not been run on Wii by the build/test process. No FPS gain is claimed.
Look for `Wii lightmaps: fast converter verified` and
`Wii lightmaps: reusable RGBA fast updates active`, then compare lm_upload and
frame behavior during the same map, view and fight. A converter-mismatch message
means the old implementation is still used. Keep the current game data/settings.
The online client and published v0.1 full package are unchanged.
