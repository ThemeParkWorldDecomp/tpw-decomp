"""Unit v5L16, two leaves of the main app, against libtpw_unit_TailSeven.dylib (tests/unit_TailSeven.cpp), src/game/ui/InterfaceDrawEngine.cpp, src/game/ui/ui_main.cpp:
  UIMain_GetObjectName 0x101810fc, CalcZTrans 0x101879d0.

UIMain_GetObjectName: CObject::GetUIName (0x10110690) is a fake on both sides that records the object (a 32-bit label, never
dereferenced), whether the buffer is the shared conversion buffer (gUIMainConvertBuffer, 0x10380c48) and the size, then writes the
case's four characters at the start of the buffer. Compared: the events, whether the answer is that buffer, and the buffer's first
five characters (the fifth is the case's guard, which must stay).

CalcZTrans: the answer's single-precision bits for z (the 40.0f is the Mac's data word at 0x102a7af8, read from the image). No fused
operations: the int to double conversion is rounded to single by its fsubs, then fdivs and fsubs, which -ffp-contract=off keeps.
"""
import ctypes as C
import struct

import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_TailSeven"
MASK = 0xFFFFFFFF
P = "tpw_t7_"

FN_NAME, FN_ZTRANS = 0x101810FC, 0x101879D0
GET_UI_NAME = 0x10110690
BUFFER = 0x10380C48


def setup(runner):
    T.install_std_stubs(runner.emu)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    g = lambda n: getattr(lib, P + n)
    g("name").restype, g("name").argtypes = C.c_int32, [C.c_uint32, C.POINTER(C.c_uint16), C.POINTER(C.c_int64)]
    g("ztrans").restype, g("ztrans").argtypes = C.c_float, [C.c_int32]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- UIMain_GetObjectName


def name_ppc(e, case):
    events = []

    def get_ui_name(e):
        events.extend([1, e.r(3) & MASK, 1 if e.r(4) == BUFFER else -1, e.r(5) & MASK])
        e.write(e.r(4), struct.pack(">4H", *case["chars"]))

    T.stub_internal(e, GET_UI_NAME, get_ui_name)
    e.write(BUFFER, struct.pack(">5H", 0x5A5A, 0x5A5A, 0x5A5A, 0x5A5A, case["guard"]))
    r3, _, _ = T.call_ppc(e, FN_NAME, [T.I(case["object"])])
    return {"events": events, "is_buffer": (r3 & MASK) == BUFFER, "text": list(struct.unpack(">5H", e.read(BUFFER, 10)))}


def name_host(lib, case):
    bind(lib)
    io = (C.c_uint16 * 5)(*case["chars"], case["guard"])
    out = (C.c_int64 * 16)()
    n = getattr(lib, P + "name")(case["object"] & MASK, io, out)
    ev = list(out[:n])
    return {"events": ev[:-1], "is_buffer": ev[-1] == 1, "text": list(io)}


def name_edges():
    for obj in (0, 1, 0x10203040, 0x80000000, MASK):
        for chars in ([0, 0, 0, 0], [0x48, 0x69, 0, 0x5A], [0xFFFF, 0x8000, 0x7FFF, 1]):
            yield {"object": obj, "chars": chars, "guard": 0x1234}


def name_rnd(r):
    return {"object": r.getrandbits(32), "chars": [r.getrandbits(16) for _ in range(4)], "guard": r.getrandbits(16)}


# ---------------------------------------------------------------------------------------------------------------- CalcZTrans


def bits(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def z_ppc(e, case):
    _, _, f1 = T.call_ppc(e, FN_ZTRANS, [T.I(case["z"] & MASK)])
    return {"answer": bits(f1)}


def z_host(lib, case):
    bind(lib)
    return {"answer": bits(getattr(lib, P + "ztrans")(T.s32(case["z"])))}


def z_edges():
    # 0, the depth itself and its neighbours, both signs, the extremes, and the integers single precision cannot hold (rounding)
    for z in (0, 1, -1, 2, 39, 40, 41, 80, -40, -41, 20, 13, 7, 3, 100, 1000, -1000, 2 ** 24, 2 ** 24 + 1, 2 ** 24 + 3, -(2 ** 24) - 1,
              2 ** 25 + 1, 2 ** 25 + 3, 0x7FFFFFFF, 0x7FFFFFBF, 0x7FFFFFC0, -(2 ** 31), -(2 ** 31) + 1, 123456789, -987654321):
        yield {"z": z & MASK}


def z_rnd(r):
    k = r.random()
    if k < 0.4:
        z = r.randrange(-200, 200)
    elif k < 0.7:
        z = r.randrange(-(2 ** 26), 2 ** 26)
    else:
        z = r.getrandbits(32)
    return {"z": z & MASK}


TESTS = [FnTest("UIMain_GetObjectName", name_ppc, name_host, edge=name_edges, rnd=name_rnd),
         FnTest("CalcZTrans", z_ppc, z_host, edge=z_edges, rnd=z_rnd)]
