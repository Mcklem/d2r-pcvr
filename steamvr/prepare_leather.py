#!/usr/bin/env python3
"""Cache the leather the tabbed menu is held on, from the installed game.

The Rogue Encampment's stretched animal hide (the pelt hanging on the Act 1
farm's racks, Cal's pick on 2026-10-27): its colour map is decoded, the hide
cut out by its own alpha, turned so its neck is at the left (by the fist) and
written with the 16:9 rectangle on its body the menu is pinned to
(leather_menu.h, leather::ParseCache).

Standard-library Python; Battle.net also needs CascLib on Windows. The cache
contains Blizzard art and stays in the user's installation, never in the mod's
release.
"""
import argparse
import hashlib
import json
import os
import struct
from array import array
from collections import deque
from pathlib import Path
from game_assets import open_assets
from potion_assets import texture

VERSION = 1
FILE = 'leather.pvl'
FILES = (FILE,)
MAGIC = b'PVLH'
FORMAT = 1
SOURCE = 'data/hd/env/texture/global/prop/act1/town/farm/act1_town_farming_pelt_hanging_b_alb.texture'
MAX_WIDTH, MAX_HEIGHT = 1024, 800   # panelatlas::Leather
OPAQUE = 200          # alpha the menu's corners must sit on
COVER = .96           # share of the menu rectangle on the hide
EDGE_RGB = (88, 66, 46)  # behind the hide's transparent edge, so filtering never bleeds the atlas's neighbours


def _565(c):
    return ((c >> 11) & 31) * 255 // 31, ((c >> 5) & 63) * 255 // 63, (c & 31) * 255 // 31


def bc_decode(width, height, data, bc3):
    """BC1 (8-byte blocks) or BC3 (16-byte) to RGBA rows, top first."""
    out = bytearray(width * height * 4)
    bw, bh = (width + 3) // 4, (height + 3) // 4
    size = 16 if bc3 else 8
    if len(data) < bw * bh * size:
        raise ValueError('Truncated block-compressed texture')
    for by in range(bh):
        for bx in range(bw):
            o = (by * bw + bx) * size
            alpha = None
            if bc3:
                a0, a1 = data[o], data[o + 1]
                bits = int.from_bytes(data[o + 2:o + 8], 'little')
                if a0 > a1:
                    table = [a0, a1] + [((7 - i) * a0 + i * a1) // 7 for i in range(1, 7)]
                else:
                    table = [a0, a1] + [((5 - i) * a0 + i * a1) // 5 for i in range(1, 5)] + [0, 255]
                alpha = [table[(bits >> (3 * i)) & 7] for i in range(16)]
                o += 8
            c0, c1 = struct.unpack_from('<HH', data, o)
            r0, g0, b0 = _565(c0)
            r1, g1, b1 = _565(c1)
            if c0 > c1 or bc3:
                colors = ((r0, g0, b0, 255), (r1, g1, b1, 255),
                          ((2 * r0 + r1) // 3, (2 * g0 + g1) // 3, (2 * b0 + b1) // 3, 255),
                          ((r0 + 2 * r1) // 3, (g0 + 2 * g1) // 3, (b0 + 2 * b1) // 3, 255))
            else:
                colors = ((r0, g0, b0, 255), (r1, g1, b1, 255),
                          ((r0 + r1) // 2, (g0 + g1) // 2, (b0 + b1) // 2, 255), (0, 0, 0, 0))
            index = int.from_bytes(data[o + 4:o + 8], 'little')
            for i in range(16):
                x, y = bx * 4 + (i & 3), by * 4 + (i >> 2)
                if x >= width or y >= height:
                    continue
                r, g, b, a = colors[(index >> (2 * i)) & 3]
                if alpha is not None:
                    a = alpha[i]
                p = (y * width + x) * 4
                out[p] = r; out[p + 1] = g; out[p + 2] = b; out[p + 3] = a
    return bytes(out)


def decode(data):
    """The game's .texture, its full-size level, as (width, height, RGBA)."""
    width, height, dxgi, mips = texture(data)
    payload = bytes(mips[0])
    if dxgi == 29:
        return width, height, payload
    if dxgi in (72, 78):
        return width, height, bc_decode(width, height, payload, dxgi == 78)
    raise ValueError(f'Unsupported leather texture format {dxgi}')


def _flood(width, height, allowed, seeds):
    """4-connected fill over `allowed` (bytearray of 0/1) from `seeds`."""
    seen = bytearray(width * height)
    queue = deque()
    for s in seeds:
        if allowed[s] and not seen[s]:
            seen[s] = 1
            queue.append(s)
    while queue:
        p = queue.popleft()
        x = p % width
        for q in (p - width if p >= width else -1, p + width if p + width < width * height else -1,
                  p - 1 if x else -1, p + 1 if x + 1 < width else -1):
            if q >= 0 and allowed[q] and not seen[q]:
                seen[q] = 1
                queue.append(q)
    return seen


def cut_hide(width, height, rgba):
    """The largest piece of the colour map, cut out by its alpha and turned a
    quarter left (its top, the neck, at the left). The light padding the
    texture's atlas spreads from its top edge is not hide."""
    n = width * height
    alpha = rgba[3::4]
    solid = bytearray(1 if a > 128 else 0 for a in alpha)
    light = bytearray(solid[i] and (rgba[4 * i] * 299 + rgba[4 * i + 1] * 587 + rgba[4 * i + 2] * 114) > 150000
                      for i in range(n))
    padding = _flood(width, height, light, range(width))
    keep = bytearray(solid[i] and not padding[i] for i in range(n))
    # The largest connected piece: one labelling pass, then its pixels.
    label = array('i', bytes(4 * n))
    best_label, best_size, current = 0, 0, 0
    for start in range(n):
        if keep[start] and not label[start]:
            current += 1
            label[start] = current
            queue, size = deque((start,)), 0
            while queue:
                p = queue.popleft()
                size += 1
                x = p % width
                for q in (p - width if p >= width else -1, p + width if p + width < n else -1,
                          p - 1 if x else -1, p + 1 if x + 1 < width else -1):
                    if q >= 0 and keep[q] and not label[q]:
                        label[q] = current
                        queue.append(q)
            if size > best_size:
                best_label, best_size = current, size
    if best_size < n // 20:
        raise ValueError('No hide found in the leather texture')
    best = bytearray(1 if label[i] == best_label else 0 for i in range(n))
    # Its holes belong to it: everything not reachable from the border.
    border = [i for i in range(width)] + [n - width + i for i in range(width)] + \
             [y * width for y in range(height)] + [y * width + width - 1 for y in range(height)]
    outside = _flood(width, height, bytearray(1 - b for b in best), border)
    inside = bytearray(1 - o for o in outside)
    xs = [i % width for i in range(n) if inside[i]]
    ys = [i // width for i in range(n) if inside[i]]
    x0, x1, y0, y1 = min(xs), max(xs) + 1, min(ys), max(ys) + 1
    cw, ch = x1 - x0, y1 - y0
    # Turned a quarter left: new (nx, ny) = old (x1-1-ny, y0+nx).
    out = bytearray(ch * cw * 4)
    for ny in range(cw):
        ox = x1 - 1 - ny
        for nx in range(ch):
            oy = y0 + nx
            i = oy * width + ox
            d = (ny * ch + nx) * 4
            if inside[i] and rgba[4 * i + 3]:
                out[d:d + 4] = rgba[4 * i:4 * i + 4]
            else:   # beyond the hide, or a hole in it
                out[d:d + 4] = bytes(EDGE_RGB + (0,))
    return ch, cw, bytes(out)


def shrink(width, height, rgba):
    """Halve (2x2 box) until it fits the atlas region."""
    while width > MAX_WIDTH or height > MAX_HEIGHT:
        w, h = width // 2, height // 2
        out = bytearray(w * h * 4)
        for y in range(h):
            for x in range(w):
                for c in range(4):
                    s = sum(rgba[((2 * y + dy) * width + 2 * x + dx) * 4 + c] for dy in (0, 1) for dx in (0, 1))
                    out[(y * w + x) * 4 + c] = (s + 2) // 4
        width, height, rgba = w, h, bytes(out)
    return width, height, rgba


def menu_rect(width, height, rgba):
    """The widest 16:9 rectangle at least COVER on the hide (alpha >= OPAQUE),
    centred near the hide's middle: (x, y, w) in pixels, y down."""
    sat = [0] * ((width + 1) * (height + 1))
    stride = width + 1
    count = sx = sy = 0
    for y in range(height):
        run = 0
        for x in range(width):
            o = 1 if rgba[(y * width + x) * 4 + 3] >= OPAQUE else 0
            run += o
            sat[(y + 1) * stride + x + 1] = sat[y * stride + x + 1] + run
            if o:
                count += 1; sx += x; sy += y
    if not count:
        raise ValueError('The leather has no opaque area')
    cx, cy = sx / count, sy / count

    def cover(x, y, w, h):
        x0, y0, x1, y1 = int(x), int(y), int(x + w), int(y + h)
        if x0 < 0 or y0 < 0 or x1 > width or y1 > height or x1 <= x0 or y1 <= y0:
            return 0
        s = sat[y1 * stride + x1] - sat[y0 * stride + x1] - sat[y1 * stride + x0] + sat[y0 * stride + x0]
        return s / ((x1 - x0) * (y1 - y0))

    best = (0, 0, 0, 0)   # width, -distance, x, y
    step = max(1, min(width, height) // 50)
    for oy in range(-6, 7):
        for ox in range(-6, 7):
            mx, my = cx + ox * step, cy + oy * step
            lo, hi = 0.0, float(width)
            for _ in range(20):
                mid = (lo + hi) / 2
                if cover(mx - mid / 2, my - mid * 9 / 32, mid, mid * 9 / 16) >= COVER:
                    lo = mid
                else:
                    hi = mid
            key = (round(lo, 1), -(ox * ox + oy * oy), mx - lo / 2, my - lo * 9 / 32)
            if key > best:
                best = key
    w = best[0]
    if w < width * .2:
        raise ValueError('No room for the menu on the leather')
    return best[2], best[3], w


def encode(width, height, rgba, rect):
    x, y, w = rect
    return MAGIC + struct.pack('<4I3f', FORMAT, width, height, 0, x, y, w) + rgba


def prepare(game, destination=None, force=False, casclib=None):
    game = Path(game)
    destination = Path(destination) if destination else game / 'd2rloader/plugins/d2r_vr_leather'
    destination.mkdir(parents=True, exist_ok=True)
    metadata = destination / 'cache.json'
    with open_assets(game, casclib) as assets:
        if not force and metadata.is_file():
            try:
                previous = json.loads(metadata.read_text())
                if previous['version'] == VERSION and previous['build'] == assets.build_hash and \
                        hashlib.sha256((destination / FILE).read_bytes()).hexdigest() == previous['sha256']:
                    return previous
            except (ValueError, KeyError, OSError):
                pass
        width, height, rgba = shrink(*cut_hide(*decode(assets.read(SOURCE))))
        rect = menu_rect(width, height, rgba)
        data = encode(width, height, rgba, rect)
        result = {'version': VERSION, 'build': assets.build_hash, 'source': SOURCE, 'width': width, 'height': height,
                  'menu': [round(v, 2) for v in rect], 'sha256': hashlib.sha256(data).hexdigest()}
        temp = destination / (FILE + '.tmp'); temp.write_bytes(data); os.replace(temp, destination / FILE)
        temp = destination / 'cache.json.tmp'; temp.write_text(json.dumps(result, indent=2) + '\n'); os.replace(temp, metadata)
        return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game', required=True, type=Path)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--force', action='store_true')
    parser.add_argument('--casclib', type=Path, help='Packaged Unicode x64 CascLib 3.0 (Battle.net only)')
    args = parser.parse_args()
    result = prepare(args.game, args.output, args.force, args.casclib)
    print(f"Leather cache: {result['width']}x{result['height']} hide, menu at {result['menu']}")
