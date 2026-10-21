"""Unit v5945, screen mode lookup, palette fade and the DirectDraw error switch, against libtpw_unit_LbScreen_IsModeAvailable_2.dylib
(tests/unit_LbScreen_IsModeAvailable_2.cpp), bullfrog, src/bullfrog/LbSurface.cpp, src/bullfrog/WinScrn.cpp:
  LbScreen_IsModeAvailable 0x10068298, LbScreen_FadePalette 0x100683ec, OS_dd_error 0x10067e70

A case is a world and a call, built by one piece of Python in both memories. The callees (ModeFindFirst/Next/End, WaitVbi,
Get/SetPaletteEntries) are fakes on both sides recording their arguments (SetPaletteEntries: a checksum of the red, green and blue bytes
only, since the original leaves byte 3 of each entry of its buffer unwritten); memset is the std stub.
The world: the init flag, the two mode flags, the fade globals and buffers, the palette and the answer queues. Compared: the answer,
the globals, the buffers, the event log. No fused operations. Not tested: a fade with a count of 0 (divide by zero).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "bullfrog symbols shared"
SHIM_LIBRARY = "unit_LbScreen_IsModeAvailable_2"
MASK = 0xFFFFFFFF

FN_MODE, FN_FADE, FN_DD = 0x10068298, 0x100683EC, 0x10067E70
SIZES = {FN_MODE: 288, FN_FADE: 728, FN_DD: 1028}
OWN = set(SIZES)

FIND_FIRST, FIND_NEXT, FIND_END = 0x10068F70, 0x10069024, 0x100690BC
GET_PAL, SET_PAL, WAIT_VBI = 0x1006A304, 0x1006A220, 0x1006A400

ADDR = dict(init=0x100750F0, fa=0x10073A40, fb=0x10073A44, active=0x100748E8, step=0x100748EC,
            saved=0x100748F0, zero=0x10074CF0)
HOST_LAYOUT = [4, 12]


def s32(v):
    return T.s32(v)


def csum(data):
    h = 0
    for i in range(256):
        for k in range(3):
            h = (h * 31 + data[i * 4 + k]) & MASK
    return h


def fill_pal(seed):
    return bytes(((seed + i * 7 + k * 13 + (i >> 3) * 5) & 0xFF) for i in range(256) for k in range(4))


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def mode_stub(prefix):
    def f(e):
        L = e.cl
        L.rec({"ff": "FindFirst", "fn": "FindNext"}[prefix], (0,))
        e.write(e.r(3), struct.pack(">i", s32(L.ret(prefix + "_res"))))
        e.write(e.r(5), struct.pack(">3i", s32(L.ret(prefix + "_w")), s32(L.ret(prefix + "_h")), s32(L.ret(prefix + "_d"))))
    return f


STUBS.internal(FIND_FIRST, mode_stub("ff"))
STUBS.internal(FIND_NEXT, mode_stub("fn"))


def stub_end(e):
    e.cl.rec("FindEnd", (0,))


def stub_vbi(e):
    e.cl.rec("Vbi", (0,))


def stub_get(e):
    L = e.cl
    L.rec("Get", (e.r(4) & MASK, e.r(5) & MASK))
    e.write(e.r(3), fill_pal(L.ret("get") & 0xFF))
    e.set_r(3, e.r(3))


def stub_set(e):
    L = e.cl
    L.rec("Set", (e.r(4) & MASK, e.r(5) & MASK, csum(e.read(e.r(3), 0x400))))


STUBS.internal(FIND_END, stub_end)
STUBS.internal(WAIT_VBI, stub_vbi)
STUBS.internal(GET_PAL, stub_get)
STUBS.internal(SET_PAL, stub_set)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


class Ppc(Side):
    fmt = ">I"

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

    def mode(self, w, h, d):
        r3, _, _ = T.call_ppc(self.e, FN_MODE, [w & MASK, h & MASK, d & MASK])
        return r3 & MASK

    def fade(self, pal, n, t):
        r3, _, _ = T.call_ppc(self.e, FN_FADE, [pal, n & MASK, t & MASK])
        return r3 & MASK

    def dd(self, code):
        T.call_ppc(self.e, FN_DD, [code & MASK])
        return 0


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_fs_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_fs_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def mode(self, w, h, d):
        return self.lib.tpw_fs_mode(w & MASK, h & MASK, d & MASK) & MASK

    def fade(self, pal, n, t):
        return self.lib.tpw_fs_fade(pal, n & MASK, s32(t)) & MASK

    def dd(self, code):
        self.lib.tpw_fs_dd(s32(code))
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_fs_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "mode").restype, getattr(lib, p + "mode").argtypes = C.c_uint32, [C.c_uint32] * 3
    getattr(lib, p + "fade").restype, getattr(lib, p + "fade").argtypes = C.c_uint32, [C.c_uint64, C.c_uint32, C.c_int32]
    getattr(lib, p + "dd").restype, getattr(lib, p + "dd").argtypes = None, [C.c_int32]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    s.words(s.g("init"), [case["init"]])
    s.words(s.g("fa"), [case["fa"]])
    s.words(s.g("fb"), [case["fb"]])
    s.words(s.g("active"), [case["active"]])
    s.words(s.g("step"), [case["step"]])
    case["_pal"] = 0
    if case["pal"] is not None:
        a = s.alloc(0x400)
        s.write(a, case["pal"])
        case["_pal"] = a


def execute(s, case):
    if case["op"] == "mode":
        return s.mode(*case["args"])
    if case["op"] == "fade":
        return s.fade(case["_pal"], case["args"][0], case["args"][1])
    return s.dd(case["args"][0])


def observe(s, case, answer):
    return {"answer": answer, "log": s.log(), "active": s.rd_words(s.g("active"), 1), "step": s.rd_words(s.g("step"), 1),
            "saved": s.read(s.g("saved"), 0x400), "zero": s.read(s.g("zero"), 0x400)}


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, execute(s, case))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def base(r, op):
    return {"op": op, "init": r.choice([0, 1, 1, 1]), "fa": r.choice([0, 0, 1, 5]), "fb": r.choice([0, 0, 1, 5]),
            "active": r.choice([0, 1]), "step": r.randrange(0, 6), "pal": None, "queues": {"get": [r.randrange(256)]}}


W, H, D = (640, 800, 320), (480, 600, 240), (8, 16, 32)


def mode_case(r, ff=None, fa=None, fb=None, init=None, hit=None):
    c = base(r, "mode")
    if init is not None:
        c["init"] = init
    if fa is not None:
        c["fa"] = fa
    if fb is not None:
        c["fb"] = fb
    k = r.randrange(0, 4)
    modes = [(r.choice(W), r.choice(H), r.choice(D)) for _ in range(k + 1)]
    cont = 0 if c["fb"] == 0 else -1
    term = -1 if c["fb"] == 0 else 0
    first = r.choice([0, 0, -1, 5]) if ff is None else ff
    q = {"get": [0], "ff_res": [first], "ff_w": [r.choice(W)], "ff_h": [r.choice(H)], "ff_d": [r.choice(D)],
         "fn_res": [cont] * k + [term], "fn_w": [m[0] for m in modes], "fn_h": [m[1] for m in modes], "fn_d": [m[2] for m in modes]}
    c["queues"] = q
    pool = modes + [(q["ff_w"][0], q["ff_h"][0], q["ff_d"][0])]
    c["args"] = list(r.choice(pool)) if (hit is None and r.random() < 0.6) or hit else [r.choice(W), r.choice(H), r.choice(D)]
    return c


def mode_edges():
    r = random.Random(11)
    for init in (0, 1):
        for fa in (0, 1):
            for fb in (0, 1):
                for ff in (0, -1):
                    for hit in (True, False):
                        yield mode_case(r, ff, fa, fb, init, hit)


def mode_random(r):
    return mode_case(r)


def fade_case(r, t=None, n=None, nopal=None):
    c = base(r, "fade")
    n = r.randrange(1, 9) if n is None else n
    t = r.choice([0, 0, 1, 2, -1]) if t is None else t
    c["args"] = [n, t]
    if nopal if nopal is not None else r.random() < 0.2:
        c["pal"] = None
    else:
        c["pal"] = bytes(r.randrange(256) for _ in range(0x400))
    c["queues"] = {"get": [r.randrange(256)]}
    c["step"] = r.randrange(0, 12)
    return c


def fade_edges():
    r = random.Random(12)
    for t in (0, 1, 2):
        for n in (1, 2, 3, 7):
            for nopal in (False, True):
                for active in (0, 1):
                    c = fade_case(r, t, n, nopal)
                    c["active"] = active
                    yield c
                    c = fade_case(r, t, n, nopal)
                    c["active"], c["step"] = 1, n - 1
                    yield c


def fade_random(r):
    return fade_case(r)


CODES = [0, 1, -1, 0x7FFFFFFF, s32(0x80000000)]
for d in (0x778A0000, 0x7FF90000, 0x80004000):
    CODES += [s32(d + o) for o in (0, 1, 0x57, 0xE, 0x4001, 0x4005)]
CODES += [s32(0x8876FEFC), s32(0x8876FF6F), s32(0x8876FD00)] + [s32(0x88760000 + o) for o in range(0, 0x260, 0x5)]


def dd_case(r, code=None):
    c = base(r, "dd")
    c["args"] = [r.choice(CODES) if code is None else code]
    return c


def dd_edges():
    r = random.Random(13)
    for code in CODES:
        yield dd_case(r, code)


TESTS = [
    FnTest("LbScreen_IsModeAvailable", ppc, host, edge=mode_edges, rnd=mode_random),
    FnTest("LbScreen_FadePalette", ppc, host, edge=fade_edges, rnd=fade_random),
    FnTest("OS_dd_error", ppc, host, edge=dd_edges, rnd=lambda r: dd_case(r)),
]
