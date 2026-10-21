"""Unit v5241, the ride list panel's info window, against libtpw_unit_UpdateInfoWindow.dylib (tests/unit_UpdateInfoWindow.cpp), main app
("SimTheme Park"), src/game/ui/ui_ridepanel.cpp:
  InitInfoWindow 0x101b6500, UpdateInfoWindow 0x101b6a5c

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. The callees outside the unit (GetWindow, GetRideInfo,
GetParkAnalyser, NumberOfObjectInPark, the draw object constructors, SetClientDrawObject, SetFont, SetClientDrawText, GetFontTableEntry,
UIMain_GetString, UIMain_IndentClient, swprintf and the windows' virtual SetValue / SetText through __ptr_glue) are fakes on both sides that
record their arguments (text as a hash where the harness records ints) and answer from the case.

The world of a case: the panel pointer, the list type, the world, the ride record's three slots (0x13c, 0x19c, 0x1b0), the strings of the table,
the windows that exist (the GetWindow queues), the park count. What is compared: the event log and the first 64 characters of the shared text
buffer (0x10380c48).

No fused operations. Not tested: the sizes operator new is given (the host classes have other sizes than the Mac's 0x19c and 0x124), a NULL
string from UIMain_GetString (the fake table always answers).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_UpdateInfoWindow"
MASK = 0xFFFFFFFF

INIT, UPDATE = 0x101b6500, 0x101b6a5c
SIZES = {INIT: 1332, UPDATE: 612}
OWN = set(SIZES)

GETWINDOW, GETRIDEINFO, GETANALYSER, NUMOBJ = 0x101dbe40, 0x10154868, 0x1013ebc8, 0x100ea52c
GETFONT, CTOR_SPRITE, CTOR_GRAD = 0x10180270, 0x1018d11c, 0x101898c8
SETOBJ, SETTEXT, SETFONT = 0x101db624, 0x101de130, 0x101da118
GETSTRING, INDENT, GLUE = 0x10180468, 0x10180940, 0x1023baa8

ADDR = dict(gRideListType=0x10395994, gRideListPanel=0x10395998, gGameWorld=0x1037ebfc, buffer=0x10380c48, rdb=0x1037f6c8)
HOST_LAYOUT = [632, 824, 864, 0x8000, 2]          # the host's balance values are 8 bytes each, from 24; the Mac's are 4 bytes from 0x0c
MAC_SLOTS = [0x13c, 0x19c, 0x1b0]
HOST_SLOTS = [632, 824, 864]

VT_SETVALUE, VT_SETTEXT = 0xD0000024, 0xD000002C
IDS = list(range(0x1ed, 0x1f8))                       # window ids; label = index + 1 (0x1ed is label 1)
LABEL = {i: n + 1 for n, i in enumerate(IDS)}
PANEL_LABEL = 12
RIDE, WORLD, ANALYSER, STRINGS, FONT, RDB = 13, 14, 15, 16, 17, 18
STRING_IDS = [0x7d, 0x7e, 0x7f, 0x80, 0x81, 0x88]
PBLK = {}


def wstr(read, a, limit=0x4000):
    out = []
    while len(out) < limit:
        chunk = read(a + 2 * len(out), 32)
        for i in range(0, 32, 2):
            c = (chunk[i] << 8) | chunk[i + 1] if read.__self__.__class__.__name__ != "Host" else chunk[i] | (chunk[i + 1] << 8)
            if c == 0:
                return out
            out.append(c)
    return out


def hash_of(chars):
    h = 0
    for c in chars:
        h = (h * 31 + c) & MASK
    return h


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def r_getwindow(e, log):
    return PBLK[log.ret("GetWindow:%d" % s32(e.r(4)))] if log.ret("GetWindow:%d" % s32(e.r(4))) else 0


STUBS.internal(GETWINDOW, CL.recorder("GetWindow", "Li", ret=r_getwindow))
STUBS.internal(GETRIDEINFO, CL.recorder("GetRideInfo", "Lw", ret=lambda e, log: PBLK[RIDE]))
STUBS.internal(GETANALYSER, CL.recorder("GetParkAnalyser", "L", ret=lambda e, log: PBLK[ANALYSER]))
STUBS.internal(NUMOBJ, CL.recorder("NumberOfObjectInPark", "Lw", ret="i"))
STUBS.internal(GETFONT, CL.recorder("GetFontTableEntry", "u", ret=lambda e, log: PBLK[FONT]))
STUBS.internal(CTOR_SPRITE, CL.recorder("InterfaceTextObjectSprite", "NL"))
STUBS.internal(CTOR_GRAD, CL.recorder("InterfaceDrawEngineHGradient", "Ns"))
STUBS.internal(SETOBJ, CL.recorder("SetClientDrawObject", "LL"))
STUBS.internal(SETFONT, CL.recorder("SetFont", "LL"))
STUBS.internal(SETTEXT, CL.recorder("SetClientDrawText", "LS"))
STUBS.internal(GETSTRING, CL.recorder("UIMain_GetString", "i", ret=lambda e, log: PBLK[STRINGS] + s32(e.r(3)) * 0x10))
STUBS.internal(INDENT, CL.recorder("UIMain_IndentClient", "Lhh"))


def ppc_wstr(e, a):
    out = []
    while len(out) < 0x4000:
        c = struct.unpack(">H", e.read(a + 2 * len(out), 2))[0]
        if c == 0:
            break
        out.append(c)
    return out


def stub_swprintf(e):
    L = e.cl
    dest, size, fmt, arg = e.r(3), e.r(4), e.r(5), s32(e.r(6))
    f = ppc_wstr(e, fmt)
    is_int = f == [0x25, 0x64]
    out = list(str(arg).encode()) if is_int else f
    if size > 0:
        out = out[:size - 1]
        e.write(dest, b"".join(struct.pack(">H", c) for c in out + [0]))
    else:
        out = []
    L.rec("swprintf", (size, 1 if is_int else 0, arg if is_int else 0, hash_of(f)))
    e.set_r(3, len(out) & MASK)


STUBS.imp("swprintf", stub_swprintf)


def stub_glue(e):
    L = e.cl
    fn, window, arg = e.r(12), e.r(3), e.r(4)
    if fn == VT_SETVALUE:
        L.rec("SetValue", (L.lab(window), s32(arg)))
    elif fn == VT_SETTEXT:
        L.rec("SetText", (L.lab(window), hash_of(ppc_wstr(e, arg))))
    else:
        raise AssertionError("unknown virtual %x" % fn)


STUBS.internal(GLUE, stub_glue)


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    vt = T.persist_alloc(e, 0x100)
    e.write(vt + 0x24, struct.pack(">I", VT_SETVALUE))
    e.write(vt + 0x2c, struct.pack(">I", VT_SETTEXT))
    for label in range(1, 19):
        PBLK[label] = T.persist_alloc(e, 0x1000 if label == STRINGS else 0x400)
        if label <= 12:
            e.write(PBLK[label] + 0x118, struct.pack(">I", vt))


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def rd_halves(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dH" % n, self.read(a, 2 * n)))


class Ppc(Side):
    fmt = ">I"
    slots = MAC_SLOTS

    def __init__(self, e, case):
        self.e = e
        fixed = {PBLK[l]: l for l in range(1, 18)}
        fixed[ADDR["rdb"]] = RDB
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def blk(self, label):
        return PBLK[label]

    def ptr(self, a, v):
        self.words(a, [v])

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, op, param):
        if op == "init":
            T.call_ppc(self.e, INIT, [])
        else:
            T.call_ppc(self.e, UPDATE, [param])


class Host(Side):
    fmt = "<I"
    slots = HOST_SLOTS

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        fixed = {l: self.blk(l) for l in range(1, 18)}
        fixed[RDB] = self.g("rdb")
        CL.host_begin(lib, fixed, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_ui_addr(name.encode())

    def blk(self, label):
        return self.lib.tpw_ui_block(label)

    def ptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def alloc(self, n):
        return self.lib.tpw_ui_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, op, param):
        if op == "init":
            self.lib.tpw_ui_init()
        else:
            self.lib.tpw_ui_update(param)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_ui_addr.restype, lib.tpw_ui_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_ui_block.restype, lib.tpw_ui_block.argtypes = C.c_uint64, [C.c_int32]
    lib.tpw_ui_alloc.restype, lib.tpw_ui_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_ui_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ui_init.restype, lib.tpw_ui_init.argtypes = None, []
    lib.tpw_ui_update.restype, lib.tpw_ui_update.argtypes = None, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_ui_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    s.write(s.g("buffer"), bytes(0x80))
    s.ptr(s.g("gRideListPanel"), s.blk(PANEL_LABEL))
    s.words(s.g("gRideListType"), [case["type"]])
    s.ptr(s.g("gGameWorld"), s.blk(WORLD))
    for offset, value in zip(s.slots, case["ride"]):
        s.words(s.blk(RIDE) + offset, [value])
    for sid in STRING_IDS:
        text = case["str88"] if sid == 0x88 and "str88" in case else [0x73, 0x30 + sid // 16 % 10, 0x30 + sid % 10]
        for k, c in enumerate(text + [0]):
            s.half(s.blk(STRINGS) + sid * 0x10 + 2 * k, c)
    s.param = s.alloc(8)
    s.half(s.param, case["id"])


def observe(s, case):
    return {"log": s.log(), "buffer": s.rd_halves(s.g("buffer"), 64)}


def run(s, case):
    try:
        build(s, case)
        s.call(case["op"], s.param)
        return observe(s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

NUMS = [0, 1, -1, 99, 100, 101, 0x7FFFFFFF, -0x80000000, 0x200000, -0x200000, 12345678, 0x3FFFFF, 0x400000, -99, -100, 7, 1000000000]
STRS = [[0x4e, 0x2f, 0x41], [], [0x2d, 0x2d], list(range(0x41, 0x4d)), [0x31, 0x32, 0x33]]


def queues(r, absent=()):
    q = {}
    for i in IDS:
        q["GetWindow:%d" % i] = [0 if (i in absent or (i != 0x1ed and r.random() < 0.12)) else LABEL[i]]
    return q


def update_case(r, ltype=None, ident=None, count=None, ride=None, absent=()):
    q = queues(r, absent)
    q["NumberOfObjectInPark"] = [r.choice([0, 1, 5, 99, 123456789, -1, -123456789, 0x7FFFFFFF]) if count is None else count]
    return {"op": "update", "queues": q, "type": r.choice([-1, 0, 0, 1, 2, 2, 3, 9, -4]) if ltype is None else ltype,
            "id": r.choice([0, 1, 17, 0x7FFF, 0x8000, 0xFFFF, 0xFFFE, r.randrange(0x10000)]) if ident is None else ident,
            "ride": [r.choice(NUMS) if r.random() < 0.6 else r.randrange(-2**31, 2**31) for _ in range(3)] if ride is None else ride,
            "str88": r.choice(STRS)}


def update_edges():
    r = random.Random(41)
    for ltype in (-1, 0, 1, 2, 3, 9):
        for ident in (0, 5, 0x7FFF, 0x8000, 0xFFFF):
            for count in (0, 7):
                yield update_case(r, ltype, ident, count)
    for ltype in (0, 2):
        for gone in IDS:
            yield update_case(r, ltype, 3, r.choice([0, 4]), absent=(gone,))
    for n in NUMS:
        yield update_case(r, 0, 1, 2, [n, n, n])
        yield update_case(r, 2, 1, 2, [n, 0, 0])
    yield update_case(r, 0, 1, -2147483648)


def update_random(r):
    return update_case(r)


TEXT_IDS = [0x1f1, 0x1f2, 0x1f3, 0x1f4, 0x1f5, 0x1f6, 0x1f7]    # the bars (0x1ee to 0x1f0) are called through their vtable: never NULL


def init_case(r, absent=()):
    q = queues(r, absent)
    for i in (0x1ee, 0x1ef, 0x1f0):
        q["GetWindow:%d" % i] = [LABEL[i]]
    return {"op": "init", "queues": q, "type": 0, "id": 0, "ride": [0, 0, 0], "str88": []}


def init_edges():
    r = random.Random(42)
    yield init_case(r)
    yield init_case(r, (0x1ed,))
    for gone in TEXT_IDS:
        yield init_case(r, (gone,))


def init_random(r):
    return init_case(r, [i for i in TEXT_IDS if r.random() < 0.15])


TESTS = [
    FnTest("UpdateInfoWindow", ppc, host, edge=update_edges, rnd=update_random),
    FnTest("InitInfoWindow", ppc, host, edge=init_edges, rnd=init_random),
]
