"""Unit v5L06, the path tool's pointer and selection accessors, against libtpw_unit_PathInput.dylib (tests/unit_PathInput.cpp), main app:
  UI_GetMouseXY 0x101ca900 (src/UI_Base_mouse.cpp), SetPotentiallyHighlightedRide 0x1006cfa4 (src/Debug_pointer.cpp),
  GetPointedAtRide 0x10067254 and LockPointedAtRide 0x10067288 (src/TPRide_pointed.cpp), CIntrface::SetCurrentThing 0x1007dde8
  (src/intrface_current.cpp), UI_GetSelectedPeep 0x101946f8, UI_GetSelectedObject 0x10194730, UI_SetSelectedObject 0x10194768 and
  UI_CheckSelected 0x101947e4 (src/ui_allKidsPanel_select.cpp); unit v5L16 adds UI_SetSelectedPeep 0x101947a8 (the same file).

A case is the eleven global words the functions touch (the pointer's x and y, the highlighted and potential rides, the keep word, the
interface's three pointer words, the selection's thing, peep and object), one argument and the function. Both
sides load the words, call, and give back the answer, every word and (UI_GetMouseXY) the two halfwords written. Pointers are 32-bit
labels on both sides and never dereferenced. CIntrface::SetCurrentThing runs on the interface object at 0x102dc1ac (gIntrface).
"""
import struct

import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_PathInput"
MASK = 0xFFFFFFFF

G_X, G_Y, G_HIGHLIGHT, G_POTENTIAL, G_KEEP = 0x10395BC2, 0x10395BC0, 0x102C6CE8, 0x102D1F08, 0x102C6CC8
G_INTRFACE, G_SEL_THING, G_SEL_PEEP, G_SEL_OBJECT = 0x102DC1AC, 0x103947B8, 0x103947B4, 0x103947B0
# (address, halfword) of each of the eleven words.
WORDS = [(G_X, True), (G_Y, True), (G_HIGHLIGHT, False), (G_POTENTIAL, False), (G_KEEP, False), (G_INTRFACE, False),
         (G_INTRFACE + 4, False), (G_INTRFACE + 8, False), (G_SEL_THING, False), (G_SEL_PEEP, False), (G_SEL_OBJECT, False)]

# (name, address, the answer is a word)
FUNCS = [("UI_GetMouseXY", 0x101CA900, False), ("SetPotentiallyHighlightedRide", 0x1006CFA4, False),
         ("GetPointedAtRide", 0x10067254, True), ("LockPointedAtRide", 0x10067288, False),
         ("CIntrface::SetCurrentThing", 0x1007DDE8, False), ("UI_GetSelectedPeep", 0x101946F8, True),
         ("UI_GetSelectedObject", 0x10194730, True), ("UI_SetSelectedObject", 0x10194768, False),
         ("UI_CheckSelected", 0x101947E4, False), ("UI_SetSelectedPeep", 0x101947A8, False)]


def ppc(e, case):
    fn = case["fn"]
    _, entry, answers = FUNCS[fn]
    for (address, half), w in zip(WORDS, case["words"]):
        e.write(address, struct.pack(">H", w & 0xFFFF) if half else struct.pack(">I", w & MASK))
    xy = e.alloc(8, b"\x5a" * 8)
    a = case["a"] & MASK
    if fn == 0:
        args = [T.I(xy), T.I(xy + 4)]
    elif fn == 4:
        args = [T.I(G_INTRFACE), T.I(a)]
    else:
        args = [T.I(a)]
    r3, _, _ = T.call_ppc(e, entry, args)
    words = []
    for address, half in WORDS:
        words.append(struct.unpack(">H", e.read(address, 2))[0] if half else e.u32(address))
    raw = e.read(xy, 8)
    return {"ret": (r3 & MASK) if answers else 0, "words": words,
            "xy": list(struct.unpack(">hh", raw[0:2] + raw[4:6])), "around": raw[2:4] + raw[6:8]}


def host(lib, case):
    import ctypes as C
    load, store, call = lib.tpw_pi_load, lib.tpw_pi_store, lib.tpw_pi_call
    load.argtypes, load.restype = [C.POINTER(C.c_uint32)], None
    store.argtypes, store.restype = [C.POINTER(C.c_uint32)], None
    call.argtypes, call.restype = [C.c_int32, C.c_uint32, C.c_uint32, C.POINTER(C.c_int16)], C.c_uint32
    words = (C.c_uint32 * 11)(*[w & MASK for w in case["words"]])
    load(words)
    xy = (C.c_int16 * 4)(0x5a5a, 0x5a5a, 0x5a5a, 0x5a5a)
    ret = call(case["fn"], case["a"] & MASK, case["b"] & MASK, C.cast(xy, C.POINTER(C.c_int16)))
    out = (C.c_uint32 * 11)()
    store(out)
    xs = list(xy)
    return {"ret": ret if FUNCS[case["fn"]][2] else 0, "words": [v & 0xFFFF if i < 2 else v for i, v in enumerate(out)],
            "xy": xs[0:2] if case["fn"] == 0 else [0x5a5a, 0x5a5a], "around": b"\x5a" * 4}


def case_of(fn, words, a=0, b=0):
    return {"fn": fn, "words": list(words), "a": a, "b": b}


def base_words():
    return [0x0010, 0x0020, 0x11111110, 0x22222220, 7, 0x33333330, 0x44444440, 0x55555550, 0x66666660, 0x77777770, 0x88888880]


def edges_for(fn):
    def edges():
        out = []
        w = base_words()
        out.append(case_of(fn, w, 0x12345678, 3))
        out.append(case_of(fn, [0] * 11, 0, 0))
        out.append(case_of(fn, [MASK] * 11, MASK, MASK))
        if fn == 0:
            for x, y in [(0x8000, 0x7FFF), (0xFFFF, 0), (0, 0xFFFF), (0x7FFF, 0x8000), (640, 480)]:
                out.append(case_of(fn, [x, y] + w[2:]))
        if fn == 3:
            for flag in (0, 1, MASK, 0x80000000, 2):
                for ride in (0, 0x10000000, 1):
                    for keep in (0, 1, 5):
                        ww = list(w)
                        ww[2], ww[4] = ride, keep
                        out.append(case_of(fn, ww, flag))
        if fn in (2, 5, 6, 8):
            for i in (2, 5, 6, 7, 9, 10):
                ww = list(w)
                ww[i] = 0
                out.append(case_of(fn, ww))
        if fn in (1, 4, 7, 9):
            for a in (0, 1, 0x80000000, 0x10395BC0):
                out.append(case_of(fn, w, a))
        return out
    return edges


def random_for(fn):
    def rnd(r):
        words = [r.choice([0, r.getrandbits(32), r.getrandbits(16)]) for _ in range(11)]
        words[0] &= 0xFFFF
        words[1] &= 0xFFFF
        a = r.choice([0, 1, r.getrandbits(32), r.getrandbits(32)])
        return case_of(fn, words, a, r.getrandbits(32))
    return rnd


TESTS = [FnTest(name, ppc, host, edge=edges_for(i), rnd=random_for(i)) for i, (name, _, _) in enumerate(FUNCS)]
