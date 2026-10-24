"""Unit v5314, the coaster edit buttons and the brace mesh lists, against libtpw_unit_SetCoasterEdit.dylib (tests/unit_SetCoasterEdit.cpp),
main app ("SimTheme Park"), src/game/rides/CoasterEdit.cpp:
  SetCoasterEdit 0x10058758, GetPylonFromBraceMesh 0x100587fc, GetNthBraceMesh 0x10058888, HandleCoasterButtonClick 0x10058510

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. The callees outside the unit (FindCoasterInstanceFromEntranceID,
the instance overload of SetCoasterEdit, DisableCurrentPylonEditState, HandleCoasterButtonDown, GetCoasterEditAvailableButtons,
UIInterface::ChangeCoasterButtonsEnabled) are fakes on both sides that record their arguments.

The world of a case: the two button masks, the action recorder's recording word, the ride set slot with its set, block, brace list and entries (Mac offsets
in blocks the loaded files own; the set's flags and word 0xd4 at the host's offsets). What is compared: the answer, the globals, the out word and the
event log. No fused operations. Not tested: an id past the slots table, an index past the entries of a list without flag 0x20 (kept inside the 8 entries
the test fills).

With the port fixes on (unit coaster64), the brace mesh views keep the places of the host brace (its pylon record mData, its word mWord70) and
of the host pylon record (the count and the braces); the layout export gives them. One deliberate difference is mapped (the port fix
in GetNthBraceMesh): the entry's mesh is the brace's 32-bit mWord70, answered widened to the pointer, where the Mac keeps a 32-bit
pointer. The host world then writes each mesh as a 32-bit word (the low half of its block's address, a different one for each block) and
the answer is matched against those words.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_SetCoasterEdit"
MASK = 0xFFFFFFFF

FN_SET, FN_PYLON, FN_NTH, FN_CLICK = 0x10058758, 0x100587FC, 0x10058888, 0x10058510
SIZES = {FN_SET: 124, FN_PYLON: 92, FN_NTH: 112, FN_CLICK: 176}
OWN = set(SIZES)

C_FIND, C_SETINST, C_DISABLE, C_DOWN, C_AVAIL, C_CHANGE = 0x10043380, 0x100585F4, 0x10053B58, 0x10056D7C, 0x10057278, 0x101F0F94

ADDR = dict(enabled=0x102C16D4, down=0x102C16D8, recording=0x1037EC44 + 0x24, slots=0x102C79A4, ui=0x10396440, dummy=0x10500000)
HL = {}


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_find(e):
    L = e.cl
    L.rec("Find", (e.r(3) & MASK,))
    e.set_r(3, ADDR["dummy"] if L.ret("Find") else 0)


def stub_setinst(e):
    L = e.cl
    L.rec("SetCoasterEditInst", (L.lab(e.r(3)), s32(e.r(4))))


def stub_disable(e):
    e.cl.rec("Disable", ())


def stub_down(e):
    e.cl.rec("ButtonDown", (e.r(3) & MASK,))


def stub_avail(e):
    L = e.cl
    L.rec("Available", (L.lab(e.r(3)),))


def stub_change(e):
    L = e.cl
    L.rec("ChangeEnabled", (L.lab(e.r(3)), e.r(4) & MASK))


STUBS.internal(C_FIND, stub_find)
STUBS.internal(C_SETINST, stub_setinst)
STUBS.internal(C_DISABLE, stub_disable)
STUBS.internal(C_DOWN, stub_down)
STUBS.internal(C_AVAIL, stub_avail)
STUBS.internal(C_CHANGE, stub_change)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]


class Ppc(Side):
    fmt = ">I"
    PS = 4
    P = dict(flags=4, fd4=0xD4, size=0x100, slot=4, bl=0x100, blksize=0x108, lflags=4, lcount=0x30, lentries=0x34, listsize=0x60,
             eflags=8, emesh=0x70, entsize=0x80)

    def ptr(self, a, p):
        self.words(a, [p])

    def mesh(self, a, p):
        """Stores an entry's mesh; answers the value GetNthBraceMesh gives back for it."""
        self.ptr(a, p)
        return p

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({ADDR["dummy"]: 1, ADDR["ui"]: 2}, case["queues"])
        self.saved = STUBS.activate(e)

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

    def set_slot(self, i, addr):
        self.words(ADDR["slots"] + 4 * i, [addr])

    def call(self, entry, args):
        return T.call_ppc(self.e, entry, args)[0] & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {1: self.g("dummy"), 2: self.g("ui")}, case["queues"])
        self.P, self.PS = HL, 8

    def ptr(self, a, p):
        self.write(a, p.to_bytes(8, "little"))

    def mesh(self, a, p):
        if not HL["portfix"]:
            self.ptr(a, p)
            return p
        self.words(a, [p & MASK])
        return p & MASK

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_sce_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_sce_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def set_slot(self, i, addr):
        self.write(self.g("slots") + HL["slot"] * i, addr.to_bytes(HL["slot"], "little"))


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_sce_"
    lib.tpw_sce_addr.restype, lib.tpw_sce_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_sce_alloc.restype, lib.tpw_sce_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_sce_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_sce_set.restype, lib.tpw_sce_set.argtypes = None, [C.c_uint64, C.c_int32]
    lib.tpw_sce_pylon.restype, lib.tpw_sce_pylon.argtypes = C.c_uint64, [C.c_uint32, C.c_uint64]
    lib.tpw_sce_nth.restype, lib.tpw_sce_nth.argtypes = C.c_uint64, [C.c_uint64, C.c_uint32]
    lib.tpw_sce_click.restype, lib.tpw_sce_click.argtypes = None, [C.c_uint32, C.c_uint32]
    lib.tpw_sce_portfix.restype, lib.tpw_sce_portfix.argtypes = C.c_int32, []
    out = (C.c_int64 * 32)()
    n = lib.tpw_sce_layout(out)
    v = list(out)[:n]
    HL.update(flags=v[0], fd4=v[1], size=v[2], slot=v[3], bl=v[4], blksize=v[5], lflags=v[6], lcount=v[7], lentries=v[8],
              listsize=v[9], eflags=v[10], emesh=v[11], entsize=v[12])
    HL["portfix"] = lib.tpw_sce_portfix() == 1
    if HL["portfix"]:
        assert v[6] == 4 and v[10] == 8 and v[11] + 4 <= v[12], v
    else:
        assert v[6] == 4 and v[10] == 8 and v[4] == 0x100 and v[7] == 0x30, v
    lib.bound = True


def build(s, case):
    s.words(s.g("enabled"), [case["enabled"]])
    s.words(s.g("down"), [case["down"]])
    s.words(s.g("recording"), [case["recording"]])
    op = case["op"]
    if op == "set":
        a = s.alloc(0x40)
        s.words(a + 0x1C, [case["entrance"]])
        case["_obj"] = a if case["present"] else 0
    elif op == "pylon":
        i = case["id"]
        P = s.P
        st = s.alloc(P["size"])
        blk = s.alloc(P["blksize"])
        y = s.alloc(P["listsize"])
        s.words(st + P["flags"], [case["setflags"]])
        s.ptr(st + P["fd4"], blk)
        s.ptr(blk + P["bl"], y)
        s.words(y + P["lflags"], [case["listflags"]])
        s.words(y + P["lcount"], [case["count"]])
        s.set_slot(i, st)
        case["_y"] = y
        case["_out"] = s.alloc(4)
        s.words(case["_out"], [0xDEADBEEF])
    elif op == "nth":
        P = s.P
        y = s.alloc(P["listsize"])
        s.words(y + P["lflags"], [case["listflags"]])
        s.words(y + P["lcount"], [case["count"]])
        case["_meshes"] = {}
        for k in range(8):
            ent = s.alloc(P["entsize"])
            s.words(ent + P["eflags"], [case["entflags"][k]])
            mesh = s.alloc(4) if case["vals"][k] else 0
            mesh = s.mesh(ent + P["emesh"], mesh)
            if mesh:
                case["_meshes"][mesh] = k + 1
            s.ptr(y + P["lentries"] + s.PS * k, ent)
        case["_y"] = y


def execute(s, case):
    op = case["op"]
    if op == "set":
        if isinstance(s, Ppc):
            s.call(FN_SET, [case["_obj"], case["flag"] & MASK])
        else:
            s.lib.tpw_sce_set(case["_obj"], s32(case["flag"]))
        return 0
    if op == "click":
        if isinstance(s, Ppc):
            s.call(FN_CLICK, [case["buttons"], case["event"]])
        else:
            s.lib.tpw_sce_click(case["buttons"], case["event"])
        return 0
    if op == "pylon":
        r = s.call(FN_PYLON, [case["id"], case["_out"]]) if isinstance(s, Ppc) else s.lib.tpw_sce_pylon(case["id"], case["_out"])
        return "Y" if r == case["_y"] else r
    r = s.call(FN_NTH, [case["_y"], case["n"]]) if isinstance(s, Ppc) else s.lib.tpw_sce_nth(case["_y"], case["n"])
    return ("M", case["_meshes"][r]) if r in case["_meshes"] else r


def observe(s, case, answer):
    d = {"answer": answer, "log": s.log(), "enabled": s.rd_word(s.g("enabled")), "down": s.rd_word(s.g("down")),
         "recording": s.rd_word(s.g("recording"))}
    if case["op"] == "pylon":
        d["out"] = s.rd_word(case["_out"])
    return d


def run(s, case):
    case = dict(case)
    try:
        build(s, case)
        return observe(s, case, execute(s, case))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def rw(r):
    return r.choice([0, 1, 2, 4, 0x1FF, 0x200, 0x80000000, MASK, r.getrandbits(32), r.getrandbits(9)])


def base(r, op):
    return {"op": op, "queues": {"Find": [r.choice([0, 1])]}, "enabled": rw(r), "down": rw(r),
            "recording": r.choice([0, 0, 1, 0x100, MASK])}


def set_case(r, present=None, flag=None, recording=None):
    c = base(r, "set")
    c["present"] = r.choice([True, True, False]) if present is None else present
    c["flag"] = r.choice([0, 1, -1, 7]) if flag is None else flag
    if recording is not None:
        c["recording"] = recording
    c["entrance"] = r.getrandbits(32)
    return c


def set_edges():
    r = random.Random(11)
    for present in (True, False):
        for flag in (0, 1, -1, 256):
            for rec in (0, 1, 0x80000000):
                for find in (0, 1):
                    c = set_case(r, present, flag, rec)
                    c["queues"]["Find"] = [find]
                    yield c


def click_case(r, buttons=None, event=None):
    c = base(r, "click")
    c["buttons"] = rw(r) if buttons is None else buttons
    c["event"] = r.randrange(8) if event is None else event
    return c


def click_edges():
    r = random.Random(12)
    for buttons in (0, 1, 0x1FF, 0x200, 0x201, 0x100, 0x80000000, 0x3F0):
        for event in range(8):
            yield click_case(r, buttons, event)
    for event in (8, 0x10, 0xFFFFFFF8, MASK):
        yield click_case(r, 3, event)


def pylon_case(r, setflags=None, listflags=None, count=None):
    c = base(r, "pylon")
    c["id"] = r.randrange(8)
    c["setflags"] = r.choice([0, 0x80000, MASK, 0x7FFFF, r.getrandbits(32)]) if setflags is None else setflags
    c["listflags"] = r.choice([0, 0x20, MASK, 0x1F, r.getrandbits(32)]) if listflags is None else listflags
    c["count"] = r.choice([0, 1, 2, 5, MASK, r.getrandbits(32)]) if count is None else count
    return c


def pylon_edges():
    r = random.Random(13)
    for sf in (0, 0x80000, 0x7FFFF, 0xFFF7FFFF):
        for lf in (0, 0x20, 0x1F, 0xFFFFFFDF):
            for cnt in (0, 1, 5, MASK):
                yield pylon_case(r, sf, lf, cnt)


def nth_case(r, listflags=None, count=None, n=None):
    c = base(r, "nth")
    c["listflags"] = r.choice([0, 0x20, MASK, r.getrandbits(32)]) if listflags is None else listflags
    c["count"] = r.randrange(9) if count is None else count
    c["n"] = r.randrange(8) if n is None else n
    c["entflags"] = [r.choice([0, 0x2000, MASK, 0xFFFFDFFF, r.getrandbits(32)]) for _ in range(8)]
    c["vals"] = [r.choice([0, 1, 1]) for _ in range(8)]
    return c


def nth_edges():
    r = random.Random(14)
    for lf in (0, 0x20):
        for cnt in (0, 1, 3, 8):
            for n in range(8):
                c = nth_case(r, lf, cnt, n)
                yield c
                c = dict(nth_case(r, lf, cnt, n))
                c["entflags"] = [0x2000 if k % 2 else 0 for k in range(8)]
                yield c


TESTS = [
    FnTest("SetCoasterEdit", ppc, host, edge=set_edges, rnd=lambda r: set_case(r)),
    FnTest("GetPylonFromBraceMesh", ppc, host, edge=pylon_edges, rnd=lambda r: pylon_case(r)),
    FnTest("GetNthBraceMesh", ppc, host, edge=nth_edges, rnd=lambda r: nth_case(r)),
    FnTest("HandleCoasterButtonClick", ppc, host, edge=click_edges, rnd=lambda r: click_case(r)),
]
