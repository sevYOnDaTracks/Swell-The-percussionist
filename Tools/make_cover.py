"""Builds the 1600 x 1600 shop visuals in the Swell style: logo, product name, plugin capture, orange glow.
Usage: python3 Tools/make_cover.py <capture.png> <out.jpg> [caption]"""
import os
import sys
from PIL import Image, ImageDraw, ImageFilter, ImageFont

W = 1600
LOGO = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'Swell-Ad-Libs-FX', 'Resources', 'swell_logo.png')
ORANGE = (255, 138, 61)


def font(size, bold=True):
    return ImageFont.truetype('/System/Library/Fonts/HelveticaNeue.ttc', size, index=1 if bold else 0)


capture, out = sys.argv[1], sys.argv[2]
caption = sys.argv[3] if len(sys.argv) > 3 else 'THE PERCUSSIONIST'

img = Image.new('RGB', (W, W), (11, 11, 12))
# orange glow rising from the bottom
glow = Image.new('L', (W, W), 0)
ImageDraw.Draw(glow).ellipse((150, 950, 1450, 2100), fill=120)
glow = glow.filter(ImageFilter.GaussianBlur(220))
img.paste(Image.new('RGB', (W, W), (120, 52, 18)), (0, 0), glow)

d = ImageDraw.Draw(img)
logo = Image.open(LOGO).convert('RGBA')
logo = logo.resize((120, int(120 * logo.height / logo.width)), Image.LANCZOS)
img.paste(logo, (130, 112), logo)
d.text((285, 98), 'SWELL', font=font(104), fill=(240, 240, 242))
d.text((290, 212), caption, font=font(54), fill=ORANGE)

shot = Image.open(capture).convert('RGBA')
width = 1340
shot = shot.resize((width, int(width * shot.height / shot.width)), Image.LANCZOS)
x, y = (W - width) // 2, 360 + (1060 - shot.height) // 2
mask = Image.new('L', shot.size, 0)
ImageDraw.Draw(mask).rounded_rectangle((0, 0, shot.width, shot.height), 18, fill=255)
shadow = Image.new('L', (W, W), 0)
ImageDraw.Draw(shadow).rounded_rectangle((x + 10, y + 30, x + shot.width - 10, y + shot.height + 40), 24, fill=200)
img.paste(Image.new('RGB', (W, W), (0, 0, 0)), (0, 0), shadow.filter(ImageFilter.GaussianBlur(40)))
img.paste(shot, (x, y), mask)

f = font(38, bold=False)
d.text((W - 130 - d.textlength('swell-audio.fr', font=f), 1505), 'swell-audio.fr', font=f, fill=(200, 200, 204))
img.save(out, quality=90)
