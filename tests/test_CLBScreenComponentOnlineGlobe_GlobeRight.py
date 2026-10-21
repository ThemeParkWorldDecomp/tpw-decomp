"""Unit v51119, the world globe's camera steps, list registration, small destructors and byte swaps, against
libtpw_unit_CLBScreenComponentOnlineGlobe_GlobeRight.dylib (tests/unit_CLBScreenComponentOnlineGlobe_GlobeRight.cpp), main app
("SimTheme Park"), src/game/HLSprite.cpp, src/game/online/lobby_globe.cpp, src/game/park/LLCellMouse.cpp, src/game/park/TPHField.cpp, src/game/rides/RideAnim.cpp:
  CLBScreenComponentOnlineGlobe GlobeLeft 0x100adb54, GlobeRight 0x100adba8, GlobeUp 0x100adbfc, GlobeDown 0x100adc50,
  Register 0x100adff4, UnRegister 0x100ae064, ProcessCancel 0x100ad690, Render 0x100ad4cc, Init 0x100acc98;
  Bitmap::~Bitmap 0x100ca4bc, MapDelta::~MapDelta 0x10082c88, basic_filebuf<char>::~basic_filebuf 0x100700e4,
  basic_ios<char>::~basic_ios 0x1006ff50, CLightPoint::~CLightPoint 0x1006427c;
  SamsUtilities::USwapInplace16<short> 0x100de404, USwapInplace32<long> 0x100de448 and 0x10079a64, USwapBlock32<long> 0x100799b0,
  USwapInplace32<unsigned long> 0x10079968 and 0x1006b6ac, USwapInplace32<int> 0x1006b668, USwapInplace16<unsigned short> 0x1006b620.

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses
(big-endian) for the original, and in the host library's memory (little-endian) for the translation. The callees outside the unit
(operator delete, delete[], ios_base::~ios_base, filebuf close, the locale pointer's destructor, _USwapBlock32) are fakes on both
sides that record the pointers (as labels) and numbers they read. The globe list functions run for real on both sides (the Mac's
TbTList AddTail, Find and RemoveAt as the original; the host's inline template): the list is walked forward and backward after the call.

The world of a case: the object's float and integer fields (random words, special floats), a list of values, the flag of a destructor
(a signed 16-bit value, with garbage above it), the file word of a filebuf (the three FILE slots of the table or another value), the
chain of pointers a CLightPoint follows. What is compared: the answer, every field read, the event log, the list. The addresses of
globals a destructor stores are compared by name, and the offsets it stores relative to its own object as numbers.

Single-precision adds and subtracts only, no fused operations. Not tested: the exception cleanup path of the filebuf destructor
(__unexpected, reachable only by unwinding).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CLBScreenComponentOnlineGlobe_GlobeRight"
MASK = 0xFFFFFFFF

# ---- the Mac addresses of the unit's functions
GLOBE_OPS = {"left": (0x100adb54, 0), "right": (0x100adba8, 1), "up": (0x100adbfc, 2), "down": (0x100adc50, 3),
             "render": (0x100ad4cc, 4), "cancel": (0x100ad690, 5), "init": (0x100acc98, 6)}
LIST_OPS = {"reg": (0x100adff4, 0), "unreg": (0x100ae064, 1)}
DTORS = {"bitmap": (0x100ca4bc, 0), "mapdelta": (0x10082c88, 1), "filebuf": (0x100700e4, 2), "ios": (0x1006ff50, 3),
         "light": (0x1006427c, 4)}
SWAPS = {0: 0x100de404, 1: 0x100de448, 2: 0x10079a64, 3: 0x10079968, 4: 0x1006b6ac, 5: 0x1006b668, 6: 0x1006b620}
BLOCK = 0x100799b0
LIST_REAL = (0x100afc60, 0x100af9b0, 0x100afac0)      # TbTList AddTail, Find, RemoveAt: run as the original
OWN = {a for a, _ in GLOBE_OPS.values()} | {a for a, _ in LIST_OPS.values()} | {a for a, _ in DTORS.values()} | set(SWAPS.values()) | {BLOCK}

# ---- the callees faked on both sides (glue imports and in-binary functions)
C_DELETE, C_DELARR, C_IOSDTOR, C_CLOSE, C_LOCDTOR, C_SWAPBLK = 0x10238218, 0x10238638, 0x10238950, 0x10071b54, 0x10071f34, 0x10239a78

# ---- the Mac's addresses of the globals; the host library's come from tpw_gl_addr(name)
ADDR = dict(pointvt=0x1029cfb4, vt2=0x10278c9c, vt3=0x10278cb4, fvtA=0x1029d0f8, fvtB=0x1029d148, files=0x103bd0bc, ostr=0x1029d0b0)
LAYOUT_NAMES = ["globes", "f0c", "f44", "f70", "olsize", "bm_data", "bm_size", "md_size"]


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def canon_f(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)
STUBS.real.update(LIST_REAL)


def _one(name, e):
    L = e.cl
    L.rec(name, (L.lab(e.r(3)),))


def _two(name, e):
    L = e.cl
    L.rec(name, (L.lab(e.r(3)), s32(e.r(4))))


STUBS.internal(C_DELETE, lambda e: _one("delete", e))
STUBS.internal(C_DELARR, lambda e: _one("delete[]", e))
STUBS.internal(C_CLOSE, lambda e: _one("close", e))
STUBS.internal(C_IOSDTOR, lambda e: _two("ios_base_dtor", e))
STUBS.internal(C_LOCDTOR, lambda e: _two("locale_dtor", e))
STUBS.internal(C_SWAPBLK, lambda e: _two("_USwapBlock32", e))


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    started = False

    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def g(self, name):
        raise NotImplementedError

    def cw(self, w):
        """A word as the name of the global (plus small offset) it holds the address of."""
        best = None
        for name in ADDR:
            d = (w - (self.g(name) & MASK)) & MASK
            if d < 0x100 and (best is None or d < best[0]):
                best = (d, name)
        return w if best is None else "%s+%x" % (best[1], best[0])

    def val(self, w, **ptrs):
        """A word as the label of an object it points at, else as a global's name, else itself."""
        for k, v in ptrs.items():
            if v and w == (v & MASK):
                return k
        return self.cw(w)


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e):
        self.e = e
        self.saved = None

    def start(self, addrs):
        fixed = {a: i + 1 for i, a in enumerate(addrs) if a}
        self.L = self.e.cl = CL.PpcLog(fixed, {})
        self.saved = STUBS.activate(self.e)

    def done(self):
        if self.saved is not None:
            CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        if name == "files":      # an import: the TOC slot 0x10260ca4 holds the address the loader bound it to
            return self.e.u32(0x10260CA4)
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def pw(self, addr):
        return addr

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def fld(self, o, m):
        return o + m

    def bm_data(self, o):
        return o + 0xC

    def wptr(self, a, p):
        self.words(a, [p])

    def rptr(self, a):
        return self.rd_word(a)

    def call(self, addr, args):
        return T.call_ppc(self.e, addr, [a & MASK for a in args])[0] & MASK

    def globe(self, op, o):
        return self.call(GLOBE_OPS[op][0], [o])

    def listop(self, op, o, v):
        self.call(LIST_OPS[op][0], [o, v])

    def mklist(self, items):
        lst = self.alloc(16)
        nodes = [self.alloc(12) for _ in items]
        for i, (n, v) in enumerate(zip(nodes, items)):
            self.words(n, [v, nodes[i + 1] if i + 1 < len(nodes) else 0, nodes[i - 1] if i else 0])
        self.words(lst, [0, nodes[0] if nodes else 0, nodes[-1] if nodes else 0, len(items)])
        return lst

    def dump(self, lst):
        _, head, tail, cnt = self.rd_words(lst, 4)
        fwd, back = [], []
        n = head
        while n and len(fwd) < 40:
            d, nx, _pv = self.rd_words(n, 3)
            fwd.append(d)
            n = nx
        n = tail
        while n and len(back) < 40:
            d, _nx, pv = self.rd_words(n, 3)
            back.append(d)
            n = pv
        return [s32(cnt), fwd, back]

    def dtor(self, which, o, flag):
        return self.call(DTORS[which][0], [o, flag])

    def swap(self, which, p):
        self.call(SWAPS[which], [p])

    def block(self, p, count):
        self.call(BLOCK, [p, count])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib):
        self.lib = lib
        bind(lib)

    def start(self, addrs):
        CL.host_begin(self.lib, {i + 1: a for i, a in enumerate(addrs) if a}, {})

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_gl_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_gl_alloc(n)

    def pw(self, addr):
        """The 32-bit pointer word for an arena address: an offset from the arena's base."""
        return (addr - self.lib.tpw_gl_base()) & MASK if addr else 0

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def fld(self, o, m):
        L = self.lib.lay
        if m == 8:
            return o + L["globes"]
        if 0x0C <= m < 0x1C:
            return o + L["f0c"] + m - 0x0C
        if 0x44 <= m < 0x6C:
            return o + L["f44"] + m - 0x44
        if 0x70 <= m < 0x7C:
            return o + L["f70"] + m - 0x70
        raise ValueError(m)

    def bm_data(self, o):
        return o + self.lib.lay["bm_data"]

    def wptr(self, a, p):
        self.write(a, struct.pack("<Q", p))

    def rptr(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0]

    def globe(self, op, o):
        return self.lib.tpw_gl_globe(GLOBE_OPS[op][1], o)

    def listop(self, op, o, v):
        self.lib.tpw_gl_listop(LIST_OPS[op][1], o, v & MASK)

    def mklist(self, items):
        lst = self.lib.tpw_gl_list_new()
        for v in items:
            self.lib.tpw_gl_list_add(lst, v)
        return lst

    def dump(self, lst):
        buf = (C.c_int64 * 128)()
        self.lib.tpw_gl_list_dump(lst, buf, 128)
        nf = buf[1]
        nb = buf[2 + nf]
        return [s32(buf[0]), [x & MASK for x in buf[2:2 + nf]], [x & MASK for x in buf[3 + nf:3 + nf + nb]]]

    def dtor(self, which, o, flag):
        return self.lib.tpw_gl_dtor(DTORS[which][1], o, flag)

    def swap(self, which, p):
        self.lib.tpw_gl_swap(which, p)

    def block(self, p, count):
        self.lib.tpw_gl_block(p, count)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_gl_addr.restype, lib.tpw_gl_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_gl_alloc.restype, lib.tpw_gl_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_gl_base.restype, lib.tpw_gl_base.argtypes = C.c_uint64, []
    lib.tpw_gl_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_gl_globe.restype, lib.tpw_gl_globe.argtypes = C.c_uint32, [C.c_int32, C.c_uint64]
    lib.tpw_gl_listop.restype, lib.tpw_gl_listop.argtypes = None, [C.c_int32, C.c_uint64, C.c_uint32]
    lib.tpw_gl_list_new.restype, lib.tpw_gl_list_new.argtypes = C.c_uint64, []
    lib.tpw_gl_list_add.restype, lib.tpw_gl_list_add.argtypes = None, [C.c_uint64, C.c_uint32]
    lib.tpw_gl_list_dump.restype, lib.tpw_gl_list_dump.argtypes = C.c_int32, [C.c_uint64, C.POINTER(C.c_int64), C.c_int32]
    lib.tpw_gl_dtor.restype, lib.tpw_gl_dtor.argtypes = C.c_uint64, [C.c_int32, C.c_uint64, C.c_int32]
    lib.tpw_gl_swap.restype, lib.tpw_gl_swap.argtypes = None, [C.c_int32, C.c_uint64]
    lib.tpw_gl_block.restype, lib.tpw_gl_block.argtypes = None, [C.c_uint64, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_gl_layout(out)
    lib.lay = dict(zip(LAYOUT_NAMES, list(out)[:n]))
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the kinds of case

GFIELDS = [0x10, 0x14, 0x18, 0x48, 0x4C, 0x50, 0x54, 0x58, 0x5C, 0x60, 0x64, 0x68, 0x70, 0x74, 0x78]


def k_globe(s, case):
    o = s.alloc(0x100)
    for m, w in zip(GFIELDS, case["words"]):
        s.words(s.fld(o, m), [w])
    s.start([o])
    ans = s.globe(case["op"], o)
    obs = {"ans": ans & MASK if case["op"] == "cancel" else None}
    for m in GFIELDS:
        w = s.rd_word(s.fld(o, m))
        obs[m] = canon_f(w) if m < 0x70 else w
    obs["log"] = s.log()
    return obs


def k_list(s, case):
    o = s.alloc(0x100)
    lst = s.mklist(case["items"])
    s.wptr(s.fld(o, 8), lst)
    s.start([o])
    s.listop(case["op"], o, case["value"])
    return {"same": s.rptr(s.fld(o, 8)) == lst, "dump": s.dump(lst)}


FLAGS = [0, 1, -1, 2, 0x7FFF, 0x8000, 0xFFFF, 0x10000, 0x10001, 0x18000, -0x8000]


def k_bitmap(s, case):
    o = s.alloc(0x20)
    data = s.alloc(16) if case["hasdata"] else 0
    s.words(o, case["wh"])
    s.wptr(s.bm_data(o), data)
    s.start([o, data])
    ret = s.dtor("bitmap", 0 if case["null"] else o, case["flag"])
    d = s.rptr(s.bm_data(o))
    return {"ret": s.val(ret & MASK, this=o) if ret else 0, "log": s.log(), "wh": s.rd_words(o, 3),
            "data": s.val(d & MASK, data=data) if d else 0}


def k_mapdelta(s, case):
    o = s.alloc(16)
    s.write(o, case["bytes"])
    s.start([o])
    ret = s.dtor("mapdelta", 0 if case["null"] else o, case["flag"])
    return {"ret": s.val(ret & MASK, this=o) if ret else 0, "log": s.log(), "bytes": s.read(o, 12)}


def k_filebuf(s, case):
    o = s.alloc(0x60)
    w = list(case["w"])
    kind, off = case["file"]
    w[0x24 // 4] = (s.g("files") + off) & MASK if kind == "files" else off
    s.words(o, w)
    s.start([o, o + 0x1C])
    ret = s.dtor("filebuf", 0 if case["null"] else o, case["flag"])
    after = s.rd_words(o, 16)
    obs = [s.val(x, this=o) for x in after]
    obs[0x24 // 4] = "unchanged" if after[0x24 // 4] == w[0x24 // 4] else after[0x24 // 4]     # a name would depend on the side's neighbours
    return {"ret": s.val(ret & MASK, this=o) if ret else 0, "log": s.log(), "w": obs}


def k_ios(s, case):
    o = s.alloc(0x20)
    s.words(o, case["w"])
    s.start([o])
    ret = s.dtor("ios", 0 if case["null"] else o, case["flag"])
    return {"ret": s.val(ret & MASK, this=o) if ret else 0, "log": s.log(), "w": [s.val(x, this=o) for x in s.rd_words(o, 8)]}


def k_light(s, case):
    me, p, q, r = s.alloc(0x80), s.alloc(0x80), s.alloc(0x10), s.alloc(0x10)
    mw, pw = list(case["me"]), list(case["P"])
    mw[0], mw[4] = s.pw(p), s.pw(r)
    pw[0], pw[0x60 // 4] = s.pw(q), 0
    s.words(me, mw)
    s.words(p, pw)
    s.words(q, [case["q0"], 0, case["q2"], case["q3"]])
    s.words(r, [case["r0"], 0, case["r2"], case["r3"]])
    s.start([me])
    ret = s.dtor("light", 0 if case["null"] else me, case["flag"])
    pme, pp, pq, pr = s.pw(me), s.pw(p), s.pw(q), s.pw(r)
    m = lambda x: s.val(x, me=pme, P=pp, Q=pq, R=pr)
    rel = lambda x, base: 0 if x == 0 else (x + s.pw(base) - pme) & MASK
    mo = [m(x) for x in s.rd_words(me, 32)]
    po = [m(x) for x in s.rd_words(p, 32)]
    po[0x60 // 4] = rel(s.rd_word(p + 0x60), p)
    qo = [m(x) for x in s.rd_words(q, 4)]
    qo[1] = rel(s.rd_word(q + 4), q)
    ro = [m(x) for x in s.rd_words(r, 4)]
    ro[1] = rel(s.rd_word(r + 4), r)
    return {"ret": s.val(ret & MASK, this=me) if ret else 0, "log": s.log(), "me": mo, "P": po, "Q": qo, "R": ro}


def k_swap(s, case):
    buf = s.alloc(32)
    s.write(buf, case["bytes"])
    s.start([buf])
    s.swap(case["which"], buf + case["off"])
    return {"bytes": s.read(buf, 32), "log": s.log()}


def k_block(s, case):
    buf = s.alloc(64)
    s.write(buf, case["bytes"])
    s.start([buf + case["off"]])
    s.block(buf + case["off"], case["count"])
    return {"bytes": s.read(buf, 64), "log": s.log()}


KINDS = {"globe": k_globe, "list": k_list, "bitmap": k_bitmap, "mapdelta": k_mapdelta, "filebuf": k_filebuf, "ios": k_ios,
         "light": k_light, "swap": k_swap, "block": k_block}


def run(s, case):
    try:
        return KINDS[case["kind"]](s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e), case)


def host(lib, case):
    return run(Host(lib), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

SPECIAL = [0, 0x80000000, 0x7FC00000, 0x7F800000, 0xFF800000, 0x00000001, 0x3F800000, 0xBF800000, 0x7F7FFFFF, 0x00800000, 0x80000001,
           0xFF7FFFFF, 0x7FA00000]


def rf(r):
    k = r.random()
    if k < 0.08:
        return r.choice(SPECIAL)
    if k < 0.6:
        return fb(r.uniform(-8, 8))
    if k < 0.9:
        return fb(r.uniform(-300, 300))
    return fb(r.uniform(-1, 1) * 10 ** r.uniform(-8, 8))


def rw(r):
    return r.getrandbits(32)


def globe_case(r, op, x=None, step=None, y=None):
    ws = [rf(r) if m < 0x70 else rw(r) for m in GFIELDS]
    ws[GFIELDS.index(0x14)] = rf(r) if step is None else step
    ws[GFIELDS.index(0x48)] = rf(r) if x is None else x
    ws[GFIELDS.index(0x50)] = rf(r) if y is None else y
    return {"kind": "globe", "op": op, "words": ws}


def globe_edges(op, seed):
    def gen():
        r = random.Random(seed)
        for x in SPECIAL:
            for step in SPECIAL[:8]:
                yield globe_case(r, op, x, step, x)
        yield globe_case(r, op, fb(1.0), fb(1e-9), fb(1e-9))        # the step is lost to rounding
        yield globe_case(r, op, fb(16777216.0), fb(1.0), fb(16777216.0))
        yield globe_case(r, op, fb(0.1), fb(0.2), fb(0.3))
    return gen


def globe_random(op):
    return lambda r: globe_case(r, op)


def fixed_edges(kind_fn, seed, n=3):
    def gen():
        r = random.Random(seed)
        for _ in range(n):
            yield kind_fn(r)
    return gen


def list_case(r, op, items=None, value=None):
    pool = [0x100 + 4 * i for i in range(6)]
    if items is None:
        items = [r.choice(pool) for _ in range(r.choice([0, 1, 1, 2, 3, 4, 6]))]
    if value is None:
        value = r.choice(items) if items and r.random() < 0.7 else r.choice(pool + [0, 0xFFFFFFFF])
    return {"kind": "list", "op": op, "items": items, "value": value}


def list_edges(op):
    def gen():
        r = random.Random(11)
        for items in ([], [5], [5, 6], [5, 6, 7], [7, 5, 7], [5, 5, 5], [1, 2, 3, 4, 5, 6]):
            for value in (0, 1, 2, 3, 4, 5, 6, 7, 0xFFFFFFFF):
                yield list_case(r, op, items, value)
    return gen


def flag_of(r):
    return r.choice(FLAGS)


def bitmap_case(r):
    return {"kind": "bitmap", "wh": [rw(r) for _ in range(3)], "hasdata": r.random() < 0.6, "null": r.random() < 0.1, "flag": flag_of(r)}


def bitmap_edges():
    r = random.Random(21)
    for flag in FLAGS:
        for hasdata in (False, True):
            for null in (False, True):
                c = bitmap_case(r)
                c.update(flag=flag, hasdata=hasdata, null=null)
                yield c


def mapdelta_case(r):
    return {"kind": "mapdelta", "bytes": bytes(r.randrange(256) for _ in range(16)), "null": r.random() < 0.1, "flag": flag_of(r)}


def mapdelta_edges():
    r = random.Random(22)
    for flag in FLAGS:
        for null in (False, True):
            c = mapdelta_case(r)
            c.update(flag=flag, null=null)
            yield c


FILE_CHOICES = [("files", 0), ("files", 0x54), ("files", 0xA8), ("files", 4), ("files", 0x53), ("files", 0xFC), ("files", -4),
                ("raw", 0), ("raw", 0x12345678)]


def filebuf_case(r):
    return {"kind": "filebuf", "w": [rw(r) for _ in range(16)], "file": r.choice(FILE_CHOICES), "null": r.random() < 0.08,
            "flag": flag_of(r)}


def filebuf_edges():
    r = random.Random(23)
    for file in FILE_CHOICES:
        for flag in (0, 1, -1, 0x10000):
            c = filebuf_case(r)
            c.update(file=file, flag=flag, null=False)
            yield c
    c = filebuf_case(r)
    c.update(null=True)
    yield c


def ios_case(r):
    return {"kind": "ios", "w": [rw(r) for _ in range(8)], "null": r.random() < 0.1, "flag": flag_of(r)}


def ios_edges():
    r = random.Random(24)
    for flag in FLAGS:
        for null in (False, True):
            c = ios_case(r)
            c.update(flag=flag, null=null)
            yield c


def light_case(r):
    return {"kind": "light", "me": [rw(r) for _ in range(32)], "P": [rw(r) for _ in range(32)], "q0": rw(r), "q2": rw(r), "q3": rw(r),
            "r0": rw(r), "r2": rw(r), "r3": rw(r), "null": r.random() < 0.08, "flag": flag_of(r)}


def light_edges():
    r = random.Random(25)
    for flag in FLAGS:
        for null in (False, True):
            c = light_case(r)
            c.update(flag=flag, null=null)
            yield c


def swap_case(r, which, off=None):
    width = 2 if which in (0, 6) else 4
    if off is None:
        off = r.randrange(0, 7) * 4
    return {"kind": "swap", "which": which, "off": off, "bytes": bytes(r.randrange(256) for _ in range(32))}


def swap_edges(which):
    def gen():
        r = random.Random(30 + which)
        for off in (0, 4, 8, 12, 24):
            yield swap_case(r, which, off)
        c = swap_case(r, which, 4)
        c["bytes"] = bytes(range(1, 33))
        yield c
        if which in (0, 6):
            for off in (2, 6, 30):
                yield swap_case(r, which, off)
    return gen


def block_case(r, count=None):
    return {"kind": "block", "off": r.randrange(0, 4) * 4, "count": r.choice([0, 1, 2, 5, 16, -1, 0x7FFFFFFF, -0x80000000]) if count is None else count,
            "bytes": bytes(r.randrange(256) for _ in range(64))}


def block_edges():
    r = random.Random(40)
    for count in (0, 1, 2, 3, 8, 16, -1, -5, 0x7FFFFFFF, -0x80000000, 0x10000):
        yield block_case(r, count)


TESTS = [
    FnTest("CLBScreenComponentOnlineGlobe::GlobeLeft", ppc, host, edge=globe_edges("left", 1), rnd=globe_random("left")),
    FnTest("CLBScreenComponentOnlineGlobe::GlobeRight", ppc, host, edge=globe_edges("right", 2), rnd=globe_random("right")),
    FnTest("CLBScreenComponentOnlineGlobe::GlobeUp", ppc, host, edge=globe_edges("up", 3), rnd=globe_random("up")),
    FnTest("CLBScreenComponentOnlineGlobe::GlobeDown", ppc, host, edge=globe_edges("down", 4), rnd=globe_random("down")),
    FnTest("CLBScreenComponentOnlineGlobe::Render", ppc, host, edge=fixed_edges(lambda r: globe_case(r, "render"), 5),
           rnd=globe_random("render")),
    FnTest("CLBScreenComponentOnlineGlobe::ProcessCancel", ppc, host, edge=fixed_edges(lambda r: globe_case(r, "cancel"), 6),
           rnd=globe_random("cancel")),
    FnTest("CLBScreenComponentOnlineGlobe::Init", ppc, host, edge=fixed_edges(lambda r: globe_case(r, "init"), 7),
           rnd=globe_random("init")),
    FnTest("CLBScreenComponentOnlineGlobe::Register", ppc, host, edge=list_edges("reg"), rnd=lambda r: list_case(r, "reg")),
    FnTest("CLBScreenComponentOnlineGlobe::UnRegister", ppc, host, edge=list_edges("unreg"), rnd=lambda r: list_case(r, "unreg")),
    FnTest("Bitmap::~Bitmap", ppc, host, edge=bitmap_edges, rnd=bitmap_case),
    FnTest("MapDelta::~MapDelta", ppc, host, edge=mapdelta_edges, rnd=mapdelta_case),
    FnTest("std::basic_filebuf<char,std::char_traits<char>>::~basic_filebuf", ppc, host, edge=filebuf_edges, rnd=filebuf_case),
    FnTest("std::basic_ios<char,std::char_traits<char>>::~basic_ios", ppc, host, edge=ios_edges, rnd=ios_case),
    FnTest("CLightPoint::~CLightPoint", ppc, host, edge=light_edges, rnd=light_case),
    FnTest("SamsUtilities::USwapInplace16<short>", ppc, host, edge=swap_edges(0), rnd=lambda r: swap_case(r, 0)),
    FnTest("SamsUtilities::USwapInplace32<long>@0x100de448", ppc, host, edge=swap_edges(1), rnd=lambda r: swap_case(r, 1)),
    FnTest("SamsUtilities::USwapInplace32<long>@0x10079a64", ppc, host, edge=swap_edges(2), rnd=lambda r: swap_case(r, 2)),
    FnTest("SamsUtilities::USwapInplace32<unsigned_long>@0x10079968", ppc, host, edge=swap_edges(3), rnd=lambda r: swap_case(r, 3)),
    FnTest("SamsUtilities::USwapInplace32<unsigned_long>@0x1006b6ac", ppc, host, edge=swap_edges(4), rnd=lambda r: swap_case(r, 4)),
    FnTest("SamsUtilities::USwapInplace32<int>", ppc, host, edge=swap_edges(5), rnd=lambda r: swap_case(r, 5)),
    FnTest("SamsUtilities::USwapInplace16<unsigned_short>", ppc, host, edge=swap_edges(6), rnd=lambda r: swap_case(r, 6)),
    FnTest("SamsUtilities::USwapBlock32<long>", ppc, host, edge=block_edges, rnd=block_case),
]
