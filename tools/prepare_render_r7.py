"""Prepare the render revision and preserve the last working binary."""
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[1]
backup = root / 'tools/boot-before-render-r7.dol'
if not backup.exists():
    shutil.copy2(root / 'dist/apps/nzportable/boot.dol', backup)
path = root / 'source/platform/sdl/gl/gl_rmain.c'
source = path.read_text()
source = source.replace('glDepthRange (', 'Hyena_SetDepthRange (')
path.write_text(source)
for name in ('source/platform/wii/boot_wii.h', 'platform/wii/meta.xml', 'package_startup_update.py'):
    path = root / name
    source = path.read_text()
    source = source.replace('startup-r6-resource', 'render-r7')
    source = source.replace('startup-r6-release', 'render-r7-release')
    path.write_text(source)
with (root / 'WII_PORT.md').open('a', encoding='utf-8') as output:
    output.write('''

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
''')
