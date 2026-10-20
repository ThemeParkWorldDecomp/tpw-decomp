"""Default constructor of TbIRLE4bitFont (ltms 0x100040d8), against libtpw_unit_TbIRLE4bitFont_TbIRLE4bitFont.dylib.

The object is built over a block filled with 0xAA on both sides. TbIFont::TbIFont runs as the original on the PowerPC side. Compared:
the reference count, the three cleared pointers (null or not), the untouched converter slot, and on the PowerPC side the vtable
word (0x10011334, checked against the constant). No fused operations. Not tested: the host vtable address (differs by platform).
"""
import ctypes as C

import tpwtest as T
from tpwtest import FnTest

PROGRAM = "ltms symbols shared"
SHIM_LIBRARY = "unit_TbIRLE4bitFont_TbIRLE4bitFont"
CTOR = 0x100040D8


def ppc(e, case):
    a = e.alloc(0x18, b"\xAA" * 0x18)
    r3, _, _ = T.call_ppc(e, CTOR, [a])
    return {
        "ret": r3 == a,
        "vt": e.u32(a),
        "refs": e.u32(a + 4),
        "data": e.u32(a + 8) != 0,
        "hdr": e.u32(a + 0xC) != 0,
        "offs": e.u32(a + 0x10) != 0,
        "conv": e.u32(a + 0x14) == 0xAAAAAAAA,
    }


def host(lib, case):
    lib.tpw_rle_ctor.argtypes = [C.POINTER(C.c_uint32)]
    out = (C.c_uint32 * 5)()
    lib.tpw_rle_ctor(out)
    return {
        "ret": True,
        "vt": 0x10011334,
        "refs": out[0],
        "data": bool(out[1]),
        "hdr": bool(out[2]),
        "offs": bool(out[3]),
        "conv": bool(out[4]),
    }


def edges():
    yield {}


TESTS = [
    FnTest("TbIRLE4bitFont::TbIRLE4bitFont", ppc, host, edge=edges, rnd=lambda r: {}),
]
