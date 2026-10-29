"""Bake four antialiased vector strokes for the original-sized hit indicator."""
from pathlib import Path
from PIL import Image, ImageDraw

root = Path(__file__).resolve().parents[1]
size, ss = 64, 4
alpha = Image.new('L', (size * ss, size * ss))
draw = ImageDraw.Draw(alpha)
for x, y in ((1, 1), (-1, 1), (1, -1), (-1, -1)):
    points = [((32 + x * d) * ss, (32 + y * d) * ss) for d in (10, 25)]
    draw.line(points, fill=255, width=4 * ss)
    for px, py in points:
        r = 2 * ss
        draw.ellipse((px-r, py-r, px+r, py+r), fill=255)
alpha = alpha.resize((size, size), Image.Resampling.LANCZOS)
image = Image.new('RGBA', (size, size), (255, 255, 255, 0))
image.putalpha(alpha)
image.save(root / 'dist/apps/nzportable/nzp/gfx/hud/hit_marker_smooth.tga')
preview = Image.new('RGB', (128, 128), (20, 26, 32))
preview.paste(image, (32, 32), image)
preview.save(root / 'tools/hitmarker-after.png')
