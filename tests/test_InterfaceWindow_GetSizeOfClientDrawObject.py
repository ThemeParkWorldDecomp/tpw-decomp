"""Unit v5119, six InterfaceWindow leaves, against libtpw_unit_InterfaceWindow_GetSizeOfClientDrawObject.dylib
(tests/unit_InterfaceWindow_GetSizeOfClientDrawObject.cpp), main app, src/InterfaceWindow_draw_InterfaceWindow_GetSizeOfClientDrawObject.cpp:
  GetSizeOfClientDrawObject 0x101dbba8, AdjustPosition 0x101db0e8, UnCaptureKb 0x101dcdb0, stopAnim 0x101dd1e8,
  setAnimTimer 0x101dd5e0, SetWindowDrawText 0x101de1bc

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's offsets (big-endian,
4-byte pointers) for the original, and in the host library's memory (host offsets from the library's layout export, 8-byte pointers) for the
translation. The callees outside the unit are fakes on both sides that record their arguments: CoolRect's constructor, destructor and
Adjust (which adds), InterfaceWindow::SetPosition, InterfaceTimer::SetSlice, UI_AddTimer (answers from the case's queue), UI_PostMessage,
UI_UpdatePointerIn, and the draw object's Change reached through __ptr_glue (the Mac's table at +0x38, slot +0x14).

The world of a case: the window's rectangle, animation offsets and state, timers, the keyboard window global, the client draw object's
rectangle, the draw object and the text. What is compared: the answer, the log, the window's fields, the global and the out rectangle.

No fused operations. Not tested: an animation timer index outside 0 to 3 (no bounds check in the original, it reads other fields).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_InterfaceWindow_GetSizeOfClientDrawObject"
MASK = 0xFFFFFFFF

FN = dict(size=0x101dbba8, adjust=0x101db0e8, uncap=0x101dcdb0, stop=0x101dd1e8, timer=0x101dd5e0, text=0x101de1bc)
SIZES = {0x101dbba8: 164, 0x101db0e8: 140, 0x101dcdb0: 104, 0x101dd1e8: 236, 0x101dd5e0: 160, 0x101de1bc: 80}
OWN = set(SIZES)

CT_RECT, CT_DTOR, CT_ADJUST = 0x101c7494, 0x101c74e0, 0x101c7a78
CT_SETSLICE, CT_ADDTIMER, CT_POST, CT_UPDATE, CT_SETPOS, CT_GLUE = 0x101d84c8, 0x101d878c, 0x101c9400, 0x101c8f18, 0x101dae98, 0x1023baa8
ADDR = dict(kb=0x10395c10)
VEC = 0xABCD0000      # the Mac's function slot the glue is called through

MAC = dict(rect=8, drawobj=0x94, cdo=0x98, anim=0xe4, frame=0xe8, offs=0xf4, animating=0x135, timers=0x120, winsize=0x140,
           cr_left=4, crsize=0xc, do_rect2=0x20, dosize=0x60, ptr=4)


def s16(v):
    return T.s16(v)


def s32(v):
    return T.s32(v)


def rs(e, a):
    return struct.unpack(">h", e.read(a, 2))[0]


def ws(e, a, v):
    e.write(a, struct.pack(">H", v & 0xFFFF))


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_ctor(e):
    this = e.r(3)
    v = [s16(e.r(4 + i)) for i in range(4)]
    e.cl.rec("CoolRect", tuple(v))
    for i in range(4):
        ws(e, this + 4 + 2 * i, v[i])


def stub_dtor(e):
    this = e.r(3)
    e.cl.rec("~CoolRect", tuple(rs(e, this + 4 + 2 * i) for i in range(4)))


def stub_adjust(e):
    this = e.r(3)
    cur = [rs(e, this + 4 + 2 * i) for i in range(4)]
    a = [s16(e.r(4 + i)) for i in range(4)]
    e.cl.rec("Adjust", tuple(cur + a))
    for i in range(4):
        ws(e, this + 4 + 2 * i, cur[i] + a[i])


def stub_setpos(e):
    L = e.cl
    r = e.r(4)
    L.rec("SetPosition", (L.lab(e.r(3)),) + tuple(rs(e, r + 4 + 2 * i) for i in range(4)))
    e.set_r(3, 1)


def stub_setslice(e):
    e.cl.rec("SetSlice", (s32(e.r(3)), s32(e.r(4))))


def stub_addtimer(e):
    L = e.cl
    L.rec("AddTimer", (L.lab(e.r(3)), s32(e.r(4)), s32(e.r(5))))
    e.set_r(3, L.ret("AddTimer") & MASK)


def stub_post(e):
    L = e.cl
    L.rec("PostMessage", (L.lab(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6))))
    e.set_r(3, 0)


def stub_update(e):
    e.cl.rec("UpdatePointerIn", ())
    e.set_r(3, 0)


def stub_glue(e):
    L = e.cl
    style = e.u32(e.r(5) + 0x10)
    text = e.u32(style) if style else 0
    chars = [struct.unpack(">H", e.read(text + 2 * i, 2))[0] for i in range(4)] if text else [0, 0, 0, 0]
    L.rec("Change", (L.lab(e.r(3)), s32(e.r(4)), 1 if e.r(12) == VEC else 0, 1 if style else 0, 1 if text else 0) + tuple(chars))


STUBS.internal(CT_RECT, stub_ctor)
STUBS.internal(CT_DTOR, stub_dtor)
STUBS.internal(CT_ADJUST, stub_adjust)
STUBS.internal(CT_SETPOS, stub_setpos)
STUBS.internal(CT_SETSLICE, stub_setslice)
STUBS.internal(CT_ADDTIMER, stub_addtimer)
STUBS.internal(CT_POST, stub_post)
STUBS.internal(CT_UPDATE, stub_update)
STUBS.internal(CT_GLUE, stub_glue)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws_):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws_))

    def shorts(self, a, vals):
        self.write(a, struct.pack(self.fmt[0] + "%dH" % len(vals), *[v & 0xFFFF for v in vals]))

    def rd_shorts(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dH" % n, self.read(a, 2 * n)))

    def ptr(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + ("I" if self.o["ptr"] == 4 else "Q"), v))

    def rd_ptr(self, a):
        return struct.unpack(self.fmt[0] + ("I" if self.o["ptr"] == 4 else "Q"), self.read(a, self.o["ptr"]))[0]


class Ppc(Side):
    fmt = ">I"
    o = MAC

    def __init__(self, e, case):
        self.e = e
        self.win = e.alloc(0x140)
        self.cdo = e.alloc(0x60)
        self.dobj = e.alloc(0x60)
        self.out = e.alloc(0x10)
        self.text = e.alloc(16)
        tab = e.alloc(0x20)
        e.write(self.dobj + 0x38, struct.pack(">I", tab))
        e.write(tab + 0x14, struct.pack(">I", VEC))
        self.L = e.cl = CL.PpcLog({self.win: 1, self.cdo: 2, self.dobj: 3}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, op, case):
        e, w = self.e, self.win
        if op == "size":
            T.call_ppc(e, FN["size"], [w, self.out])
            return None
        if op == "adjust":
            T.call_ppc(e, FN["adjust"], [w] + [v & MASK for v in case["args"]])
            return None
        if op == "uncap":
            return T.call_ppc(e, FN["uncap"], [w, case["args"][0] & MASK])[0] & 0xFF
        if op == "stop":
            return T.call_ppc(e, FN["stop"], [w])[0] & 0xFF
        if op == "timer":
            return T.call_ppc(e, FN["timer"], [w] + [v & MASK for v in case["args"]])[0] & 0xFFFF
        T.call_ppc(e, FN["text"], [w, self.text if case["text"] is not None else 0])
        return None


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        lay = lib.lay
        self.o = dict(rect=lay[0], drawobj=lay[1], cdo=lay[2], anim=lay[3], frame=lay[4], offs=lay[5], animating=lay[6], timers=lay[7],
                      winsize=lay[8], cr_left=lay[9], crsize=lay[10], do_rect2=lay[11], dosize=lay[12], ptr=8)
        self.win = lib.tpw_iw_alloc(lay[8])
        self.cdo = lib.tpw_iw_alloc_do()
        self.dobj = lib.tpw_iw_alloc_do()
        self.out = lib.tpw_iw_alloc(lay[10])
        self.text = lib.tpw_iw_alloc(16)
        CL.host_begin(lib, {1: self.win, 2: self.cdo, 3: self.dobj}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_iw_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, op, case):
        lib, w = self.lib, self.win
        if op == "size":
            lib.tpw_iw_size(w, self.out)
            return None
        if op == "adjust":
            lib.tpw_iw_adjust(w, *[s16(v) for v in case["args"]])
            return None
        if op == "uncap":
            return lib.tpw_iw_uncapture(w, s32(case["args"][0])) & 0xFF
        if op == "stop":
            return lib.tpw_iw_stop(w) & 0xFF
        if op == "timer":
            return lib.tpw_iw_timer(w, s32(case["args"][0]), s32(case["args"][1])) & 0xFFFF
        lib.tpw_iw_text(w, self.text if case["text"] is not None else 0)
        return None


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_iw_addr.restype, lib.tpw_iw_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_iw_alloc.restype, lib.tpw_iw_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_iw_alloc_do.restype, lib.tpw_iw_alloc_do.argtypes = C.c_uint64, []
    lib.tpw_iw_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_iw_size.restype, lib.tpw_iw_size.argtypes = None, [C.c_uint64, C.c_uint64]
    lib.tpw_iw_adjust.restype, lib.tpw_iw_adjust.argtypes = None, [C.c_uint64] + [C.c_int32] * 4
    lib.tpw_iw_uncapture.restype, lib.tpw_iw_uncapture.argtypes = C.c_uint32, [C.c_uint64, C.c_int32]
    lib.tpw_iw_stop.restype, lib.tpw_iw_stop.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_iw_timer.restype, lib.tpw_iw_timer.argtypes = C.c_uint32, [C.c_uint64, C.c_int32, C.c_int32]
    lib.tpw_iw_text.restype, lib.tpw_iw_text.argtypes = None, [C.c_uint64, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_iw_layout(out)
    lib.lay = list(out)[:n]
    assert n == 13, lib.lay
    lib.bound = True


def build(s, case):
    o, w = s.o, s.win
    s.shorts(w + o["rect"], case["rect"])
    s.shorts(w + o["offs"], case["offs"])
    s.write(w + o["animating"], bytes([case["animating"]]))
    s.ptr(w + o["anim"], case["anim"])
    s.shorts(w + o["frame"], [case["frame"]])
    for i, v in enumerate(case["timers"]):
        s.ptr(w + o["timers"] + i * o["ptr"], v)
    s.ptr(w + o["cdo"], s.cdo if case["client"] else 0)
    s.ptr(w + o["drawobj"], s.dobj if case["dobj"] else 0)
    s.shorts(s.cdo + o["do_rect2"], case["cdo_rect"])
    s.shorts(s.out + o["cr_left"], case["out"])
    kb = {"this": w, "other": 0x5555, "none": 0}[case["kb"]]
    s.ptr(s.g("kb"), kb)
    if case["text"] is not None:
        s.shorts(s.text, case["text"] + [0] * (8 - len(case["text"])))


def observe(s, case, answer):
    o, w = s.o, s.win
    kb = s.rd_ptr(s.g("kb"))
    return {
        "answer": answer,
        "log": s.log(),
        "rect": s.rd_shorts(w + o["rect"], 4),
        "offs": s.rd_shorts(w + o["offs"], 4),
        "animating": s.read(w + o["animating"], 1),
        "anim": s.rd_ptr(w + o["anim"]) & MASK,
        "frame": s.rd_shorts(w + o["frame"], 1),
        "timers": [s.rd_ptr(w + o["timers"] + i * o["ptr"]) & MASK for i in range(4)],
        "kb": -1 if kb == w else kb & MASK,
        "out": s.rd_shorts(s.out + o["cr_left"], 4),
    }


def make_run(op):
    def run(s, case):
        try:
            build(s, case)
            return observe(s, case, s.call(op, case))
        finally:
            s.done()

    return run


def ppc_fn(op):
    r = make_run(op)
    return lambda e, case: r(Ppc(e, case), case)


def host_fn(op):
    r = make_run(op)
    return lambda lib, case: r(Host(lib, case), case)


def sh(r):
    return r.choice([0, 1, -1, 2, -2, 32767, -32768, r.randrange(-32768, 32768), r.randrange(-300, 300)])


def base(r, op, **kw):
    c = {"op": op,
         "rect": [sh(r) for _ in range(4)],
         "offs": [sh(r) for _ in range(4)],
         "animating": r.choice([0, 1, 2, 0x80, 0xFF]),
         "anim": r.choice([0, 0x1234]),
         "frame": sh(r),
         "timers": [r.choice([0, 0x4000 + i]) for i in range(4)],
         "kb": r.choice(["this", "other", "none"]),
         "client": r.random() < 0.7,
         "cdo_rect": [sh(r) for _ in range(4)],
         "out": [sh(r) for _ in range(4)],
         "dobj": r.random() < 0.7,
         "text": r.choice([None, [], [65], [0x20AC, 0x41, 0x42], [0xFFFF, 1, 2]]),
         "args": [],
         "queues": {"AddTimer": [r.choice([0, 0x7001])]}}
    c.update(kw)
    return c


def gen(op, argf, variants):
    def rnd(r):
        return base(r, op, args=argf(r))

    def edges():
        r = random.Random(sum(map(ord, op)))
        for kw in variants(r):
            yield base(r, op, args=argf(r), **kw)

    return edges, rnd


def no_args(r):
    return []


def adj_args(r):
    return [sh(r) & 0xFFFF for _ in range(4)]


def kb_args(r):
    return [r.choice([0, 1, -1, 0x7FFFFFFF, r.randrange(-1000, 1000)]) & MASK]


def timer_args(r):
    return [r.choice([0, 1, 2, 3, 0x80080, 0x80081, 0x80082, 0x80083, 0x80080 + r.randrange(4), r.randrange(4)]) & MASK,
            r.choice([0, 1, -1, 2000, r.randrange(-100000, 100000)]) & MASK]


def v_size(r):
    for c in (True, False):
        yield dict(client=c)
        yield dict(client=c, cdo_rect=[0, 0, 0, 0])
        yield dict(client=c, cdo_rect=[-32768, 32767, -1, 1])


def v_adj(r):
    yield dict()
    yield dict(rect=[0, 0, 0, 0])
    yield dict(rect=[32767, -32768, 32767, -32768])


def v_uncap(r):
    for k in ("this", "other", "none"):
        yield dict(kb=k)


def v_stop(r):
    for a in (0, 1, 0x80, 0xFF):
        yield dict(animating=a)
        yield dict(animating=a, offs=[-32768, -32768, -32768, -32768])
        yield dict(animating=a, offs=[0, 0, 0, 0])
        yield dict(animating=a, offs=[32767, 5, -7, 1])


def v_timer(r):
    for i in range(4):
        for tm in (0, 0x4000 + i):
            for q in (0, 0x7001):
                yield dict(timers=[tm] * 4, queues={"AddTimer": [q]})


def v_text(r):
    for d in (True, False):
        for t in (None, [], [65], [1, 2, 3, 4]):
            yield dict(dobj=d, text=t)


def test(name, op, argf, variants):
    edges, rnd = gen(op, argf, variants)
    return FnTest(name, ppc_fn(op), host_fn(op), edge=edges, rnd=rnd)


TESTS = [
    test("InterfaceWindow::GetSizeOfClientDrawObject", "size", no_args, v_size),
    test("InterfaceWindow::AdjustPosition", "adjust", adj_args, v_adj),
    test("InterfaceWindow::UnCaptureKb", "uncap", kb_args, v_uncap),
    test("InterfaceWindow::stopAnim", "stop", no_args, v_stop),
    test("InterfaceWindow::setAnimTimer", "timer", timer_args, v_timer),
    test("InterfaceWindow::SetWindowDrawText", "text", no_args, v_text),
]
