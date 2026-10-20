"""Unit v5895, the options panel's DoScroll and StartScroll, against libtpw_unit_DoScroll_3.dylib (tests/unit_DoScroll_3.cpp), main app:
  DoScroll 0x101a8fb8, StartScroll 0x101a96f8 (src/ui_mainpanel2_window_bumhole_DoScroll_3.cpp)

The callees are fakes on both sides that record their arguments: GetWindow, UIMain_GetString (writes a marker string into the
convert buffer), the virtual SetText and SetValue (through __ptr_glue), SetResolution, SetGraphicsDetail, SetSoundQuality, the three
SOUND volumes, and wcsncpy, wcscat and swprintf (which really copy, append and write a marker). The world: the settings block, the
convert buffer, gSetOnOffRoot, gOptionsPanelF0C. Compared: the log, the settings, the buffer, the flag.
No fused operations. Not tested: nothing skipped.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_DoScroll_3"
MASK = 0xFFFFFFFF

FN_DO, FN_ST = 0x101A8FB8, 0x101A96F8
SIZES = {FN_DO: 1824, FN_ST: 236}
OWN = set(SIZES)

GETWINDOW, GETSTRING, PTRGLUE = 0x101DBE40, 0x10180468, 0x1023BAA8
SETRES, SETDETAIL, SETSQ = 0x10166558, 0x10166628, 0x101664BC
SFX, MUSIC, SPEECH = 0x100DD238, 0x100DD1F8, 0x100DD278
WCSNCPY, WCSCAT, SWPRINTF = 0x10238CB0, 0x10239370, 0x10239160

ADDR = dict(settings=0x1038071C, buf=0x10380C48, root=0x10394A14, f0c=0x10394A0C, fmt=0x1024E168)
HOST_LAYOUT = [0x3C, 4, 8, 0x10, 0x18, 0x20, 0x28, 0x30, 4, 8]


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def chars(e, p, n):
    out = []
    for i in range(n):
        c = struct.unpack(">H", e.read(p + 2 * i, 2))[0]
        if c == 0:
            break
        out.append(c)
    return out + [0] * (n - len(out))


def wstr(e, p):
    out = []
    while True:
        c = struct.unpack(">H", e.read(p + 2 * len(out), 2))[0]
        if c == 0:
            return out
        out.append(c)


def putw(e, p, ws):
    e.write(p, b"".join(struct.pack(">H", c & 0xFFFF) for c in ws) + b"\0\0")


def st_getwindow(e):
    e.cl.rec("GetWindow", (e.cl.lab(e.r(3)), s32(e.r(4))))
    e.set_r(3, e.cl.win)


def st_getstring(e):
    i = s32(e.r(3))
    e.cl.rec("GetString", (i,))
    s = [0x1000 + i, 0x58]
    putw(e, ADDR["buf"], s)
    p = e.alloc(16)
    putw(e, p, s)
    e.set_r(3, p)


def st_glue(e):
    t = e.r(12) & MASK
    w = e.cl.lab(e.r(3))
    if t == 0xA000002C:
        e.cl.rec("SetText", (w,) + tuple(chars(e, e.r(4), 6)))
    elif t == 0xA0000024:
        e.cl.rec("SetValue", (w, s32(e.r(4))))
    else:
        raise AssertionError("glue target %x" % t)


def st_wcsncpy(e):
    dst, src, n = e.r(3), e.r(4), e.r(5)
    s = wstr(e, src)
    e.cl.rec("wcsncpy", (e.cl.lab(dst), n) + tuple((s + [0] * 4)[:4]))
    out = (s[:n] + [0] * n)[:n]
    e.write(dst, b"".join(struct.pack(">H", c) for c in out))


def st_wcscat(e):
    dst, src = e.r(3), e.r(4)
    d, s = wstr(e, dst), wstr(e, src)
    e.cl.rec("wcscat", (e.cl.lab(dst),) + tuple((d + [0] * 4)[:4]) + tuple((s + [0] * 4)[:4]))
    putw(e, dst, d + s)


def st_swprintf(e):
    buf, size, fmt, arg = e.r(3), e.r(4), e.r(5), e.r(6)
    off = (fmt - ADDR["fmt"]) // 2
    a = chars(e, arg, 1)[0] if off == 0 else s32(arg)
    e.cl.rec("swprintf", (e.cl.lab(buf), size, off, a))
    putw(e, buf, [0x70, 0x30 + off, a])


def rec1(name, fn):
    def f(e):
        fn(e)
    return f


STUBS.internal(GETWINDOW, st_getwindow)
STUBS.internal(GETSTRING, st_getstring)
STUBS.internal(PTRGLUE, st_glue)
STUBS.internal(WCSNCPY, st_wcsncpy)
STUBS.internal(WCSCAT, st_wcscat)
STUBS.internal(SWPRINTF, st_swprintf)
STUBS.internal(SETRES, lambda e: e.cl.rec("SetResolution", (e.cl.lab(e.r(3)), s32(e.r(4)))))
STUBS.internal(SETDETAIL, lambda e: e.cl.rec("SetGraphicsDetail", (e.cl.lab(e.r(3)), s32(e.r(4)))))
STUBS.internal(SETSQ, lambda e: e.cl.rec("SetSoundQuality", (e.cl.lab(e.r(3)), s32(e.r(4)))))
STUBS.internal(SFX, lambda e: e.cl.rec("SetSfx", (e.r(3) & MASK,)))
STUBS.internal(MUSIC, lambda e: e.cl.rec("SetMusic", (e.r(3) & MASK,)))
STUBS.internal(SPEECH, lambda e: e.cl.rec("SetSpeech", (e.r(3) & MASK,)))


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.root = e.alloc(0x40)
        self.win = e.alloc(0x200)
        vt = e.alloc(0x40)
        e.write(self.win + 0x118, struct.pack(">I", vt))
        e.write(vt + 0x24, struct.pack(">I", 0xA0000024))
        e.write(vt + 0x2C, struct.pack(">I", 0xA000002C))
        self.L = e.cl = CL.PpcLog({self.root: 1, self.win: 2, ADDR["settings"]: 3, ADDR["buf"]: 4}, case["queues"])
        self.L.win = self.win
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, fn, a, b):
        T.call_ppc(self.e, fn, [a & MASK, b & MASK])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.root = lib.tpw_ds_alloc(0x40)
        self.win = lib.tpw_ds_alloc(0x200)
        lib.tpw_ds_setwin(self.win)
        CL.host_begin(lib, {1: self.root, 2: self.win, 3: self.g("settings"), 4: self.g("buf")}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_ds_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, fn, a, b):
        (self.lib.tpw_ds_doscroll if fn == FN_DO else self.lib.tpw_ds_startscroll)(s32(a), s32(b))


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_ds_addr.restype, lib.tpw_ds_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_ds_alloc.restype, lib.tpw_ds_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_ds_setwin.argtypes = [C.c_uint64]
    lib.tpw_ds_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ds_doscroll.argtypes = [C.c_int32, C.c_int32]
    lib.tpw_ds_startscroll.argtypes = [C.c_int32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_ds_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    st = s.g("settings")
    s.words(st, [case["ren"], case["res"], case["det"], case["card"]])
    for i, (on, vol) in enumerate(case["vols"]):
        a = st + 0x10 + 8 * i
        s.write(a, bytes([on, 0, 0, 0]))
        s.words(a + 4, [vol])
    s.words(st + 0x30, [case["sq"]])
    s.words(s.g("f0c"), [case["f0c"]])
    s.write(s.g("root"), struct.pack(s.fmt[0] + ("Q" if s.fmt[0] == "<" else "I"), s.root))
    s.words(s.g("buf"), [0] * 8)


def observe(s, case):
    st = s.g("settings")
    d = {"log": s.log(), "head": s.rd_words(st, 4), "sq": s.rd_word(st + 0x30), "f0c": s.rd_word(s.g("f0c"))}
    d["vols"] = [(s.read(st + 0x10 + 8 * i, 1), s.rd_word(st + 0x14 + 8 * i)) for i in range(4)]
    d["buf"] = list(struct.unpack(s.fmt[0] + "16H", s.read(s.g("buf"), 32)))
    return d


def run(s, case):
    try:
        build(s, case)
        fn = FN_DO if case["op"] == "do" else FN_ST
        s.call(fn, *case["args"])
        return observe(s, case)
    finally:
        s.done()


def ppc(e, case):
    if not getattr(e, "probed", False):
        e.probed = True
        print("DO table", [hex(e.u32(0x102AB66C + 4 * i)) for i in range(12)])
        print("ST table", [hex(e.u32(0x102AB69C + 4 * i)) for i in range(9)])
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def base(r, op):
    c = {"op": op, "queues": {}, "ren": r.choice([0, 1, 2, 5]), "res": r.choice([-1, 0, 1, 2, 3, 4, 7]),
         "det": r.choice([-1, 0, 1, 2, 3, 5]), "card": r.randrange(3),
         "vols": [(r.choice([0, 1]), r.choice([0, 50, -3, 100])) for _ in range(4)], "sq": r.choice([0, 7, -1]),
         "f0c": r.choice([0, 1, 99])}
    return c


VALUES = [0, 1, -1, 5, 49, 50, 99, 100, 101, 255, -50, 0x40000000, 0x7FFFFFFF, -0x80000000, 1000]


def case(r, op, id_, v):
    c = base(r, op)
    c["args"] = [id_, v]
    return c


def do_edges():
    r = random.Random(11)
    for idx in range(-2, 15):
        for v in VALUES:
            yield case(r, "do", 0x1D4D5 + idx, v)
    for id_ in (0, -1, 0x7FFFFFFF):
        yield case(r, "do", id_, 5)
    for _ in range(40):
        c = case(r, "do", 0x1D4D5 + r.randrange(12), r.choice(VALUES))
        c["vols"] = [(1, 3)] * 4
        yield c


def do_random(r):
    return case(r, "do", 0x1D4D5 + r.randrange(-1, 14), r.choice([r.randrange(-200, 300), r.choice(VALUES)]))


def st_edges():
    r = random.Random(12)
    for idx in range(-2, 12):
        for v in VALUES:
            yield case(r, "st", 0x1D4D8 + idx, v)
    for id_ in (0, -1, 0x7FFFFFFF):
        yield case(r, "st", id_, 5)


def st_random(r):
    return case(r, "st", 0x1D4D8 + r.randrange(-1, 11), r.choice([r.randrange(-200, 300), r.choice(VALUES)]))


TESTS = [
    FnTest("DoScroll", ppc, host, edge=do_edges, rnd=do_random),
    FnTest("StartScroll", ppc, host, edge=st_edges, rnd=st_random),
]
