"""Unit v5340, UIStatusPanel::ShowEditNamePanel, against libtpw_unit_UIStatusPanel_ShowEditNamePanel.dylib
(tests/unit_UIStatusPanel_ShowEditNamePanel.cpp), main app ("SimTheme Park"),
src/game/ui/ui_StatusPanel.cpp:
  UIStatusPanel::ShowEditNamePanel 0x10194208

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. Every callee outside the unit (the window calls,
the two constructors, the font and string getters, GetSignNameA/B, the virtual SetText) is a fake on both sides that records its
arguments; the sign-name fakes write "NameA" / "NameB" into the shared text buffer.

The world of a case: the panel (window at +8, parent window at +0xc whose halfword at +0xdc is the z), the global thing pointer (NULL or
a block whose byte 2 is the kind), the shared text buffer's first words. The parent's z is written at the Mac's +0xdc on the PPC
side and at the host window's mZ (from tpw_ses_layout) on the host side: the host window is laid out differently, so a raw +0xdc read
on the host misses it. What is compared: the event log, the panel's window field
(as a label), the first words of the text buffer, the thing's bytes. No fused operations. Not tested: nothing is skipped.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_UIStatusPanel_ShowEditNamePanel"
MASK = 0xFFFFFFFF

FN = 0x10194208
OWN = {FN}

ADDR = dict(script=0x102A8530, callback=0x10267960,   # callback: the tvector of 0x10193fac, TwoEditsMsgStatus on the host
            buf=0x10380C48, bufA=0x10394782, bufB=0x10394764, thing=0x103947A4)
PPC_WINDOW = (0xDC, 0xE0)

TEXT_TITLE = "Edit"
TEXT_FAKE_CODE = 0x11112222

STUBS = CL.Stubs()
STUBS.real.update(OWN)
WORLD = {}


def s32(v):
    return T.s32(v)


def text_of(e, addr):
    out = []
    for i in range(0x4000):
        c = struct.unpack(">H", e.read(addr + 2 * i, 2))[0]
        if c == 0:
            break
        out.append(chr(c))
    return "".join(out)


def put_text(e, addr, s):
    e.write(addr, b"".join(struct.pack(">H", ord(c)) for c in s) + b"\0\0")


def sign_stub(name, text):
    rec = CL.recorder(name, "LLu")

    def fn(e):
        rec(e)
        put_text(e, e.r(4), text)
    return fn


STUBS.internal(0x101DEB78, CL.recorder("Build", "LLL", ret=lambda e, log: WORLD["W"]))
STUBS.internal(0x101DD8BC, CL.recorder("setZ", "Lh"))
STUBS.internal(0x101DCFD0, CL.recorder("setExtraParam", "LhL"))
STUBS.internal(0x1018FBE8, CL.recorder("TitleCtor", "Ni"))
STUBS.internal(0x1018F1D0, CL.recorder("CursorCtor", "Ni"))
STUBS.internal(0x101DBE40, CL.recorder("GetWindow", "Li", ret=lambda e, log: WORLD[{0x3943E: "T", 0x3943C: "A"}.get(e.r(4), "B")]))
STUBS.internal(0x101DB624, CL.recorder("SetClientDrawObject", "LL"))
STUBS.internal(0x10180270, CL.recorder("GetFontTableEntry", "u", ret=lambda e, log: WORLD["font5"] if e.r(3) == 5 else WORLD["font7"]))
STUBS.internal(0x101DA118, CL.recorder("SetFont", "LL"))
STUBS.internal(0x101DA204, CL.recorder("SetFontJust", "Lii"))
STUBS.internal(0x10180468, CL.recorder("GetString", "i", ret=lambda e, log: WORLD["str"]))
STUBS.internal(0x1023BAA8, CL.recorder("SetText", "LS"))
STUBS.internal(0x1018BD3C, CL.recorder("UI_TITLE_COLOUR", "L"))
STUBS.internal(0x101DA3AC, CL.recorder("SetFontColour", "Liiii"))
STUBS.internal(0x101CD7E0, CL.recorder("SetStaticTextArea", "LLi"))
STUBS.internal(0x101CD2D4, CL.recorder("FindDrawObject", "s", ret=lambda e, log: WORLD["draw"]))
STUBS.internal(0x101CDAC4, CL.recorder("SetCursorDrawObject", "LL"))
STUBS.internal(0x101107C4, sign_stub("GetSignNameA", "NameA"))
STUBS.internal(0x101108DC, sign_stub("GetSignNameB", "NameB"))
STUBS.internal(0x101CE8A4, CL.recorder("SelectAll", "L"))
STUBS.internal(0x10182364, CL.recorder("DisableAllKeyTables", ""))
STUBS.internal(0x101DCCEC, CL.recorder("CaptureKb", "Li"))


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def make_world(self, case):
        w = {}
        w["P"] = self.alloc(0x10)
        z_off, window_size = self.window_layout()
        w["PW"] = self.alloc(window_size)
        for k in ("W", "T", "A", "B"):
            w[k] = self.alloc(0x130)
            self.hook_window(w[k])
        for k in ("font5", "font7", "draw"):
            w[k] = self.alloc(16)
        w["str"] = self.alloc(32)
        w["thing"] = self.alloc(0x10)
        self.write(w["PW"] + z_off, struct.pack(self.fmt[0] + "h", case["z"]))
        self.words(w["P"] + 8, [case["panel8"]])
        self.words(w["P"] + 0xC, [self.p32(w["PW"])])
        self.write(w["thing"], bytes(case["thingbytes"]))
        self.write(w["thing"] + 2, bytes([case["kind"]]))
        self.write(w["str"], b"".join(struct.pack(self.fmt[0] + "H", ord(c)) for c in TEXT_TITLE) + b"\0\0")
        self.ptr(self.g("thing"), w["thing"] if case["thing"] else 0)
        self.write(self.g("buf"), struct.pack(self.fmt[0] + "16H", *case["buf"]))
        return w

    def fixed(self, w):
        return {w["W"]: 1, w["T"]: 2, w["A"]: 3, w["B"]: 4, w["PW"]: 5, w["thing"]: 6, w["font5"]: 7, w["font7"]: 8, w["draw"]: 9,
                w["str"]: 10, w["P"]: 16}


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        w = self.w = self.make_world(case)
        WORLD.clear()
        WORLD.update(w)
        fixed = self.fixed(w)
        for lab, key in ((11, "script"), (12, "callback"), (13, "buf"), (14, "bufA"), (15, "bufB")):
            fixed[ADDR[key]] = lab
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.saved = STUBS.activate(e)

    def hook_window(self, wnd):
        tbl = self.alloc(0x40)
        tv = self.alloc(8)
        self.words(tv, [TEXT_FAKE_CODE, 0])
        self.words(tbl + 0x2C, [tv])
        self.words(wnd + 0x118, [tbl])

    def ptr(self, a, v):
        self.words(a, [v])

    def window_layout(self):
        return PPC_WINDOW

    def p32(self, v):
        return v

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
        T.call_ppc(self.e, FN, [self.w["P"] & MASK])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        w = self.w = self.make_world(case)
        arr = (C.c_uint64 * 8)(w["W"], w["T"], w["A"], w["B"], w["str"], w["font5"], w["font7"], w["draw"])
        lib.tpw_ses_setworld(arr)
        fixed = {v: k for k, v in self.fixed(w).items()}
        for lab, key in ((11, "script"), (12, "callback"), (13, "buf"), (14, "bufA"), (15, "bufB")):
            fixed[lab] = self.g(key)
        CL.host_begin(lib, fixed, case["queues"])

    def hook_window(self, wnd):
        v = self.lib.tpw_ses_addr(b"vtable")
        self.words(wnd, [v & MASK, v >> 32])

    def ptr(self, a, v):
        self.words(a, [v & MASK, v >> 32])

    def window_layout(self):
        return self.lib.window_layout

    def p32(self, v):
        return v - self.lib.tpw_ses_addr(b"base")

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_ses_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_ses_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self):
        self.lib.tpw_ses_show(self.w["P"])


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_ses_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "setworld").argtypes = [C.POINTER(C.c_uint64)]
    getattr(lib, p + "show").argtypes = [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    lib.window_layout = tuple(out[:n])
    lib.bound = True


def run(s, case):
    try:
        w = s.w if hasattr(s, "w") else None
        s.call()
        p8 = s.rd_word(w["P"] + 8)
        return {
            "log": s.log(),
            "panel8": "W" if p8 == s.p32(w["W"]) else p8,
            "buf": list(struct.unpack(s.fmt[0] + "16H", s.read(s.g("buf"), 32))),
            "thing": s.read(w["thing"], 16),
        }
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def mk(r, panel8=None, thing=None, kind=None, z=None):
    return {
        "queues": {},
        "panel8": r.choice([0, 0, 0, 0x1234, 0xFFFFFFFF]) if panel8 is None else panel8,
        "thing": r.choice([True, True, True, False]) if thing is None else thing,
        "kind": r.choice([3, 3, 3, 0, 1, 2, 4, 255]) if kind is None else kind,
        "z": r.choice([0, 1, -1, 0x7FFF, -0x8000, 0x100, r.randrange(-0x8000, 0x8000)]) if z is None else z,
        "thingbytes": [r.randrange(256) for _ in range(16)],
        "buf": [r.getrandbits(16) for _ in range(16)],
    }


def edges():
    r = random.Random(5340)
    for panel8 in (0, 1, 0x80000000):
        for thing in (False, True):
            for kind in (0, 2, 3, 4, 255):
                yield mk(r, panel8, thing, kind, r.choice([0, 0x7FFF, -0x8000, -0x11]))
    for z in (0, 0x7FFF, -0x8000, -1, -0x11, 0x7FEF, 0x7FEE):
        yield mk(r, 0, True, 3, z)


def rnd(r):
    return mk(r)


TESTS = [
    FnTest("UIStatusPanel::ShowEditNamePanel", ppc, host, edge=edges, rnd=rnd),
]
