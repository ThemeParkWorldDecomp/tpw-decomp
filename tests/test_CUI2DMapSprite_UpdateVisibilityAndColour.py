"""Unit v5652, CUI2DMapSprite::UpdateVisibilityAndColour 0x10231134, against libtpw_unit_CUI2DMapSprite_UpdateVisibilityAndColour.dylib
(tests/unit_CUI2DMapSprite_UpdateVisibilityAndColour.cpp), main app, src/CUI2DMap_..._CUI2DMapSprite_UpdateVisibilityAndColour.cpp.

A case is a sprite object and a call. The object, the colour table, the thing block and the details block are built twice by ONE piece of
Python code (emulator memory big-endian, host memory little-endian). The callees outside the unit are fakes on both sides:
CEZSprite::GetSpriteDetails (logs the instance, answers the case's details block) and UI2DMReturnScaleColour (logs the index and the
offsets of its three colour arguments inside the table, answers a colour word from the queue "Scale").

Compared: the object's fields, the whole details block, the table and the event log. The host layout has a native pointer at 0x18, so the
host offsets of the fields come from tpw_uvc_layout. Fctiwz is run under HardwareRounding; values stay far below 2^31.
Not tested: NaN or huge floats in the thing (fctiwz saturation differs in the emulator).
"""
import ctypes as C
import random
import struct

from unicorn import UC_HOOK_CODE

import calllog as CL
import tpwtest as T
from tpwtest import FnTest
from test_DynamicMesh import HardwareRounding

SHIM_LIBRARY = "unit_CUI2DMapSprite_UpdateVisibilityAndColour"
MASK = 0xFFFFFFFF

FN = 0x10231134
SIZES = {FN: 860}
OWN = set(SIZES)
GET_DETAILS = 0x1023ae40
SCALE = 0x1022bb44
ADDR = dict(colours=0x103970f0)
MAC = dict(kind=0x14, thing=0x18, mode=0x1c, match=0x20, sprite=0x28, visible=0x2c)
HOST = {}
HOST_NAMES = ["kind", "thing", "mode", "match", "sprite", "visible"]
HOST_SIZE = [0]
TABLE_WORDS = 23 * 9


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def s32(v):
    return T.s32(v)


class ROUNDING(HardwareRounding):
    def __enter__(self):
        for begin, size in SIZES.items():
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=begin, end=begin + size - 1))


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_details(e):
    L = e.cl
    L.rec("GetSpriteDetails", (L.lab(e.r(4)),))
    e.set_r(3, L.det)


def stub_scale(e):
    L = e.cl
    base = ADDR["colours"]
    L.rec("Scale", (s32(e.r(4)), s32(e.r(5) - base), s32(e.r(6) - base), s32(e.r(7) - base)))
    w = L.ret("Scale") & MASK
    e.write(e.r(3), struct.pack(">I", w))


STUBS.internal(GET_DETAILS, stub_details)
STUBS.internal(SCALE, stub_scale)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def prepare(self, case):
        self.sprite = self.alloc(16)
        self.det = self.alloc(0x40)
        self.obj = self.alloc(self.objsize())


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.sprite = e.alloc(16)
        self.det = e.alloc(0x40)
        self.L = e.cl = CL.PpcLog({self.sprite: 1}, case["queues"])
        self.L.det = self.det
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

    def off(self, name):
        return MAC[name]

    def setf(self, obj, name, v):
        if name in ("thing", "sprite"):
            self.write(obj + MAC[name], struct.pack(">I", v & MASK))
        else:
            self.words(obj + MAC[name], [v])

    def getf(self, obj, name):
        return self.rd_words(obj + MAC[name], 1)[0]

    def objsize(self):
        return 0x30

    def run(self, obj, mask, second, third):
        with ROUNDING(self.e):
            T.call_ppc(self.e, FN, [obj, mask & MASK, second & MASK, third & MASK])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.sprite = self.alloc(16)
        self.det = self.alloc(0x40)
        CL.host_begin(lib, {1: self.sprite}, case["queues"])
        self.write(self.g("det"), struct.pack("<Q", self.det))

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_uvc_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_uvc_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def setf(self, obj, name, v):
        if name in ("thing", "sprite"):
            self.write(obj + HOST[name], struct.pack("<Q", v))
        else:
            self.words(obj + HOST[name], [v])

    def getf(self, obj, name):
        return self.rd_words(obj + HOST[name], 1)[0]

    def objsize(self):
        return HOST_SIZE[0]

    def run(self, obj, mask, second, third):
        self.lib.tpw_uvc_run(obj, s32(mask), s32(second), s32(third))


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_uvc_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "run").restype, getattr(lib, p + "run").argtypes = None, [C.c_uint64, C.c_int32, C.c_int32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    vals = list(out)[:n]
    assert vals[7] == 0x24, vals
    for k, nm in enumerate(HOST_NAMES):
        HOST[nm] = vals[k]
    HOST_SIZE[0] = vals[6]
    lib.bound = True


def build(s, case):
    s.write(s.g("colours"), b"".join(struct.pack(">I", w) for w in case["table"]))   # bytes, same on both sides (the code reads bytes)
    s.words(s.det, case["det"])
    s.thing = 0
    if case["thing"] is not None:
        s.thing = s.alloc(0x200)
        t = case["thing"]
        s.write(s.thing + 2, bytes([t["type"]]))
        s.words(s.thing + 0x19c, [t["f19c"]])
        s.words(s.thing + 0x1f4, [t["f1f4"]])
    s.obj = s.alloc(s.objsize())
    s.setf(s.obj, "kind", case["kind"])
    s.setf(s.obj, "thing", s.thing)
    s.setf(s.obj, "mode", case["mode"])
    s.setf(s.obj, "match", case["match"])
    s.setf(s.obj, "sprite", s.sprite)
    s.setf(s.obj, "visible", case["visible"])


def observe(s, case):
    d = {"log": s.log(), "det": s.rd_words(s.det, 16), "table": s.read(s.g("colours"), TABLE_WORDS * 4)}
    for nm in ("kind", "mode", "match", "visible"):
        d[nm] = s.getf(s.obj, nm)
    return d


def run(s, case):
    try:
        build(s, case)
        s.run(s.obj, *case["args"])
        return observe(s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def case_of(r, kind=None, mode=None, hit=None, visible=None, ttype=None):
    kind = r.choice([-1, 0, 1, 2, 3, 4, 5, 6]) if kind is None else kind
    mode = r.choice([0, 1, 2, 3, 4]) if mode is None else mode
    hit = r.choice([True, False]) if hit is None else hit
    visible = r.choice([0, 1, 2]) if visible is None else visible
    m = r.choice([1, 2, 4, 0x80, 7, r.randrange(1, 1 << 31)]) if mode == 1 else r.choice([0, 1, 5, -1, r.randrange(-50, 50)])
    if mode == 1:
        mask = (m | r.randrange(0, 1 << 32)) if hit else (r.randrange(0, 1 << 32) & ~m)
    else:
        mask = r.randrange(0, 1 << 32)
    sec = m if (mode == 2 and hit) else m + r.choice([1, -1, 3])
    thr = m if (mode == 3 and hit) else m + r.choice([1, -1, 3])
    if mode == 2 and not hit:
        thr = m
    if mode == 3 and not hit:
        sec = m
    thing = None
    if r.random() < 0.85:
        thing = {"type": r.choice([1, 6, 0, 2, 255]) if ttype is None else ttype,
                 "f19c": fb(r.choice([0.0, -0.0, 0.9, 50.5, 255.0, 256.0, 300.7, -1.5, -300.0, r.uniform(-1000, 1000)])),
                 "f1f4": fb(r.choice([0.0, 1.0, 99.99, 255.5, 257.0, -7.9, r.uniform(-1000, 1000)]))}
    return {"kind": kind, "mode": mode, "match": m & MASK, "visible": visible,
            "args": [mask, sec, thr], "thing": thing,
            "table": [r.randrange(0, 1 << 32) for _ in range(TABLE_WORDS)],
            "det": [r.randrange(0, 1 << 32) for _ in range(16)],
            "queues": {"Scale": [r.randrange(0, 1 << 32)]}}


def edges():
    r = random.Random(5652)
    for kind in (-1, 0, 1, 2, 3, 4, 5, 6):
        for mode in (0, 1, 2, 3, 4):
            for hit in (True, False):
                for visible in (0, 1, 2):
                    yield case_of(r, kind, mode, hit, visible)
    for ttype in (1, 6, 0):
        for kind in (0, 4):
            yield case_of(r, kind, 1, True, 0, ttype)
    c = case_of(r, 0, 1, True, 0, 1)
    c["thing"] = None
    yield c
    c = case_of(r, 4, 2, True, 1, 6)
    c["thing"] = None
    yield c


def rnd(r):
    return case_of(r)


TESTS = [
    FnTest("CUI2DMapSprite::UpdateVisibilityAndColour", ppc, host, edge=edges, rnd=rnd),
]
