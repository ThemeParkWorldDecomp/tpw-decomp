"""Unit v5207, the staff status panel's InitStatusWindow, UpdateStatusWindow and UpdateWindow, against
libtpw_unit_StaffStatusPanel_UpdateStatusWindow.dylib (tests/unit_StaffStatusPanel_UpdateStatusWindow.cpp), main app
("SimTheme Park"), src/game/ui/ui_staffStatus.cpp:
  StaffStatusPanel::UpdateStatusWindow 0x101c4990, UpdateWindow 0x101c4d2c, InitStatusWindow 0x101c4304

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. Every callee outside
the unit is a fake on both sides that records what it reads (the staff getters, GetWindow, the window setters, FormatString with the tag it
is given, the font and string getters, the draw objects' constructors, the advisor message, the button's SetState). The virtual calls
(the windows' Enable / SetValue / SetText and the panel table's entries 0x20 and 0x1c) are recorded through __ptr_glue on the PowerPC side
and through a fake virtual table / function blocks on the host side.

The world of a case: the panel (status window at 0x10 or none, window at 0xc, name window at 0x1c, table at 0x20), 18 fake windows
(ids 0x50e to 0x51f), the staff block (happiness and energy floats), gStaffRoomPanelThing, gText's resource 6, the convert buffer, and
the answer queues. What is compared: the event log and the first characters of the convert buffer.

No fused operations (the floats are only truncated). Not tested: a NULL gStaffRoomPanelThing in UpdateStatusWindow with a status window
(the original reads the staff's floats from address 0x1f4), floats at or past 2^31 (Unicorn's fctiwz answers 0x80000000 where the hardware
saturates), NaN floats.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_StaffStatusPanel_UpdateStatusWindow"
MASK = 0xFFFFFFFF

FN_INIT, FN_STATUS, FN_UPDATE = 0x101C4304, 0x101C4990, 0x101C4D2C
SIZES = {FN_INIT: 1552, FN_STATUS: 616, FN_UPDATE: 348}
OWN = set(SIZES)

ADDR = dict(buffer=0x10380C48, thing=0x103947A4, table=0x1037FF18 + 0x564)
FIRST_ID = 0x50E
NWIN = 18
CUR = {}          # the objects of the case being run


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def getter(name, key):
    def f(e):
        L = e.cl
        L.rec(name, (L.lab(e.r(3)),))
        e.set_r(3, L.ret(key) & MASK)
    return f


def stub_get_name(e):
    L = e.cl
    L.rec("GetName", (L.lab(e.r(3)),))
    e.set_r(3, CUR["o"]["name"])


def stub_get_window(e):
    L = e.cl
    wid = s32(e.r(4))
    L.rec("GetWindow", (L.lab(e.r(3)), wid))
    e.set_r(3, CUR["o"]["wins"][wid - FIRST_ID])


def stub_set_frame(e):
    L = e.cl
    L.rec("SetFrame", (L.lab(e.r(3)), s32(e.r(4))))


def stub_set_font(e):
    L = e.cl
    L.rec("SetFont", (L.lab(e.r(3)), L.lab(e.r(4))))


def stub_set_client_draw_text(e):
    L = e.cl
    p = e.r(4)
    L.rec("SetClientDrawText", (L.lab(e.r(3)), (p - CUR["o"]["str"]) if p else -1))


def stub_set_client_draw_object(e):
    L = e.cl
    L.rec("SetClientDrawObject", (L.lab(e.r(3)), L.lab(e.r(4))))


def stub_set_state(e):
    L = e.cl
    L.rec("SetState", (L.lab(e.r(3)), T.s8(e.r(4)), T.s8(e.r(5))))
    e.set_r(3, 0)


def stub_format_string(e):
    L = e.cl
    out, size, fid, tag = e.r(4), e.r(5), e.r(6), e.r(7)
    magic, _vt, tid, val = struct.unpack(">4I", e.read(tag, 16))
    L.rec("FormatString", (L.lab(e.r(3)), int(out == ADDR["buffer"]), size, fid, magic, tid, s32(val)))
    e.write(out, struct.pack(">3H", 0x41 + (fid & 15), val & 0xFFFF, 0))
    e.set_r(3, 0)


def stub_get_font(e):
    L = e.cl
    L.rec("GetFontTableEntry", (e.r(3),))
    e.set_r(3, CUR["o"]["font"])


def stub_get_string(e):
    L = e.cl
    sid = s32(e.r(3))
    L.rec("UIMain_GetString", (sid,))
    e.set_r(3, (CUR["o"]["str"] + 2 * sid) & MASK)


def stub_frame(e):
    L = e.cl
    L.rec("UIMain_GetStaffStateFrame", (s32(e.r(3)),))
    e.set_r(3, L.ret("frame") & MASK)


def stub_state_text(e):
    L = e.cl
    L.rec("UIMain_SetStaffStateText", (L.lab(e.r(3)), s32(e.r(4))))


def stub_indent(e):
    L = e.cl
    L.rec("UIMain_IndentClient", (L.lab(e.r(3)), T.s16(e.r(4)), T.s16(e.r(5))))


def stub_advisor(e):
    L = e.cl
    L.rec("UIMain_AdvisorMessage", (s32(e.r(3)),))


def stub_ptr_glue(e):
    L = e.cl
    code = CUR["o"]["descmap"].get(e.r(12))
    r3, r4 = e.r(3), e.r(4)
    if code == 0:
        L.rec("Enable", (L.lab(r3), T.s16(r4)))
    elif code == 1:
        L.rec("SetValue", (L.lab(r3), s32(r4)))
    elif code == 2:
        L.rec("SetText", (L.lab(r3),) + struct.unpack(">3H", e.read(r4, 6)))
    elif code == 3:
        L.rec("PanelSlot", (0x20, L.lab(r3)))
    elif code == 4:
        L.rec("PanelSlot", (0x1C, L.lab(r3)))
    else:
        raise AssertionError("unknown virtual target %x" % e.r(12))


STUBS.internal(0x101267CC, getter("GetGeneralStaffState", "state"))
STUBS.internal(0x10125A10, getter("GetEmploymentDuration", "duration"))
STUBS.internal(0x10126FC8, getter("GetSalary", "salary"))
STUBS.internal(0x1012686C, getter("GetSkill", "skill"))
STUBS.internal(0x10127A78, getter("HasPatrolArea", "patrol"))
STUBS.internal(0x10125AEC, getter("CanBePickedUp", "pickup"))
STUBS.internal(0x10126F98, stub_get_name)
STUBS.internal(0x101DBE40, stub_get_window)
STUBS.internal(0x101DB7DC, stub_set_frame)
STUBS.internal(0x101DA118, stub_set_font)
STUBS.internal(0x101DE130, stub_set_client_draw_text)
STUBS.internal(0x101DB624, stub_set_client_draw_object)
STUBS.internal(0x101CB1D0, stub_set_state)
STUBS.internal(0x10239730, stub_format_string)
STUBS.internal(0x10180270, stub_get_font)
STUBS.internal(0x10180468, stub_get_string)
STUBS.internal(0x10180A1C, stub_frame)
STUBS.internal(0x10180AA0, stub_state_text)
STUBS.internal(0x10180940, stub_indent)
STUBS.internal(0x101821CC, stub_advisor)
STUBS.internal(0x1018D11C, CL.recorder("SpriteCtor", "Ni"))
STUBS.internal(0x1018DE60, CL.recorder("CashCtor", "Ni"))
STUBS.internal(0x101898C8, CL.recorder("HGradient", "Ns"))
STUBS.internal(0x1023BAA8, stub_ptr_glue)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def halves(self, a, hs):
        self.write(a, struct.pack(self.fmt[0] + "%dH" % len(hs), *hs))

    def rd_halves(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dH" % n, self.read(a, 2 * n)))

    def layout(self):
        """The same blocks in the same order on both sides."""
        o = {}
        o["panel"] = self.alloc(0x30)
        o["staff"] = self.alloc(0x300)
        o["table"] = self.alloc(0x10)
        o["parent"] = self.alloc(0x130)
        o["status"] = self.alloc(0x130)
        o["namewin"] = self.alloc(0x130)
        o["wins"] = [self.alloc(0x130) for _ in range(NWIN)]
        o["font"] = self.alloc(0x10)
        o["str"] = self.alloc(0x400)
        o["name"] = self.alloc(0x40)
        o["ptbl"] = self.alloc(0x40)
        o["desc"] = self.alloc(0x40)
        o["wvt"] = self.alloc(0x40)
        o["descmap"] = {o["desc"] + 8 * k: k for k in range(5)}
        CUR["o"] = o
        self.o = o
        return o

    def labels(self):
        o = self.o
        fixed = {o["panel"]: 1, o["staff"]: 2, o["table"]: 3, o["parent"]: 4, o["status"]: 5, o["namewin"]: 6, o["font"]: 7}
        for k, w in enumerate(o["wins"]):
            fixed[w] = 10 + k
        return fixed


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.layout()
        self.L = e.cl = CL.PpcLog(self.labels(), case["queues"])
        self.saved = STUBS.activate(e)
        self.hap, self.en = 0x1F4, 0x1F8

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

    def off(self, a):
        return a

    def native(self, a, v):
        self.words(a, [v])

    def register(self, kind, index, p):
        pass

    def slot_fn(self, which):
        return None

    def call(self, fn):
        T.call_ppc(self.e, fn, [self.o["panel"] & MASK])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.layout()
        self.hap, self.en = lib.hap, lib.en
        fixed = {label: addr for addr, label in self.labels().items()}
        CL.host_begin(lib, fixed, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_ss_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_ss_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def off(self, a):
        return self.lib.tpw_ss_off(a)

    def native(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def register(self, kind, index, p):
        self.lib.tpw_ss_set(kind, index, p)

    def slot_fn(self, which):
        return self.lib.tpw_ss_slot(which)

    def call(self, fn):
        {FN_INIT: self.lib.tpw_ss_init, FN_STATUS: self.lib.tpw_ss_update_status, FN_UPDATE: self.lib.tpw_ss_update}[fn](
            self.o["panel"])


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_ss_addr.restype, lib.tpw_ss_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_ss_alloc.restype, lib.tpw_ss_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_ss_off.restype, lib.tpw_ss_off.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_ss_set.restype, lib.tpw_ss_set.argtypes = None, [C.c_int32, C.c_int32, C.c_uint64]
    lib.tpw_ss_slot.restype, lib.tpw_ss_slot.argtypes = C.c_uint64, [C.c_int32]
    lib.tpw_ss_layout.argtypes = [C.POINTER(C.c_int64)]
    for n in ("init", "update_status", "update"):
        f = getattr(lib, "tpw_ss_" + n)
        f.restype, f.argtypes = None, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_ss_layout(out)
    lib.hap, lib.en = out[0], out[1]
    assert out[2] <= 0x300 and out[1] == out[0] + 4, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    o = s.o
    # the registered objects (host only) and the windows' virtual tables (Mac only: the fake __ptr_glue reads the descriptors)
    allwin = [o["parent"], o["status"], o["namewin"]] + o["wins"]
    for k, w in enumerate(o["wins"]):
        s.register(0, k, w)
    for w in (o["parent"], o["status"], o["namewin"]):
        s.register(0, -1, w)
    s.register(1, 0, o["name"])
    s.register(2, 0, o["font"])
    s.register(3, 0, o["str"])
    if s.fmt == ">I":
        s.words(o["wvt"] + 0x1C, [o["desc"], o["desc"] + 8, o["desc"] + 16])
        s.words(o["wvt"] + 0x1C, [o["desc"]])
        s.words(o["wvt"] + 0x24, [o["desc"] + 8])
        s.words(o["wvt"] + 0x2C, [o["desc"] + 16])
        for w in allwin:
            s.words(w + 0x118, [o["wvt"]])
    # the panel's table: entries 0x20 and 0x1c are blocks holding the function (descriptors 3 and 4)
    d20, d1c = o["desc"] + 24, o["desc"] + 32
    if s.fmt == "<I":
        s.native(d20, s.slot_fn(0x20))
        s.native(d1c, s.slot_fn(0x1C))
    s.words(o["ptbl"] + 0x1C, [s.off(d1c)])
    s.words(o["ptbl"] + 0x20, [s.off(d20)])
    # the panel
    s.words(o["panel"], [0, 0, 0, s.off(o["parent"]), s.off(o["status"]) if case["has10"] else 0, 0, 0, s.off(o["namewin"]),
                         s.off(o["ptbl"])])
    # the staff
    s.words(o["staff"] + s.hap, [case["hap"]])
    s.words(o["staff"] + s.en, [case["en"]])
    s.native(s.g("thing"), o["staff"] if case["staff"] else 0)
    s.native(s.g("table"), o["table"])
    s.halves(o["name"], case["name"])
    s.halves(s.g("buffer"), [0] * 6)


def execute(s, case):
    s.call(case["fn"])
    return 0


def observe(s, case, answer):
    return {"log": s.log(), "buffer": s.rd_halves(s.g("buffer"), 6)}


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


# ---------------------------------------------------------------------------------------------------------------- case generators

FLOATS = [0.0, -0.0, 0.5, -0.5, 1.0, 99.9, 100.0, 255.0, 255.9, 256.0, 257.5, 300.7, 511.0, 1000.0, -1.0, -255.0, -256.0, -300.7,
          65535.5, 2e9, -2e9]


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def rfloat(r):
    if r.random() < 0.4:
        return fb(r.choice(FLOATS))
    return fb(r.uniform(-2000, 2000))


def base(r, fn):
    q = {
        "state": [r.choice([0, 1, 1, 2, 3, 4, 5, r.randrange(-3, 12)])],
        "duration": [r.choice([0, 1, 365, -1, r.randrange(-100000, 100000)])],
        "salary": [r.choice([0, 1, 500, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, r.randrange(0, 100000)])],
        "skill": [r.choice([0, 1, 99, 100, 101, -1, 0x3FFFFF, 0x400000, 0x7FFFFFFF, 0x80000000, r.randrange(-1000, 100000)])],
        "patrol": [r.choice([0, 1])],
        "pickup": [r.choice([0, 1])],
        "frame": [r.choice([0, 1, 7, -1, r.randrange(-50, 50)])],
    }
    return {"fn": fn, "queues": q, "has10": r.random() < 0.85, "staff": True, "hap": rfloat(r), "en": rfloat(r),
            "name": [r.choice([0, 0x41, 0x263A, 0xFFFF]) if r.random() < 0.5 else r.randrange(0, 0x10000) for _ in range(4)]}


def status_case(r, **kw):
    c = base(r, FN_STATUS)
    c.update(kw)
    return c


def status_edges():
    r = random.Random(5207)
    for has10 in (False, True):
        for state in (0, 1, 2, 3, 4, 5, -1, 6):
            for patrol in (0, 1):
                yield status_case(r, has10=has10, queues=dict(base(r, FN_STATUS)["queues"], state=[state], patrol=[patrol]))
    for h in FLOATS:
        yield status_case(r, hap=fb(h), en=fb(h))
        yield status_case(r, hap=fb(h), en=fb(-h))
    for sk in (0, 1, 99, 100, 0x3FFFFF, 0x400000, 0x7FFFFFFF, 0x80000000, -1):
        yield status_case(r, queues=dict(base(r, FN_STATUS)["queues"], skill=[sk]))
    yield status_case(r, has10=False, staff=False)


def status_random(r):
    return status_case(r)


def update_case(r, **kw):
    c = base(r, FN_UPDATE)
    c.update(kw)
    return c


def update_edges():
    r = random.Random(5208)
    yield update_case(r, staff=False)
    for patrol in (0, 1):
        for pickup in (0, 1):
            q = dict(base(r, FN_UPDATE)["queues"], patrol=[patrol], pickup=[pickup])
            yield update_case(r, queues=q)
            yield update_case(r, queues=q, name=[0, 0, 0, 0])


def update_random(r):
    return update_case(r, staff=r.random() < 0.9)


def init_case(r, **kw):
    c = base(r, FN_INIT)
    c.update(kw)
    return c


def init_edges():
    r = random.Random(5209)
    yield init_case(r, has10=False)
    yield init_case(r, has10=True)


def init_random(r):
    return init_case(r)


TESTS = [
    FnTest("StaffStatusPanel::UpdateStatusWindow", ppc, host, edge=status_edges, rnd=status_random),
    FnTest("StaffStatusPanel::UpdateWindow", ppc, host, edge=update_edges, rnd=update_random),
    FnTest("StaffStatusPanel::InitStatusWindow", ppc, host, edge=init_edges, rnd=init_random),
]
