"""Bone Armor piece split and normalization on synthetic meshes; no Blizzard data."""
import math
import random
import unittest
import bone_pieces as bp


def box(cx, cy, cz, sx, sy, sz):
    """A closed box mesh: 8 corners, 12 triangles, (x y z nx ny nz u v)."""
    corners = [(cx + sx * (i & 1 and 1 or -1) * .5, cy + sy * (i & 2 and 1 or -1) * .5, cz + sz * (i & 4 and 1 or -1) * .5) for i in range(8)]
    verts = [(*c, 0., 1., 0., .5, .5) for c in corners]
    faces = [(0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3)]
    ix = []
    for a, b, c, d in faces:
        ix += [a, b, c, a, c, d]
    return verts, ix


def merged(*boxes):
    verts, ix = [], []
    for v, i in boxes:
        ix += [n + len(verts) for n in i]
        verts += v
    return verts, ix


class PiecesTest(unittest.TestCase):
    def test_split_separate_pieces(self):
        v, ix = merged(box(0, 0, 0, .1, .5, .05), box(1, 0, 0, .1, .3, .05), box(2, 0, 0, .1, .2, .05))
        found = bp.components([('bones', v, ix)])
        self.assertEqual(len(found), 3)
        self.assertTrue(all(len(p[1]) == 36 and len(p[0]) == 8 for p in found))

    def test_welded_duplicates_join(self):
        v, ix = box(0, 0, 0, .1, .5, .05)
        # the same corner positions repeated as separate vertices (split normals): still one piece
        v2 = v + [(*p[:3], 1., 0., 0., 0., 0.) for p in v]
        ix2 = ix[:18] + [i + 8 for i in ix[18:]]
        self.assertEqual(len(bp.components([('b', v2, ix2)])), 1)

    def test_normalized_long_axis_y_thin_z_unit_length(self):
        random.seed(3)
        # a bone 0.4 long along a slanted direction, 0.08 wide, 0.02 thick
        d = (math.cos(.6), math.sin(.6) * .7, math.sin(.6) * .71)
        n = math.sqrt(sum(c * c for c in d)); d = tuple(c / n for c in d)
        w = bp.cross(d, (0, 0, 1)); n = math.sqrt(bp.dot(w, w)); w = tuple(c / n for c in w)
        t = bp.cross(d, w)
        pts = []
        for _ in range(400):
            a, b, c = random.uniform(-.2, .2), random.uniform(-.04, .04), random.uniform(-.01, .01)
            pts.append((5 + d[0] * a + w[0] * b + t[0] * c, 2 + d[1] * a + w[1] * b + t[1] * c, d[2] * a + w[2] * b + t[2] * c, 0, 1, 0, 0, 0))
        out = bp.normalize_piece(pts)
        ext = [max(p[a] for p in out) - min(p[a] for p in out) for a in range(3)]
        self.assertAlmostEqual(ext[1], 1.0, places=5)
        self.assertGreater(ext[0], ext[2] * 2)
        self.assertLess(ext[2], .1)
        self.assertTrue(all(abs(p[a]) <= .5 + 1e-6 for p in out for a in range(3)))
        self.assertTrue(all(len(p) == 9 and p[8] == 0. for p in out))

    def test_pieces_picks_largest_and_repeats(self):
        v, ix = merged(box(0, 0, 0, .1, .5, .05), box(1, 0, 0, .1, .3, .05))
        chosen, found = bp.pieces([('b', v, ix)])
        self.assertEqual(found, 2)
        self.assertEqual(len(chosen), 3)
        self.assertEqual(chosen[2][1], chosen[0][1])

    def test_tiny_fragments_ignored(self):
        v, ix = box(0, 0, 0, .1, .5, .05)
        tri = [(9, 9, 9, 0, 1, 0, 0, 0), (9.1, 9, 9, 0, 1, 0, 0, 0), (9, 9.1, 9, 0, 1, 0, 0, 0)]
        chosen, found = bp.pieces([('b', v + tri, ix + [8, 9, 10])])
        self.assertEqual(found, 1)
        with self.assertRaises(ValueError):
            bp.pieces([('b', tri, [0, 1, 2])])


if __name__ == '__main__':
    unittest.main()
