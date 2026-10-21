"""Unit v5723, seven small ui_main functions, against libtpw_unit_UIMain_ZoomToMapId.dylib (tests/unit_UIMain_ZoomToMapId.cpp), main app
("SimTheme Park"), src/game/ui/ui_main.cpp:
  UIMain_ZoomToMapId 0x10181d34, UIMain_ZoomToPeep 0x10181c70, UIMain_GetShiftKeyState 0x10182088, UIMain_DestroySpritePreview 0x10181864,
  UIMain_UpdateSpritePreview 0x101817e4, UIMain_MidWindowParticles 0x10182404, UIMain_DestroyRidePreview 0x101815d4

A case is a world and a call, built twice by one piece of Python code (emulator memory for the original, host memory for the translation).
The callees outside the unit are fakes on both sides that record their arguments: SetCameraPOI (the two floats it reads), the peep position
(it writes four outputs from the case), SCR_KillScript, ESP_Update2DScript, PAR_InitEmitter, DestroyRidePreview, the window's GetVisibility and
GetPosition (the rectangle from the case), the CoolRect constructor and destructor, the keyboard constructor and GetModifiers.

The world: the preview globals, the tick clocks, gGame's pause word, gKeyboard. Compared: the event log and the globals the functions write.
No fused operations. Not tested: a NULL window (read unconditionally). The keyboard made by the constructor fake answers no modifiers on the
host (the header's inline), so the PowerPC fake answers 0 and logs nothing for it, too.
"""
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_UIMain_ZoomToMapId"
MASK = 0xFFFFFFFF

FN = dict(zoommap=0x10181d34, zoompeep=0x10181c70, shift=0x10182088, destroysprite=0x10181864, updatesprite=0x101817e4,
          mid=0x10182404, destroyride=0x101815d4)
SIZES = {0x10181d34: 164, 0x10181c70: 148, 0x10182088: 168, 0x10181864: 76, 0x101817e4: 76, 0x10182404: 256, 0x101815d4: 68}
OWN = set(SIZES)

SETCAM, INTERP, KILL, UPD2D, PAR, DESTROYRIDE = 0x10033650, 0x101147e8, 0x100d9478, 0x100da6d8, 0x100bec30, 0x1006ccd0
GETVIS, GETPOS, RECT_CT, RECT_DT, KB_CT, PTRGLUE = 0x101dbd9c, 0x101db884, 0x101c7448, 0x101c74e0, 0x102398f8, 0x1023baa8

ADDR = dict(peepg=0x10380c3c, handle=0x10380c40, rideg=0x10380c44, tick=0x103bc180, last=0x103bc184, kbg=0x103bcef4,
            gamepause=0x1037f134 + 0x1c)
OBJ = ["win", "peep", "ride", "id", "kbd"]
LABELS = {"win": 1, "peep": 2, "ride": 3}
HOST_LAYOUT = [1]
CUR = {}


def s32(v):
    return T.s32(v)


def canon_f(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_setcam(e):
    def rd(p):
        return canon_f(e.u32(p)) if p else -1
    e.cl.rec("SetCameraPOI", (rd(e.r(3)), rd(e.r(4)), int(e.r(5) != 0), int(e.r(6) != 0), int(e.r(7) != 0)))


def stub_interp(e):
    L = e.cl
    L.rec("Interp", (L.lab(e.r(3)), canon_f(fb(e.f(1))), s32(e.r(5))))
    for reg, key in ((6, "px"), (7, "py"), (8, "pz"), (9, "pst")):
        e.write(e.r(reg), struct.pack(">I", L.ret(key) & MASK))


def stub_kill(e):
    e.cl.rec("SCR_KillScript", (s32(e.r(3)),))


def stub_upd(e):
    e.cl.rec("ESP_Update2DScript", (s32(e.r(3)), s32(e.r(4))))


def stub_par(e):
    e.cl.rec("PAR_InitEmitter", (s32(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6))))
    e.set_r(3, 0)


def stub_destroyride(e):
    e.cl.rec("DestroyRidePreview", (e.cl.lab(e.r(3)),))


def stub_getvis(e):
    e.cl.rec("GetVisibility", (e.cl.lab(e.r(3)),))
    e.set_r(3, e.cl.ret("vis") & MASK)


def stub_getpos(e):
    L = e.cl
    L.rec("GetPosition", (L.lab(e.r(3)), L.lab(e.r(4)), T.s16(e.r(5))))
    e.write(e.r(4) + 4, struct.pack(">4h", T.s16(L.ret("posL")), T.s16(L.ret("posT")), T.s16(L.ret("posR")), T.s16(L.ret("posB"))))
    e.set_r(3, 0)


def stub_rect_ct(e):
    e.cl.rec("CoolRect", (e.cl.lab(e.r(3)),))


def stub_rect_dt(e):
    e.cl.rec("~CoolRect", (e.cl.lab(e.r(3)),))


def stub_kb_ct(e):
    e.cl.rec("UKeyboard", ())
    e.write(e.r(3), struct.pack(">I", e.alloc(0x40)))


def stub_glue(e):
    if e.r(3) == CUR["kbd"]:
        e.cl.rec("GetModifiers", ())
        e.set_r(3, e.cl.ret("mods") & MASK)
    else:
        e.set_r(3, 0)


def stub_new(e):
    a = e.alloc(max(e.r(3), 4))
    e.set_r(3, a)


for a, f in ((0x102384b8, stub_new), (SETCAM, stub_setcam), (INTERP, stub_interp), (KILL, stub_kill), (UPD2D, stub_upd), (PAR, stub_par),
             (DESTROYRIDE, stub_destroyride), (GETVIS, stub_getvis), (GETPOS, stub_getpos), (RECT_CT, stub_rect_ct),
             (RECT_DT, stub_rect_dt), (KB_CT, stub_kb_ct), (PTRGLUE, stub_glue)):
    STUBS.internal(a, f)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]

    def ptr(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + ("Q" if self.fmt == "<I" else "I"), v))

    def rd_ptr(self, a):
        return struct.unpack(self.fmt[0] + ("Q" if self.fmt == "<I" else "I"), self.read(a, 8 if self.fmt == "<I" else 4))[0]

    def h16(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def lab(self, v):
        if v == 0:
            return 0
        for n in ("win", "peep", "ride", "kbd"):
            if v == self.g(n):
                return n
        return "other"


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.objs = {n: e.alloc(0x100) for n in OBJ}
        CUR["kbd"] = self.objs["kbd"]
        fixed = {self.objs[n]: l for n, l in LABELS.items()}
        fixed.update({SETCAM: "SetCameraPOI"} if False else {})
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        if name == "kbg":
            return self.e.u32(0x102611c4)    # the import variable the loader resolved
        return self.objs[name] if name in self.objs else ADDR[name]

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def setkb(self):
        vt = self.e.alloc(0x40)
        self.words(self.g("kbd"), [vt])
        self.words(self.g("kbg"), [self.g("kbd")])

    def call(self, op, args):
        r3, _, _ = T.call_ppc(self.e, FN[op], [a & MASK for a in args])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {l: self.g(n) for n, l in LABELS.items()}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_uim_addr(name.encode())

    def read(self, a, n):
        import ctypes as C
        return C.string_at(a, n)

    def write(self, a, data):
        import ctypes as C
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def setkb(self):
        self.lib.tpw_uim_setkb()

    def call(self, op, args):
        f = getattr(self.lib, "tpw_uim_" + op)
        return f(*args) & MASK if op == "shift" else (f(*args) or 0)


def bind(lib):
    import ctypes as C
    if getattr(lib, "bound", False):
        return
    lib.tpw_uim_addr.restype, lib.tpw_uim_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_uim_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_uim_setkb.restype = None
    lib.tpw_uim_zoommap.restype, lib.tpw_uim_zoommap.argtypes = None, [C.c_uint64]
    lib.tpw_uim_zoompeep.restype, lib.tpw_uim_zoompeep.argtypes = None, [C.c_uint64]
    lib.tpw_uim_shift.restype, lib.tpw_uim_shift.argtypes = C.c_uint32, []
    lib.tpw_uim_destroysprite.restype = None
    lib.tpw_uim_updatesprite.restype = None
    lib.tpw_uim_mid.restype, lib.tpw_uim_mid.argtypes = None, [C.c_int32, C.c_uint64]
    lib.tpw_uim_destroyride.restype = None
    out = (C.c_int64 * 32)()
    n = lib.tpw_uim_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    s.ptr(s.g("peepg"), s.g("peep") if case["peepset"] else 0)
    s.words(s.g("handle"), [case["handle"]])
    s.ptr(s.g("rideg"), s.g("ride") if case["rideset"] else 0)
    s.words(s.g("tick"), [case["tick"]])
    s.words(s.g("last"), [case["last"]])
    s.words(s.g("gamepause"), [case["pause"]])
    s.words(s.g("peep") + 0xc, [case["source"]])
    s.h16(s.g("id"), case["mapid"])
    if case["kb"]:
        s.setkb()
    else:
        s.ptr(s.g("kbg"), 0)


def execute(s, case):
    op = case["op"]
    if op == "zoompeep":
        return s.call(op, [s.g("peep") if case["peepArg"] else 0])
    if op == "zoommap":
        return s.call(op, [s.g("id")])
    if op == "mid":
        return s.call(op, [case["mid_id"], s.g("win")])
    return s.call(op, [])


def observe(s, case, answer):
    kb = s.rd_ptr(s.g("kbg"))
    return {"answer": answer if case["op"] == "shift" else 0, "log": s.log(),
            "peepg": s.lab(s.rd_ptr(s.g("peepg"))), "handle": s.rd_word(s.g("handle")), "rideg": s.lab(s.rd_ptr(s.g("rideg"))),
            "kb": 0 if kb == 0 else ("mock" if s.lab(kb) == "kbd" else "new")}


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, execute(s, case))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def rbits(r):
    k = r.random()
    if k < 0.1:
        return r.choice([0, 0x80000000, 0x7FC00000, 0x7F800000, 0x3F800000, 0xC2C80000])
    return fb(r.uniform(-500, 500))


def rs16(r):
    return r.choice([0, 1, -1, 32767, -32768, r.randrange(-32768, 32768), r.randrange(-2000, 2000)])


def base(r, op):
    return {"op": op, "peepset": r.choice([0, 1]), "handle": r.choice([0, 1, -1, r.randrange(0, 1000)]),
            "rideset": r.choice([0, 1]), "tick": r.randrange(0, 1 << 32), "last": r.randrange(0, 1 << 32),
            "pause": r.choice([0, 0, 1, 2]), "source": r.choice([0, 0, r.randrange(1, 1 << 32)]),
            "mapid": r.choice([0, 1, 2, 128, 129, 65535, r.randrange(0, 65536)]), "kb": r.choice([0, 1]),
            "peepArg": r.choice([0, 1, 1]), "mid_id": r.choice([0, 0x5a, -1, r.randrange(-1000, 1000)]),
            "queues": {"px": [rbits(r)], "py": [rbits(r)], "pz": [rbits(r)], "pst": [r.randrange(0, 1 << 32)],
                       "vis": [r.choice([0, 1, -1, 0x10000, 0x8000, r.randrange(0, 1 << 32)])],
                       "posL": [rs16(r)], "posT": [rs16(r)], "posR": [rs16(r)], "posB": [rs16(r)],
                       "mods": [r.choice([0, 0x200, 0x100, 0x1000, 0x800, 0x1100, 0x1900, 0x1B00, 0x400, 0xFFFFFFFF,
                                          r.randrange(0, 1 << 32), r.randrange(0, 1 << 12)])]}}


def gen(op, edges):
    def e():
        r = random.Random(sum(map(ord, op)))
        for over in edges:
            c = base(r, op)
            for k, v in over.items():
                if k in c["queues"]:
                    c["queues"][k] = [v]
                else:
                    c[k] = v
            yield c

    def rnd(r):
        return base(r, op)
    return e, rnd


MAPS = [{"mapid": v} for v in (0, 1, 2, 127, 128, 129, 255, 256, 257, 0x7FFF, 0x8000, 65535, 1000)]
PEEPS = [{"peepArg": 0}] + [{"peepArg": 1, "tick": t, "last": l} for t, l in
                            ((0, 0), (248, 0), (0, 248), (0x7FFFFFFF, 0xFFFFFFFF), (0x80000000, 0), (5, 1000), (1 << 31, 1 << 31))]
SHIFTS = [{"kb": k, "mods": m} for k in (0, 1) for m in (0, 0x200, 0x100, 0x1000, 0x800, 0x1100, 0x1900, 0x1B00, 0x400, 0xFFFFFFFF)]
SPRITE = [{"peepset": p, "source": s_, "handle": 7} for p in (0, 1) for s_ in (0, 5, 0xFFFFFFFF)]
MIDS = [{"vis": v, "pause": p} for v in (0, 1, 0x10000, 0x8000) for p in (0, 1)]
MIDS += [{"vis": 1, "pause": 0, "posL": l, "posT": t, "posR": rr, "posB": b} for l, t, rr, b in
         ((0, 0, 0, 0), (-32768, -32768, 32767, 32767), (32767, 32767, -32768, -32768), (100, 100, 101, 101), (-5, -5, -4, -4),
          (-1, -1, 0, 0), (1, 1, 2, 2), (0, 0, 640, 480), (3, 3, 10, 7), (-3, -3, 4, 2))]
RIDES = [{"rideset": 0}, {"rideset": 1}]

TESTS = []
for name, op, edges in (("UIMain_ZoomToMapId", "zoommap", MAPS), ("UIMain_ZoomToPeep", "zoompeep", PEEPS),
                        ("UIMain_GetShiftKeyState", "shift", SHIFTS), ("UIMain_DestroySpritePreview", "destroysprite", SPRITE),
                        ("UIMain_UpdateSpritePreview", "updatesprite", SPRITE), ("UIMain_MidWindowParticles", "mid", MIDS),
                        ("UIMain_DestroyRidePreview", "destroyride", RIDES)):
    e_, r_ = gen(op, edges)
    TESTS.append(FnTest(name, ppc, host, edge=e_, rnd=r_))
