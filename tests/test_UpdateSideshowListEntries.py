"""Unit v5335, the sideshows list rows and the all-items list's row window, against libtpw_unit_UpdateSideshowListEntries.dylib
(tests/unit_UpdateSideshowListEntries.cpp), main app ("SimTheme Park"), src/AddItemToAllRidesList_UpdateSideshowListEntries.cpp:
  UpdateSideshowListEntries 0x10196208, LCFormat 0x10196388 (the host calls it AllItemsListLCFormat: the name LCFormat is taken)

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside
the unit are fakes on both sides that record their arguments: the list's GetData and ChangeItem, the history's GetSumOfLast, the four CObject
answers (read from words of the object the case builds), GetFontTableEntry, the font's virtual call (the glue 0x1023baa8), operator new, the
window constructor, SetCallbackFunction and CreateObject. The runtime's double to unsigned conversion (0x10237cd8) runs as the original.

The world of UpdateSideshowListEntries: a list of 0 to 6 rows, each row's data word the address of an object block (tag, excitement word,
profitability, revenue, satisfaction, the history's sum as a double). Of LCFormat: the row height global (0 or set), the font index, the list
kind, the real screen height, the font's answer and whether operator new answers. What is compared: the answer (the new block or not), the
event log, the globals.

No fused operations. Not tested: a screen height of 0 (divw by 0 differs between the emulator and the hardware); a sum below 0 or beyond
2^32 (the runtime conversion is out of range there).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_UpdateSideshowListEntries"
MASK = 0xFFFFFFFF

FN_UPDATE, FN_LCFORMAT = 0x10196208, 0x10196388
SIZES = {FN_UPDATE: 312, FN_LCFORMAT: 836}
OWN = set(SIZES)

GETDATA, GETSUM, GETEXC, GETPROFIT, GETREV, GETSAT, CHANGEITEM = 0x101d41f0, 0x100f1000, 0x1010cfb0, 0x1010f8d4, 0x1010f894, 0x1010f9d0, 0x101d3660
GETFONT, PTRGLUE, NEW, WINCTOR, SETCB, CREATEOBJ = 0x10180270, 0x1023baa8, 0x102384b8, 0x101d98d8, 0x101dcba8, 0x101dc750
HANDLER_TV = 0x10267a40
HISTORY_OFFSET = 0x1a4
LIST_COUNT_OFFSET = 0x150

ADDR = dict(rowheight=0x103947d4, fontindex=0x102a8cf0, listtype=0x103947dc, screenheight=0x10395bba)

ITEMS = [0] * 16
FONT = [0]


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_getdata(e):
    pos = s32(e.r(4))
    e.cl.rec("GetData", (pos,))
    e.write(e.r(5), struct.pack(">I", ITEMS[pos & 15] & MASK))
    e.set_r(3, 0)


def stub_getsum(e):
    obj = (e.r(3) - HISTORY_OFFSET) & MASK
    e.cl.rec("GetSumOfLast", (e.u32(obj), s32(e.r(5))))
    e.write(e.r(4), e.read(obj + 24, 8))
    e.set_r(3, 0)


def obj_word(offset):
    def stub(e):
        e.cl.rec(NAMES[offset][0], (e.u32(e.r(3)),) + ((e.r(4) & 0xFF,) if offset == 4 else ()))
        e.set_r(3, e.u32(e.r(3) + offset))
    return stub


NAMES = {4: ("GetExcitementLevel",), 8: ("GetProfitability",), 12: ("GetTotalRevenue",), 16: ("GetCustomerSatisfaction",)}


def stub_change(e):
    cells = tuple(s32(e.u32(e.r(5) + 4 * i)) for i in range(6))
    e.cl.rec("ChangeItem", (s32(e.r(4)),) + cells + (s32(e.r(6)), s32(e.r(7)), s32(e.r(8))))
    e.set_r(3, 0)


def stub_getfont(e):
    e.cl.rec("GetFontTableEntry", (e.r(3) & MASK,))
    e.set_r(3, FONT[0])


def save_toc(e):
    """The glue stores the caller's TOC pointer at 20(r1); the caller reloads it after the call."""
    e.write((e.r(1) + 0x14) & MASK, struct.pack(">I", e.r(2) & MASK))


def stub_glue(e):
    save_toc(e)
    e.cl.rec("FontVirtual", (0,))
    e.set_r(3, e.cl.ret("FontVirtual") & MASK)


def stub_new(e):
    save_toc(e)
    e.cl.rec("new", (e.r(3) & MASK,))
    block = e.alloc(max(e.r(3), 4)) if e.cl.ret("new") else 0
    e.lastnew = block
    e.set_r(3, block)


def stub_ctor(e):
    e.cl.rec("WindowCtor", (e.r(4) & MASK, s32(e.r(5)), s32(e.r(6)), T.s16(e.r(7)), T.s16(e.r(8)), T.s16(e.r(9)), T.s16(e.r(10))))


def stub_setcb(e):
    e.cl.rec("SetCallbackFunction", (1 if (e.r(4) & MASK) == HANDLER_TV else 0,))


def stub_create(e):
    e.cl.rec("CreateObject", (s32(e.r(4)), s32(e.r(5)), s32(e.r(6)), T.s16(e.r(7)), T.s16(e.r(8)), T.s16(e.r(9)), T.s16(e.r(10))))
    e.set_r(3, 0)


STUBS.internal(GETDATA, stub_getdata)
STUBS.internal(GETSUM, stub_getsum)
STUBS.internal(GETEXC, obj_word(4))
STUBS.internal(GETPROFIT, obj_word(8))
STUBS.internal(GETREV, obj_word(12))
STUBS.internal(GETSAT, obj_word(16))
STUBS.internal(CHANGEITEM, stub_change)
STUBS.internal(GETFONT, stub_getfont)
STUBS.internal(PTRGLUE, stub_glue)
STUBS.internal(NEW, stub_new)
STUBS.internal(WINCTOR, stub_ctor)
STUBS.internal(SETCB, stub_setcb)
STUBS.internal(CREATEOBJ, stub_create)


def setup(runner):
    pass


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def put16(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "h", v))

    def get16(self, a):
        return struct.unpack(self.fmt[0] + "h", self.read(a, 2))[0]


class Ppc(Side):
    fmt = ">I"
    count_offset = LIST_COUNT_OFFSET

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        e.lastnew = 0
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

    def set_item(self, i, a):
        ITEMS[i] = a

    def setup_font(self):
        a, b, c = self.alloc(0x20), self.alloc(0x20), self.alloc(8)
        self.words(a + 0x10, [b])
        self.words(b + 0xC, [c])
        FONT[0] = a

    def call_update(self, lst):
        T.call_ppc(self.e, FN_UPDATE, [lst & MASK])
        return 0

    def call_lcformat(self):
        r3, _, _ = T.call_ppc(self.e, FN_LCFORMAT, [])
        return 1 if (r3 & MASK) == (self.e.lastnew & MASK) else 0


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.count_offset = lib.layout[0]
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_us_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_us_alloc(max(n, 0x2000) if n >= 0x200 else n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def set_item(self, i, a):
        self.lib.tpw_us_set_item(i, a)

    def setup_font(self):
        pass

    def call_update(self, lst):
        self.lib.tpw_us_update(lst)
        return 0

    def call_lcformat(self):
        r = self.lib.tpw_us_lcformat()
        return 1 if r == self.lib.tpw_us_last_new() else 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_us_addr.restype, lib.tpw_us_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_us_alloc.restype, lib.tpw_us_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_us_set_item.restype, lib.tpw_us_set_item.argtypes = None, [C.c_int32, C.c_uint64]
    lib.tpw_us_last_new.restype, lib.tpw_us_last_new.argtypes = C.c_uint64, []
    lib.tpw_us_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_us_update.restype, lib.tpw_us_update.argtypes = None, [C.c_uint64]
    lib.tpw_us_lcformat.restype, lib.tpw_us_lcformat.argtypes = C.c_uint64, []
    out = (C.c_int64 * 32)()
    n = lib.tpw_us_layout(out)
    lib.layout = list(out)[:n]
    assert lib.layout[1] == HISTORY_OFFSET, lib.layout
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    if case["op"] == "update":
        lst = s.alloc(0x2000)
        s.words(lst + s.count_offset, [len(case["objs"])])
        for i, o in enumerate(case["objs"]):
            obj = s.alloc(0x40)
            s.words(obj, [i + 1, o["exc"], o["profit"], o["revenue"], o["sat"]])
            s.write(obj + 24, struct.pack(s.fmt[0] + "d", o["sum"]))
            s.set_item(i, obj)
        case["list"] = lst if case["haslist"] else 0
    else:
        s.setup_font()
        s.words(s.g("rowheight"), [case["rowheight"]])
        s.words(s.g("fontindex"), [case["fontindex"]])
        s.words(s.g("listtype"), [case["listtype"]])
        s.put16(s.g("screenheight"), case["screen"])


def observe(s, case, answer):
    r = {"answer": answer, "log": s.log()}
    if case["op"] == "lcformat":
        r["rowheight"] = s.rd_words(s.g("rowheight"), 1)
        r["fontindex"] = s.rd_words(s.g("fontindex"), 1)
        r["listtype"] = s.rd_words(s.g("listtype"), 1)
        r["screen"] = s.get16(s.g("screenheight"))
    return r


def run(s, case):
    try:
        build(s, case)
        if case["op"] == "update":
            ans = s.call_update(case["list"])
        else:
            ans = s.call_lcformat()
        return observe(s, case, ans)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- cases

SUMS = [0.0, 0.5, 0.999999, 1.0, 1.5, 29.0, 100.25, 2147483647.0, 2147483647.5, 2147483648.0, 2147483648.75, 3000000000.0, 4000000000.5]
SATS = [0, 1, 99, 100, 101, -1, -100, -101, 0x7FFFFFFF, -0x80000000, 0x200000, 0x400000, 0x3FFFFF, 50, 150]
EXCS = [0, 1, 99, 100, 254, 255, 256, 0x1FF, 0xFFFFFF55, 0x80]


def rnd_obj(r):
    k = r.random()
    return {
        "exc": r.choice(EXCS) if k < 0.4 else r.getrandbits(32),
        "profit": r.choice([0, 1, -1, 0x7FFFFFFF, -0x80000000]) & MASK if k < 0.3 else r.getrandbits(32),
        "revenue": r.getrandbits(32),
        "sat": r.choice(SATS) & MASK if k < 0.6 else r.getrandbits(32),
        "sum": r.choice(SUMS) if r.random() < 0.4 else r.uniform(0, 4.2e9) if r.random() < 0.3 else float(r.randrange(0, 100000)) + r.choice([0, 0.5, 0.99]),
    }


def update_case(r, n=None, haslist=True):
    n = r.randrange(0, 7) if n is None else n
    return {"op": "update", "queues": {}, "objs": [rnd_obj(r) for _ in range(n)], "haslist": haslist}


def update_edges():
    r = random.Random(5335)
    yield update_case(r, 0, False)
    yield update_case(r, 3, False)
    for n in (0, 1, 2, 3, 6):
        yield update_case(r, n)
    for s in SUMS:
        c = update_case(r, 1)
        c["objs"][0]["sum"] = s
        yield c
    for v in SATS:
        c = update_case(r, 2)
        c["objs"][1]["sat"] = v & MASK
        yield c
    for v in EXCS:
        c = update_case(r, 1)
        c["objs"][0]["exc"] = v
        yield c


def update_random(r):
    return update_case(r)


def lc_case(r, listtype=None, rowheight=None, screen=None, height=None, new=None):
    return {
        "op": "lcformat",
        "queues": {"FontVirtual": [r.getrandbits(32) if height is None else height & MASK], "new": [r.choice([1, 1, 1, 0]) if new is None else new]},
        "rowheight": r.choice([0, 0, r.getrandbits(32), 17]) if rowheight is None else rowheight & MASK,
        "fontindex": r.choice([7, 0, 0xFFFFFFFF, r.getrandbits(32)]),
        "listtype": r.choice([-3, -1, 0, 1, 2, 3, 4, 5, 9]) if listtype is None else listtype & MASK,
        "screen": r.choice([480, 600, 768, 1024, 1, -1, 0x7FFF, -0x8000, r.randrange(-3000, 3000) or 5]) if screen is None else screen,
    }


def lc_edges():
    r = random.Random(10196)
    for lt in (-3, -1, 0, 1, 2, 3, 4, 5, 100):
        yield lc_case(r, lt, 0, 768, 16, 1)
        yield lc_case(r, lt, 33, 768, 16, 1)
    yield lc_case(r, 2, 0, 768, 16, 0)
    yield lc_case(r, 0, 0, 480, 0x8000, 1)
    yield lc_case(r, 0, 0, 480, 0xFFFF, 1)
    yield lc_case(r, 0, 0, 480, 0x17FFF, 1)
    yield lc_case(r, 1, 0, -480, 0x7FFF, 1)
    yield lc_case(r, 3, 0, -1, 0x8001, 1)
    yield lc_case(r, 3, 0, 1, 1, 1)
    yield lc_case(r, 3, 0, 0x7FFF, 1, 1)
    yield lc_case(r, 3, 0, 1000, 1000, 1)


def lc_random(r):
    return lc_case(r)


TESTS = [
    FnTest("UpdateSideshowListEntries", ppc, host, edge=update_edges, rnd=update_random),
    FnTest("LCFormat", ppc, host, edge=lc_edges, rnd=lc_random),
]
