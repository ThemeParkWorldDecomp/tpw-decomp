"""Unit v5076, the staff panel's status and location leaves, against libtpw_unit_hackfollowmode.dylib (tests/unit_hackfollowmode.cpp), main app,
src/ui_staffPanel_msg_hackfollowmode.cpp:
  UpdateStatus 0x101bf5c4, UpdateLocator 0x101bf798, StaffLocationSwitch 0x101bf9f4, hackfollowmode 0x101bfb48

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian) for the
original, and in the host library's memory (little-endian) for the translation. The callees outside the unit (GetNextStaffMember, the window
members GetWindow / Show / unlink / link / SetFrame, the virtuals SetValue and SetText through the glue, the CStaff queries, TbITable::FormatString,
LLCC_DrawLine, UIMain_ZoomToPeep, UIMain_GetStaffStateFrame) are fakes on both sides that record their arguments; the functions of the unit call each
other as the original does (StaffLocationSwitch -> UpdateLocator -> UpdateStatus).

The world of a case: the panel thing (a block with a kind byte and the words at +0x30 and +0x1d8), the thing GetNextStaffMember may install, the
location, guard and frame globals, the first person flags, the conversion buffer, the answer queues. What is compared: the log, the thing global,
the globals the functions write, the flags and the first characters of the conversion buffer.

No fused operations. Not tested: nothing is skipped.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_hackfollowmode"
MASK = 0xFFFFFFFF

FN_STATUS, FN_LOCATOR, FN_SWITCH, FN_HACK = 0x101bf5c4, 0x101bf798, 0x101bf9f4, 0x101bfb48
SIZES = {FN_STATUS: 432, FN_LOCATOR: 464, FN_SWITCH: 296, FN_HACK: 616}
OWN = set(SIZES)

ZOOM, GETNEXT, DRAW, FRAME = 0x10181c70, 0x101bf32c, 0x1009c0d0, 0x10180a1c
HASPATROL, GETPATROL, GETSTATE, GETNAME = 0x10127a78, 0x1012798c, 0x101267cc, 0x10126f98
GETWINDOW, SETFRAME, SHOW, LINK, UNLINK = 0x101dbe40, 0x101db7dc, 0x101dc1ec, 0x101ddf20, 0x101de084
FORMAT, PTRGLUE = 0x10239730, 0x1023baa8
VT_INT = 0x103bcaf0
SLOT_SETVALUE, SLOT_SETTEXT = 0x11110024, 0x1111002c

ADDR = dict(thing=0x10395a3c, panel=0x10395a40, main=0x103949d8, flags=0x102b5a40, loc=0x10395a34, guard=0x10395a2c,
            frame=0x10395a30, buf=0x10380c48, table=0x1037ff18 + 0x564)
HOST_LAYOUT = [2]
NAME_OFF = 0x7000

# labels: the objects of a case
LAB = dict(panel=1, main=2, k147=3, k148=4, k14a=5, thing=6, thing2=7, buf=8, name=9, table=10)

STUBS = CL.Stubs()
STUBS.real.update(OWN)
WORLD = {}        # the PowerPC side's objects of the running case


def s32(v):
    return T.s32(v)


def stub_zoom(e):
    e.cl.rec("ZoomToPeep", (e.cl.lab(e.r(3)),))


def stub_getnext(e):
    L = e.cl
    L.rec("GetNextStaffMember", (e.u32(ADDR["loc"]), L.lab(e.u32(ADDR["thing"]))))
    e.write(ADDR["thing"], struct.pack(">I", WORLD["thing2"] if L.ret("GetNext") else 0))


def stub_draw(e):
    e.cl.rec("DrawLine", (s32(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.u32(e.r(6))), s32(e.u32(e.r(7)))))


def stub_frame(e):
    e.cl.rec("GetStaffStateFrame", (s32(e.r(3)),))
    e.set_r(3, e.cl.ret("frame") & MASK)


def stub_state(e):
    e.cl.rec("GetGeneralStaffState", (e.cl.lab(e.r(3)),))
    e.set_r(3, e.cl.ret("state") & MASK)


def stub_name(e):
    e.cl.rec("GetName", (e.cl.lab(e.r(3)),))
    e.set_r(3, ADDR["buf"] + NAME_OFF)


def stub_has(e):
    e.cl.rec("HasPatrolArea", (e.cl.lab(e.r(3)),))
    e.set_r(3, e.cl.ret("has") & MASK)


def stub_getpatrol(e):
    L = e.cl
    L.rec("GetPatrolArea", (L.lab(e.r(3)),))
    e.write(e.r(4), struct.pack(">H", L.ret("pstart") & 0xFFFF))
    e.write(e.r(5), struct.pack(">H", L.ret("pend") & 0xFFFF))


def stub_getwindow(e):
    L = e.cl
    L.rec("GetWindow", (L.lab(e.r(3)), s32(e.r(4))))
    e.set_r(3, {0x147: WORLD["k147"], 0x148: WORLD["k148"], 0x14a: WORLD["k14a"]}.get(e.r(4), 0))


def stub_setframe(e):
    e.cl.rec("SetFrame", (e.cl.lab(e.r(3)), s32(e.r(4))))


def stub_show(e):
    e.cl.rec("Show", (e.cl.lab(e.r(3)), T.s16(e.r(4))))


def stub_link(e):
    L = e.cl
    L.rec("link", (L.lab(e.r(3)), L.lab(e.r(4)), e.r(5) & 0xFF, e.r(6) & 0xFF))


def stub_unlink(e):
    e.cl.rec("unlink", (e.cl.lab(e.r(3)),))


def stub_format(e):
    save_toc(e)
    L = e.cl
    this, out, size, fid = e.r(3), e.r(4), e.r(5), e.r(6)
    if fid == 0x1C8:
        tag = e.r(7)
        magic, vptr, tid, value = struct.unpack(">4I", e.read(tag, 16))
        vt_slot = e.u32(0x10261344)   # the TOC slot of the imported vtable: what the original stores as the tag's vtable
        L.rec("FormatString", (L.lab(this), L.lab(out), size, fid, magic, 1 if vptr == vt_slot else 0, tid, s32(value)))
        e.write(out, struct.pack(">3H", 0x41 + (fid & 0xF), value & 0xFFFF, 0))
    else:
        L.rec("FormatString", (L.lab(this), L.lab(out), size, fid))
        e.write(out, struct.pack(">3H", 0x41 + (fid & 0xF), 0x2D, 0))


def save_toc(e):
    # the import glue saves the caller's TOC pointer at 0x14(r1); the caller reloads r2 from there
    e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2)))


def stub_glue(e):
    save_toc(e)
    L = e.cl
    slot = e.r(12)
    if slot == SLOT_SETVALUE:
        L.rec("SetValue", (L.lab(e.r(3)), s32(e.r(4))))
    elif slot == SLOT_SETTEXT:
        L.rec("SetText", (L.lab(e.r(3)), L.lab(e.r(4))))
    else:
        L.rec("BadGlue", (slot,))


for _a, _f in ((ZOOM, stub_zoom), (GETNEXT, stub_getnext), (DRAW, stub_draw), (FRAME, stub_frame), (HASPATROL, stub_has),
               (GETPATROL, stub_getpatrol), (GETSTATE, stub_state), (GETNAME, stub_name), (GETWINDOW, stub_getwindow),
               (SETFRAME, stub_setframe), (SHOW, stub_show), (LINK, stub_link), (UNLINK, stub_unlink), (FORMAT, stub_format),
               (PTRGLUE, stub_glue)):
    STUBS.internal(_a, _f)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def ptrs(self, a, ws):
        self.write(a, b"".join(struct.pack(self.pfmt, w) for w in ws))

    def rd_ptr(self, a):
        return struct.unpack(self.pfmt, self.read(a, struct.calcsize(self.pfmt)))[0]

    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]

    def thing_block(self, kind, w30, w1d8):
        a = self.alloc(0x200)
        self.write(a + 2, bytes([kind & 0xFF]))
        self.words(a + 0x30, [w30])
        self.words(a + 0x1D8, [w1d8])
        return a

    def lab_of(self, w):
        return self.rev.get(w, 0 if w == 0 else -1)


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"

    def __init__(self, e, case):
        self.e = e
        for n in ("k147", "k148", "k14a", "panel", "main"):
            WORLD[n] = self.window()
        WORLD["thing"] = self.thing_block(case["kind"], case["w30"], case["w1d8"])
        WORLD["thing2"] = self.thing_block(case["kind2"], case["w30b"], case["w1d8b"])
        WORLD["table"] = self.alloc(16)
        fixed = {WORLD[n]: LAB[n] for n in ("panel", "main", "k147", "k148", "k14a", "thing", "thing2", "table")}
        fixed[ADDR["buf"]] = LAB["buf"]
        fixed[ADDR["buf"] + NAME_OFF] = LAB["name"]
        self.rev = dict(fixed)
        self.obj = WORLD
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.saved = STUBS.activate(e)

    def window(self):
        w = self.alloc(0x140)
        vt = self.alloc(0x40)
        self.words(vt + 0x24, [SLOT_SETVALUE])
        self.words(vt + 0x2C, [SLOT_SETTEXT])
        self.words(w + 0x118, [vt])
        return w

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def kids(self):
        pass

    def call(self, op, arg):
        entry = {"status": FN_STATUS, "locator": FN_LOCATOR, "switch": FN_SWITCH, "hack": FN_HACK}[op]
        T.call_ppc(self.e, entry, [arg & MASK] if op == "switch" else [])


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.obj = {}
        for n in ("k147", "k148", "k14a", "panel", "main"):
            self.obj[n] = lib.tpw_sp_mkwin()
        self.obj["thing"] = self.thing_block(case["kind"], case["w30"], case["w1d8"])
        self.obj["thing2"] = self.thing_block(case["kind2"], case["w30b"], case["w1d8b"])
        self.obj["table"] = self.alloc(16)
        buf = self.g("buf")
        fixed = {LAB[n]: self.obj[n] for n in ("panel", "main", "k147", "k148", "k14a", "thing", "thing2", "table")}
        fixed[LAB["buf"]] = buf
        fixed[LAB["name"]] = buf + NAME_OFF
        self.rev = {v: k for k, v in fixed.items()}
        CL.host_begin(lib, fixed, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_sp_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_sp_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def kids(self):
        self.ptrs(self.g("kids"), [self.obj["k147"], self.obj["k148"], self.obj["k14a"]])
        self.ptrs(self.g("next"), [self.obj["thing2"]])

    def call(self, op, arg):
        if op == "switch":
            self.lib.tpw_sp_switch(s32(arg))
        else:
            getattr(self.lib, "tpw_sp_" + op)()


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_sp_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "mkwin").restype, getattr(lib, p + "mkwin").argtypes = C.c_uint64, []
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_sp_switch.argtypes = [C.c_int32]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    o = s.obj
    s.kids()
    s.ptrs(s.g("thing"), [o["thing"] if case["has_thing"] else 0])
    s.ptrs(s.g("panel"), [o["panel"]])
    s.ptrs(s.g("main"), [o["main"]])
    s.words(s.g("flags"), [case["flags"]])
    s.words(s.g("loc"), [case["loc"]])
    s.write(s.g("guard"), bytes([case["guard"]]))
    s.words(s.g("frame"), [case["frame"]])
    s.ptrs(s.g("table"), [o["table"]])
    s.write(s.g("buf"), b"".join(struct.pack(">H" if s.fmt[0] == ">" else "<H", 0x5A5A) for _ in range(4)))


def observe(s, case):
    return {"log": s.log(), "thing": s.lab_of(s.rd_ptr(s.g("thing"))), "flags": s.rd_word(s.g("flags")), "loc": s.rd_word(s.g("loc")),
            "guard": s.read(s.g("guard"), 1), "frame": s.rd_word(s.g("frame")), "buf": struct.unpack(s.fmt[0] + "4H", s.read(s.g("buf"), 8))}


def run(s, case):
    try:
        build(s, case)
        s.call(case["op"], case["arg"])
        return observe(s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def base(r, op):
    kind = r.choice([1, 3, 4, 5, 6, 7, 8, 9, 0x13])
    return {"op": op, "arg": 0, "kind": kind, "kind2": r.choice([1, 3, 4, 5, 6, 7, 8, 9]),
            "w30": r.choice([0, 1, 2, 0x29a, r.randrange(0, 1 << 32)]), "w1d8": r.choice([0, 1, 5, r.randrange(0, 1 << 32)]),
            "w30b": r.choice([0, 1, 7]), "w1d8b": r.choice([0, 3, 100]),
            "has_thing": r.random() < 0.8, "flags": r.choice([0, 0x1000, 0xFFFFFFFF, r.randrange(0, 1 << 32)]),
            "loc": r.choice([0, 1, 2, 3]), "guard": r.choice([0, 1, 0x80, 0xFF]),
            "frame": r.choice([9, 0, 4, -1 & MASK]),
            "queues": {"GetNext": [r.choice([0, 1])], "state": [r.randrange(0, 8)], "frame": [r.choice([9, 4, 0, 6, -1])],
                       "has": [r.choice([0, 1])], "pstart": [r.choice(AREA)], "pend": [r.choice(AREA)]}}


AREA = [0, 1, 2, 3, 128, 129, 130, 0x4000, 0x4001, 0xFFFF, 257, 258, 129 + 128 * 5, 3 + 128 * 5]


def rcase(r, op):
    c = base(r, op)
    if op == "switch":
        c["arg"] = r.choice([1, 2, 3, 4, 5, 6])
    if op == "hack" and r.random() < 0.3:
        s = r.choice(AREA)
        c["queues"]["pend"] = [(s + 128 * r.randrange(0, 4)) & 0xFFFF]
        c["queues"]["pstart"] = [s]
    if c["has_thing"] and r.random() < 0.3:
        c["kind"] = r.choice([4, 5, 6, 7, 8])
    return c


def edges(op):
    r = random.Random({"status": 11, "locator": 12, "switch": 13, "hack": 14}[op])
    for kind in (0, 1, 3, 4, 5, 6, 7, 8, 9, 0xFF):
        for has in (True, False):
            for k in range(2):
                c = rcase(r, op)
                c["kind"] = kind
                c["has_thing"] = has
                if op == "hack":
                    c["flags"] = [0x1000, 0, 0xFFFFEFFF, 0xFFFFFFFF][k * 2 % 4 + r.randrange(0, 2)]
                    c["queues"]["has"] = [k]
                if op == "status":
                    c["guard"] = k
                    c["w30"] = [0, 1][k]
                yield c
    if op == "switch":
        for loc in (0, 1, 2):
            for arg in (1, 2):
                for nxt in (0, 1):
                    for has in (True, False):
                        c = rcase(r, op)
                        c["loc"], c["arg"], c["has_thing"] = loc, arg, has
                        c["queues"]["GetNext"] = [nxt]
                        c["kind2"] = r.choice([1, 4, 8])
                        yield c
    if op == "locator":
        for w in (0, 1, 0x80000000):
            c = rcase(r, op)
            c["kind"], c["has_thing"], c["w1d8"] = 9, True, w
            yield c
    if op == "status":
        for fr in (9, 4):
            for gf in (9, 4):
                c = rcase(r, op)
                c["kind"], c["has_thing"], c["guard"], c["frame"] = 5, True, 1, gf
                c["queues"]["frame"] = [fr]
                yield c
        for w in (0, 1, 0x80000000):
            c = rcase(r, op)
            c["kind"], c["has_thing"], c["w30"] = 9, True, w
            yield c
    if op == "hack":
        for ps, pe in ((1, 1), (1, 2), (2, 1), (129, 130), (130, 129), (0, 0), (0xFFFF, 1), (1, 0xFFFF), (5, 5 + 128), (1 + 128 * 3, 9 + 128 * 7)):
            c = rcase(r, op)
            c["kind"], c["has_thing"], c["flags"] = 6, True, 0x1000
            c["queues"]["has"], c["queues"]["pstart"], c["queues"]["pend"] = [1], [ps], [pe]
            yield c


TESTS = [
    FnTest("UpdateStatus", ppc, host, edge=lambda: edges("status"), rnd=lambda r: rcase(r, "status")),
    FnTest("UpdateLocator", ppc, host, edge=lambda: edges("locator"), rnd=lambda r: rcase(r, "locator")),
    FnTest("StaffLocationSwitch", ppc, host, edge=lambda: edges("switch"), rnd=lambda r: rcase(r, "switch")),
    FnTest("hackfollowmode", ppc, host, edge=lambda: edges("hack"), rnd=lambda r: rcase(r, "hack")),
]
