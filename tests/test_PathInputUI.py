"""Unit v5L06, the build panel's small UI callees, against libtpw_unit_PathInputUI.dylib (tests/unit_PathInputUI.cpp), main app:
  UIMain_TickRidePreview 0x10181570 and UIMain_NotEnoughDough 0x10182160 (src/game/ui/ui_main.cpp), RemoveRideListPanel 0x101b8c28
  (src/game/ui/ui_ridepanel.cpp), InterfaceWindow::FinishWithClientObjectSurface 0x101dbd1c and InterfaceTimer::SetSlice 0x101d84c8
  (src/game/ui/InterfaceTimer.cpp, src/game/ui/InterfaceWindow.cpp); unit v5L16 adds InterfaceWindow::GetClientObjectSurface 0x101dbc98 (the same file).

A case sets the ride preview (gRidePreview), the ui sound category (gSoundBanks[3]), the ride list panel (gRideListPanel), whether the
window has a client draw object, and SetSlice's argument and the timer's old slice. The callees are fakes on both sides that record a
code and their arguments (pointers as 32-bit labels); the draw object's Refresh is the virtual at +0x20 of the vtable at object +0x38
on the Mac. SetSlice also gives back the timer's slice and its id word (which must stay). GetClientObjectSurface's GetWritableDataArea
is the virtual at +0x24: the fake records whether it got the object and the two size pointers (or NULL), writes the case's sizes
through them and answers the case's pixel word; the answer, both sizes and the events are compared. Unit v5L28: the host answers
the arena address whose low half is the word, so the answer is compared as the word plus where it lies (1 in the arena, 0 for none).
"""
import struct

import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_PathInputUI"
MASK = 0xFFFFFFFF

G_PREVIEW, G_BANKS, G_PANEL = 0x10380C44, 0x102F7C94, 0x10395998
C_TICK, C_SEND_EVENT, C_SEND_MESSAGE = 0x1006C970, 0x100DD9FC, 0x101C9518
FUNCS = [("UIMain_TickRidePreview", 0x10181570), ("UIMain_NotEnoughDough", 0x10182160), ("RemoveRideListPanel", 0x101B8C28),
         ("InterfaceWindow::FinishWithClientObjectSurface", 0x101DBD1C), ("InterfaceTimer::SetSlice", 0x101D84C8),
         ("InterfaceWindow::GetClientObjectSurface", 0x101DBC98)]


def s32(v):
    return T.s32(v & MASK)


def ppc(e, case):
    fn, w = case["fn"], case["w"]
    events = []

    def tick(e):
        events.extend([1, e.r(3) & MASK])

    def send_event(e):
        events.extend([2] + [s32(e.r(i)) for i in range(3, 9)])
        e.set_r(3, 12345)

    def send_message(e):
        events.extend([3, e.r(3) & MASK] + [s32(e.r(i)) for i in range(4, 7)])
        e.set_r(3, 12345)

    T.stub_internal(e, C_TICK, tick)
    T.stub_internal(e, C_SEND_EVENT, send_event)
    T.stub_internal(e, C_SEND_MESSAGE, send_message)
    e.write(G_PREVIEW, struct.pack(">I", w[0] & MASK))
    e.write(G_BANKS, struct.pack(">8i", *([-1] * 3 + [s32(w[1])] + [-1] * 4)))
    e.write(G_PANEL, struct.pack(">I", w[2] & MASK))
    slice_out = None
    if fn == 3:
        def refresh(e):
            events.extend([4, 77])
        tvector = T.fake_function(e, refresh)
        vtable = e.alloc(0x40, struct.pack(">16I", *([0] * 8 + [tvector] + [0] * 7)))
        obj = e.alloc(0x40, b"\0" * 0x38 + struct.pack(">I", vtable) + b"\0" * 4)
        window = e.alloc(0x120, b"\0" * 0x98 + struct.pack(">I", obj if w[3] else 0) + b"\0" * (0x120 - 0x9c))
        T.call_ppc(e, FUNCS[fn][1], [T.I(window)])
    elif fn == 5:
        sizes = e.alloc(8, b"\x5a" * 8)
        wp = 0 if w[1] & 1 else sizes
        hp = 0 if w[1] & 2 else sizes + 4

        def area(e):
            kind = lambda v, want: 0 if v == 0 else (1 if v == want else -1)
            events.extend([7, 1 if e.r(3) == obj else -1, kind(e.r(4), wp), kind(e.r(5), hp)])
            if e.r(4):
                e.write(e.r(4), struct.pack(">I", w[4] & MASK))
            if e.r(5):
                e.write(e.r(5), struct.pack(">I", w[5] & MASK))
            e.set_r(3, w[0] & MASK)
        tvector = T.fake_function(e, area)
        vtable = e.alloc(0x40, struct.pack(">16I", *([0] * 9 + [tvector] + [0] * 6)))
        obj = e.alloc(0x40, b"\0" * 0x38 + struct.pack(">I", vtable) + b"\0" * 4)
        window = e.alloc(0x120, b"\0" * 0x98 + struct.pack(">I", obj if w[3] else 0) + b"\0" * (0x120 - 0x9c))
        r3, _, _ = T.call_ppc(e, FUNCS[fn][1], [T.I(window), T.I(wp), T.I(hp)])
        events.extend([8, r3 & MASK, s32(e.u32(sizes)), s32(e.u32(sizes + 4)), 1 if r3 & MASK else 0])
    elif fn == 4:
        timer = e.alloc(0x20, struct.pack(">4i", 0, 0, s32(w[5]), 0x5A5A) + b"\0" * 0x10)
        T.call_ppc(e, FUNCS[fn][1], [T.I(timer), T.I(w[4] & MASK)])
        slice_out = s32(e.u32(timer + 8))
        events.extend([6, s32(e.u32(timer + 12))])
    else:
        T.call_ppc(e, FUNCS[fn][1], [])
    return {"events": events, "slice": slice_out}


def host(lib, case):
    import ctypes as C
    run = lib.tpw_piu_run
    run.argtypes = [C.c_int32, C.POINTER(C.c_int32), C.POINTER(C.c_int64), C.POINTER(C.c_int32)]
    run.restype = C.c_int32
    w = (C.c_int32 * 6)(*[s32(v) for v in case["w"]])
    out = (C.c_int64 * 64)()
    sl = C.c_int32(0)
    n = run(case["fn"], w, out, C.byref(sl))
    return {"events": list(out[:n]), "slice": sl.value if case["fn"] == 4 else None}


def case_of(fn, w):
    return {"fn": fn, "w": list(w)}


def edges_for(fn):
    def edges():
        base = [0x10203040, 7, 0x20304050, 1, 30, 10]
        out = [case_of(fn, base), case_of(fn, [0] * 6), case_of(fn, [MASK] * 6)]
        if fn == 5:   # with and without the object, each size pointer NULL or not, a NULL and a high pixel word
            for has in (0, 1):
                for nulls in range(4):
                    for pixels in (0, 0x10203040, 0xFFFFFFF0):
                        out.append(case_of(fn, [pixels, nulls, 0, has, 0x80000000, 0x7FFFFFFF]))
        for v in (0, 1, -1 & MASK, 0x80000000, 0x7FFFFFFF):
            ww = list(base)
            ww[fn] = v
            out.append(case_of(fn, ww))
        return out
    return edges


def random_for(fn):
    def rnd(r):
        return case_of(fn, [r.choice([0, r.getrandbits(32)]), r.getrandbits(32), r.choice([0, r.getrandbits(32)]), r.choice([0, 1]),
                            r.getrandbits(32), r.getrandbits(32)])
    return rnd


TESTS = [FnTest(name, ppc, host, edge=edges_for(i), rnd=random_for(i)) for i, (name, _) in enumerate(FUNCS)]
