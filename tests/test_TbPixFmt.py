"""Differential tests for bullfrog TbPixFmt.cpp. See difftest.py and tpwtest.py.

TbMaskInfo and TbPixelCaps hold no pointers, so both sides use the same field values.
The static initialiser runs on the PowerPC over random garbage in the four TbPixFormat
globals; the host's globals hold what its (constant) initialisation produced.
"""
import ctypes
import struct

import tpwtest as T
from tpwtest import FnTest

PROGRAM = "bullfrog symbols shared"

FORMATS = [("TbPixFormat8", 0x10073ed0), ("TbPixFormat16", 0x10073eb8), ("TbPixFormat24", 0x10073ea0),
           ("TbPixFormat32", 0x10073e88)]
VP = ctypes.c_void_p
MASKS = [0, 1, 0x80000000, 0xFFFFFFFF, 0x7C00, 0x3E0, 0x1F, 0xF800, 0x7E0, 0xFF0000, 0xFF00, 0xFF, 0xFF000000,
         0x8000, 0xF000, 0xF00, 0xF0, 0xF, 0x40000000, 0x00800000, 0x00400000]


class MaskInfo(ctypes.Structure):
    _fields_ = [("mask", ctypes.c_uint32), ("right", ctypes.c_int16), ("left", ctypes.c_int16)]


class PixFormat(ctypes.Structure):
    _fields_ = [("red", ctypes.c_uint32), ("green", ctypes.c_uint32), ("blue", ctypes.c_uint32),
                ("alpha", ctypes.c_uint32), ("bits", ctypes.c_int32), ("pal", ctypes.c_int32)]


def F(lib, name, res, *args):
    f = lib[name]
    f.restype = res
    f.argtypes = list(args)
    return f


def rmask(r):
    k = r.random()
    if k < 0.3:
        return r.choice(MASKS)
    if k < 0.5:
        return 1 << r.randrange(32)
    if k < 0.7:
        lo = r.randrange(32)
        return ((1 << r.randint(1, 32 - lo)) - 1) << lo
    return r.getrandbits(32)


def ppc_mask(raw):
    m, right, left = struct.unpack(">Ihh", raw)
    return [m, right, left]


def maskset_ppc(e, c):
    obj = e.alloc(8, c["garbage"])
    T.call_ppc(e, e.func("Set__10TbMaskInfoFUlUi"), [T.I(obj), T.I(c["mask"]), T.I(c["top"])])
    return dict(info=ppc_mask(e.read(obj, 8)))


def maskset_host(lib, c):
    m = MaskInfo.from_buffer_copy(c["garbage"])
    F(lib, "tpw_TbMaskInfo_Set", None, VP, ctypes.c_uint32, ctypes.c_uint32)(ctypes.byref(m), c["mask"], c["top"])
    return dict(info=[m.mask, m.right, m.left])


def caps_ppc(e, c):
    fmt = e.alloc(0x18, struct.pack(">4I2i", *c["format"]))
    obj = e.alloc(0x20, c["garbage"])
    T.call_ppc(e, e.func("Set__11TbPixelCapsFRC11TbPixFormat"), [T.I(obj), T.I(fmt)])
    raw = e.read(obj, 0x20)
    return dict(caps=[ppc_mask(raw[i:i + 8]) for i in range(0, 0x20, 8)])


def caps_host(lib, c):
    fmt = PixFormat(*c["format"])
    caps = (MaskInfo * 4).from_buffer_copy(c["garbage"])
    F(lib, "tpw_TbPixelCaps_Set", None, VP, VP)(caps, ctypes.byref(fmt))
    return dict(caps=[[m.mask, m.right, m.left] for m in caps])


def sinit_ppc(e, c):
    for (_, a), words in zip(FORMATS, c["garbage"]):
        e.write(a, struct.pack(">6I", *words))
    T.call_ppc(e, e.func("__sinit_TbPixFmt_cpp"), [])
    return dict(formats=[list(struct.unpack(">4I2i", e.read(a, 0x18))) for _, a in FORMATS])


def sinit_host(lib, c):
    out = []
    for name, _ in FORMATS:
        f = PixFormat.in_dll(lib, name)
        out.append([f.red, f.green, f.blue, f.alpha, f.bits, f.pal])
    return dict(formats=out)


TOPS = [0, 1, 7, 8, 15, 16, 23, 24, 30, 31, 32, 33, 63, 0x7FFF, 0x8000, 0x10007, 0xFFFFFFFF, 0x80000000]


def rtop(r):
    return r.choice(TOPS) if r.random() < 0.4 else r.randint(0, 40) if r.random() < 0.7 else r.getrandbits(32)


def rformat(r):
    return [rmask(r) for _ in range(4)] + [T.s32(r.getrandbits(32)), T.s32(r.getrandbits(32))]


TESTS = [
    FnTest("TbMaskInfo::Set", maskset_ppc, maskset_host,
           edge=lambda: [dict(mask=m, top=t, garbage=bytes(8)) for m in MASKS for t in (7, 15, 23, 31, 0, 32, 0xFFFFFFFF)],
           rnd=lambda r: dict(mask=rmask(r), top=rtop(r), garbage=r.randbytes(8))),
    FnTest("TbPixelCaps::Set", caps_ppc, caps_host,
           edge=lambda: [dict(format=f, garbage=bytes([g]) * 0x20) for g in (0, 0xFF) for f in (
               [0, 0, 0, 0, 8, 1], [0x7C00, 0x3E0, 0x1F, 0, 16, 0], [0xFF0000, 0xFF00, 0xFF, 0, 24, 0],
               [0xFF0000, 0xFF00, 0xFF, 0xFF000000, 32, 0], [0xF800, 0x7E0, 0x1F, 0, 16, 0],
               [0xFFFFFFFF] * 4 + [-1, -1], [0x80000000, 1, 0x40000000, 2, 0, 0])],
           rnd=lambda r: dict(format=rformat(r), garbage=r.randbytes(0x20))),
    FnTest("__sinit_TbPixFmt_cpp", sinit_ppc, sinit_host,
           edge=lambda: [dict(garbage=[[w] * 6 for _ in FORMATS]) for w in (0, 0xFFFFFFFF)],
           rnd=lambda r: dict(garbage=[T.rwords(r, 6) for _ in FORMATS])),
]


def setup(runner):
    T.install_std_stubs(runner.emu)
