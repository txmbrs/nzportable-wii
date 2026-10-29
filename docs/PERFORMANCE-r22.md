# Single player VBO upload correction r22

## Evidence from the supplied Wii r21 log

The client reached gameplay and returned to the menu normally. Pose-cache hits
are present, but there is no matched baseline measurement proving an FPS gain.
Typical reported server time is around 1-2.3 ms, alias-model work around 1-5 ms,
and screen update around 14-21 ms including presentation. The scopes overlap:
model/entity and presentation times must not be added to screen time. These
averages do not rule out occasional AI/pathfinding spikes.

Each map load reports `World VBO upload failed; retaining client array`. The
old helper checked GL error state only after all upload calls, so it could
mistake a prior error for an upload failure. This is a confirmed control-flow
problem, not yet proof that it was the specific cause on this Wii.

## Changes

- Drain and report pending GL errors separately before the upload operation.
- Check buffer generation, binding, upload and unbind independently. Real
  failures preserve the original client-array fallback and report their stage,
  GL error code and buffer size. Successful uploads are reported at load time.
- Pad owned Wii array storage to 32 bytes and zero the padding. The SDK's
  glBufferData implementation copies its rounded allocation length, so its
  source must be valid through that last cache line too. NULL-data array
  allocations are zero-initialized instead of copied from a NULL pointer.
- Preserve r21 cache budgets, animation, textures, fog, lightmaps and gameplay.
  No extra per-frame GPU waits or changes to VSync are introduced.

The matching ELF contains strong glGenBuffers, glBufferData, glGetError and
_ogx_texture_get_texobj implementations. Their presence does not prove a
particular upload succeeded on hardware.

## Validation and next observation

Wii cross-compilation passed. The test compiles the actual buffer/upload helpers
with a mock GL backend and ASan/UBSan: a pending unrelated error no longer
rejects a valid upload; real generate/bind/upload failures retain CPU storage;
rounded source reads, optional UV upload failure and cleanup pass. The 2048-draw
alias-cache regression also passes. No engine or emulator is run by these tests.

The r22 executable still requires a Wii test. In wii-startup.txt, inspect
`Wii VBO: uploaded ...` or the exact failure stage/error and then `WII PERF`.
The first cold-loading frame is not representative of steady gameplay. The
published v0.1 full archive is left unchanged until this candidate is verified.
