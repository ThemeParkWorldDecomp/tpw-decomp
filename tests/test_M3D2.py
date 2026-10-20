"""The mesh file loader: LoadM3D2Header, FreeM3D2Header, the Fix* functions it calls and
AlignTo64Bit (engine library, 0x100483f4..0x1004b9c8; src/m3d2_load.cpp).

A case is a mesh file the test builds itself (class Mesh below: every table of the format,
with small random counts and random contents), a set of load flags and the distance of
the block from an 8-byte boundary. Both sides load the same bytes. The test compares the
result, the fakes' events (the allocator, the file callbacks with where the file is read
to, the debug message) and, for a file that is accepted, every field of the loaded image.

How the images compare. The original swaps each field into the Mac's byte order and the
translation on a little-endian host has nothing to swap, so a field's BYTES differ between
the sides. The builder keeps a map of the file (offset, kind and width of every field),
and for each field the test works out what could have become of the file's value on each
side: left as read ("raw": the Mac then sees it byte-swapped), swapped ("same": the value
the file means), and what the loader can do to a field of that kind (an offset relocated
into the block, a pointer cleared, a bit field rebuilt, a flag cleared, a count set to 1,
the header's pointers to itself and to its block). The sides must agree on one of them.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "engine symbols shared"
SHIM_LIBRARY = "unit_M3D2"

LOAD, FREE, ALIGN_VALUE, ALIGN_ADDRESS, UDEBUG = 0x100483f4, 0x10049574, 0x10039078, 0x100390cc, 0x10043b20
P_MALLOC, P_FREE = 0x10051f70, 0x10051f74
FILE_OPEN, FILE_SIZE, FILE_READ, FILE_CLOSE = 0x101735e4, 0x101735d4, 0x101735d8, 0x101735e0
# Every function of the unit in the original: they run as the original's own code.
REAL = [
    0x10039078, 0x100390cc, 0x100483f4, 0x10048aa0, 0x10048af8, 0x10048b58, 0x10048bb0, 0x10048c14, 0x10048c8c,
    0x10048d00, 0x10048d68, 0x10048dd0, 0x10048e3c, 0x10048e80, 0x10048ef8, 0x10048f70, 0x10048ff8, 0x1004906c,
    0x100490e0, 0x10049148, 0x100491ac, 0x1004920c, 0x10049274, 0x100492d8, 0x10049340, 0x100493b4, 0x10049424,
    0x1004948c, 0x100494e4, 0x1004952c, 0x10049574, 0x100495dc, 0x100497bc, 0x1004982c, 0x10049884, 0x100498e4,
    0x10049af4, 0x10049c38, 0x10049d38, 0x10049eec, 0x10049f64, 0x10049fe4, 0x1004a150, 0x1004a21c, 0x1004a2d4,
    0x1004a524, 0x1004a57c, 0x1004a5cc, 0x1004a6cc, 0x1004a7fc, 0x1004a8dc, 0x1004a9f4, 0x1004ab84, 0x1004abdc,
    0x1004ac44, 0x1004acbc, 0x1004ad88, 0x1004aedc, 0x1004af68, 0x1004afe0, 0x1004b044, 0x1004b11c, 0x1004b17c,
    0x1004b364, 0x1004b3dc, 0x1004b86c, 0x1004b8d4, 0x1004b920, 0x1004b990,
]
MAGIC, VERSION, ANIM_VERSION = 0x1cd15d46, 0xdd, 0xcb
MASK = 0xFFFFFFFF


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    if not hasattr(e, "m3_fns"):
        e.m3_fns = {name: T.fake_function(e, (lambda n: lambda emu: getattr(emu.m3_side, n)())(name))
                    for name in ("cb_malloc", "cb_free", "cb_open", "cb_size", "cb_read", "cb_close")}


def w32(e, a, v):
    e.write(a, struct.pack(">I", v & MASK))


# ------------------------------------------------------------------ the file builder


class Mesh:
    """A mesh file under construction: its bytes and the kind of every field in them."""

    def __init__(self, r):
        self.r, self.data, self.fields = r, bytearray(), []
        self.labels, self.fixups = {}, []

    def here(self):
        return len(self.data)

    def field(self, kind, width, value):
        self.fields.append((len(self.data), kind, width))
        self.data += (value & ((1 << (8 * width)) - 1)).to_bytes(width, "little")

    def u8(self, value=None, count=1):
        for _ in range(count):
            self.field("plain", 1, self.r.getrandbits(8) if value is None else value)

    def u16(self, value=None, kind="plain"):
        self.field(kind, 2, self.r.getrandbits(16) if value is None else value)

    def u32(self, value=None, kind="plain", count=1):
        for _ in range(count):
            self.field(kind, 4, self.r.getrandbits(32) if value is None else value)

    def ptr(self, label, kind="ptr"):
        """An offset field: the place of `label` once the file is laid out (None: 0)."""
        if label is not None:
            self.fixups.append((len(self.data), label))
        self.field(kind, 4, 0)

    def text(self, value):
        for byte in value.encode("latin-1") + b"\0":
            self.field("plain", 1, byte)

    def mark(self, label):
        while len(self.data) % 4:
            self.field("plain", 1, 0)
        self.labels[label] = len(self.data)

    def finish(self):
        for offset, label in self.fixups:
            self.data[offset:offset + 4] = self.labels[label].to_bytes(4, "little")
        return bytes(self.data)


def maybe(r, label, chance=0.85):
    return label if r.random() < chance else None


def build(r, o):
    """A file with the options `o`: magic, version, anim_version, geometry (header flag 1),
    anim (an animation block), junk (random high bits in the header's flags), bare (no
    face and face set tables: the header's two pointers are 0 and no node has face sets)."""
    m = Mesh(r)
    textures = r.randrange(0, 4)
    nodes = r.randrange(0, 4)
    mini_nodes = r.randrange(0, 3)
    shapes = r.randrange(0, 3)
    uv_sets = r.randrange(0, 4)
    embedded = r.randrange(0, 3)
    with_field = r.random() < 0.5
    with_names = r.random() < 0.5
    # Per node: vertices, face sets of (first face, face count), 16-bit words, with points.
    plan, face_total, set_total = [], 0, 0
    for _ in range(nodes):
        faces = r.randrange(0, 5)
        sets = []
        for _ in range(r.randrange(0, 3)):
            first = r.randrange(0, faces + 1)
            sets.append((first, r.randrange(0, faces - first + 1)))
        plan.append(dict(vertices=r.randrange(0, 10), faces=faces, sets=sets, words=r.randrange(0, 5),
                         indices=[r.randrange(0, 9) for _ in range(faces)], points=r.random() < 0.8,
                         face_base=face_total, set_base=set_total,
                         with_sets=not o["bare"] and r.random() < 0.85))
        face_total += faces
        set_total += len(sets)
    # Faces no face set uses, so that their index can be anything (a node's points are
    # counted from the indices of the faces its sets use, which stay small).
    spare_faces = r.randrange(0, 3)
    link_targets = [f"node{i}" for i in range(nodes)] + [f"mini{i}" for i in range(mini_nodes)]
    link = lambda: r.choice(link_targets) if link_targets and r.random() < 0.7 else None

    # ---- header, 0xb8 bytes
    m.u32(o["magic"])
    m.u32(o["version"])
    m.u32(o["anim_version"])
    m.u32(count=3)
    m.u8(count=0x14)
    m.u32()
    m.u32((r.getrandbits(32) & ~1 if o["junk"] else r.choice([0, 0x20, 0x220])) | (1 if o["geometry"] else 0), "hflags")
    for count in (r.getrandbits(16), textures, r.getrandbits(16), uv_sets, set_total, face_total + spare_faces, shapes,
                  nodes + mini_nodes, nodes, r.getrandbits(16), embedded, r.getrandbits(16)):
        m.u16(count)
    m.ptr("names" if with_names else None)
    m.ptr(maybe(r, "textures"))
    m.ptr("tinfos" if textures else maybe(r, "tinfos", 0.5))
    m.ptr(maybe(r, "pool"))
    m.ptr(maybe(r, "pool"))
    m.ptr(maybe(r, "uvsets"))
    # A node's points are counted from its faces, which the loader fixes through these two
    # tables first: a file whose nodes have face sets always has both.
    m.ptr(None if o["bare"] else "facesets")
    m.ptr(None if o["bare"] else "faces")
    m.ptr("hfield" if with_field else None)
    m.ptr(maybe(r, "nodes"))
    m.ptr("minis")
    m.ptr(r.choice(link_targets) if link_targets else None)
    m.ptr(maybe(r, "embedded"))
    m.u32(count=6)
    m.ptr("anim" if o["anim"] else None, "animptr")
    m.u32(kind="self")
    m.u32()
    m.u32(r.choice([0, r.randrange(1, 0x400)]))          # 0xa4: a pointer the loader only swaps
    m.u8(count=4)
    m.ptr(maybe(r, "shapes"))
    m.u32(kind="block")
    m.u8(count=4)
    assert m.here() == 0xb8

    m.mark("pool")
    m.u32(count=r.randrange(0, 6))
    m.mark("names")
    m.u32(count=r.randrange(1, 4))
    m.mark("textures")
    for _ in range(textures):
        m.u16()
        m.u16()
        m.u32(r.choice([0, 0, r.randrange(1, 0x400)]))
    m.mark("tinfos")
    for i in range(textures):
        m.u32(kind="tflags")
        m.u32(r.choice([0, r.getrandbits(32)]), "zeroptr")
        m.u8(count=2)
        m.u16(kind="frames")
        m.ptr(maybe(r, f"tname{i}"))
    for i in range(textures):
        m.mark(f"tname{i}")
        m.text("tex%d.tga" % r.randrange(100))
    m.mark("uvsets")
    m.u32(count=8 * uv_sets)
    m.mark("shapes")
    shape_table(m, r, "shape", shapes)
    m.mark("facesets")
    for node in plan:
        for first, count in node["sets"]:
            m.ptr(maybe(r, "pool", 0.5))
            m.u16(first)
            m.u16(count)
            m.u16()
            m.u16()
            m.u8(count=4)
    m.mark("faces")
    for node in plan:
        for index in node["indices"]:
            m.u16(index | r.randrange(2) << 15, "face")
            m.u16()
            m.u16()
            m.u16()
    for _ in range(spare_faces):
        m.u16(kind="face")
        m.u16()
        m.u16()
        m.u16()
    m.mark("nodes")
    for i, node in enumerate(plan):
        m.mark(f"node{i}")
        mini_node(m, r, link, f"nname{i}")
        m.u16(node["vertices"])
        m.u16(len(node["sets"]))
        m.u16()
        m.u16(node["words"])
        m.ptr(maybe(r, f"nverts{i}"))
        m.ptr(f"npoints{i}" if node["points"] else None)
        m.ptr(maybe(r, "uvsets"))
        m.ptr(f"nsets{i}" if node["with_sets"] else None)
        m.ptr(f"nfaces{i}")
        m.u32(r.choice([0, r.randrange(1, 0x400)]))      # 0x74: only swapped
        m.u32(count=6)
        m.u8(count=4)
        m.ptr(maybe(r, f"nwords{i}"))
        m.u32(r.choice([0, r.randrange(1, 0x400)]))      # 0x98: only swapped
        m.u8(count=4)
    m.labels["minis"] = m.here()
    for i in range(mini_nodes):
        m.mark(f"mini{i}")
        mini_node(m, r, link, f"mname{i}")
    for i, node in enumerate(plan):
        m.labels[f"nsets{i}"] = m.labels["facesets"] + 0x10 * node["set_base"]
        m.labels[f"nfaces{i}"] = m.labels["faces"] + 8 * node["face_base"]
        m.mark(f"nname{i}")
        m.text("node%d" % i)
        m.mark(f"nverts{i}")
        m.u32(count=((node["vertices"] + 3) >> 2) * 12)
        m.mark(f"nwords{i}")
        for _ in range(node["words"]):
            m.u16()
        m.mark(f"npoints{i}")
        last = node["words"] - 1
        if node["with_sets"]:
            for first, count in node["sets"]:
                last = max([last] + node["indices"][first:first + count])
        m.u32(count=(last + 1) * 3)
    for i in range(mini_nodes):
        m.mark(f"mname{i}")
        m.text("mini%d" % i)
    m.mark("embedded")
    for i in range(embedded):
        with_parent, with_light = r.random() < 0.7, r.random() < 0.7
        back = with_parent and r.random() < 0.6
        m.u32(r.getrandbits(32) & ~0x40000 | (0x40000 if back else 0))
        m.u32()
        m.u32()
        m.ptr(f"parent{i}" if with_parent else None)
        m.ptr(f"light{i}" if with_light else None)
        o.setdefault("parents", []).append((i, with_parent, back, with_light))
    for i, with_parent, back, with_light in o.get("parents", []):
        if with_parent:
            m.mark(f"before{i}")
            m.u32(count=4 if back else 0)
            m.mark(f"parent{i}")
            m.u16()
            m.u16(16 if back else r.getrandbits(16))
            m.u32(count=3)
        if with_light:
            m.mark(f"light{i}")
            m.u32(count=6)
    if with_field:
        heights, quads = r.randrange(0, 6), r.randrange(0, 10)
        m.mark("hfield")
        m.u32()
        m.u16(heights)
        m.u16(quads)
        m.u32(count=8)
        m.ptr(maybe(r, "heights"))
        m.ptr(maybe(r, "quads"))
        m.mark("heights")
        m.u32(count=heights)
        m.mark("quads")
        for _ in range(quads * 2):
            m.u16()
    if o["anim"]:
        animation(m, r, link_targets)
    return m


def mini_node(m, r, link, name):
    m.u32()
    m.ptr(link())
    m.ptr(link())
    m.ptr(link())
    m.u32(count=16)
    m.u16()
    m.u16()
    m.ptr(maybe(r, name))


def shape_table(m, r, prefix, count):
    sizes = []
    for i in range(count):
        points, pairs = r.randrange(0, 5), r.randrange(0, 4)
        sizes.append((points, pairs))
        m.u32()
        m.u16(points)
        m.u16(pairs)
        m.ptr(maybe(r, f"{prefix}points{i}"))
        m.ptr(maybe(r, f"{prefix}words{i}"))
    for i, (points, pairs) in enumerate(sizes):
        m.mark(f"{prefix}points{i}")
        m.u32(count=points * 3)
        m.mark(f"{prefix}words{i}")
        for _ in range(pairs * 2):
            m.u16()


def animation(m, r, link_targets):
    shapes, scale_keys, quat_keys = r.randrange(0, 3), r.randrange(0, 4), r.randrange(0, 4)
    node_anims, textures, disabled = r.randrange(0, 3), r.randrange(0, 3), r.randrange(0, 5)
    m.mark("anim")
    m.u32(count=3)
    for count in (shapes, scale_keys, quat_keys, node_anims, r.getrandbits(16), r.getrandbits(16), textures,
                  disabled, r.getrandbits(16)):
        m.u16(count)
    m.u8(count=2)
    m.ptr(maybe(r, "ashapes"))
    m.ptr(maybe(r, "scalekeys"))
    m.ptr(maybe(r, "quatkeys"))
    m.ptr(maybe(r, "nodeanims"))
    m.ptr(maybe(r, "atextures"))
    m.ptr(maybe(r, "uvanims"))
    m.ptr(maybe(r, "disabled"))
    m.ptr(maybe(r, "apool"))
    m.u32(r.choice([0, r.randrange(1, 0x400)]))          # 0x40, 0x44: only swapped
    m.u32(r.choice([0, r.randrange(1, 0x400)]))
    m.mark("apool")
    m.u8(count=8 * r.randrange(0, 3))
    m.mark("ashapes")
    shape_table(m, r, "ashape", shapes)
    m.mark("scalekeys")
    for _ in range(scale_keys):
        m.u16()
        m.u16()
        m.u32(count=3)
    m.mark("quatkeys")
    for _ in range(quat_keys):
        m.u16()
        m.u16()
        m.u32(count=4)
    m.mark("atextures")
    pairs = [r.randrange(0, 4) for _ in range(textures)]
    for i in range(textures):
        m.u16()
        m.u16(pairs[i])
        m.ptr(maybe(r, f"apairs{i}"))
    for i in range(textures):
        m.mark(f"apairs{i}")
        for _ in range(pairs[i] * 2):
            m.u16()
    m.mark("disabled")
    for _ in range(disabled):
        m.u16()
    plans = []
    m.mark("nodeanims")
    for i in range(node_anims):
        plan = dict(points=r.random() < 0.4, enables=r.randrange(0, 5), path=r.random() < 0.6,
                    morph=r.random() < 0.75, uv=r.random() < 0.7)
        plans.append(plan)
        m.u32()
        m.u32(r.getrandbits(32) & ~0x4000 | (0x4000 if plan["points"] else 0))
        m.u32()
        m.u32()
        m.u16()
        m.u16()
        m.u16()
        m.u16(plan["enables"])
        m.ptr(maybe(r, "ashapes"))
        m.ptr(maybe(r, "quatkeys"))
        m.ptr(maybe(r, "scalekeys"))
        m.ptr(f"path{i}" if plan["path"] else None)
        m.ptr(f"morph{i}" if plan["morph"] else None)
        m.ptr(f"uvanim{i}" if plan["uv"] else None)
        m.ptr(maybe(r, f"enables{i}"))
        m.ptr(maybe(r, "apool"))
        m.u8(count=4)
        m.u32(r.choice([0, r.randrange(1, 0x400)]))      # 0x3c: only swapped
    m.labels["uvanims"] = m.here()
    for i, plan in enumerate(plans):
        m.mark(f"enables{i}")
        for _ in range(plan["enables"]):
            m.u16()
        if plan["path"]:
            words = r.randrange(0, 5)
            m.mark(f"path{i}")
            m.u32()
            m.u32(words)
            m.u32()
            m.ptr(maybe(r, f"pathwords{i}"))
            m.mark(f"pathwords{i}")
            m.u32(count=words)
        if plan["uv"]:
            tracks, count = r.randrange(0, 6), r.randrange(0, 5)
            m.mark(f"uvanim{i}")
            m.u32(tracks)
            m.ptr(maybe(r, f"uvtrack{i}"))
            m.u32(count)
            m.ptr(maybe(r, f"uvwords{i}"))
            m.ptr(maybe(r, f"uvfloats{i}"))
            m.mark(f"uvtrack{i}")
            for _ in range(tracks * 2):
                m.u16()
            m.mark(f"uvwords{i}")
            for _ in range(count):
                m.u16()
            m.mark(f"uvfloats{i}")
            m.u32(count=count * 2)
        if plan["morph"] and plan["points"]:
            points = r.randrange(0, 5)
            m.mark(f"morph{i}")
            m.ptr(maybe(r, "apool"))
            m.u32(points)
            m.ptr(maybe(r, f"morphpoints{i}"))
            m.mark(f"morphpoints{i}")
            m.u32(count=points * 3)
        elif plan["morph"]:
            keys, duped = r.randrange(0, 3), r.randrange(0, 6)
            sizes = [(r.randrange(0, 4), r.randrange(0, 4)) for _ in range(keys)]
            m.mark(f"morph{i}")
            m.u16()
            m.u16(keys)
            m.u16()
            m.u16(duped)
            m.u16()
            m.u8(count=2)
            m.ptr(maybe(r, f"keydata{i}"))
            m.ptr(maybe(r, f"duped{i}"))
            m.u32(count=6)
            m.mark(f"keydata{i}")
            for k, (vertices, key_count) in enumerate(sizes):
                m.u16(vertices)
                m.u16(key_count)
                m.ptr(maybe(r, f"keywords{i}_{k}"))
                m.ptr(maybe(r, f"vertexwords{i}_{k}"))
                m.ptr(maybe(r, f"cverts{i}_{k}"))
                m.u32()
            for k, (vertices, key_count) in enumerate(sizes):
                m.mark(f"keywords{i}_{k}")
                for _ in range(key_count):
                    m.u16()
                m.mark(f"vertexwords{i}_{k}")
                for _ in range(vertices):
                    m.u16()
                m.mark(f"cverts{i}_{k}")
                m.u32(kind="cvert", count=vertices * key_count)
            m.mark(f"duped{i}")
            for _ in range(duped * 2):
                m.u16()
    m.mark("end")


# ------------------------------------------------------------------ comparing two images


def swapped(value, width):
    return int.from_bytes(value.to_bytes(width, "little"), "big")


def outcomes(kind, value, width, base, block, mac):
    """What the file's `value` can have become in a field of this kind: name -> value."""
    found = {"raw": swapped(value, width) if mac else value, "same": value}
    if kind in ("ptr", "animptr") and value:
        found["relocated"] = (base + value) & MASK
    if kind in ("animptr", "zeroptr", "hflags"):
        found["cleared"] = 0
    if kind == "hflags":
        found["no animation"] = value & ~0x20
    if kind == "self":
        found["self"] = base
    if kind == "block":
        found["block"] = block
    if kind == "tflags":
        found["bit 0 cleared"] = value & ~1
    if kind == "frames":
        found["one"] = 1
    if kind == "face":
        found["rebuilt"] = (value & 0x7fff) << 1 | value >> 15
    if kind == "cvert":
        found["rebuilt"] = (value & 0x3ff) << 22 | (value >> 10 & 0x3ff) << 12 | (value >> 20 & 0x3ff) << 2
    return found


def compare_images(case, mac_image, mac_base, mac_block, host_image, host_base, host_block):
    data, problems = case["file"], []
    for offset, kind, width in case["fields"]:
        value = int.from_bytes(data[offset:offset + width], "little")
        mac = int.from_bytes(mac_image[offset:offset + width], "big")
        host = int.from_bytes(host_image[offset:offset + width], "little")
        mac_names = {n for n, v in outcomes(kind, value, width, mac_base, mac_block, True).items() if v == mac}
        host_names = {n for n, v in outcomes(kind, value, width, host_base, host_block, False).items() if v == host}
        if not mac_names & host_names:
            problems.append(f"field {offset:#x} ({kind}, file {value:#x}): original {mac:#x} {sorted(mac_names)}, "
                            f"translation {host:#x} {sorted(host_names)}")
    return problems


# ------------------------------------------------------------------ the two sides


class PpcSide:
    def __init__(self, e, case):
        self.e, self.case = e, case
        e.m3_side = self
        self.L = CL.PpcLog({}, case.get("q"))
        e.cl = self.L
        self.handle = e.alloc(8)
        self.block = 0

    def cb_malloc(self):
        e = self.e
        size = e.r(3)
        self.L.rec("Malloc", (size,))
        self.block = e.alloc(size + 16, bytes(size + 16)) + self.case["skew"]
        e.set_r(3, self.block)

    def cb_free(self):
        e = self.e
        self.L.rec("Free", (1 if e.u32(e.r(3)) == self.block else -1,))
        w32(e, e.r(3), 0)
        self.block = 0

    def cb_open(self):
        e = self.e
        self.L.rec("Open", (), (e.cstr(e.r(3)), e.cstr(e.r(4))))
        self.position = 0
        e.set_r(3, self.handle if self.L.ret("Open", 1) else 0)

    def cb_size(self):
        self.L.rec("Size", (int(self.e.r(3) == self.handle),))
        self.e.set_r(3, len(self.case["file"]))

    def cb_read(self):
        e = self.e
        dest, size, count = e.r(3), e.r(4), e.r(5)
        self.L.rec("Read", (size, count, int(e.r(6) == self.handle), T.s32(dest - self.block)))
        data = self.case["file"][self.position:self.position + size * count]
        if data:
            e.write(dest, data)
        self.position += len(data)
        e.set_r(3, len(data) // size if size else 0)

    def cb_close(self):
        self.L.rec("Close", (int(self.e.r(3) == self.handle),))
        self.e.set_r(3, 0)

    def stubs(self):
        st = CL.Stubs()
        st.real = set(REAL)
        st.internal(UDEBUG, lambda e: self.L.rec("UDebug", (), (e.cstr(e.r(3)), e.cstr(e.r(4)))))

        def swap16(e):
            for i in range(max(T.s32(e.r(4)), 0)):
                e.write(e.r(3) + 2 * i, e.read(e.r(3) + 2 * i, 2)[::-1])

        def swap32(e):
            for i in range(max(T.s32(e.r(4)), 0)):
                e.write(e.r(3) + 4 * i, e.read(e.r(3) + 4 * i, 4)[::-1])

        st.imp("_USwapBlock16__13SamsUtilitiesFPUsl", swap16)
        st.imp("_USwapBlock32__13SamsUtilitiesFPUll", swap32)
        return st


def ppc(e, case):
    side = PpcSide(e, case)
    words = (P_MALLOC, P_FREE, FILE_OPEN, FILE_SIZE, FILE_READ, FILE_CLOSE)
    saved_words = [(a, e.u32(a)) for a in words]
    saved = side.stubs().activate(e)
    try:
        for a, name in zip(words, ("cb_malloc", "cb_free", "cb_open", "cb_size", "cb_read", "cb_close")):
            w32(e, a, e.m3_fns[name])
        name = e.alloc(64, case["name"].encode() + b"\0") if case["name"] is not None else 0
        header = T.call_ppc(e, LOAD, [name, case["flags"]])[0]
        loaded = len(side.L.log)
        image = e.read(header, len(case["file"])) if header else b""
        base, block = header, side.block
        if header and case["free"]:
            T.call_ppc(e, FREE, [header])
    finally:
        CL.Stubs.deactivate(e, saved)
        for a, v in saved_words:
            w32(e, a, v)
    return dict(accepted=int(header != 0), log=side.L.log, loaded=loaded, image=image, base=base, block=block)


def host(lib, case):
    def F(name, result, *args):
        fn = lib[name]
        fn.restype, fn.argtypes = result, list(args)
        return fn

    F("tpw_m3_reserve", None)()
    load = F("tpw_m3_load", C.c_void_p, C.c_char_p, C.c_uint32, C.c_uint32, C.c_int32, C.c_char_p)
    arena = F("tpw_m3_arena_base", C.c_void_p)()
    CL.host_begin(lib, {}, case.get("q"))
    try:
        data = bytes(case["file"])
        header = load(data, len(data), case["flags"], case["skew"],
                      case["name"].encode() if case["name"] is not None else None) or 0
        loaded = C.c_int32.in_dll(lib, "tpw_cl_n").value
        image = C.string_at(header, len(data)) if header else b""
        base = header - arena if header else 0
        block = (F("tpw_m3_block", C.c_void_p)() or 0) - arena if header else 0
        if header and case["free"]:
            F("tpw_m3_free", None, C.c_void_p)(header)
        elif header:
            F("tpw_m3_release", None)()
    finally:
        log = CL.host_end(lib)
    return dict(accepted=int(header != 0), log=log, loaded=loaded, image=image, base=base, block=block)


class LoadTest(FnTest):
    """Results and events compare as they are; the two images field by field."""

    def run_case(self, runner, case):
        emu, lib = runner.emu, runner.lib
        emu.reset_scratch()
        p = self.ppc(emu, case)
        h = self.host(lib, case)
        problems = T.same_values({k: p[k] for k in ("accepted", "log", "loaded")},
                                 {k: h[k] for k in ("accepted", "log", "loaded")}, "", False)
        if p["accepted"] and h["accepted"]:
            problems += compare_images(case, p["image"], p["base"], p["block"], h["image"], h["base"], h["block"])
        return problems[:12]


# ------------------------------------------------------------------ cases


def case_of(r, **given):
    o = dict(magic=MAGIC, version=VERSION, anim_version=ANIM_VERSION, geometry=True, anim=False, junk=False,
             bare=False)
    o.update({k: v for k, v in given.items() if k in o})
    mesh = build(r, o)
    flags = given.get("flags", 0 if o["anim"] else 2)
    return dict(file=mesh.finish(), fields=mesh.fields, flags=flags, skew=given.get("skew", r.randrange(8)),
                name=given.get("name", "data:mesh%d.md2" % r.randrange(100)), free=given.get("free", r.random() < 0.5),
                q=given.get("q"))


def edge():
    r = random.Random(101)
    for skew in range(8):
        yield case_of(r, skew=skew)
    for _ in range(12):
        yield case_of(r, anim=True)
    # Refusals: no name, a file that does not open, not a mesh, versions either side of the
    # one the loader takes with and without "old meshes too", animation against the flags.
    yield case_of(r, name=None)
    yield case_of(r, q={"Open": [0]})
    yield case_of(r, magic=MAGIC ^ 1)
    yield case_of(r, magic=0)
    for version in (0, 0xdc, 0xdd, 0xde, 0xffffffff):
        for flags in (2, 3):
            yield case_of(r, version=version, flags=flags)
            yield case_of(r, version=version, flags=flags & 1, anim=True)
    for anim in (False, True):
        for flags in (0, 1, 2, 3, 4, 6, 7):
            yield case_of(r, anim=anim, flags=flags)
    # The animation's own version: the one it takes, older, newer.
    for anim_version in (0, 0xca, 0xcb, 0xcc, 0xffffffff):
        for junk in (False, True):
            yield case_of(r, anim=True, anim_version=anim_version, junk=junk)
            yield case_of(r, anim=False, anim_version=anim_version, junk=junk)
    # No geometry records (header flag 1 off): only the header's own pointers are fixed.
    for anim in (False, True):
        for _ in range(4):
            yield case_of(r, geometry=False, anim=anim)
    for anim in (False, True):
        for _ in range(4):
            yield case_of(r, bare=True, anim=anim)
    # The texture infos' one-frame flag.
    for _ in range(6):
        yield case_of(r, flags=6)
    for _ in range(40):
        yield case_of(r, anim=r.random() < 0.5, junk=r.random() < 0.3)


def rnd(r):
    anim = r.random() < 0.5
    return case_of(r, anim=anim, junk=r.random() < 0.3, geometry=r.random() < 0.9, bare=r.random() < 0.1,
                   flags=(0 if anim else 2) | r.choice([0, 0, 1, 4, 5]),
                   version=r.choice([VERSION] * 6 + [0xdc]), anim_version=r.choice([ANIM_VERSION] * 6 + [0xca, 0xcc]))


def align_ppc(which):
    def run(e, case):
        value = e.alloc(4, struct.pack(">I", case["value"]))
        moved = e.alloc(4, struct.pack(">I", case["moved"]))
        T.call_ppc(e, which, [value, moved if case["with_moved"] else 0])
        return [e.u32(value), e.u32(moved)]
    return run


def align_host(name):
    def run(lib, case):
        out = (C.c_uint32 * 2)()
        fn = lib[name]
        fn.restype, fn.argtypes = None, [C.c_uint32, C.c_uint32, C.c_int32, C.POINTER(C.c_uint32)]
        fn(case["value"], case["moved"], case["with_moved"], out)
        return list(out)
    return run


def align_edge():
    for value in list(range(0, 18)) + [0x7ffffff8, 0x7ffffff9, 0x7fffffff, 0xfffffff0, 0x12345679]:
        for with_moved in (0, 1):
            yield dict(value=value, moved=0x1000, with_moved=with_moved)


def align_rnd(r):
    return dict(value=r.getrandbits(31), moved=r.getrandbits(32), with_moved=r.randrange(2))


TESTS = [
    LoadTest("LoadM3D2Header and the Fix functions, FreeM3D2Header", ppc, host, edge=edge, rnd=rnd),
    FnTest("AlignTo64Bit (a size)", align_ppc(ALIGN_VALUE), align_host("tpw_m3_align_value"), edge=align_edge,
           rnd=align_rnd),
    FnTest("AlignTo64Bit (an address)", align_ppc(ALIGN_ADDRESS), align_host("tpw_m3_align_address"),
           edge=align_edge, rnd=align_rnd),
]
