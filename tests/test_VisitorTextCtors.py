"""The visitor time and visitor number text objects as the game builds and feeds them, against libtpw_unit_VisitorTextCtors.dylib
(tests/unit_VisitorTextCtors.cpp), main app ("SimTheme Park"), src/game/ui/InterfaceTextObjectSprite.cpp, src/game/ui/InterfaceWindow.cpp:
  InterfaceVisitorTimeTextObject::InterfaceVisitorTimeTextObject 0x1018edf4 (VisitorStatusPanel::InitStatusWindow 0x10142238 window 0x372d, and
  column 2 of LCPostFormat 0x10194fa8), InterfaceVisitorNumTextObject::InterfaceVisitorNumTextObject 0x1018eb38 (column 0 of LCPostFormat), then
  InterfaceWindow::SetValue 0x101dfe84, which reaches InterfaceVisitorTimeTextObject::Change 0x1018efcc or InterfaceVisitorNumTextObject::Change
  0x1018ed10 through the object's own vtable (slot 0x14 of the table at +0x38 on the Mac).

A case constructs one object (the host by the glue the panels call, vsp_callee::TimeCtor or kidlist_callee::VisitorNumCtor; the PowerPC by the
constructor with NULL data), puts it in a window as the draw object, the client draw object, both or neither, and calls the real SetValue with
a value: nothing in between is faked, so a wrong class, a vtable that misses, or a Change that reads the word in the wrong byte order all show.
The creators' choice of constructor per window is compared by tests/test_VisitorStatusPanel_InitStatusWindow.py and
tests/test_FillAllRidesListCallback_Upgrades.py.

The fakes on both sides: the draw object sprite's constructor and the formatter's (log who and the data), the base text sprite's Change (logs the
flags, and with flag 0x100 the text's length and hash), TbITable::FormatString (logs the table, the size, the id and the tag; writes "id:value" or
"id"). The PowerPC's swprintf stub mirrors the host's msl and checks that the format in the Mac image is "%d".
Compared: the event log, the style bytes the constructor leaves, SetValue's answer.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_VisitorTextCtors"
MASK = 0xFFFFFFFF

CTOR = {"time": 0x1018EDF4, "num": 0x1018EB38}
WHICH = {"time": 0, "num": 1}
SET_VALUE = 0x101DFE84
SIZES = {0x1018EDF4: 164, 0x1018EB38: 164, 0x1018EFCC: 444, 0x1018ED10: 156, SET_VALUE: 120}
OWN = set(SIZES)

TABLE_PTR = 0x1038047C   # gText +0x564
FIELD_OFFS = [(0xDC, 1), (0xDD, 1), (0xDE, 1), (0xDF, 1), (0xE0, 1), (0xE1, 1), (0xE2, 2), (0xE4, 2), (0x48, 1), (0x49, 1), (0x4A, 1), (0x4B, 1)]
TAG_VTABLE = 0x10261344
CUR = {"obj": 0, "table": 0}

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def units(e, a, limit=0x400):
    out = []
    while len(out) < limit:
        u = struct.unpack(">H", e.read(a + 2 * len(out), 2))[0]
        if u == 0:
            break
        out.append(u)
    return out


def put_units(e, a, us):
    e.write(a, b"".join(struct.pack(">H", u) for u in us) + b"\0\0")


def fnv(us):
    h = 2166136261
    for u in us:
        h = ((h ^ u) * 16777619) & MASK
    return h


def lab(a):
    off = (a & MASK) - CUR["obj"]
    return {0: 1, 0xE8: 2}.get(off, -1)


def toc(e):
    # the glue saves the TOC at 0x14(r1) and the caller reloads it afterwards
    e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2) & MASK))


def stub_sprite_ctor(e):
    e.cl.rec("SpriteCtor", (lab(e.r(3)), e.r(4) & MASK))


def stub_formatter_ctor(e):
    toc(e)
    e.cl.rec("FormatterCtor", (lab(e.r(3)),))


def stub_change(e):
    self_, flags, ch = e.r(3), e.r(4) & MASK, e.r(5)
    length, h = -1, 0
    if flags & 0x100:
        us = units(e, e.u32(e.u32(ch + 0x10)))
        length, h = len(us), fnv(us)
    e.cl.rec("change", (lab(self_), flags, length, h))


def stub_format(e):
    toc(e)
    table, size, id_ = e.r(3), e.r(5) & MASK, e.r(6) & MASK
    tagged = id_ != 0x1C9
    magic = tid = val = neg = isvt = 0
    if tagged:
        tag = e.r(7)
        magic, isvt, tid, val, neg = e.u32(tag), int(e.u32(tag + 4) == e.u32(TAG_VTABLE)), e.u32(tag + 8), e.u32(tag + 12), e.u32(tag + 16)
    e.cl.rec("format", (int(table == CUR["table"]), size, id_, int(tagged), magic, tid, val, neg, isvt))
    text = "%d:%d" % (id_, T.s32(val)) if tagged else "%d" % id_
    put_units(e, e.r(4), [ord(c) for c in text])
    e.set_r(3, 0)


def stub_swprintf(e):
    toc(e)
    buf, size, fmt, v = e.r(3), e.r(4), e.r(5), e.r(6)
    assert units(e, fmt) == [0x25, 0x64], "the visitor number's format is not %d"
    put_units(e, buf, [ord(c) for c in ("%d" % T.s32(v))[:size - 1]])


STUBS.internal(0x1018C978, stub_sprite_ctor)
STUBS.internal(0x10238F08, stub_formatter_ctor)
STUBS.internal(0x1018D798, stub_change)
STUBS.internal(0x10239730, stub_format)
STUBS.internal(0x10239160, stub_swprintf)


def setup(runner):
    T.install_std_stubs(runner.emu)


def slots(obj, place):
    return (obj if place in ("draw", "both") else 0, obj if place in ("client", "both") else 0)


def ppc(e, case):
    e.cl = CL.PpcLog({}, {})
    saved = STUBS.activate(e)
    try:
        table = e.alloc(16)
        CUR["table"] = table
        e.write(TABLE_PTR, struct.pack(">I", table))
        obj = e.alloc(0x400)
        CUR["obj"] = obj
        e.write(obj, b"\xAA" * 0x19C)
        T.call_ppc(e, CTOR[case["kind"]], [obj, 0])
        b = e.read(obj, 0x19C)
        fields = [int.from_bytes(b[o:o + n], "big") for o, n in FIELD_OFFS]
        window = e.alloc(0x200)
        e.write(window, b"\0" * 0x200)
        draw, client = slots(obj, case["place"])
        e.write(window + 0x94, struct.pack(">II", draw, client))
        answer = T.call_ppc(e, SET_VALUE, [window, case["value"] & MASK])[0] & MASK
        return {"fields": fields, "answer": answer, "log": e.cl.log}
    finally:
        CL.Stubs.deactivate(e, saved)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_vtc_alloc.restype, lib.tpw_vtc_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_vtc_settable.argtypes = [C.c_uint64]
    lib.tpw_vtc_make.restype, lib.tpw_vtc_make.argtypes = None, [C.c_int32, C.c_uint64, C.POINTER(C.c_int64)]
    lib.tpw_vtc_setvalue.restype, lib.tpw_vtc_setvalue.argtypes = C.c_int32, [C.c_uint64, C.c_uint64, C.c_int32]
    lib.tpw_vtc_sizes.argtypes = [C.POINTER(C.c_int64)]
    sizes = (C.c_int64 * 3)()
    lib.tpw_vtc_sizes(sizes)
    assert sizes[0] == sizes[2] and sizes[1] == sizes[2] and sizes[0] <= 0x400, list(sizes)
    lib.bound = True


def host(lib, case):
    bind(lib)
    CL.host_begin(lib, {}, {})
    lib.tpw_vtc_settable(lib.tpw_vtc_alloc(16))
    obj = lib.tpw_vtc_alloc(0x400)
    out = (C.c_int64 * 12)()
    lib.tpw_vtc_make(WHICH[case["kind"]], obj, out)
    draw, client = slots(obj, case["place"])
    answer = lib.tpw_vtc_setvalue(draw, client, T.s32(case["value"])) & MASK
    return {"fields": list(out), "answer": answer, "log": CL.host_end(lib)}


VALUES = [0, 1, 2, 5, 0x17, 0x18, 0x19, 0x1A, 47, 0x30, 0x31, 72, 96, 1000, 0x100, 0x10000, 0x1000000, 0x12345678, 0x7FFFFFFF,
          -1, -2, -0x18, -0x19, -0x100, -0x12345678, -0x80000000]
PLACES = ["client", "draw", "both", "none"]


def tests_for(kind, name):
    def edges():
        for place in PLACES:
            for value in VALUES:
                yield {"kind": kind, "place": place, "value": value}

    def rnd(r):
        value = r.choice([r.randrange(-100, 200), r.randrange(-0x10000, 0x10000), T.s32(r.getrandbits(32))])
        return {"kind": kind, "place": r.choice(["client", "client", "client", "draw", "both"]), "value": value}

    return FnTest(name, ppc, host, edge=edges, rnd=rnd)


TESTS = [
    tests_for("time", "visitor time text object: glue, class, SetValue, Change"),
    tests_for("num", "visitor number text object: glue, class, SetValue, Change"),
]
