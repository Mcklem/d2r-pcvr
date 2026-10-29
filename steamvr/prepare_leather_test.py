"""The leather cache's image steps on synthetic pictures; no Blizzard data in fixtures."""
import struct
import unittest
from prepare_leather import (bc_decode, decode, cut_hide, shrink, menu_rect, encode, MAGIC, FORMAT,
                             MAX_WIDTH, MAX_HEIGHT, EDGE_RGB, FILES, SOURCE)


def picture(width, height, paint):
    rgba = bytearray(width * height * 4)
    for y in range(height):
        for x in range(width):
            rgba[(y * width + x) * 4:(y * width + x) * 4 + 4] = bytes(paint(x, y))
    return bytes(rgba)


def at(width, rgba, x, y):
    return tuple(rgba[(y * width + x) * 4:(y * width + x) * 4 + 4])


class LeatherTests(unittest.TestCase):
    def test_bc1_and_bc3_blocks(self):
        red, blue = 0xF800, 0x001F
        # BC3: alpha 255, colour 0 everywhere.
        block = bytes([255, 255]) + bytes(6) + struct.pack('<HH', red, blue) + bytes(4)
        rgba = bc_decode(8, 4, block * 2, True)
        self.assertEqual(rgba[:4], bytes((255, 0, 0, 255)))
        self.assertEqual(rgba[-4:], bytes((255, 0, 0, 255)))
        # BC1 with c0 <= c1: index 3 is transparent black.
        rgba = bc_decode(4, 4, struct.pack('<HH', blue, red) + b'\xff' * 4, False)
        self.assertEqual(rgba[:4], bytes((0, 0, 0, 0)))
        with self.assertRaises(ValueError):
            bc_decode(8, 8, bytes(8), False)

    def test_decodes_the_game_texture_container(self):
        block = bytes([255, 255]) + bytes(6) + struct.pack('<HH', 0x07E0, 0) + bytes(4)
        data = struct.pack('<IHHII', 0x2845443c, 61, 0, 8, 8) + bytes(12) + struct.pack('<I', 1) + bytes(4) + \
            struct.pack('<II', 64, 4) + block * 4
        width, height, rgba = decode(data)
        self.assertEqual((width, height), (8, 8))
        self.assertEqual(rgba[:4], bytes((0, 255, 0, 255)))

    def test_cuts_the_hide_turns_its_neck_left_and_keeps_its_holes(self):
        w, h = 64, 48
        def paint(x, y):
            if y < 6:                                   # the atlas's light padding from the top edge
                return (230, 210, 180, 255)
            if 10 <= x < 40 and 6 <= y < 40:            # the hide (dark), touching the padding
                if 20 <= x < 24 and 20 <= y < 24:       # a hole in it
                    return (0, 0, 0, 0)
                return (90, 60, 40, 255)
            if 50 <= x < 56 and 30 <= y < 36:           # a smaller scrap
                return (90, 60, 40, 255)
            return (0, 200, 0, 0)                       # transparent
        cw, ch, rgba = cut_hide(w, h, picture(w, h, paint))
        # The hide was 30 wide, 34 tall (rows 6-39): turned, 34 wide, 30 tall.
        self.assertEqual((cw, ch), (34, 30))
        # Every opaque pixel is hide colour: no padding, no scrap, no green.
        for y in range(ch):
            for x in range(cw):
                r, g, b, a = at(cw, rgba, x, y)
                self.assertEqual((r, g, b, a), (90, 60, 40, 255) if a else (r, g, b, a))
        # The hole stays a hole (alpha 0) in the edge colour, so filtering never bleeds into it.
        holes = [(x, y) for y in range(ch) for x in range(cw) if at(cw, rgba, x, y)[3] == 0]
        self.assertEqual(len(holes), 16)
        self.assertTrue(all(at(cw, rgba, x, y) == EDGE_RGB + (0,) for x, y in holes))
        # The top row of the hide (y = 6) is now its left column.
        self.assertTrue(all(at(cw, rgba, 0, y)[3] == 255 for y in range(ch)))

    def test_outside_the_hide_is_transparent_edge_colour(self):
        w, h = 40, 40
        def paint(x, y):
            return (100, 70, 50, 255) if (x - 20) ** 2 + (y - 20) ** 2 < 15 ** 2 else (0, 255, 0, 0)
        cw, ch, rgba = cut_hide(w, h, picture(w, h, paint))
        self.assertEqual(at(cw, rgba, 0, 0), EDGE_RGB + (0,))

    def test_no_hide_is_refused(self):
        with self.assertRaises(ValueError):
            cut_hide(16, 16, bytes(16 * 16 * 4))

    def test_shrinks_to_the_atlas_region(self):
        w, h = MAX_WIDTH * 2 + 2, 10
        width, height, rgba = shrink(w, h, picture(w, h, lambda x, y: (200, 100, 50, 255)))
        self.assertLessEqual(width, MAX_WIDTH)
        self.assertLessEqual(height, MAX_HEIGHT)
        self.assertEqual(at(width, rgba, 0, 0), (200, 100, 50, 255))
        self.assertEqual(shrink(4, 4, bytes(64)), (4, 4, bytes(64)))

    def test_menu_rectangle_is_16_9_and_on_the_hide(self):
        w, h = 200, 120
        def paint(x, y):   # an ellipse of hide
            return (90, 60, 40, 255) if ((x - 100) / 95) ** 2 + ((y - 60) / 57) ** 2 <= 1 else (0, 0, 0, 0)
        rgba = picture(w, h, paint)
        x, y, mw = menu_rect(w, h, rgba)
        mh = mw * 9 / 16
        self.assertGreater(mw, 100)
        self.assertTrue(0 <= x and 0 <= y and x + mw <= w and y + mh <= h)
        inside = sum(rgba[(yy * w + xx) * 4 + 3] >= 200 for yy in range(int(y), int(y + mh)) for xx in range(int(x), int(x + mw)))
        self.assertGreaterEqual(inside / (int(mw) * int(mh)), .95)
        # Centred on the ellipse.
        self.assertAlmostEqual(x + mw / 2, 100, delta=6)
        self.assertAlmostEqual(y + mh / 2, 60, delta=6)
        with self.assertRaises(ValueError):
            menu_rect(10, 10, bytes(400))

    def test_cache_layout(self):
        data = encode(2, 1, bytes(range(8)), (0.5, 0.25, 1.5))
        self.assertEqual(data[:4], MAGIC)
        self.assertEqual(struct.unpack_from('<4I3f', data, 4), (FORMAT, 2, 1, 0, .5, .25, 1.5))
        self.assertEqual(data[32:], bytes(range(8)))
        self.assertEqual(len(data), 32 + 8)
        self.assertEqual(FILES, ('leather.pvl',))
        self.assertTrue(SOURCE.endswith('_alb.texture'))


if __name__ == '__main__':
    unittest.main()
