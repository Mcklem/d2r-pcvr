"""Bone Armor's bones for the forearm grafts (steamvr/bone_graft.h), read-only.

The game's Bone Armor mesh (data/hd/vfx/meshes/lit_mesh/bonearmor/bonearmor_lod0.model)
is read in its bind pose (potion_assets.Granny: static geometry, no skeleton
or animation). It is split into its separate pieces (triangles joined by shared,
welded vertices); the three largest become bn1..bn3, each turned so its longest
extent lies along +Y and its thinnest along +Z (the side that faces out from the
arm), centred, and 1 m long: the renderer scales it to the drawn forearm.
"""
import math

MODEL = 'data/hd/vfx/meshes/lit_mesh/bonearmor/bonearmor_lod0.model'
ALBEDO = 'data/hd/vfx/meshes/lit_mesh/bonearmor/textures/bonearmor_alb.texture'
CODES = ('bn1', 'bn2', 'bn3')
WELD = 1e-4   # positions this close count as one vertex when joining triangles
MIN_TRIANGLES = 8


def components(meshes):
    """Each separate piece: (vertices, indices) with its own vertex numbering."""
    pieces = []
    for _, verts, ix in meshes:
        parent = list(range(len(verts)))

        def find(a):
            while parent[a] != a:
                parent[a] = parent[parent[a]]
                a = parent[a]
            return a

        def join(a, b):
            a, b = find(a), find(b)
            if a != b:
                parent[b] = a
        welded = {}
        for i, v in enumerate(verts):
            key = tuple(round(c / WELD) for c in v[:3])
            if key in welded:
                join(welded[key], i)
            else:
                welded[key] = i
        for t in range(0, len(ix), 3):
            join(ix[t], ix[t + 1])
            join(ix[t], ix[t + 2])
        groups = {}
        for t in range(0, len(ix), 3):
            groups.setdefault(find(ix[t]), []).append(ix[t:t + 3])
        for triangles in groups.values():
            used = sorted({i for tri in triangles for i in tri})
            remap = {old: new for new, old in enumerate(used)}
            pieces.append(([verts[i] for i in used], [remap[i] for tri in triangles for i in tri]))
    return pieces


def axes(points):
    """The principal axes of points, longest first (3x3 covariance, Jacobi)."""
    n = len(points)
    c = [sum(p[a] for p in points) / n for a in range(3)]
    m = [[sum((p[i] - c[i]) * (p[j] - c[j]) for p in points) / n for j in range(3)] for i in range(3)]
    v = [[1.0 if i == j else 0.0 for j in range(3)] for i in range(3)]
    for _ in range(50):
        off = max(((i, j) for i in range(3) for j in range(i + 1, 3)), key=lambda ij: abs(m[ij[0]][ij[1]]))
        p, q = off
        if abs(m[p][q]) < 1e-12:
            break
        theta = .5 * math.atan2(2 * m[p][q], m[q][q] - m[p][p])
        cs, sn = math.cos(theta), math.sin(theta)
        for k in range(3):
            mkp, mkq = m[k][p], m[k][q]
            m[k][p], m[k][q] = cs * mkp - sn * mkq, sn * mkp + cs * mkq
        for k in range(3):
            mpk, mqk = m[p][k], m[q][k]
            m[p][k], m[q][k] = cs * mpk - sn * mqk, sn * mpk + cs * mqk
        for k in range(3):
            vkp, vkq = v[k][p], v[k][q]
            v[k][p], v[k][q] = cs * vkp - sn * vkq, sn * vkp + cs * vkq
    order = sorted(range(3), key=lambda i: -m[i][i])
    return [tuple(v[k][i] for k in range(3)) for i in order]


def dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def normalize_piece(vertices):
    """Vertices (x y z nx ny nz u v) turned and scaled: longest extent along +Y,
    1 m long, thinnest along +Z, centred on the bounding box."""
    y, x, _ = axes([v[:3] for v in vertices])
    z = cross(x, y)   # right-handed: x, y, z
    turned = [(dot(v[:3], x), dot(v[:3], y), dot(v[:3], z), dot(v[3:6], x), dot(v[3:6], y), dot(v[3:6], z), v[6], v[7])
              for v in vertices]
    lo = [min(t[a] for t in turned) for a in range(3)]
    hi = [max(t[a] for t in turned) for a in range(3)]
    length = hi[1] - lo[1]
    if length < 1e-4:
        raise ValueError('Degenerate Bone Armor piece')
    centre = [(lo[a] + hi[a]) * .5 for a in range(3)]
    return [((t[0] - centre[0]) / length, (t[1] - centre[1]) / length, (t[2] - centre[2]) / length, *t[3:8], 0.)
            for t in turned]


def pieces(meshes, count=3):
    """The count largest pieces (by triangles), normalized; fewer pieces repeat
    the largest so every code has a model."""
    found = sorted((p for p in components(meshes) if len(p[1]) // 3 >= MIN_TRIANGLES), key=lambda p: -len(p[1]))
    if not found:
        raise ValueError('No Bone Armor pieces found')
    chosen = [found[i] if i < len(found) else found[0] for i in range(count)]
    return [(normalize_piece(v), ix) for v, ix in chosen], len(found)
