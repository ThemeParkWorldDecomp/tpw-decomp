"""The ride animation set's set-up (src/RideAnimInit.cpp, and src/EmbeddedData.cpp): RAInitType,
CorrectNodeFlags1, ResetAnimTimeStats, RAInitNodeAnimNodes, InitRideAnimSet, and the two
embedded data functions. Each case builds the same mesh, header, nodes and animations on the
PowerPC side (big-endian, Mac offsets) and in the host unit (native structures), runs the
function on both and compares the fields it touches."""
import ctypes as C
import math
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_RideAnimInit"
RAI_TYPE = 0x100c3430      # RAInitType(tag_sMesh*, tag_sAnimHeader*)
MASK = 0xffffffff


def setup(runner):
    T.install_std_stubs(runner.emu)


def ws(e, a, values):
    e.write(a, struct.pack(">" + "I" * len(values), *[v & MASK for v in values]))


def stubs_for(entries):
    """A call-log stub set with the unit's own functions left as the original code."""
    st = CL.Stubs()
    st.real.update(entries)
    return st


# ---------------------------------------------------------------- RAInitType

def ppc_type(e, case):
    flags, count, has = case
    header = e.alloc(0xb8)
    ws(e, header + 0x30, [flags])
    mesh = e.alloc(0x40)
    ws(e, mesh + 4, [header])
    anim = e.alloc(0x48)
    e.write(anim + 0x12, struct.pack(">H", count & 0xffff))
    ws(e, anim + 0x2c, [0x5a5a5a5a])        # node animation pointer: read, never followed
    st = stubs_for([RAI_TYPE])
    old = st.activate(e)
    try:
        ret = T.call_ppc(e, RAI_TYPE, [mesh, anim if has else 0])[0]
        return {"ret": [T.s32(ret)], "flags": [e.u32(header + 0x30)]}
    finally:
        CL.Stubs.deactivate(e, old)


def host_type(lib, case):
    flags, count, has = case
    fn = lib.tpw_rai_type
    fn.argtypes = [C.c_uint32, C.c_uint32, C.c_int32, C.POINTER(C.c_uint32)]
    fn.restype = None
    out = (C.c_uint32 * 2)()
    fn(flags, count, has, out)
    return {"ret": [C.c_int32(out[0]).value], "flags": [out[1]]}


def type_case(flags=0, count=0, has=1):
    return (flags & MASK, count & 0xffff, int(bool(has)))


def type_edges():
    for flags in (0, 0x20, 0x1f, 0x3f, 0xffffffff, 0x80000000):
        for count in (0, 1, 2, 0xffff):
            yield type_case(flags, count, 1)
        yield type_case(flags, 5, 0)
    yield type_case(0, 0, 0)


def type_rnd(r):
    return type_case(r.getrandbits(32), r.choice([0, 0, 1, r.getrandbits(16)]), r.randrange(4) != 0)


# ---------------------------------------------------------------- CorrectNodeFlags1
#
# A case is (header flags, nodes, minis, textures, animations): nodes are (flags, face
# texture indices, -1 for none), minis is a count, textures are first words, animations are
# lists of (node animation flags, node index). Node indices past the full nodes name mini
# nodes. Mini node flags stay zero, and so do the bytes past them: the original reads a mini
# node's face count and face set pointer from the next record, and those bytes are in a
# different order on the Mac and on the host, so only zero bytes read the same on both.

CNF = 0x10067cf4


def cnf_case(hdr=0, nodes=(), minis=0, textures=(), anims=()):
    return (hdr & MASK,
            tuple((flags & MASK, tuple(faces)) for flags, faces in nodes),
            minis,
            tuple(textures),
            tuple(tuple((flags & MASK, index) for flags, index in anim) for anim in anims))


def cnf_words(case):
    """The flat words the host unit reads (see tpw_cnf in tests/unit_RideAnimInit.cpp)."""
    hdr, nodes, minis, textures, anims = case
    w = [hdr, len(nodes), minis, len(textures), len(anims)]
    for flags, faces in nodes:
        w += [flags, len(faces)] + [f & MASK for f in faces]
    w += [0] * minis
    w += list(textures)
    for anim in anims:
        w.append(len(anim))
        for flags, index in anim:
            w += [flags, index]
    return [x & MASK for x in w]


def ppc_cnf(e, case):
    hdr, nodes, minis, textures, anims = case
    header = e.alloc(0xb8)
    ws(e, header + 0x30, [hdr])
    e.write(header + 0x44, struct.pack(">H", len(nodes)))
    node_base = e.alloc(len(nodes) * 0xa0 + 0x40)
    mini_base = e.alloc(minis * 0x58 + 0x40)
    tex_base = e.alloc(len(textures) * 8 + 0x40)
    ws(e, header + 0x70, [node_base])
    ws(e, header + 0x74, [mini_base])
    for t, value in enumerate(textures):
        e.write(tex_base + 8 * t, struct.pack(">H", value))
    for i, (flags, faces) in enumerate(nodes):
        node = node_base + i * 0xa0
        ws(e, node, [flags])
        e.write(node + 0x5a, struct.pack(">H", len(faces)))
        if faces:
            sets = e.alloc(len(faces) * 0x10)
            ws(e, node + 0x6c, [sets])
            for f, index in enumerate(faces):
                ws(e, sets + f * 0x10, [tex_base + 8 * index if index >= 0 else 0])
    arr = e.alloc(4 * len(anims) + 4)
    for k, anim in enumerate(anims):
        a = e.alloc(0x48)
        e.write(a + 0x12, struct.pack(">H", len(anim)))
        na = e.alloc(len(anim) * 0x40 + 0x40)
        ws(e, a + 0x2c, [na])
        for b, (flags, index) in enumerate(anim):
            ws(e, na + b * 0x40 + 4, [flags])
            e.write(na + b * 0x40 + 0x14, struct.pack(">H", index))
        ws(e, arr + 4 * k, [a])
    mesh = e.alloc(0x40)
    ws(e, mesh + 4, [header])
    rset = e.alloc(0x100)
    ws(e, rset + 8, [mesh])
    ws(e, rset + 0xa8, [len(anims)])
    ws(e, rset + 0xac, [arr])
    st = stubs_for([CNF])
    old = st.activate(e)
    try:
        T.call_ppc(e, CNF, [rset])
        out = [e.u32(header + 0x30)]
        out += [e.u32(node_base + i * 0xa0) for i in range(len(nodes))]
        out += [e.u32(mini_base + j * 0x58) for j in range(minis)]
        out += [struct.unpack(">H", e.read(tex_base + 8 * t, 2))[0] for t in range(len(textures))]
        return {"out": out}
    finally:
        CL.Stubs.deactivate(e, old)


def host_cnf(lib, case):
    words = cnf_words(case)
    hdr, nodes, minis, textures, anims = case
    fn = lib.tpw_cnf
    fn.argtypes = [C.POINTER(C.c_uint32), C.POINTER(C.c_uint32)]
    fn.restype = None
    out = (C.c_uint32 * (1 + len(nodes) + minis + len(textures)))()
    fn((C.c_uint32 * len(words))(*words), out)
    return {"out": list(out)}


def cnf_edges():
    yield cnf_case()
    yield cnf_case(hdr=0x1234)
    yield cnf_case(hdr=0xffffffff, nodes=[(0xffffffff, [])])
    yield cnf_case(nodes=[(5, [])])
    yield cnf_case(nodes=[(0, [])], anims=[[(0x1000, 0)]])
    yield cnf_case(nodes=[(0x40, [0, 1])], textures=[0x1111, 0x2222], anims=[[(0x10000, 0)]])
    yield cnf_case(nodes=[(0, [-1, 0, -1])], textures=[0x7fff], anims=[[(0x10000, 0)]])
    yield cnf_case(nodes=[(0, [0]), (0, [0])], textures=[5], anims=[[(0x10000, 0), (0x10000, 1)]])
    yield cnf_case(nodes=[(0, [0])], minis=2, textures=[9], anims=[[(0x10000, 1), (0x11000, 2)]])
    yield cnf_case(nodes=[(0, [0])], minis=1, textures=[9], anims=[[(0x80, 1)]])
    yield cnf_case(nodes=[(0, [0])], textures=[0xffff], anims=[[(0x10000, 0)]])
    yield cnf_case(nodes=[(0, [])], anims=[[], []])
    yield cnf_case(nodes=[(1, [0])], textures=[3], anims=[[(0x1000, 0), (0x10000, 0)], [(0x11000, 0)]])


def cnf_rnd(r):
    n = r.randrange(0, 5)
    m = r.randrange(0, 3)
    t = r.randrange(0, 4)
    choices = [-1] + list(range(t))
    nodes = [(r.getrandbits(32), [r.choice(choices) for _ in range(r.randrange(0, 4))]) for _ in range(n)]
    anims = []
    for _ in range(r.randrange(0, 4)):
        anim = []
        if n + m:
            for _ in range(r.randrange(0, 4)):
                flags = r.choice([0, 0x1000, 0x10000, 0x11000, 0x80, r.getrandbits(32)])
                anim.append((flags, r.randrange(n + m)))
        anims.append(anim)
    textures = [r.getrandbits(16) for _ in range(t)]
    return cnf_case(r.getrandbits(32), nodes, m, textures, anims)


# ---------------------------------------------------------------- ResetAnimTimeStats
#
# A case is (control words, frame start f04, frame end f08, has animation, mode, limit,
# TPGlobals words 0x4010 and 0x4018). The control is 14 words (0x38 bytes), read and written
# as words on both sides; the float fields are compared as bit patterns.

RATS = 0x100c5a90
TPG_POINTER = 0x10260660      # the TOC cell holding gTPGlobals' address


def rats_case(ctl, f04=0, f08=0, has=1, mode=0, limit=0.0, g4010=0, g4018=0):
    assert len(ctl) == 14
    return (tuple(v & MASK for v in ctl), f04 & MASK, f08 & MASK, int(bool(has)), mode,
            T.f32(limit) if not math.isnan(limit) else limit, g4010 & MASK, g4018 & MASK)


def rats_ctl(flags=0, f04=0, f08=0, rate=1.0, t10=0, t14=0, t18=0, f1c=0.0, f20=0.0, f24=0):
    """The control's 14 words with the named fields set (zero elsewhere)."""
    w = [flags, f04, 0, T.f32_bits(rate), t10, t14, t18, T.f32_bits(f1c), T.f32_bits(f20), f24, 0, 0, 0, 0]
    return [v & MASK for v in w]


def ppc_rats(e, case):
    ctl_words, f04, f08, has, mode, limit, g4010, g4018 = case
    ctl = e.alloc(0x38)
    ws(e, ctl, ctl_words)
    anim = e.alloc(0x48)
    ws(e, anim + 4, [f04, f08])
    gl = e.u32(TPG_POINTER)
    saved = [(a, e.read(a, 4)) for a in (gl + 0x4010, gl + 0x4018)]
    ws(e, gl + 0x4010, [g4010])
    ws(e, gl + 0x4018, [g4018])
    st = stubs_for([RATS])
    old = st.activate(e)
    try:
        T.call_ppc(e, RATS, [ctl, anim if has else 0, mode, T.F(limit)])
        return {"control": list(struct.unpack(">14I", e.read(ctl, 0x38)))}
    finally:
        CL.Stubs.deactivate(e, old)
        for a, data in saved:
            e.write(a, data)


def host_rats(lib, case):
    ctl_words, f04, f08, has, mode, limit, g4010, g4018 = case
    fn = lib.tpw_rats
    fn.argtypes = [C.POINTER(C.c_uint32), C.c_uint32, C.c_uint32, C.c_int32, C.c_int32, C.c_float,
                   C.c_uint32, C.c_uint32, C.POINTER(C.c_uint32)]
    fn.restype = None
    out = (C.c_uint32 * 14)()
    fn((C.c_uint32 * 14)(*ctl_words), f04, f08, has, mode, limit, g4010, g4018, out)
    return {"control": list(out)}


RATS_LIMITS = (0.0, 1.0, 2.5, 30.0, 1000.0, 1e6, -1.0, math.inf, math.nan)
RATS_RATES = (1.0, 0.0, -1.0, 2.5)
RATS_RANGES = ((0, 0), (5, 3), (0, 0xffffffff), (2, 10), (7, 7))


def rats_edges():
    for has in (0, 1):
        for mode in (0, 1, -1):
            for flags in (0, 0x40, 0xffffffbf):
                for limit in RATS_LIMITS:
                    for f04, f08 in RATS_RANGES[:3]:
                        yield rats_case(rats_ctl(flags=flags, t14=1000, rate=RATS_RATES[0]), f04, f08,
                                        has, mode, limit, 0x11223344, 0x55667788)
    for rate in RATS_RATES:
        for limit in RATS_LIMITS:
            yield rats_case(rats_ctl(t14=7, rate=rate, f1c=1.0), 0, 90, 1, 0, limit)
    for f04, f08 in RATS_RANGES:
        yield rats_case(rats_ctl(t14=-5, rate=2.0), f04, f08, 1, 0, 3.0)
    yield rats_case(rats_ctl(flags=0x40, t14=1, f1c=9.0, f20=8.0), 0, 0, 0, 0, 0.0)
    yield rats_case(rats_ctl(flags=0x40, t14=1), 1, 2, 1, 2, 0.0, 0xdeadbeef, 0x0badf00d)
    yield rats_case(rats_ctl(flags=0, t14=1), 1, 2, 1, 2, 0.0, 0xdeadbeef, 0x0badf00d)
    # Limit and rate pairs whose frame count truncates differently when the rate divides the
    # scaled limit than when it divides the product 30 * rate (the one-ulp difference shows
    # only next to a whole number of frames).
    for limit, rate in ((1.9500000476837158, 4.333333492279053), (3.509999990463257, 4.333333492279053),
                        (3.9000000953674316, 4.333333492279053), (7.019999980926514, 4.333333492279053)):
        yield rats_case(rats_ctl(t14=100, rate=rate), 0, 1000, 1, 0, limit)


def rats_rnd(r):
    ctl = [r.getrandbits(32) for _ in range(14)]
    ctl[3] = T.f32_bits(r.choice([T.rfloat_mod(r, 0.1, 10.0), T.rfloat(r), 0.0, 1.0]))
    ctl[7] = T.f32_bits(T.rfloat(r))
    ctl[8] = T.f32_bits(T.rfloat(r))
    frames = r.choice([0, 1, r.randrange(0, 10000), r.getrandbits(32)])
    f04 = r.getrandbits(32) if r.random() < 0.3 else r.randrange(0, 5000)
    f08 = (f04 + frames) & MASK
    return rats_case(ctl, f04, f08, r.randrange(4) != 0, r.choice([0, 0, 0, 1, -1, r.getrandbits(32)]),
                     T.rfloat(r, nan=True), r.getrandbits(32), r.getrandbits(32))


# ---------------------------------------------------------------- TPSetupEmbeddedHeirarchy
#
# A case is (first record mF46, record count, header record flags, the mesh set's record flags,
# nodes, root): a node is (mF50 index, flags, child, sibling) with node numbers for the child
# and sibling (NONE for none), and the root is a node number. Every node is reachable from the
# root in the random cases.

TPSET = 0x101ff674
NONE = 0xffffffff


def set_case(first=0, emb=(), rec=None, nodes=(), root=0):
    rec = list(rec) if rec is not None else [0] * len(emb)
    return (first & 0xffff, len(emb), tuple(v & MASK for v in emb), tuple(v & MASK for v in rec),
            tuple((index & 0xffff, flags & MASK, child & MASK, sibling & MASK)
                  for index, flags, child, sibling in nodes), root)


def set_words(case):
    first, count, emb, rec, nodes, root = case
    w = [first, count] + list(emb) + list(rec) + [len(nodes)]
    for index, flags, child, sibling in nodes:
        w += [index, flags, child, sibling]
    return [v & MASK for v in w + [root]]


def ppc_set(e, case):
    first, count, emb, rec, nodes, root = case
    header = e.alloc(0xb8)
    e.write(header + 0x46, struct.pack(">H", first))
    e.write(header + 0x48, struct.pack(">H", count))
    embedded = e.alloc(count * 0x14 + 0x20)
    ws(e, header + 0x7c, [embedded])
    for k, flags in enumerate(emb):
        ws(e, embedded + 0x14 * k, [flags])
    set_obj = e.alloc(0x14)
    records = e.alloc(count * 0x14 + 0x20)
    ws(e, set_obj + 4, [records])
    for k, flags in enumerate(rec):
        ws(e, records + 0x14 * k, [flags])
    mesh = e.alloc(0x40)
    ws(e, mesh + 0x28, [set_obj])
    node_base = e.alloc(len(nodes) * 0xa0 + 0x20)
    def at(i):
        return node_base + 0xa0 * i
    for i, (index, flags, child, sibling) in enumerate(nodes):
        n = at(i)
        e.write(n + 0x50, struct.pack(">H", index))
        ws(e, n, [flags])
        ws(e, n + 0x0c, [at(child) if child != NONE else 0])
        ws(e, n + 0x08, [at(sibling) if sibling != NONE else 0])
    st = stubs_for([TPSET])
    old = st.activate(e)
    try:
        ret = T.call_ppc(e, TPSET, [mesh, at(root), header])[0]
        return {"ret": [T.s32(ret)],
                "nodes": [e.u32(at(i)) for i in range(len(nodes))],
                "records": [e.u32(records + 0x14 * k) for k in range(count)]}
    finally:
        CL.Stubs.deactivate(e, old)


def host_set(lib, case):
    words = set_words(case)
    first, count, emb, rec, nodes, root = case
    fn = lib.tpw_set
    fn.argtypes = [C.POINTER(C.c_uint32), C.POINTER(C.c_uint32)]
    fn.restype = None
    out = (C.c_uint32 * (1 + len(nodes) + count))()
    fn((C.c_uint32 * len(words))(*words), out)
    return {"ret": [C.c_int32(out[0]).value],
            "nodes": list(out[1:1 + len(nodes)]),
            "records": list(out[1 + len(nodes):])}


def set_edges():
    yield set_case(0, (), nodes=[(0, 0, NONE, NONE)])
    yield set_case(0, (0,), nodes=[(0, 0, NONE, NONE)])
    yield set_case(0, (0x30,), nodes=[(0, 0, NONE, NONE)])
    yield set_case(0, (0x10,), nodes=[(0, 0, NONE, NONE)])
    yield set_case(0, (0x20,), nodes=[(0, 0x40000000, NONE, NONE)])
    yield set_case(2, (0, 0x10), rec=(5, 6), nodes=[(3, 1, 1, NONE), (2, 0, NONE, NONE)])
    yield set_case(2, (0x30, 0), rec=(5, 6), nodes=[(3, 1, 1, NONE), (2, 0, NONE, NONE)])
    yield set_case(2, (0, 0), nodes=[(3, 0, 1, NONE), (2, 0, NONE, 2), (4, 0, NONE, NONE)])
    yield set_case(2, (0, 0), nodes=[(1, 0, 1, NONE), (2, 0, NONE, NONE)])
    yield set_case(1, (0,), nodes=[(2, 0, 1, NONE), (1, 0, NONE, NONE)])
    yield set_case(0, (0, 0), nodes=[(0, 0, 1, NONE), (1, 0, 2, NONE), (0, 0, NONE, 3), (2, 0, NONE, NONE)])
    yield set_case(0, (0x40,), nodes=[(0, 0, 1, NONE), (0, 0x0, NONE, NONE)])
    yield set_case(0, (0x30, 0x20), nodes=[(0, 0, 1, 2), (0, 0, NONE, NONE), (1, 0, NONE, NONE)])


def set_rnd(r):
    first = r.randrange(0, 4)
    count = r.randrange(0, 5)
    emb = [r.choice([0, 0x30, 0x10, 0x20, 0x1, 0x2, r.getrandbits(32)]) for _ in range(count)]
    rec = [r.getrandbits(32) for _ in range(count)]
    n = r.randrange(1, 7)
    parent = [None] + [r.randrange(0, i) for i in range(1, n)]
    child = [NONE] * n
    sibling = [NONE] * n
    for i in range(n - 1, 0, -1):          # children in increasing order, chained as siblings
        p = parent[i]
        sibling[i] = child[p]
        child[p] = i
    nodes = []
    for i in range(n):
        index = r.randrange(0, first + count + 2)
        nodes.append((index, r.getrandbits(32), child[i], sibling[i]))
    return set_case(first, emb, rec, nodes, 0)


# ---------------------------------------------------------------- RAInitNodeAnimNodes
#
# A case is (mesh flags, nodes, minis, first kind, animation): a node is (mF50, flags,
# vertex count); the animation is a list of node animations (flags, index, has morph points).
# Each node animation starts at the first node. Flag 0x4000 entries have full-node indices
# only: the original reads a mini node's vertex count from the record after it, and that is
# another byte order on the host (see the note above CorrectNodeFlags1).

RNAI = 0x100c32ac
SWAP = "_USwapBlock32__13SamsUtilitiesFPUll"


def rnai_case(mesh_flags=0, nodes=(), minis=0, first_kind=0, anims=()):
    return (mesh_flags & MASK,
            tuple((f50 & 0xffff, flags & MASK, count & 0xffff) for f50, flags, count in nodes),
            minis,
            first_kind & MASK,
            tuple((flags & MASK, index & 0xffff, int(bool(has))) for flags, index, has in anims))


def rnai_words(case):
    mesh_flags, nodes, minis, first_kind, anims = case
    w = [mesh_flags, len(nodes), minis, first_kind, len(anims)]
    for f50, flags, count in nodes:
        w += [f50, flags, count]
    for flags, index, has in anims:
        w += [flags, index, has]
    return [v & MASK for v in w]


def ppc_rnai(e, case):
    mesh_flags, nodes, minis, first_kind, anims = case
    e.cl = CL.PpcLog({})
    header = e.alloc(0xb8)
    e.write(header + 0x44, struct.pack(">H", len(nodes)))
    node_base = e.alloc(len(nodes) * 0xa0 + 0x40)
    mini_base = e.alloc(minis * 0x58 + 0x40)
    ws(e, header + 0x70, [node_base])
    ws(e, header + 0x74, [mini_base])

    def node_at(i):
        return node_base + 0xa0 * i

    def mini_at(j):
        return mini_base + 0x58 * j
    for i, (f50, flags, count) in enumerate(nodes):
        n = node_at(i)
        e.write(n + 0x50, struct.pack(">H", f50))
        ws(e, n, [flags])
        e.write(n + 0x58, struct.pack(">H", count))
    mesh = e.alloc(0x40)
    ws(e, mesh, [mesh_flags])
    ws(e, mesh + 4, [header])
    start = node_at(0) if nodes else mini_at(0)
    anim_base = e.alloc(len(anims) * 0x40 + 0x40)
    blocks = []
    for b, (flags, index, has) in enumerate(anims):
        na = anim_base + 0x40 * b
        ws(e, na + 4, [flags])
        e.write(na + 0x14, struct.pack(">H", index))
        ws(e, na + 0x3c, [start])
        if has:
            block = e.alloc(16)
            blocks.append(block)
            morph = e.alloc(0xc)
            ws(e, morph, [block])
            ws(e, na + 0x28, [morph])
    anim = e.alloc(0x48)
    e.write(anim + 0x12, struct.pack(">H", len(anims)))
    ws(e, anim + 0x2c, [anim_base])

    def swap(e):
        arg = e.r(3)
        index = blocks.index(arg) if arg in blocks else -1
        e.cl.rec("Swap", [index, T.s32(e.r(4))])
    st = stubs_for([RNAI])
    st.imp(SWAP, swap)
    old = st.activate(e)
    try:
        T.call_ppc(e, RNAI, [mesh, anim, first_kind])
        targets = []
        for b in range(len(anims)):
            target = e.u32(anim_base + 0x40 * b + 0x3c)
            ident = -1
            for i in range(len(nodes)):
                if target == node_at(i):
                    ident = i
            for j in range(minis):
                if target == mini_at(j):
                    ident = len(nodes) + j
            targets.append(ident & MASK)
        return {"nodes": [e.u32(node_at(i)) for i in range(len(nodes))],
                "minis": [e.u32(mini_at(j)) for j in range(minis)],
                "targets": targets,
                "swaps": e.cl.log}
    finally:
        CL.Stubs.deactivate(e, old)


def host_rnai(lib, case):
    mesh_flags, nodes, minis, first_kind, anims = case
    words = rnai_words(case)
    fn = lib.tpw_rnai
    fn.argtypes = [C.POINTER(C.c_uint32), C.POINTER(C.c_uint32)]
    fn.restype = None
    out = (C.c_uint32 * (len(nodes) + minis + len(anims) + 1))()
    CL.host_begin(lib, {})
    fn((C.c_uint32 * len(words))(*words), out)
    log = CL.host_end(lib)
    n, m, b = len(nodes), minis, len(anims)
    return {"nodes": list(out[:n]),
            "minis": list(out[n:n + m]),
            "targets": list(out[n + m:n + m + b]),
            "swaps": log}


def rnai_edges():
    yield rnai_case(0, [(0, 0, 2)], 0, 1, [(0, 0, 0)])
    yield rnai_case(0, [(0, 0x100, 2)], 0, 0, [(0, 0, 0)])
    yield rnai_case(0, [(0, 0x100, 2)], 0, 1, [(0, 0, 0)])
    yield rnai_case(0, [(0, 0, 3)], 0, 1, [(0x4000, 0, 1)])
    yield rnai_case(4, [(7, 0, 1), (3, 0, 1)], 0, 1, [(0, 3, 0)])
    yield rnai_case(4, [(7, 0, 1), (3, 0, 1)], 0, 1, [(0, 9, 0)])
    yield rnai_case(4, [(7, 0, 1), (3, 0, 1)], 0, 1, [(0x4000, 3, 1)])
    yield rnai_case(0, [(0, 0, 0)], 2, 1, [(0, 2, 0), (0, 1, 0)])
    yield rnai_case(0, [(0, 0, 0)], 0, 1, [(0x4000, 0, 1), (0x4000, 0, 1)])
    yield rnai_case(0, [(5, 1, 2), (6, 0x100, 0)], 1, 0, [(0, 1, 0), (0, 0, 0), (0, 2, 0)])
    yield rnai_case(0, [], 1, 1, [(0, 0, 0)])
    yield rnai_case(0, [], 0, 1, [])
    yield rnai_case(0, [(0, 0, 0)], 0, 1, [])
    yield rnai_case(4, [(0, 0, 0)], 0, 0, [(0x1000, 0, 0)])
    yield rnai_case(0xffffffff, [(0xffff, 0xffffffff, 0xffff)], 1, 1, [(0xffffffff, 0xffff, 1)])


def rnai_rnd(r):
    n = r.randrange(0, 5)
    m = r.randrange(0, 3)
    nodes = [(r.randrange(0, 8), r.choice([r.getrandbits(32), 0, 0x100, 0x800]), r.randrange(0, 4))
             for _ in range(n)]
    anims = []
    if n + m:
        for _ in range(r.randrange(0, 4)):
            flags = r.choice([0, 0x4000, 0x4000 | r.getrandbits(16), r.getrandbits(32), 0x1000])
            if flags & 0x4000 and n == 0:
                flags &= ~0x4000
            index = r.randrange(n) if flags & 0x4000 else r.randrange(n + m)
            anims.append((flags, index, flags & 0x4000 != 0))
    mesh_flags = r.choice([0, 4, 4 | r.getrandbits(32), r.getrandbits(32)])
    return rnai_case(mesh_flags, nodes, m, r.choice([0, 1, 2]), anims)


# ---------------------------------------------------------------- InitRideAnimSet
#
# A case is (init, mode, keepNodes, set flags, mesh flags, nodes, minis, time controls,
# animations, groups): nodes are (mF50, flags, vertex count), minis are flags, a time control is
# its 14 words, an animation is a list of node animations (flags, index, has morph), and a group
# is the animation numbers of its entries (NONE for a null entry), one list per kind. The
# animations are the ones RAInitNodeAnimNodes sees, so the same rules hold: flag 0x4000 only on
# full node indices. Each node animation starts at the first node.

RIRAS = 0x100c7768
NONE_ANIM = -1


def iras_case(init=0, mode=0, keep=0, set_flags=0, mesh_flags=0, nodes=(), minis=(), controls=(),
              anims=(), groups=None):
    groups = [list(g) for g in (groups or [])] + [[]] * (12 - len(groups or []))
    return (init & MASK, mode & MASK, keep & MASK, set_flags & MASK, mesh_flags & MASK,
            tuple((f50 & 0xffff, flags & MASK, count & 0xffff) for f50, flags, count in nodes),
            tuple(v & MASK for v in minis),
            tuple(tuple(v & MASK for v in control) for control in controls),
            tuple(tuple((flags & MASK, index & 0xffff, int(bool(has))) for flags, index, has in anim)
                  for anim in anims),
            tuple(tuple(v & MASK for v in g) for g in groups))


def iras_words(case):
    init, mode, keep, set_flags, mesh_flags, nodes, minis, controls, anims, groups = case
    w = [init, mode, keep, set_flags, mesh_flags, len(nodes), len(minis), len(controls), len(anims)]
    for control in controls:
        w += list(control)
    for f50, flags, count in nodes:
        w += [f50, flags, count]
    w += list(minis)
    for anim in anims:
        w.append(len(anim))
        for flags, index, has in anim:
            w += [flags, index, has]
    for g in groups:
        w.append(len(g))
        w += [NONE_ANIM & MASK if index == NONE_ANIM else index for index in g]
    return [v & MASK for v in w]


def ppc_iras(e, case):
    init, mode, keep, set_flags, mesh_flags, nodes, minis, controls, anims, groups = case
    e.cl = CL.PpcLog({})
    header = e.alloc(0xb8)
    e.write(header + 0x42, struct.pack(">H", len(nodes) + len(minis)))
    e.write(header + 0x44, struct.pack(">H", len(nodes)))
    node_base = e.alloc(len(nodes) * 0xa0 + 0x40)
    mini_base = e.alloc(len(minis) * 0x58 + 0x40)
    ws(e, header + 0x70, [node_base])
    ws(e, header + 0x74, [mini_base])

    def node_at(i):
        return node_base + 0xa0 * i

    def mini_at(j):
        return mini_base + 0x58 * j
    for i, (f50, flags, count) in enumerate(nodes):
        n = node_at(i)
        e.write(n + 0x50, struct.pack(">H", f50))
        ws(e, n, [flags])
        e.write(n + 0x58, struct.pack(">H", count))
    for j, flags in enumerate(minis):
        ws(e, mini_at(j), [flags])
    mesh = e.alloc(0x40)
    ws(e, mesh, [mesh_flags])
    ws(e, mesh + 4, [header])
    controls_base = e.alloc(len(controls) * 0x38 + 0x20)
    for t, control in enumerate(controls):
        ws(e, controls_base + 0x38 * t, list(control))
    start = node_at(0) if nodes else mini_at(0)
    blocks = []
    anim_addrs = []
    for anim in anims:
        a = e.alloc(0x48)
        anim_addrs.append(a)
        e.write(a + 0x12, struct.pack(">H", len(anim)))
        na_base = e.alloc(len(anim) * 0x40 + 0x40)
        ws(e, a + 0x2c, [na_base])
        for b, (flags, index, has) in enumerate(anim):
            na = na_base + 0x40 * b
            ws(e, na + 4, [flags])
            e.write(na + 0x14, struct.pack(">H", index))
            ws(e, na + 0x3c, [start])
            if has:
                block = e.alloc(16)
                blocks.append(block)
                morph = e.alloc(0xc)
                ws(e, morph, [block])
                ws(e, na + 0x28, [morph])
    rset = e.alloc(0x100)
    ws(e, rset + 4, [set_flags])
    ws(e, rset + 8, [mesh])
    e.write(rset + 0xe, struct.pack(">H", len(controls)))
    ws(e, rset + 0x10, [controls_base])
    for kind, g in enumerate(groups):
        group = rset + 0x18 + 12 * kind
        ws(e, group + 4, [len(g)])
        entries = e.alloc(len(g) * 8 + 8)
        ws(e, group + 8, [entries])
        for k, index in enumerate(g):
            ws(e, entries + 8 * k, [k, anim_addrs[index] if index != MASK else 0])

    def swap(e):
        arg = e.r(3)
        index = blocks.index(arg) if arg in blocks else -1
        e.cl.rec("Swap", [index, T.s32(e.r(4))])
    st = stubs_for([RIRAS])
    st.imp(SWAP, swap)
    old = st.activate(e)
    try:
        ret = T.call_ppc(e, RIRAS, [rset, init, mode, keep])[0]
        targets = []
        for a, anim in zip(anim_addrs, anims):
            na_base = e.u32(a + 0x2c)
            for b in range(len(anim)):
                target = e.u32(na_base + 0x40 * b + 0x3c)
                ident = -1
                for i in range(len(nodes)):
                    if target == node_at(i):
                        ident = i
                for j in range(len(minis)):
                    if target == mini_at(j):
                        ident = len(nodes) + j
                targets.append(ident & MASK)
        return {"ret": [T.s32(ret)],
                "set": [e.u32(rset + 4)],
                "controls": [v for t in range(len(controls))
                             for v in struct.unpack(">14I", e.read(controls_base + 0x38 * t, 0x38))],
                "nodes": [e.u32(node_at(i)) for i in range(len(nodes))],
                "minis": [e.u32(mini_at(j)) for j in range(len(minis))],
                "targets": targets,
                "swaps": e.cl.log}
    finally:
        CL.Stubs.deactivate(e, old)


def host_iras(lib, case):
    init, mode, keep, set_flags, mesh_flags, nodes, minis, controls, anims, groups = case
    words = iras_words(case)
    count = 2 + 14 * len(controls) + len(nodes) + len(minis) + sum(len(a) for a in anims)
    fn = lib.tpw_iras
    fn.argtypes = [C.POINTER(C.c_uint32), C.POINTER(C.c_uint32)]
    fn.restype = None
    out = (C.c_uint32 * count)()
    CL.host_begin(lib, {})
    fn((C.c_uint32 * len(words))(*words), out)
    log = CL.host_end(lib)
    o = 0
    res = {"ret": [C.c_int32(out[0]).value], "set": [out[1]]}
    o = 2
    res["controls"] = list(out[o:o + 14 * len(controls)])
    o += 14 * len(controls)
    res["nodes"] = list(out[o:o + len(nodes)])
    o += len(nodes)
    res["minis"] = list(out[o:o + len(minis)])
    o += len(minis)
    res["targets"] = list(out[o:o + sum(len(a) for a in anims)])
    res["swaps"] = log
    return res


def iras_edges():
    empty = [[]] * 12
    yield iras_case(groups=empty)
    yield iras_case(keep=1, groups=empty)
    yield iras_case(keep=1, set_flags=0x10, groups=empty)
    yield iras_case(init=1, controls=[[0xffffffff] * 14, [1] * 14], groups=empty)
    yield iras_case(mode=1, controls=[[0] * 14, [0x40] * 14], groups=empty)
    yield iras_case(mode=1, init=1, controls=[[7] * 14], groups=empty)
    yield iras_case(keep=0, nodes=[(1, 0x100, 2), (2, 0, 0)], minis=(0x100, 0), groups=empty)
    yield iras_case(keep=0, nodes=[(1, 0x100, 2)], groups=[[]] + [[]] * 10 + [[NONE_ANIM]])
    yield iras_case(keep=0, nodes=[(1, 0, 2)], anims=[[(0, 0, 0)]], groups=[[0]] + [[]] * 11)
    yield iras_case(keep=1, nodes=[(1, 0, 2)], anims=[[(0, 0, 0)]], groups=[[0]] + [[]] * 11)
    yield iras_case(keep=0, nodes=[(5, 0, 0), (6, 0, 0)], anims=[[(0, 6, 0)], [(0x1000, 5, 0)]],
                    groups=[[0, NONE_ANIM], [1], [], [], [], [], [], [], [], [], [], []])
    yield iras_case(keep=0, nodes=[(1, 0, 3)], anims=[[(0x4000, 0, 1)], [(0x4000, 0, 1)]],
                    groups=[[0], [1]] + [[]] * 10)
    yield iras_case(keep=0, nodes=[(1, 0, 3)], minis=(0,), anims=[[(0, 1, 0)]],
                    groups=[[0]] + [[]] * 11)
    yield iras_case(keep=0, nodes=[(1, 0, 3)], minis=(0x800,), anims=[[(0, 1, 0)]],
                    groups=[[], [0]] + [[]] * 10)
    yield iras_case(mode=1, keep=1, set_flags=0x20, controls=[[1] * 14], nodes=[(0, 0x40, 1)],
                    anims=[[(0x10000, 0, 0)]], groups=[[0]] * 12)


def iras_rnd(r):
    n = r.randrange(0, 4)
    m = r.randrange(0, 3)
    nodes = [(r.randrange(0, 8), r.choice([r.getrandbits(32), 0, 0x100]), r.randrange(0, 4)) for _ in range(n)]
    minis = [r.choice([0, 0x100, r.getrandbits(32)]) for _ in range(m)]
    controls = [[r.getrandbits(32) for _ in range(14)] for _ in range(r.randrange(0, 3))]
    anims = []
    for _ in range(r.randrange(0, 4)):
        anim = []
        if n + m:
            for _ in range(r.randrange(0, 3)):
                flags = r.choice([0, 0x4000, 0x1000, 0x10000, r.getrandbits(32)])
                if flags & 0x4000 and n == 0:
                    flags &= ~0x4000
                index = r.randrange(n) if flags & 0x4000 else r.randrange(n + m)
                anim.append((flags, index, flags & 0x4000 != 0))
        anims.append(anim)
    groups = []
    for _ in range(12):
        entries = []
        for _ in range(r.choice([0, 0, 0, 1, 2])):
            entries.append(r.randrange(len(anims)) if anims and r.random() < 0.8 else NONE_ANIM)
        groups.append(entries)
    return iras_case(r.getrandbits(2), r.choice([0, 0, 1, -1]), r.choice([0, 0, 1, 0x80000000]),
                     r.getrandbits(32), r.choice([0, 4, r.getrandbits(32)]), nodes, minis, controls, anims,
                     groups)


# ---------------------------------------------------------------- TPInitialiseEmbeddedData
#
# A case is (first record mF46, entry count, full nodes, mini nodes, entry flags, nodes): a node
# is (flags, mF50, mF04 link, child, sibling), node numbers are full nodes first, and node 0 is
# the root. Node number mF46 + i belongs to entry i, so first + count stays within the nodes.
# The engine's allocator (0x10064bd4) is a stub that records each call (its order, the flags,
# and the size of the third call) and gives zeroed memory; the blocks are compared by number.

IED = 0x101ff7e4
MRA = 0x10064bd4


def ied_case(first=0, count=0, full=1, minis=0, emb=(), nodes=()):
    return (first & 0xffff, count, full, minis, tuple(v & MASK for v in emb),
            tuple((flags & MASK, f50 & 0xffff, link, child & MASK, sibling & MASK)
                  for flags, f50, link, child, sibling in nodes))


def ied_words(case):
    first, count, full, minis, emb, nodes = case
    w = [first, count, full, minis] + list(emb)
    for flags, f50, link, child, sibling in nodes:
        w += [flags, f50, link, child, sibling]
    return [v & MASK for v in w]


def block_where(blk, first_block, blocks):
    """The block number a record points at (0xffffffff for none, 0xfffffffe for a stray pointer)."""
    if blk == 0:
        return MASK
    if first_block is not None and first_block <= blk < first_block + 0x40 * blocks and (blk - first_block) % 0x40 == 0:
        return (blk - first_block) // 0x40
    return 0xfffffffe


def ppc_ied(e, case):
    first, count, full, minis, emb, nodes = case
    e.cl = CL.PpcLog({})
    header = e.alloc(0xb8)
    e.write(header + 0x46, struct.pack(">H", first))
    e.write(header + 0x48, struct.pack(">H", count))
    e.write(header + 0x44, struct.pack(">H", full))
    embedded = e.alloc(count * 0x14 + 0x20)
    ws(e, header + 0x7c, [embedded])
    for k, flags in enumerate(emb):
        ws(e, embedded + 0x14 * k, [flags])
    node_base = e.alloc(full * 0xa0 + 0x20)
    mini_base = e.alloc(minis * 0x58 + 0x20)
    ws(e, header + 0x70, [node_base])
    ws(e, header + 0x74, [mini_base])

    def node_at(i):
        return node_base + 0xa0 * i if i < full else mini_base + 0x58 * (i - full)
    for i, (flags, f50, link, child, sibling) in enumerate(nodes):
        n = node_at(i)
        ws(e, n, [flags])
        e.write(n + 0x50, struct.pack(">H", f50))
        ws(e, n + 4, [node_at(link)])
        ws(e, n + 8, [node_at(sibling) if sibling != NONE else 0])
        ws(e, n + 0xc, [node_at(child) if child != NONE else 0])
    ws(e, header + 0x78, [node_at(0)])
    mesh = e.alloc(0x40)
    ws(e, mesh + 4, [header])
    allocs = []

    def alloc(e):
        size, flags = e.r(3), e.r(4)
        ordinal = len(allocs)
        e.cl.rec("Alloc", [ordinal, flags, size if ordinal == 2 else 0])
        addr = e.alloc(size)
        allocs.append(addr)
        e.set_r(3, addr)
    st = stubs_for([IED, TPSET])
    st.internal(MRA, alloc)
    old = st.activate(e)
    try:
        T.call_ppc(e, IED, [mesh])
        obj = e.u32(mesh + 0x28)
        if obj == 0:
            set_vals, records = [0] * 6, []
        else:
            flags, blocks_count = e.u32(obj), e.u32(obj + 0xc)
            recs, blocks = e.u32(obj + 4), e.u32(obj + 0x10)
            a0 = allocs[0] if len(allocs) > 0 else None
            a1 = allocs[1] if len(allocs) > 1 else None
            a2 = allocs[2] if len(allocs) > 2 else None
            set_vals = [1, flags, blocks_count, int(obj == a0),
                        int(recs == (a1 if a1 is not None else 0)), int(blocks == (a2 if a2 is not None else 0))]
            records = []
            for i in range(count):
                rec = recs + 0x14 * i
                records += [e.u32(rec), e.u32(rec + 0xc), block_where(e.u32(rec + 4), a2, blocks_count)]
        return {"set": set_vals, "records": records,
                "nodes": [e.u32(node_at(i)) for i in range(full + minis)],
                "allocs": e.cl.log}
    finally:
        CL.Stubs.deactivate(e, old)


def host_ied(lib, case):
    first, count, full, minis, emb, nodes = case
    words = ied_words(case)
    total = full + minis
    size = 6 + 3 * count + total
    fn = lib.tpw_ied
    fn.argtypes = [C.POINTER(C.c_uint32), C.POINTER(C.c_uint32)]
    fn.restype = None
    out = (C.c_uint32 * size)()
    CL.host_begin(lib, {})
    fn((C.c_uint32 * len(words))(*words), out)
    log = CL.host_end(lib)
    return {"set": list(out[:6]), "records": list(out[6:6 + 3 * count]),
            "nodes": list(out[6 + 3 * count:]), "allocs": log}


def ied_edges():
    yield ied_case(count=0, full=1, nodes=[(0, 0, 0, NONE, NONE)])
    yield ied_case(count=0, full=1, nodes=[(0x400, 0, 0, NONE, NONE)])
    yield ied_case(count=1, full=1, emb=(0x30,), nodes=[(0, 0, 0, NONE, NONE)])
    yield ied_case(count=1, full=1, emb=(0,), nodes=[(0, 0, 0, NONE, NONE)])
    yield ied_case(count=1, full=1, emb=(0x40,), nodes=[(0x400, 0, 0, NONE, NONE)])
    yield ied_case(count=1, full=1, minis=1, emb=(0x40040,), nodes=[(0x400, 0, 1, NONE, NONE), (0, 0, 0, NONE, NONE)])
    yield ied_case(count=1, full=1, minis=1, emb=(0x30,), nodes=[(0x400, 0, 1, NONE, NONE), (0, 0, 0, NONE, NONE)])
    yield ied_case(first=1, count=2, full=1, minis=2, emb=(0x30, 0x8),
                   nodes=[(0, 0, 0, NONE, NONE), (0, 1, 0, NONE, NONE), (0, 2, 0, NONE, NONE)])
    yield ied_case(first=0, count=2, full=2, emb=(0x10000, 0x2),
                   nodes=[(0, 0, 0, 1, NONE), (0x100, 1, 1, NONE, NONE)])
    yield ied_case(first=0, count=2, full=2, emb=(0x30, 0x30),
                   nodes=[(0, 0, 0, 1, NONE), (0, 1, 1, NONE, NONE)])
    yield ied_case(first=0, count=2, full=2, emb=(0x30, 0x0),
                   nodes=[(0, 0, 0, 1, NONE), (0, 1, 0, NONE, NONE)])
    yield ied_case(first=0, count=1, full=3, emb=(0x30,),
                   nodes=[(0, 0, 2, 1, NONE), (0, 3, 0, NONE, 2), (0, 5, 0, NONE, NONE)])
    yield ied_case(first=0, count=1, full=1, emb=(0x20,), nodes=[(0x20000000, 0, 0, NONE, NONE)])
    yield ied_case(first=0, count=1, full=1, emb=(0x8 | 0x10000,), nodes=[(0, 0, 0, NONE, NONE)])


def ied_rnd(r):
    first = r.randrange(0, 3)
    count = r.randrange(0, 4)
    if count == 0:
        total = r.randrange(1, 4)
    else:
        total = r.randrange(max(1, first + count), first + count + 3)
    full = r.randrange(0, total + 1)
    minis = total - full
    emb = [r.choice([0, 0x30, 0x40040, 0x10000, 0x8, 0x2, 0x20, r.getrandbits(32)]) for _ in range(count)]
    parent = [None] + [r.randrange(0, i) for i in range(1, total)]
    child = [NONE] * total
    sibling = [NONE] * total
    for i in range(total - 1, 0, -1):
        sibling[i] = child[parent[i]]
        child[parent[i]] = i
    nodes = []
    for i in range(total):
        f50 = r.randrange(first, first + count) if count and r.random() < 0.7 else r.randrange(0, 6)
        flags = r.choice([0, 0x400, 0x20000000, 0x100, r.getrandbits(32)])
        nodes.append((flags, f50, r.randrange(total), child[i], sibling[i]))
    return ied_case(first, count, full, minis, emb, nodes)


TESTS = [
    FnTest("Ride animation type flag", ppc_type, host_type, edge=type_edges, rnd=type_rnd),
    FnTest("Correct node flags 1", ppc_cnf, host_cnf, edge=cnf_edges, rnd=cnf_rnd),
    FnTest("Reset animation time stats", ppc_rats, host_rats, edge=rats_edges, rnd=rats_rnd),
    FnTest("Embedded hierarchy set-up", ppc_set, host_set, edge=set_edges, rnd=set_rnd),
    FnTest("Ride node animation nodes", ppc_rnai, host_rnai, edge=rnai_edges, rnd=rnai_rnd),
    FnTest("Ride animation set init", ppc_iras, host_iras, edge=iras_edges, rnd=iras_rnd),
    FnTest("Embedded data initialise", ppc_ied, host_ied, edge=ied_edges, rnd=ied_rnd),
]
