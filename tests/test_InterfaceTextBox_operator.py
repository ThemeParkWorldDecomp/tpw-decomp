"""Unit v5712, InterfaceTextBox::operator-, against libtpw_unit_InterfaceTextBox_operator.dylib
(tests/unit_InterfaceTextBox_operator.cpp), main app ("SimTheme Park"), src/ui_handlers_small_InterfaceTextBox_operator.cpp:
  InterfaceTextBox::operator- 0x101d8034

A case is two windows (this and other) built twice by one piece of Python: the Mac's vtable pointer at +0x118 leads (slot +0x14) to a
function descriptor whose code address is a fake WhatAmI answering the word at +0x144; on the host a Probe object supplies the
virtual. wcscmp is a fake on both sides that records both strings and answers from the case. Compared: the answer and the event log
(the two WhatAmI calls in order, the wcscmp call). No fused operations. Not tested: NULL text blocks (wcscmp would be called on
NULL, which the original does not guard and a real wcscmp would crash on).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_InterfaceTextBox_operator"
MASK = 0xFFFFFFFF

FN = 0x101d8034
SIZES = {FN: 132}
OWN = set(SIZES)
WHATAMI_CODE = 0x101d9eb0       # where the fake descriptors point; stubbed below

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_whatami(e):
    kind = e.u32(e.r(3) + 0x144)
    e.cl.rec("WhatAmI", (T.s32(kind),))
    e.set_r(3, kind)


STUBS.internal(WHATAMI_CODE, stub_whatami)
STUBS.imp("wcscmp", CL.recorder("wcscmp", "SS", ret="i"))


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def utf16(self, text):
        a = self.alloc(2 * len(text) + 2)
        self.write(a, b"".join(struct.pack(self.fmt[0] + "H", ord(c)) for c in text))
        return a


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def window(self, kind, text):
        tv = self.alloc(8)
        self.words(tv, [WHATAMI_CODE, 0])
        vt = self.alloc(0x20)
        self.words(vt + 0x14, [tv])
        w = self.alloc(0x150)
        self.words(w + 0x118, [vt])
        self.words(w + 0x138, [self.utf16(text)])
        self.words(w + 0x144, [kind])
        return w

    def call(self, a, b):
        return T.call_ppc(self.e, FN, [a, b])[0] & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_itb_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def window(self, kind, text):
        w = self.lib.tpw_itb_window()
        self.write(w + self.lib.off_text, struct.pack("<Q", self.utf16(text)))
        self.write(w + self.lib.off_kind, struct.pack("<I", kind))
        return w

    def call(self, a, b):
        return self.lib.tpw_itb_minus(a, b) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_itb_alloc.restype, lib.tpw_itb_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_itb_window.restype, lib.tpw_itb_window.argtypes = C.c_uint64, []
    lib.tpw_itb_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_itb_minus.restype, lib.tpw_itb_minus.argtypes = C.c_uint32, [C.c_uint64, C.c_uint64]
    out = (C.c_int64 * 8)()
    lib.tpw_itb_layout(out)
    lib.off_text, lib.off_kind = out[0], out[1]
    lib.bound = True


def run(s, case):
    try:
        a = s.window(case["ka"], case["ta"])
        b = s.window(case["kb"], case["tb"])
        answer = s.call(a, b)
        return {"answer": answer, "log": s.log()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


TEXTS = ["", "a", "abc", "abd", "Hello World", "hello world", "x" * 40]


def make(r, ka, kb, ta, tb):
    return {"ka": ka, "kb": kb, "ta": ta, "tb": tb, "queues": {"wcscmp": [r.choice([-3, -1, 0, 1, 2, 77, -0x7fffffff])]}}


def edges():
    r = random.Random(5712)
    yield make(r, 10, 10, "abc", "abc")
    yield make(r, 10, 10, "abc", "abd")
    yield make(r, 10, 11, "abc", "abc")
    yield make(r, 11, 10, "abc", "abc")
    yield make(r, 0, 0, "", "")
    yield make(r, 0x7fffffff, 0x7fffffff, "a", "b")
    yield make(r, 0x80000000, 0x80000000, "a", "b")
    yield make(r, 0xffffffff, 0, "a", "b")


def rnd(r):
    ka = r.choice([10, 10, 1, 2, 3, 0, 0xffffffff, r.randrange(0, 30)])
    kb = ka if r.random() < 0.6 else r.choice([10, 1, 2, r.randrange(0, 30)])
    return make(r, ka, kb, r.choice(TEXTS), r.choice(TEXTS))


TESTS = [
    FnTest("InterfaceTextBox::operator-", ppc, host, edge=edges, rnd=rnd),
]
