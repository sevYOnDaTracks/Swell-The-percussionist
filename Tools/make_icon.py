"""Draws the app icon: an orange djembe with its skin and rope lacing, on the dark Swell tile.
Usage: python3 -I Tools/make_icon.py Resources/icon.png"""
import sys, zlib, struct
import numpy as np

N = 1024
y, x = np.mgrid[0:N, 0:N].astype(np.float64) + .5
c = N / 2


def coverage(d):  # signed distance (negative inside) -> anti-aliased coverage
    return np.clip(.5 - d, 0, 1)


def rounded_square(cx, cy, half, radius):
    qx, qy = np.abs(x - cx) - half + radius, np.abs(y - cy) - half + radius
    return np.hypot(np.maximum(qx, 0), np.maximum(qy, 0)) + np.minimum(np.maximum(qx, qy), 0) - radius


def ellipse(cx, cy, rx, ry):  # approximate signed distance, good near the edge
    k = np.hypot((x - cx) / rx, (y - cy) / ry)
    return (k - 1) * np.minimum(rx, ry)


def segment(ax, ay, bx, by):
    abx, aby = bx - ax, by - ay
    t = np.clip(((x - ax) * abx + (y - ay) * aby) / (abx * abx + aby * aby), 0, 1)
    return np.hypot(x - ax - t * abx, y - ay - t * aby)


def over(img, colour, alpha):
    col = np.array(colour, dtype=np.float64) / 255
    a = np.clip(alpha, 0, 1)[..., None]
    img[..., :3] = img[..., :3] * (1 - a) + col * a
    img[..., 3] = img[..., 3] + a[..., 0] * (1 - img[..., 3])


img = np.zeros((N, N, 4))

# Tile: rounded square, dark gradient from top to bottom, faint orange glow behind the drum
tile = coverage(rounded_square(c, c, 452, 200))
grad = np.array([32, 32, 36]) + (np.array([10, 10, 11]) - np.array([32, 32, 36])) * (y / N)[..., None]
img[..., :3] = grad / 255 * tile[..., None]
img[..., 3] = tile
glow = np.exp(-((x - c) ** 2 + (y - 470) ** 2) / (2 * 300 ** 2)) * .22
over(img, (255, 138, 61), glow * tile)

# Djembe body: goblet profile (bowl narrowing to the waist, then the flared foot)
TOP, WAIST, BOTTOM = 290, 640, 870
HEAD_RX, WAIST_W, FOOT_W = 232, 70, 158


def half_width(yy):
    t = np.clip((yy - TOP) / (WAIST - TOP), 0, 1)
    bowl = WAIST_W + (HEAD_RX - WAIST_W) * (1 - t ** 1.6) ** 1.25  # rounded cup
    u = np.clip((yy - WAIST) / (BOTTOM - WAIST), 0, 1)
    foot = WAIST_W + (FOOT_W - WAIST_W) * u ** 1.8
    return np.where(yy < WAIST, bowl, foot)


w = half_width(y)
body_d = np.maximum(np.abs(x - c) - w, np.maximum(TOP - y, y - BOTTOM))
body = coverage(body_d)
shade = np.clip((x - (c - w)) / (2 * w + 1e-6), 0, 1)  # light from the left
body_colour = np.array([255, 150, 74]) * (1 - shade[..., None] * .55) + np.array([120, 52, 18]) * shade[..., None] * .55
img[..., :3] = img[..., :3] * (1 - body[..., None]) + body_colour / 255 * body[..., None]
img[..., 3] = np.maximum(img[..., 3], body)
over(img, (0, 0, 0), coverage(ellipse(c, BOTTOM, FOOT_W, 18)) * .35)  # foot rim

# Rope lacing: a ring under the head, a ring lower on the bowl, and the V pattern between them
RING_A, RING_B = TOP + 50, TOP + 235
rope = np.full(x.shape, 1e9)
for yy in (RING_A, RING_B):
    rope = np.minimum(rope, np.abs(y - yy) + np.maximum(0, np.abs(x - c) - half_width(yy)) * 4)
count = 8
for i in range(count + 1):
    xa = c - half_width(RING_A) + 2 * half_width(RING_A) * i / count
    for j in (-1, 1):
        k = i + j * .5
        if 0 <= k <= count:
            xb = c - half_width(RING_B) + 2 * half_width(RING_B) * k / count
            rope = np.minimum(rope, segment(xa, RING_A, xb, RING_B))
over(img, (58, 28, 12), coverage(rope - 4) * body)

# Skin: light ellipse with a dark rim, seen from slightly above
over(img, (40, 24, 14), coverage(ellipse(c, TOP, HEAD_RX + 10, 70)))
skin_light = np.clip(1 - np.hypot((x - c + 60) / HEAD_RX, (y - TOP + 14) / 62), 0, 1)
skin = coverage(ellipse(c, TOP, HEAD_RX - 6, 60))
skin_colour = np.array([214, 204, 188]) + np.array([34, 34, 34]) * skin_light[..., None]
img[..., :3] = img[..., :3] * (1 - skin[..., None]) + skin_colour / 255 * skin[..., None]

# Write PNG (RGBA, 8 bit)
data = (np.clip(img, 0, 1) * 255 + .5).astype(np.uint8)
raw = b''.join(b'\x00' + row.tobytes() for row in data)
chunk = lambda tag, body: struct.pack('>I', len(body)) + tag + body + struct.pack('>I', zlib.crc32(tag + body) & 0xffffffff)
png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', N, N, 8, 6, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b'')
open(sys.argv[1], 'wb').write(png)
