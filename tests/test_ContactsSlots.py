"""The virtual calls the online panels make through the Mac's vtables, against libtpw_unit_ContactsSlots.dylib
(tests/unit_ContactsSlots.cpp), main app ("SimTheme Park"). Each case takes the code from the Mac's own table and runs it, and on
the host calls the program's definition (src/game/GameShimGlue.cpp) on an object with its real vtable:
  contacts_callee::FontCharWidth: slot 0x14 of InterfaceFontWrapper's 0x102a71c0 (code 0x10168e04) and of InterfaceFont's 0x102a71e4
  (code 0x10169264), the call OnlineReceiveString 0x101e7dd8 makes for each character of a chat line.
  contacts_callee::WindowSlot1c: slot 0x1c of InterfaceWindow's 0x102a7d94 (code 0x101dc320) and of InterfaceButton's 0x102af974 (code
  0x101caf48, reaching 0x101dc320 and CalcFrame 0x101cad20), the call ContactListHandler 0x101e7854 makes on window 0xc3129 after a vote.

The world of a case: the object's fields and the call's arguments, built by ONE piece of Python code on both sides. The callees outside
the bodies are fakes that record the same events. What is compared: the answer of a font's slot, the attributes after a window's slot and the event log. No fused operations.
Not tested: the window slot's answer, which the program's definition drops as the Mac's only caller does.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_ContactsSlots"
MASK = 0xFFFFFFFF

VSLOT = dict(font_wrapper=(0x102a71c0, 0x14, 0x10168e04), font_plain=(0x102a71e4, 0x14, 0x10169264),
             window=(0x102a7d94, 0x1c, 0x101dc320), button=(0x102af974, 0x1c, 0x101caf48))
OWN = set(v[2] for v in VSLOT.values()) | {0x101dc320, 0x101cad20}

GLUE, GETMB, UPDATE_POINTER_IN = 0x1023BAA8, 0x1023B9C8, 0x101C8F18
GTEXT, RES_MAC = 0x1037FF18, 0x54C
TV_INDEX, TV_SIZE, TV_CHANGE, TV_CALLBACK = 0x70001000, 0x70002000, 0x70003000, 0x70004000
L_OBJECT, L_INNER, L_CONVERTER, L_TABLE, L_DRAW, L_CLIENT = 20, 21, 22, 23, 24, 25


def stub_getmb(e):
    L = e.cl
    L.rec("GetMBValue", (L.lab(e.r(3)), e.r(4) & 0xFFFF))
    e.set_r(3, L.ret("GetMBValue") & 0xFFFF)


def stub_glue(e):
    L = e.cl
    tv = e.r(12)
    if tv == TV_INDEX:
        L.rec("GetCharIndex", (L.lab(e.r(3)), struct.unpack(">H", e.read(e.u32(e.r(4)), 2))[0]))
        e.set_r(3, L.ret("GetCharIndex") & MASK)
    elif tv == TV_SIZE:
        L.rec("GetCharSize", (L.lab(e.r(4)), e.r(5) & 0xFFFF))
        e.write(e.r(3), struct.pack(">II", L.ret("width") & MASK, 0))
    elif tv == TV_CHANGE:
        L.rec("Change", (L.lab(e.r(3)), e.r(4) & MASK, struct.unpack(">h", e.read(e.r(5) + 4, 2))[0]))
    elif tv == TV_CALLBACK:
        L.rec("Callback", (L.lab(e.r(3)), T.s32(e.r(4)), T.s32(e.r(5)), T.s32(e.r(6))))
        e.set_r(3, 0)
    else:
        raise AssertionError("unknown TVector %#x" % tv)


STUBS = CL.Stubs()
STUBS.real.update(OWN)
STUBS.internal(GETMB, stub_getmb)
STUBS.internal(GLUE, stub_glue)
STUBS.internal(UPDATE_POINTER_IN, CL.recorder("UpdatePointerIn", "", ret=lambda e, L: 1))


def glyphs(case):
    return b"".join(struct.pack(">b7x", w) for w in case["glyphs"]) or b"\0"


def code_of(e, op):
    table, slot, expected = VSLOT[op]
    code = e.u32(e.u32(table + slot))
    assert code == expected, hex(code)
    return code


def ppc_window(e, case):
    this, draw, client = e.alloc(0x200), e.alloc(0x40), e.alloc(0x40)
    e.cl = L = CL.PpcLog({this: L_OBJECT, draw: L_DRAW, client: L_CLIENT}, {})
    saved = STUBS.activate(e)
    try:
        vt = e.alloc(0x40)
        e.write(vt + 0x14, struct.pack(">I", TV_CHANGE))
        e.write(draw + 0x38, struct.pack(">I", vt))
        e.write(client + 0x38, struct.pack(">I", vt))
        e.write(this + 0x44, struct.pack(">I", case["attributes"]))
        e.write(this + 0x94, struct.pack(">II", draw if case["draw"] else 0, client if case["client"] else 0))
        e.write(this + 0x110, struct.pack(">I", TV_CALLBACK if case["callback"] else 0))
        e.write(this + 0x138, struct.pack(">bb", case["state"], case["hilight"]))
        T.call_ppc(e, code_of(e, case["op"]), [this, case["value"] & MASK])
        return e.u32(this + 0x44), L.log
    finally:
        CL.Stubs.deactivate(e, saved)


def ppc(e, case):
    op = case["op"]
    if op in ("window", "button"):
        return ppc_window(e, case)
    this, inner, converter, table = e.alloc(0x200), e.alloc(0x40), e.alloc(0x40), e.alloc(0x1000)
    labels = {this: L_OBJECT, inner: L_INNER, converter: L_CONVERTER, table: L_TABLE}
    e.cl = L = CL.PpcLog(labels, case["queues"])
    saved = STUBS.activate(e)
    try:
        if op == "font_plain":
            e.write(table, glyphs(case))
            e.write(this, struct.pack(">HH", case["count"] & 0xFFFF, case["spacing"] & 0xFFFF))
            e.write(this + 8, struct.pack(">I", table if case["has_table"] else 0))
        else:
            d = e.alloc(0x40)
            e.write(inner, struct.pack(">I", d))
            e.write(d + 0x1C, struct.pack(">I", TV_INDEX))
            e.write(d + 0x24, struct.pack(">I", TV_SIZE))
            e.write(this + 0x14, struct.pack(">I", inner if case["has_font"] else 0))
            e.write(GTEXT + RES_MAC + 4, struct.pack(">I", converter))
        r3, _, _ = T.call_ppc(e, code_of(e, op), [this, case["ch"]])
        return T.s32(r3), L.log
    finally:
        CL.Stubs.deactivate(e, saved)


def host_window(lib, case):
    lib.tpw_cs_window.restype = C.c_uint32
    lib.tpw_cs_window.argtypes = [C.c_int32, C.c_uint32] + [C.c_int32] * 6
    CL.host_begin(lib, {}, {})
    attributes = lib.tpw_cs_window(case["op"] == "button", case["attributes"], case["state"], case["hilight"], case["callback"], case["draw"],
                                   case["client"], T.s32(case["value"] & MASK))
    return attributes, CL.host_end(lib)


def host(lib, case):
    op = case["op"]
    if op in ("window", "button"):
        return host_window(lib, case)
    lib.tpw_cs_plain_font.argtypes = [C.c_int32, C.c_int32, C.c_void_p, C.c_int32]
    lib.tpw_cs_wrapper_font.argtypes = [C.c_void_p, C.c_int32]
    lib.tpw_cs_gtext.restype = C.c_void_p
    lib.tpw_cs_offsets.argtypes = [C.c_void_p]
    res = C.c_int64()
    lib.tpw_cs_offsets(C.byref(res))
    table = C.create_string_buffer(0x1000)
    inner, converter = C.create_string_buffer(0x40), C.create_string_buffer(0x40)
    C.memmove(lib.tpw_cs_gtext() + res.value + 8, struct.pack("<Q", C.addressof(converter)), 8)
    labels = {L_INNER: C.addressof(inner), L_CONVERTER: C.addressof(converter), L_TABLE: C.addressof(table)}
    CL.host_begin(lib, labels, case["queues"])
    if op == "font_plain":
        C.memmove(table, glyphs(case), len(glyphs(case)))
        answer = lib.tpw_cs_plain_font(case["count"], case["spacing"], C.addressof(table) if case["has_table"] else None, case["ch"])
    else:
        answer = lib.tpw_cs_wrapper_font(C.addressof(inner) if case["has_font"] else None, case["ch"])
    return answer, CL.host_end(lib)


# ---------------------------------------------------------------------------------------------------------------- case generators

CHARS = [0, 1, 0x20, 0x41, 0x7F, 0x80, 0xFF, 0x100, 0x7FFF, 0x8000, 0xFFFF, 0x3042]
WIDTHS = [0, 1, 7, 8, 0x7F, 0x7FFF, 0x8000, 0xFFFF, 0x10000, 0x12345, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF]
GLYPHS = [0, 1, 7, 8, 16, 127, -1, -8, -128]


def plain(ch, count, spacing, has_table, values):
    return dict(op="font_plain", ch=ch, count=count, spacing=spacing, has_table=has_table, glyphs=values, queues={})


def wrapper(ch, has_font, mb, index, width):
    return dict(op="font_wrapper", ch=ch, has_font=has_font,
                queues={"GetMBValue": [mb], "GetCharIndex": [index], "width": [width]})


def plain_edges():
    for has_table in (0, 1):
        for spacing in (0, 1, 2, 0x7FFF, 0x8000, 0xFFFF):
            for value in GLYPHS:
                # the character inside, on the last entry, on the limit and past it
                for ch, count in ((0, 1), (3, 4), (4, 4), (5, 4), (0, 0), (0x41, 0x42), (0x42, 0x42), (0xFF, 0x100)):
                    yield plain(ch, count, spacing, has_table, [value] * count)


def wrapper_edges():
    for has_font in (0, 1):
        for ch in CHARS:
            for width in WIDTHS:
                yield wrapper(ch, has_font, ch & 0xFF, 5, width)
    for mb in (0, 1, 0xFF, 0x100, 0x8140, 0xFFFF):
        for index in (0, 1, 0xFFFF, 0x10000, 0x12345, 0xFFFFFFFF):
            yield wrapper(0x41, 1, mb, index, 9)


def plain_rnd(r):
    count = r.choice([0, 1, 2, 5, 0x41, 0x7F, 0x80])
    return plain(r.choice(CHARS + [r.randrange(0x90)]), count, r.randrange(0x10000), r.choice([0, 1, 1, 1]),
                 [r.choice(GLYPHS) for _ in range(count)])


def wrapper_rnd(r):
    return wrapper(r.randrange(0x10000), r.choice([0, 1, 1, 1]), r.randrange(0x10000), r.randrange(1 << 32), r.randrange(1 << 32))


def window(op, attributes, value, state=0, hilight=0, callback=1, draw=1, client=1):
    return dict(op=op, attributes=attributes, value=value, state=state, hilight=hilight, callback=callback, draw=draw, client=client)


ATTRIBUTES = [0, 1, 2, 3, 0x100, 0x102, 0xFFFFFFFF, 0xFFFFFFFD]
VALUES = [0, 1, 2, -1, 0x7FFF, 0x8000, 0xFFFF, 0x10000, 0x10001, -0x10000]


def window_edges(op):
    def edges():
        for attributes in ATTRIBUTES:
            for value in VALUES:
                for callback in (0, 1):
                    yield window(op, attributes, value, callback=callback)
                    if op == "button":
                        for state in (0, 1, 2, 3, -1):
                            for hilight in (0, 1):
                                for draw, client in ((0, 0), (1, 0), (0, 1), (1, 1)):
                                    yield window(op, attributes, value, state, hilight, callback, draw, client)
    return edges


def window_rnd(op):
    def rnd(r):
        return window(op, r.choice(ATTRIBUTES + [r.randrange(1 << 32)]), r.choice(VALUES + [r.randrange(-0x20000, 0x20000)]),
                      r.randrange(-128, 128), r.choice([0, 0, 1, -1, 2]), r.randrange(2), r.randrange(2), r.randrange(2))
    return rnd


TESTS = [
    FnTest("contacts_callee::FontCharWidth (InterfaceFontWrapper)", ppc, host, edge=wrapper_edges, rnd=wrapper_rnd),
    FnTest("contacts_callee::FontCharWidth (InterfaceFont)", ppc, host, edge=plain_edges, rnd=plain_rnd),
    FnTest("contacts_callee::WindowSlot1c (InterfaceWindow)", ppc, host, edge=window_edges("window"), rnd=window_rnd("window")),
    FnTest("contacts_callee::WindowSlot1c (InterfaceButton)", ppc, host, edge=window_edges("button"), rnd=window_rnd("button")),
]
