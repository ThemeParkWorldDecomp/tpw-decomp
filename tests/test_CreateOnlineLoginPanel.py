"""Unit v5339, CreateOnlineLoginPanel (makes the online login dialog and fits its controls), against libtpw_unit_CreateOnlineLoginPanel.dylib
(tests/unit_CreateOnlineLoginPanel.cpp), main app ("SimTheme Park"), src/game/ui/ui_onlineLogin.cpp:
  CreateOnlineLoginPanel 0x101e27bc

A case is a world and a call. The world is built twice by ONE piece of Python code (emulator memory for the original, host memory for the
translation). Every callee outside the unit (Dialog, GetWindow, the window setters, the constructors of the draw objects, operator new,
GetFontTableEntry, UIMain_GetString, FindDrawObject, the virtual SetText, UI_TITLE_COLOUR) is a fake on both sides that records its arguments.
The six windows (the dialog and its five children) are fixed blocks; labels name them.

The world of a case: the failure function argument, the old values of the failure-function global and the panel flag, and the answers of
GetFontTableEntry, UIMain_GetString and FindDrawObject. What is compared: the event log, the failure-function global and whether the flag
holds the dialog.

No fused operations. Not tested: nothing excluded.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CreateOnlineLoginPanel"
MASK = 0xFFFFFFFF

FN = 0x101E27BC
SIZES = {FN: 1012}
OWN = set(SIZES)

ADDR = dict(script=0x102B02FC, empty=0x1024EE76, callback=0x10395D28, flag=0x10395D2C, pass_=0x10395D30, name=0x10395D72,
            handler=0x102684B8)
HOST_NAMES = dict(script="script", empty="empty", callback="callback", flag="flag", pass_="pass", name="name", handler="handler")
WINS = ["win%d" % i for i in range(6)]
LABELS = ["win0", "win1", "win2", "win3", "win4", "win5", "script", "handler", "name", "pass_", "empty"]


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def R2(e):
    """The glue saves the TOC pointer where the caller restores it from."""
    e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2) & MASK))


def stub_dialog(e):
    L = e.cl
    L.rec("Dialog", (L.lab(e.r(3)), L.lab(e.r(4)), s32(e.r(5))))
    e.set_r(3, ADDR["win0"])


def stub_getwindow(e):
    L = e.cl
    i = s32(e.r(4))
    L.rec("GetWindow", (L.lab(e.r(3)), i))
    e.set_r(3, ADDR["win%d" % (i - 0x49 + 1)] if 0x49 <= i <= 0x4D else 0)


def stub_new(e):
    L = e.cl
    L.rec("new", (e.r(3) & MASK,))
    e.set_r(3, e.alloc(e.r(3)))
    R2(e)


def stub_ptr_glue(e):
    L = e.cl
    w, t = e.r(3), e.r(4)
    k = L.lab(t)
    L.rec("SetText", (L.lab(w), k, (t & MASK) if k == -1 else 0))
    e.set_r(3, 1)
    R2(e)


def stub_findobj(e):
    L = e.cl
    L.rec("FindDrawObject", (e.cstr(e.r(3)),))
    e.set_r(3, L.ret("FindDrawObject") & MASK)


def simple(addr, name, spec, ret=None):
    STUBS.internal(addr, CL.recorder(name, spec, ret))


STUBS.internal(0x101F12C8, stub_dialog)
STUBS.internal(0x101DBE40, stub_getwindow)
simple(0x101CD7E0, "SetStaticTextArea", "LLi")
simple(0x1018F1D0, "CursorCtor", "Nu")
simple(0x101DB624, "SetClientDrawObject", "LL")
simple(0x10180270, "GetFontTableEntry", "u", "i")
simple(0x101DA118, "SetFont", "Lu")
simple(0x101DA3AC, "SetFontColour", "Liiii")
simple(0x101DA204, "SetFontJust", "Lii")
simple(0x101CD2D4, "FindDrawObject", "s", "i")
simple(0x101CDAC4, "SetCursorDrawObject", "Lu")
STUBS.internal(0x1023BAA8, stub_ptr_glue)
simple(0x1018D11C, "SpriteCtor", "Nu")
simple(0x10180468, "UIMain_GetString", "i", "i")
simple(0x101DE130, "SetClientDrawText", "Lu")
simple(0x1018DC38, "PasswordCtor", "Nu")
simple(0x101DCCEC, "CaptureKb", "Li")
simple(0x1018FBE8, "TitleCtor", "Nu")
simple(0x1018BD3C, "UI_TITLE_COLOUR", "L")
STUBS.internal(0x102384B8, stub_new)


def setup(runner):
    T.install_std_stubs(runner.emu)
    for n in WINS:
        data = bytearray(0x400)
        vt = T.persist_alloc(runner.emu, 0x100)
        struct.pack_into(">I", data, 0x118, vt)
        ADDR[n] = T.persist_alloc(runner.emu, 0x400, bytes(data))


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
        self.L = e.cl = CL.PpcLog({ADDR[n]: i + 1 for i, n in enumerate(LABELS)}, case["queues"])
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

    def create(self, fn):
        T.call_ppc(self.e, FN, [fn & MASK])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {i + 1: self.g(n) for i, n in enumerate(LABELS)}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_cr_addr(HOST_NAMES.get(name, name).encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def create(self, fn):
        self.lib.tpw_cr_create(fn & MASK)


HOST_LAYOUT = [4, 33]


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_cr_addr.restype, lib.tpw_cr_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_cr_init.restype, lib.tpw_cr_init.argtypes = None, []
    lib.tpw_cr_create.restype, lib.tpw_cr_create.argtypes = C.c_uint32, [C.c_uint32]
    lib.tpw_cr_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_cr_init()
    out = (C.c_int64 * 32)()
    n = lib.tpw_cr_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    s.words(s.g("callback"), [case["old_cb"]])
    s.words(s.g("flag"), [case["old_flag"]])


def observe(s, case):
    s.create(case["fn"])
    flag = s.rd_word(s.g("flag"))
    return {"log": s.log(), "callback": s.rd_word(s.g("callback")), "flag_is_dialog": flag == (s.g("win0") & MASK)}


def run(s, case):
    try:
        build(s, case)
        return observe(s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def case(r, fn=None):
    def q():
        return [r.randrange(1, 0x7FFFFFFF) for _ in range(r.choice([1, 3, 6]))]

    return {"fn": r.choice([0, 1, 0x101E27BC, r.randrange(0, 1 << 32)]) if fn is None else fn,
            "old_cb": r.choice([0, r.randrange(0, 1 << 32)]), "old_flag": r.choice([0, MASK, r.randrange(0, 1 << 32)]),
            "queues": {"GetFontTableEntry": q(), "UIMain_GetString": q(), "FindDrawObject": q()}}


def edges():
    r = random.Random(3391)
    for fn in (0, 1, 0x1000, 0x7FFFFFFF, 0x80000000, MASK):
        yield case(r, fn)


def rnd(r):
    return case(r)


TESTS = [
    FnTest("CreateOnlineLoginPanel", ppc, host, edge=edges, rnd=rnd),
]
