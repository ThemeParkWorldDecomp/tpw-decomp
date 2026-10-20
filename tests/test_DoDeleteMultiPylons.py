"""Unit v5647, the editor's multi pylon delete, against libtpw_unit_DoDeleteMultiPylons.dylib (tests/unit_DoDeleteMultiPylons.cpp), main app
("SimTheme Park"), src/DisableCurrentPylonEditState_SetCoasterEditState_DoDeleteMultiPylons.cpp:
  DoDeleteMultiPylons 0x10053f98

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian, native pointers, the host's field offsets) for the translation. The callees
outside the unit (FlagGenerateTrack, RecordAction, DeleteTrackBrace, DestroyEndPylons, DisableCurrentPylonEditState, SetCoasterButtonState,
GetCoasterEditAvailableButtons, ChangeCoasterButtonsEnabled/Down, SetPotentialPylonArea, SetCoasterState, CoasterPlaceMouseMode) are fakes on both
sides that record their arguments (braces by their number, the instance and the objects as 0/1 "is the global one").

The world of a case: the editor's coaster with a chain of braces (numbers 0..N-1, +0x64 leads to the next higher number, +0x68 to the lower),
random flags, pylon data, ride animation sets with their head tables, the drag, mark and 0x1788 globals, the input flags. What is compared: the
globals, the instance's words and pointers (as brace numbers), every brace's flags and the event log.

No fused operations. Not tested: the cases where the original reads address 0 (a NULL 0x1788 brace with a recording, a selection that runs past
the end of the list); a Python model of the walk filters them out.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_DoDeleteMultiPylons"
MASK = 0xFFFFFFFF

FN = 0x10053F98
SIZES = {FN: 1424}
OWN = set(SIZES)

ADDR = dict(inst=0x102B68B0, ext=0x102B74A8, enabled=0x102C16D4, down=0x102C16D8, mark2=0x102C1780, mark1=0x102C1784, w1788=0x102C1788,
            drag=0x102C178C, flags=0x102C1794, rideanim=0x102C79A4)
ACTREC = 0x1037EC44
UIIF = 0x10396440

MAC = dict(i_type=4, i_b3c=0x3C, i_f40=0x40, i_braces=0x58, i_f118=0x118, i_prev=0x60, i_b64=0x64,
           b_next=0x68, b_w70=0x70, b_data=0x100, b_flags=8, b_prev=0x64,
           d_flags=4, d_f18=0x18, t_fa4=0xA4, a_heads=8, m_f28=0x28, tb_entries=4, e_rec=4, e_stride=0x14, r_y=0x34, b_num=0)
HNAMES = ["i_type", "i_b3c", "i_f40", "i_braces", "i_f118", "i_prev", "i_b64", "b_next", "b_w70", "b_data", "b_flags", "b_prev",
          "d_flags", "d_f18", "t_fa4", "a_heads", "m_f28", "tb_entries", "e_rec", "e_stride", "r_y", "b_num"]


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def canon_f(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def is_inst(e):
    return 1 if e.r(3) == e.u32(ADDR["inst"]) else 0


STUBS.internal(0x10040F08, lambda e: e.cl.rec("FlagGenerateTrack", (is_inst(e), s32(e.r(4)))))
STUBS.internal(0x10056660, lambda e: e.cl.rec("DeleteTrackBrace", (is_inst(e), e.u32(e.r(4)))))
STUBS.internal(0x10042728, lambda e: e.cl.rec("DestroyEndPylons", (is_inst(e),)))
STUBS.internal(0x10053B58, lambda e: e.cl.rec("DisableCurrentPylonEditState", ()))
STUBS.internal(0x10057064, lambda e: e.cl.rec("SetCoasterButtonState", (e.r(3) & MASK, e.r(4) & MASK)))
STUBS.internal(0x10057278, lambda e: e.cl.rec("GetCoasterEditAvailableButtons", (e.r(3) & MASK,)))
STUBS.internal(0x10041A58, lambda e: e.cl.rec("SetPotentialPylonArea", ()))
STUBS.internal(0x10042BB8, lambda e: e.cl.rec("SetCoasterState", (is_inst(e), e.r(4) & MASK)))
STUBS.internal(0x1019B018, lambda e: e.cl.rec("CoasterPlaceMouseMode", ()))
STUBS.internal(0x1014430C, lambda e: e.cl.rec("RecordAction", (1 if e.r(3) == ACTREC else 0, e.r(4) & MASK, e.r(5) & MASK, e.r(6) & MASK)))
STUBS.internal(0x101F0F94, lambda e: e.cl.rec("ChangeCoasterButtonsEnabled", (1 if e.r(3) == UIIF else 0, e.r(4) & MASK)))
STUBS.internal(0x101F0F2C, lambda e: e.cl.rec("ChangeCoasterButtonsDown", (1 if e.r(3) == UIIF else 0, e.r(4) & MASK)))


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def wptr(self, a, v):
        self.write(a, struct.pack(self.pfmt, v))

    def rptr(self, a):
        return struct.unpack(self.pfmt, self.read(a, self.pw))[0]


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    pw = 4
    OFF = MAC

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self):
        T.call_ppc(self.e, FN, [])


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"
    pw = 8
    OFF = None

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_ddmp_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_ddmp_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self):
        self.lib.tpw_ddmp_run()


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_ddmp_addr.restype, lib.tpw_ddmp_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_ddmp_alloc.restype, lib.tpw_ddmp_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_ddmp_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ddmp_run.restype, lib.tpw_ddmp_run.argtypes = None, []
    out = (C.c_int64 * 32)()
    n = lib.tpw_ddmp_layout(out)
    assert n == len(HNAMES), n
    off = dict(zip(HNAMES, list(out)[:n]))
    assert off["b_flags"] == 8 and off["b_num"] == 0, off
    Host.OFF = off
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world

NSETS = 4
NENT = 4


def build(s, c):
    O = s.OFF
    # ride animation sets: set -> mesh -> table -> entries -> record -> y
    for k in range(NSETS):
        entries = s.alloc(NENT * O["e_stride"] + 16)
        for j in range(NENT):
            y = c["recs"][k][j]
            if y is not None:
                rec = s.alloc(0x80)
                s.words(rec + O["r_y"], [y])
                s.wptr(entries + j * O["e_stride"] + O["e_rec"], rec)
        table = s.alloc(32)
        s.wptr(table + O["tb_entries"], entries)
        mesh = s.alloc(0x200)
        s.wptr(mesh + O["m_f28"], table)
        aset = s.alloc(0x400)
        s.wptr(aset + O["a_heads"], mesh)
        s.wptr(s.g("rideanim") + k * s.pw, aset)

    N = c["N"]
    nodes = [s.alloc(0x400) for _ in range(N)]
    s.nodes = nodes
    for i, b in enumerate(nodes):
        s.words(b + O["b_num"], [i])
        s.words(b + O["b_flags"], [c["flags"][i]])
        s.wptr(b + O["b_prev"], nodes[i + 1] if i + 1 < N else 0)
        s.wptr(b + O["b_next"], nodes[i - 1] if i > 0 else 0)
        s.words(b + O["b_w70"], [c["w70"][i]])
        d = s.alloc(0x400)
        s.words(d + O["d_flags"], [c["dflags"][i]])
        blk = s.alloc(0x100)
        s.words(blk + 0x2C, [c["idx"][i]])
        s.wptr(d + O["d_f18"], blk)
        s.wptr(b + O["b_data"], d)

    typ = s.alloc(0x400)
    s.words(typ, [c["twd"]])
    s.words(typ + O["t_fa4"], [c["fa4"]])
    inst = s.alloc(0x800)
    s.words(inst, [c["iflags"]])
    s.wptr(inst + O["i_type"], typ)
    s.words(inst + O["i_b3c"], [c["b3c"]])
    s.words(inst + O["i_f40"], [c["f40"]])
    s.wptr(inst + O["i_braces"], nodes[0])
    s.words(inst + O["i_f118"], [c["f118"]])
    s.wptr(inst + O["i_prev"], nodes[c["prevb"]])
    s.wptr(inst + O["i_b64"], nodes[c["b64"]])
    s.inst = inst

    def node(i):
        return 0 if i is None else nodes[i]

    s.wptr(s.g("inst"), inst if c["inst"] else 0)
    s.words(s.g("ext"), [c["ext"]])
    s.words(s.g("enabled"), [c["enabled"]])
    s.words(s.g("down"), [c["down"]])
    s.wptr(s.g("mark2"), node(c["mark2"]))
    s.wptr(s.g("mark1"), node(c["mark1"]))
    s.words(s.g("w1788"), [node(c["W"])])        # the host keeps the low 32 bits here and the pointer in gCoasterStartBrace
    if s.pw == 8:
        s.wptr(s.g("startbrace"), node(c["W"]))
    s.wptr(s.g("drag"), node(c["drag"]))
    s.words(s.g("flags"), [c["inflags"]])


def execute(s, case):
    s.call()


def observe(s, case):
    O = s.OFF
    m = {a: i + 1 for i, a in enumerate(s.nodes)}
    m32 = {a & MASK: i + 1 for i, a in enumerate(s.nodes)}

    def lab(p):
        return m.get(p, 0 if p == 0 else -1)

    inst = s.inst
    return {
        "log": s.log(),
        "ext": s.rd_word(s.g("ext")),
        "enabled": s.rd_word(s.g("enabled")),
        "down": s.rd_word(s.g("down")),
        "flags": s.rd_word(s.g("flags")),
        "mark1": lab(s.rptr(s.g("mark1"))),
        "mark2": lab(s.rptr(s.g("mark2"))),
        "drag": lab(s.rptr(s.g("drag"))),
        "w1788": m32.get(s.rd_word(s.g("w1788")), 0 if s.rd_word(s.g("w1788")) == 0 else -1),
        "inst": s.rptr(s.g("inst")) == inst,
        "iflags": s.rd_word(inst),
        "b3c": s.rd_word(inst + O["i_b3c"]),
        "f118": canon_f(s.rd_word(inst + O["i_f118"])),
        "prev": lab(s.rptr(inst + O["i_prev"])),
        "b64": lab(s.rptr(inst + O["i_b64"])),
        "braces": lab(s.rptr(inst + O["i_braces"])),
        "bflags": [s.rd_word(b + O["b_flags"]) for b in s.nodes],
    }


def run(s, case):
    try:
        build(s, case)
        execute(s, case)
        return observe(s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- the model that keeps the cases off address 0


class Null(Exception):
    pass


def safe(c):
    """True when the original does not read address 0 or run off the list (a Python walk over brace numbers)."""
    if not c["inst"] or not c["f40"] or not (c["inflags"] & 0x80) or c["drag"] is None:
        return True
    N = c["N"]
    fl = c["flags"]

    def d(i):
        if i is None:
            raise Null
        return i

    def pv(i):
        d(i)
        return i + 1 if i + 1 < N else None

    def nx(i):
        d(i)
        return i - 1 if i > 0 else None

    try:
        hi = c["drag"]
        W = c["W"]
        rec = True
        if W is not None and hi == W and (c["dflags"][W] & 2):
            rec = False
        m1, m2 = c["mark1"], c["mark2"]
        if m2 is not None:
            d(m1)
        if not rec:
            return True
        w = d(W)
        e = hi
        if w == 0:
            pass
        elif hi == 0:
            pass
        if w > e:
            w, e = e, w
        if pv(w) == e and (fl[w] & 0x40) and (fl[w] & 0x80) and (fl[e] & 0x40):
            if nx(w) is not None:
                w = nx(w)
            if pv(e) is not None:
                e = pv(e)
        if w == e:
            w = nx(w)
            e = pv(e)
        d(w)
        last = e
        before = pv(w)
        frm = w
        if before != e:
            nxe = nx(e)
            f = fl[d(before)]
            if (f & 0x40) and not (f & 0x80):
                frm = nx(frm)
            if fl[d(nxe)] & 0x80:
                last = pv(last)
        if pv(frm) == last:
            d(last)
            if fl[frm] & 0x40:
                walk = nx(frm)
                while walk is not None:
                    if not (fl[walk] & 0x40):
                        break
                    walk = nx(walk)
                d(walk)
        else:
            d(last)
            b = nx(last)
            while b != frm:
                b = nx(b)
    except Null:
        return False
    return True


# ---------------------------------------------------------------------------------------------------------------- case generators


def rf(r):
    k = r.random()
    if k < 0.1:
        return r.choice([0, 0x80000000, 0x7FC00000, 0x7F800000, 0xFF800000, 0x3F800000])
    return fb(r.uniform(-300, 300))


def sample(r, ov):
    N = ov.get("N", r.randint(3, 9))
    c = {"queues": {}, "N": N}
    c["inst"] = r.random() < 0.97
    c["f40"] = r.choice([1, 1, 1, 0x1234, 0x80000000]) if r.random() < 0.97 else 0
    c["inflags"] = (r.getrandbits(32) & ~0x80 & MASK) | (0x80 if r.random() < 0.85 else 0)
    c["flags"] = [(r.getrandbits(32) & ~0xC0 & MASK) | r.choice([0, 0x40, 0x80, 0xC0, 0xC0]) for _ in range(N)]
    c["dflags"] = [r.getrandbits(32) & 3 if r.random() < 0.8 else r.getrandbits(32) for _ in range(N)]
    c["w70"] = [r.randrange(NSETS) for _ in range(N)]
    c["idx"] = [r.randrange(NENT) for _ in range(N)]
    c["recs"] = [[None if r.random() < 0.25 else rf(r) for _ in range(NENT)] for _ in range(NSETS)]
    c["drag"] = r.randrange(N) if r.random() < 0.93 else None
    c["W"] = c["drag"] if r.random() < 0.15 else (r.randrange(N) if r.random() < 0.95 else None)
    c["mark2"] = r.randrange(N) if r.random() < 0.5 else None
    c["mark1"] = r.randrange(N) if (c["mark2"] is not None or r.random() < 0.5) else None
    c["twd"] = (r.getrandbits(32) & ~2 & MASK) | (2 if r.random() < 0.75 else 0)
    c["fa4"] = rf(r)
    c["f118"] = rf(r)
    c["b3c"] = (r.getrandbits(32) & ~3 & MASK) | r.randrange(4)
    c["iflags"] = r.getrandbits(32)
    c["prevb"] = r.randrange(N)
    c["b64"] = r.randrange(N)
    c["ext"] = r.getrandbits(32)
    c["enabled"] = r.getrandbits(32)
    c["down"] = r.getrandbits(32)
    c.update({k: v for k, v in ov.items() if k != "N"})
    return c


def make(r, **ov):
    for _ in range(20000):
        c = sample(r, ov)
        if safe(c):
            return c
    raise AssertionError("no safe case for %r" % (ov,))


def edges():
    r = random.Random(647)
    yield make(r, inst=False)
    yield make(r, f40=0)
    yield make(r, inflags=0x0, drag=None)
    yield make(r, inflags=0x0, drag=3, N=6)
    yield make(r, inflags=0x7F, drag=2, N=6)
    yield make(r, inflags=0xFFFFFF80 & MASK, drag=None)
    yield make(r, inflags=0x80, drag=2, N=7, W=2, dflags=[2] * 7)
    yield make(r, inflags=0x80, drag=2, N=7, W=2, dflags=[1] * 7)
    yield make(r, inflags=0x80, drag=2, N=7, W=2, dflags=[3] * 7, mark1=4, mark2=1)
    yield make(r, inflags=0x80, drag=0, N=7, W=3, mark2=None)
    yield make(r, inflags=0x80, drag=4, N=7, W=0, mark2=None)
    yield make(r, inflags=0x80, drag=0, N=7, W=0, mark2=None)
    yield make(r, inflags=0x80, drag=5, N=8, W=2, flags=[0xC0] * 8, mark1=2, mark2=5)
    yield make(r, inflags=0x80, drag=3, N=8, W=2, flags=[0xC0] * 8, mark1=3, mark2=3)
    yield make(r, inflags=0x80, drag=2, N=8, W=3, flags=[0x40] * 8, b3c=0)
    yield make(r, inflags=0x80, drag=2, N=8, W=5, flags=[0x40] * 8, b3c=3, twd=2)
    yield make(r, inflags=0x80, drag=2, N=8, W=5, flags=[0x80] * 8, b3c=1, twd=0)
    yield make(r, inflags=0x80, drag=6, N=8, W=1, flags=[0] * 8, b3c=2)
    yield make(r, inflags=0x80, drag=7, N=8, W=1, flags=[0xC0] * 8, b3c=3, twd=3, dflags=[1] * 8)
    yield make(r, inflags=0x80, drag=4, N=9, W=1, dflags=[0] * 9, b3c=0, twd=2)
    yield make(r, inflags=0x80, drag=1, N=9, W=7, dflags=[1] * 9, b3c=1, twd=3)
    yield make(r, inflags=0x80, drag=3, N=9, W=3, dflags=[0] * 9, b3c=0, twd=2)
    for _ in range(30):
        yield make(r, inflags=0x80 | r.getrandbits(32) & ~0x80 & MASK)


def rnd(r):
    return make(r)


TESTS = [
    FnTest("DoDeleteMultiPylons", ppc, host, edge=edges, rnd=rnd),
]
