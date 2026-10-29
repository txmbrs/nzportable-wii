"""Bake a scalable sans-serif font into NZP's 16x16 glyph layout.

Usage: python tools/build_smooth_font.py path/to/LiberationSans-Regular.ttf
Requires Pillow. Existing game symbols are retained from the original atlas.
"""
import math
from pathlib import Path
import sys
from PIL import Image, ImageDraw, ImageFont

root = Path(__file__).resolve().parents[1]
gfx = root / 'dist/apps/nzportable/nzp/gfx'
font_path = Path(sys.argv[1]) if len(sys.argv) > 1 else root / 'tools/fonts/LiberationSans-Regular.ttf'
supersample, tile = 4, 32
font = ImageFont.truetype(str(font_path), tile * supersample)
original = Image.open(gfx / 'charset.tga').convert('RGBA')
atlas = original.resize((512, 512), Image.Resampling.NEAREST)
metrics = [8] * 96

for code in range(33, 127):
    # Render from a shared baseline, preserving ascenders and descenders.
    bounds = font.getbbox(chr(code), anchor='ls')
    glyph = Image.new('L', (tile * supersample, tile * supersample))
    width = bounds[2] - bounds[0]
    ink = Image.new('L', (max(width, 1), tile * supersample))
    ImageDraw.Draw(ink).text((-bounds[0], 24 * supersample), chr(code),
                             font=font, fill=255, anchor='ls')
    # A glyph has a one-pixel margin at each side, preventing atlas bleed.
    ink_width = min(width, (tile - 2) * supersample)
    if width > ink_width:
        ink = ink.resize((ink_width, tile * supersample), Image.Resampling.LANCZOS)
    glyph.paste(ink, (supersample, 0))
    alpha = glyph.resize((tile, tile), Image.Resampling.LANCZOS)
    # Draw_ColoredString adds one logical pixel to this value already.
    metrics[code - 33] = min(7, max(1, math.ceil((ink_width + supersample) / (4 * supersample)) - 1))
    for index, color in ((code, (255, 255, 255)), (code + 128, (255, 55, 55))):
        cell = Image.new('RGBA', (tile, tile), (*color, 0))
        cell.putalpha(alpha)
        atlas.paste(cell, ((index % 16) * tile, (index // 16) * tile))

# White RGB even in transparent font texels avoids dark filtering fringes.
atlas.save(gfx / 'charset_smooth.tga')
(gfx / 'kerning_smooth.txt').write_text(','.join(map(str, metrics)) + '\n')

# Inspect real atlas sampling at representative menu sizes without launching a game.
preview = Image.new('RGB', (800, 280), (17, 22, 29))
canvas = ImageDraw.Draw(preview)
for line, label in enumerate(('NAZI ZOMBIES: PORTABLE', 'Single Player', 'Video Settings',
                              'Reload  |  Ammo: 24 / 120', 'Press A to continue')):
    x, y, size = 24, 18 + line * 50, 24
    for char in label:
        if char == ' ':
            x += size // 2
            continue
        code = ord(char)
        cell = atlas.crop(((code % 16) * tile, (code // 16) * tile,
                           (code % 16 + 1) * tile, (code // 16 + 1) * tile))
        cell = cell.resize((size, size), Image.Resampling.BILINEAR)
        preview.paste(cell, (x, y), cell)
        x += (metrics[code - 33] + 1) * size // 8
preview.save(root / 'tools/smooth-font-preview.png')
print('Created 512x512 RGBA font atlas and matching text metrics.')
