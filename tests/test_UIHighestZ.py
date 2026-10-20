"""Differential tests for highestZ and InterfaceListControl2::GetZOfEntries (unit w50), against libtpw_unit_UIHighestZ.dylib
(tests/unit_UIHighestZ.cpp):

  src/UI_Base_highestz.cpp   highestZ 0x101ca6dc (and its three static words)
  src/ui_controls_small.cpp  InterfaceListControl2::GetZOfEntries 0x101d518c

Real on both sides: those two, the rectangles (CoolRect constructor, destructor, RectIntersects, Adjust) and InterfaceWindow::GetPosition
(0x101db884), GetChild, GetNext and GetAttr (0x101dbac4, 0x101dbb38, 0x101dbb70). On the PowerPC side the windows are built by hand in
emulated memory (0x178 bytes of 0xa5, then the fields highestZ reads: the rectangle, the attributes, the links, the depth, the animation
offsets, the vtable pointer, and a list control's item pointer); a plain window's vtable pointer is the window class's (0x102a7d94, whose
type query answers 1), a list control's is the list control's (0x102afbfc, whose type query, the real 0x101d1918, answers 7). On the host
the windows are made with the real constructor (a list control with a fake constructor and a WhatAmI that answers 7, in the unit file) and
then given the same fields.

A case is a forest (windows in order, each with its parent, kind, attributes, depth, rectangle, animation offsets and, for a list control,
its first item: none, a NULL pointer, or another window of the forest), the window that is the UI's root (or none), the probe rectangles,
and a script of calls: highestZ on a window with a probe rectangle or NULL and the three static words set first, or GetZOfEntries on a
list control. After each call the answer and the three static words are compared, and at the end the heap traffic (none: the CoolRect is
on the stack) and the call log (empty: the unit has no recorded callees).
"""
import ctypes as C
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_UIHighestZ"

HIGHEST_Z, GET_Z_OF_ENTRIES = 0x101CA6DC, 0x101D518C
# what they call that is real too: GetChild, GetNext, GetAttr, GetPosition, the list control's and the window's type queries, and the
# rectangles (constructor, destructor, RectIntersects, Adjust); other modules leave stubs on some of these
REAL = [HIGHEST_Z, GET_Z_OF_ENTRIES, 0x101DBAC4, 0x101DBB38, 0x101DBB70, 0x101DB884, 0x101D1918, 0x101D9EB0, 0x101C7448, 0x101C74E0,
        0x101C7788, 0x101C7A78]
ROOT_WORD = 0x10395C24
WORDS = dict(answer=0x10395B80, attr=0x10395B78, child_z=0x10395B7C)
VTABLE_WINDOW, VTABLE_LIST, VTABLE_RECT = 0x102A7D94, 0x102AFBFC, 0x102AF8FC
SIZE_WINDOW = 0x178
MINUS = 0xFFFFFFFF
SPEC_WORDS = 13


def s16(v):
    return T.s16(v & 0xFFFF)


def s32(v):
    return T.s32(v & MINUS)


def win(parent, attr=1, z=1, rect=(0, 0, 20, 20), anim=(0, 0, 0, 0), kind=0, item=-2):
    """One window of a forest: parent (-1 none), kind (0 window, 1 list control), attributes, depth, rectangle, animation offsets, and
    for a list control the first item (-2 no array, -1 a NULL first pointer, k the window k)."""
    return (parent, kind, attr, z) + tuple(rect) + tuple(anim) + (item,)


def case(windows, calls, root=0, rects=((0, 0, 10, 10),)):
    return dict(windows=windows, calls=calls, root=root, rects=list(rects))


def hz(slot, rect=-1, preset=None):
    """highestZ on a window; rect -1 for NULL, else an index of the case's probe rectangles; preset = (answer, attr, child Z) of the words."""
    return ("hz", slot, rect, preset)


def zoe(slot):
    return ("zoe", slot)


def ppc(e, case):
    e.cl = CL.PpcLog({})
    S = CL.Stubs()
    S.real.update(REAL)
    windows = case["windows"]
    addresses = [e.alloc(SIZE_WINDOW, b"\xa5" * SIZE_WINDOW) for _ in windows]
    arrays = [e.alloc(8, b"\xa5" * 8) for _ in windows]
    probes = [e.alloc(12, struct.pack(">I4h", VTABLE_RECT, *[s16(v) for v in r])) for r in case["rects"]]
    for i, w in enumerate(windows):
        parent, kind, attr, z = w[0], w[1], w[2], w[3]
        at = addresses[i]
        e.write(at + 8, struct.pack(">4h", *[s16(v) for v in w[4:8]]))
        e.write(at + 0x44, struct.pack(">I", attr & MINUS))
        e.write(at + 0x50, struct.pack(">IIII", 0, 0, 0, 0))   # next, previous, child, parent
        e.write(at + 0xDC, struct.pack(">h", s16(z)))
        e.write(at + 0xF4, struct.pack(">4h", *[s16(v) for v in w[8:12]]))
        e.write(at + 0x118, struct.pack(">I", VTABLE_LIST if kind == 1 else VTABLE_WINDOW))
        if kind == 1 and w[12] != -2:
            first = addresses[w[12]] if w[12] >= 0 else 0
            e.write(arrays[i], struct.pack(">I", first))
            e.write(at + 0x148, struct.pack(">I", arrays[i]))
        elif kind == 1:
            e.write(at + 0x148, struct.pack(">I", 0))
    for i, w in enumerate(windows):
        if 0 <= w[0] < len(windows):
            parent = addresses[w[0]]
            e.write(addresses[i] + 0x5C, struct.pack(">I", parent))
            link = parent + 0x58
            last = 0
            while e.u32(link) != 0:
                last = e.u32(link)
                link = last + 0x50
            e.write(link, struct.pack(">I", addresses[i]))
            e.write(addresses[i] + 0x54, struct.pack(">I", last))
    saved = {a: bytes(e.read(a, 4)) for a in list(WORDS.values()) + [ROOT_WORD]}
    old = S.activate(e)
    out = []
    try:
        e.write(ROOT_WORD, struct.pack(">I", addresses[case["root"]] if case["root"] >= 0 else 0))
        for address in WORDS.values():
            e.write(address, struct.pack(">I", 0))   # each case starts with the three words clear, as the host's do
        for call in case["calls"]:
            if call[0] == "hz":
                _, slot, rect, preset = call
                if preset is not None:
                    for name, v in zip(("answer", "attr", "child_z"), preset):
                        e.write(WORDS[name], struct.pack(">I", v & MINUS))
                r3 = T.call_ppc(e, HIGHEST_Z, [addresses[slot], probes[rect] if rect >= 0 else 0])[0]
                out += [s32(r3)] + [s32(e.u32(WORDS[n])) for n in ("answer", "attr", "child_z")]
            else:
                out.append(s16(T.call_ppc(e, GET_Z_OF_ENTRIES, [addresses[call[1]]])[0]))
        out += [0, 0, 0, 0]
        return out, e.cl.log
    finally:
        CL.Stubs.deactivate(e, old)
        for a, v in saved.items():
            e.write(a, v)


def host(lib, case):
    CL.host_begin(lib, {})
    windows = case["windows"]
    flat = [v if -(1 << 31) <= v < (1 << 31) else v - (1 << 32) for w in windows for v in w]
    lib.tpw_hz_build.argtypes = [C.c_int32, C.POINTER(C.c_int32)]
    lib.tpw_hz_build(len(windows), (C.c_int32 * (len(flat) + 1))(*flat))
    lib.tpw_hz_set_root.argtypes = [C.c_int32]
    lib.tpw_hz_set_root(case["root"])
    lib.tpw_hz_set_words.argtypes = [C.c_int32] * 3
    lib.tpw_hz_get_words.argtypes = [C.POINTER(C.c_int32)]
    lib.tpw_hz_highest.argtypes = [C.c_int32, C.c_int32, C.POINTER(C.c_int32)]
    lib.tpw_hz_highest.restype = C.c_int32
    lib.tpw_hz_zofentries.argtypes = [C.c_int32]
    lib.tpw_hz_zofentries.restype = C.c_int32
    lib.tpw_hz_set_words(0, 0, 0)
    out = []
    words = (C.c_int32 * 3)()
    for call in case["calls"]:
        if call[0] == "hz":
            _, slot, rect, preset = call
            if preset is not None:
                lib.tpw_hz_set_words(*[s32(v) for v in preset])
            r = case["rects"][rect] if rect >= 0 else (0, 0, 0, 0)
            answer = lib.tpw_hz_highest(slot, 1 if rect >= 0 else 0, (C.c_int32 * 4)(*[s16(v) for v in r]))
            lib.tpw_hz_get_words(words)
            out += [answer] + list(words)
        else:
            out.append(lib.tpw_hz_zofentries(call[1]))
    heap = (C.c_int32 * 4).in_dll(lib, "tpw_cl_heap")
    out += list(heap)
    return out, CL.host_end(lib)


# ---- the cases

ATTRS = [0, 1, 2, 3, 0x10, 0x11, 0x13, 0x80000001, 0xFFFFFFFF, 0xFFFFFFFD, 0x1001, 0x100, 0x40000000]
ZS = [1, 3, 5, 0x100, 0, -1, -5, 9, 0x7FFF, -0x8000]
PRESETS = [None, (0, 0, 0), (4, 0, 0), (50, 0, 0), (-1, 5, 5), (0x7FFFFFFF, 0, 0), (-0x80000000, 0, 0), (0x7FFE, 1, 1), (0x100, 9, 9)]
RECTS = [(0, 0, 10, 10), (100, 100, 110, 110), (5, 5, 6, 6), (10, 10, 20, 20), (-20, -20, 0, 0), (-32768, -32768, 32767, 32767), (0, 0, 0, 0)]
WRECTS = [(0, 0, 20, 20), (50, 50, 60, 60), (-10, -10, -5, -5), (8, 8, 9, 9), (10, 0, 20, 10), (0, 10, 10, 20), (-32768, 0, 32767, 100)]


def edges():
    # the root over one child: attributes, depths, no rectangle (the answer is the root's depth, or the child's plus 1)
    for root_z in (1, 3, 0x100, -5, 0):
        for attr in ATTRS:
            for z in ZS:
                yield case([win(-1, 1, root_z), win(0, attr, z)], [hz(0)])
    # the same with a rectangle that the child's overlaps, touches and misses
    for attr in (0, 1, 2, 3, 0x11):
        for z in (1, 3, 5, 0x100):
            for rect in range(len(RECTS)):
                for wrect in WRECTS:
                    yield case([win(-1, 1, 1), win(0, attr, z, wrect)], [hz(0, rect)], rects=RECTS)
    # triage: a child with animation offsets, which GetPosition(rect, 1) takes off again: each offset alone, both signs, all four together
    for anim in ((30, 0, 0, 0), (0, 30, 0, 0), (0, 0, 30, 0), (0, 0, 0, 30), (-30, 0, 0, 0), (0, -30, 0, 0), (0, 0, -30, 0), (0, 0, 0, -30),
                 (30, 30, 30, 30), (-30, -30, -30, -30), (15, -15, 15, -15), (-15, 15, -15, 15), (60, 7, 3, 40)):
        for wrect in ((0, 0, 20, 20), (50, 50, 60, 60)):
            for rect in range(len(RECTS)):
                yield case([win(-1, 1, 1), win(0, 1, 5, wrect, anim)], [hz(0, rect)], rects=RECTS)
    # two children, three levels: the walk finds the highest of them, hidden and invisible windows hide their descendants
    for a1 in (0, 1, 2, 3):
        for a2 in (0, 1, 2, 3):
            for a3 in (0, 1, 2, 3):
                for z1, z2, z3 in ((3, 5, 7), (7, 5, 3), (5, 5, 5), (3, 0x100, 9)):
                    tree = [win(-1, 1, 1), win(0, a1, z1), win(0, a2, z2), win(1, a3, z3), win(2, a3, z3 + 2), win(3, 1, 0x100)]
                    yield case(tree, [hz(0), hz(1), hz(2), hz(3)])
                    yield case(tree, [hz(0, 0), hz(1, 1), hz(2, 2), hz(3, 3)], rects=RECTS)
    # a window that is not the root keeps the answer word as it was (its floor): preset 0, 4, 50 and others
    for preset in PRESETS:
        for rect in (-1, 0, 1):
            tree = [win(-1, 1, 1), win(0, 1, 3, (0, 0, 20, 20)), win(1, 1, 5, (0, 0, 20, 20)), win(1, 1, 9, (100, 100, 120, 120))]
            yield case(tree, [hz(1, rect, preset), hz(2, rect, preset), hz(0, rect, preset), hz(1, rect)], rects=RECTS)
            yield case(tree, [hz(1, rect, preset), hz(0, rect)], root=-1, rects=RECTS)
    # a list control child: its own depth is not used, its first item's is; no array, a NULL first pointer, an item that is the window
    for item in (-2, -1, 2, 3, 4):
        for own_z in (1, 30, 0x100):
            for attr in (0, 1, 3):
                tree = [win(-1, 1, 1), win(0, attr, own_z, kind=1, item=item), win(0, 1, 9), win(0, 1, 4), win(0, 1, -3)]
                yield case(tree, [hz(0), hz(0, 0), zoe(1), hz(1, 0, (0, 0, 0)), zoe(1)], rects=RECTS)
    # a list control with children of its own, the argument of the call, and nested in a window
    tree = [win(-1, 1, 1), win(0, 1, 3, kind=1, item=2), win(1, 1, 20), win(1, 1, 5, kind=1, item=-1), win(3, 1, 50)]
    yield case(tree, [hz(0), hz(1, preset=(0, 0, 0)), hz(3, preset=(0, 0, 0)), zoe(1), zoe(3)])
    # GetZOfEntries alone
    for item in (-2, -1, 0, 1):
        for z in ZS:
            yield case([win(-1, 1, 1, kind=1, item=item), win(0, 1, z)], [zoe(0)])
    # the root is none of the windows (a window that is not the root never resets the answer); a leaf as the argument
    yield case([win(-1, 1, 5), win(0, 1, 7)], [hz(0, preset=(2, 0, 0)), hz(1)], root=-1)
    yield case([win(-1, 1, 5), win(0, 1, 7)], [hz(1, preset=(2, 0, 0)), hz(1)], root=0)
    # the static words after calls with no children keep their old values (attr and child Z)
    yield case([win(-1, 1, 5)], [hz(0, preset=(2, 0x55, 0x66))])
    yield case([win(-1, 1, 5), win(0, 2, 7)], [hz(0, preset=(2, 0x55, 0x66)), hz(0)])


def rnd(r):
    n = r.randint(1, 9)
    windows = []
    for i in range(n):
        kind = 1 if r.random() < 0.3 else 0
        item = r.choice([-2, -1] + list(range(n))) if kind == 1 else -2
        windows.append(win(-1 if i == 0 else r.randrange(i), r.choice(ATTRS + [1, 1, 1, 3]), r.choice(ZS),
                           r.choice(WRECTS), r.choice([(0, 0, 0, 0), (0, 0, 0, 0), (r.randint(-60, 60), r.randint(-60, 60), r.randint(-60, 60),
                                                                                   r.randint(-60, 60))]), kind, item))
    calls = []
    for _ in range(r.randint(1, 4)):
        if r.random() < 0.15:
            lists = [i for i, w in enumerate(windows) if w[1] == 1]
            if lists:
                calls.append(zoe(r.choice(lists)))
                continue
        calls.append(hz(r.choice([0, 0, r.randrange(n)]), r.choice([-1] + list(range(len(RECTS)))), r.choice(PRESETS)))
    return case(windows, calls, r.choice([0, 0, 0, -1, r.randrange(n)]), RECTS)


TESTS = [FnTest("highestZ, InterfaceListControl2::GetZOfEntries", ppc, host, edge=edges, rnd=rnd)]
