"""LbDraw_GetClipRect (bullfrog 0x1002b554, src/bullfrog/LbDraw.cpp, src/bullfrog/LbDrawText.cpp, src/bullfrog/LbRect.cpp; unit w45b), against libtpw_unit_LbDrawGetClipRect.dylib
(tests/unit_LbDrawGetClipRect.cpp). The function (8 bytes) answers the address of the clip rectangle, the library's word at
0x10074860 (_lbCurrentClipRect). The ltms font render method reads the clip through it (CalculateSourceRect and DrawChar).

Each case writes the four words of the clip rectangle, calls the function, reads the rectangle through the returned pointer, checks
that the pointer is the rectangle itself (the PowerPC side compares it with 0x10074860) and writes a word through it and looks at
the rectangle again, so a copy of the rectangle (or a pointer into somewhere else) would show.
"""
import ctypes as C
import struct

import tpwtest as T
from tpwtest import FnTest

PROGRAM = "bullfrog symbols shared"
SHIM_LIBRARY = "unit_LbDrawGetClipRect"
GET_CLIP_RECT = 0x1002B554
CLIP = 0x10074860
INT_MIN, INT_MAX = -0x80000000, 0x7FFFFFFF


def ppc(e, case):
    rect, poke = case
    e.write(CLIP, struct.pack(">4i", *rect))
    pointer, _, _ = T.call_ppc(e, GET_CLIP_RECT, [])
    seen = struct.unpack(">4i", e.read(pointer, 16)) if pointer == CLIP else (0, 0, 0, 0)
    if pointer == CLIP:
        e.write(pointer, struct.pack(">i", poke))
    return {"rect": list(seen), "is clip": int(pointer == CLIP), "word after": struct.unpack(">i", e.read(CLIP, 4))[0]}


def host(lib, case):
    rect, poke = case
    out = (C.c_int32 * 6)()
    lib.tpw_gc_get((C.c_int32 * 4)(*rect), poke, out)
    return {"rect": list(out[:4]), "is clip": out[4], "word after": out[5]}


def edges():
    for rect in ([0, 0, 0, 0], [0, 0, 640, 480], [-5, -7, 3, 9], [INT_MIN, INT_MIN, INT_MAX, INT_MAX], [1, 2, 3, 4], [4, 3, 2, 1],
                 [-1, -1, -1, -1]):
        for poke in (0, -1, INT_MAX, INT_MIN, 77):
            yield (rect, poke)


def rnd(r):
    return ([r.choice([r.randint(-1000, 1000), INT_MIN, INT_MAX, 0]) for _ in range(4)], r.choice([r.randint(-50, 50), INT_MIN, INT_MAX]))


TESTS = [FnTest("LbDraw_GetClipRect", ppc, host, edge=lambda: list(edges()), rnd=rnd)]
