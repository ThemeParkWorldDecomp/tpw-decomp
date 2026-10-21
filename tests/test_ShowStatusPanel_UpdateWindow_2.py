"""Unit v51007, the ship status panel's figures, gUpgradeStatusPanel's constructor and the year end panel's handler, against
libtpw_unit_ShowStatusPanel_UpdateWindow_2.dylib (tests/unit_ShowStatusPanel_UpdateWindow_2.cpp), main app, src/ShowStatusPanel_UpdateStatusWindow_ShowStatusPanel_UpdateWindow_2.cpp:
  ShowStatusPanel::UpdateWindow 0x101c21c4, __sinit_ui_upgradeStatus_cpp 0x101c6738, YearEndPanelMsg 0x101c67b4

A case is a world and a call. The world is built twice by ONE piece of Python code (emulator memory, big-endian, and the host library's
memory). Every callee outside the unit is a fake on both sides that records its arguments: GetWindow, Show, SetState, IsClosed, the
CObject getters, GetDBInfo, UIMain_GetString, the panel constructor, the panel removers and creators, UIMain_DefaultPanelMsg. The calls
through __ptr_glue (the panel's table slots 0x1c, 0x20, 0x3c and the windows' virtuals SetValue 0x24 and SetText 0x2c) are fakes too:
on the PowerPC the table holds magic words that the __ptr_glue fake decodes, on the host the table holds arena offsets of blocks with
function pointers.

The world of a case: the answers of the getters (32 random bits, so the byte mask of the chance shows), the closed flag, the ride record's
word at +0x10c, whether window 0xa08a exists, the message and its arguments. What is compared: the answer, the event log, the year end
panel global, the nine words of the upgrade status panel. No fused operations. Not tested: the windows other than 0xa08a being NULL
(the original uses them unchecked).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_ShowStatusPanel_UpdateWindow_2"
MASK = 0xFFFFFFFF

FN_UW, FN_SINIT, FN_YE = 0x101C21C4, 0x101C6738, 0x101C67B4
SIZES = {FN_UW: 784, FN_SINIT: 76, FN_YE: 224}
OWN = set(SIZES)

PTR_GLUE = 0x1023BAA8
GET_WINDOW, SHOW, SET_STATE = 0x101DBE40, 0x101DC1EC, 0x101CB1D0
IS_CLOSED, GET_CHANCE, GET_PRICE, GET_PRIZE, GET_DBINFO = 0x1010CAE8, 0x1010FBB8, 0x1010F4F4, 0x1010F52C, 0x10107CD4
GET_STRING, CONSTRUCT = 0x10180468, 0x101929E8
REMOVE_YE, CREATE_FIN, CREATE_PARK, DEFAULT_MSG = 0x101C73E4, 0x101A001C, 0x101ACA78, 0x1018493C

ADDR = dict(thing=0x103947A4, upgrade=0x10395B4C, yearend=0x10395B70)
HOST_LAYOUT = [0, 0x0C, 0x1C, 0x20, 0x24]

# labels of the blocks
L_PANEL, L_THING, L_DB, L_PARENT, L_W1C = 1, 2, 3, 4, 5
L_WIN = {0xA08A: 6, 0xA08E: 7, 0xA08F: 8, 0xA090: 9, 0xA0A6: 10, 0xA0A7: 11, 0xA0A5: 12}
L_WTABLE, L_STR33, L_STR34, L_ANSWER, L_PTABLE, L_YEWIN, L_UPGRADE = 13, 14, 15, 16, 17, 18, 40
MAGIC = {0x1C: 0xA000001C, 0x20: 0xA0000020, 0x3C: 0xA000003C, 0x24: 0xA0000024, 0x2C: 0xA000002C}
CUR = {}


def s32(v):
    return T.s32(v)


def A(label):
    return CUR["addr"][label]


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def st_get_window(e):
    L = e.cl
    wid = s32(e.r(4))
    L.rec("GetWindow", (L.lab(e.r(3)), wid))
    if wid == 0xA08A and L.ret("Win8aNull"):
        e.set_r(3, 0)
    else:
        e.set_r(3, A(L_WIN[wid]) if wid in L_WIN else 0)


def st_show(e):
    e.cl.rec("Show", (e.cl.lab(e.r(3)), T.s16(e.r(4))))


def st_set_state(e):
    e.cl.rec("SetState", (e.cl.lab(e.r(3)), T.s8(e.r(4)), T.s8(e.r(5))))


def getter(name, mask):
    def fn(e):
        e.cl.rec(name, (e.cl.lab(e.r(3)),))
        e.set_r(3, e.cl.ret(name) & mask)
    return fn


def st_dbinfo(e):
    e.cl.rec("GetDBInfo", (e.cl.lab(e.r(3)),))
    e.set_r(3, A(L_DB))


def st_get_string(e):
    sid = s32(e.r(3))
    e.cl.rec("UIMain_GetString", (sid,))
    e.set_r(3, A(L_STR33) if sid == 0x33 else A(L_STR34) if sid == 0x34 else 0)


def st_construct(e):
    e.cl.rec("Construct", (e.cl.lab(e.r(3)),))


def plain(name):
    def fn(e):
        e.cl.rec(name, ())
    return fn


def st_default(e):
    L = e.cl
    L.rec("DefaultPanelMsg", (L.lab(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6))))
    e.set_r(3, L.ret("DefaultPanelMsg") & MASK)


def st_glue(e):
    L = e.cl
    m = e.r(12) & MASK
    if m == MAGIC[0x1C]:
        L.rec("Slot1c", (L.lab(e.r(3)),))
    elif m == MAGIC[0x20]:
        L.rec("Slot20", (L.lab(e.r(3)),))
    elif m == MAGIC[0x3C]:
        L.rec("Slot3c", (L.lab(e.r(3)),))
        e.set_r(3, A(L_ANSWER))
    elif m == MAGIC[0x24]:
        L.rec("WindowSetValue", (L.lab(e.r(3)), s32(e.r(4))))
    elif m == MAGIC[0x2C]:
        L.rec("WindowSetText", (L.lab(e.r(3)), L.lab(e.r(4))))
    else:
        raise AssertionError("__ptr_glue with r12 = %x" % m)


STUBS.internal(GET_WINDOW, st_get_window)
STUBS.internal(SHOW, st_show)
STUBS.internal(SET_STATE, st_set_state)
STUBS.internal(IS_CLOSED, getter("IsClosed", MASK))
STUBS.internal(GET_CHANCE, getter("GetChanceOfWinning", MASK))
STUBS.internal(GET_PRICE, getter("GetPricePerUse", MASK))
STUBS.internal(GET_PRIZE, getter("GetPrizeValue", MASK))
STUBS.internal(GET_DBINFO, st_dbinfo)
STUBS.internal(GET_STRING, st_get_string)
STUBS.internal(CONSTRUCT, st_construct)
STUBS.internal(REMOVE_YE, plain("RemoveYearEndPanel"))
STUBS.internal(CREATE_FIN, plain("CreateFinancePanel"))
STUBS.internal(CREATE_PARK, plain("CreateParkStatusPanel"))
STUBS.internal(DEFAULT_MSG, st_default)
STUBS.internal(PTR_GLUE, st_glue)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def make_blocks(self):
        self.blk = {}
        for label in list(range(1, 19)):
            self.blk[label] = self.alloc(0x4000 if label == L_DB else 0x400)


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.make_blocks()
        CUR["addr"] = self.blk
        fixed = {a: l for l, a in self.blk.items()}
        fixed[ADDR["upgrade"]] = L_UPGRADE
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
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

    def sinit_table(self, word):
        return word

    def slot_entry(self, kind):
        return MAGIC[kind]

    def ptr(self, addr):
        return addr

    def set_thing(self, addr):
        self.words(self.g("thing"), [addr])

    def set_db(self, v):
        self.words(self.blk[L_DB] + 0x10C, [v])

    def call(self, entry, args):
        r3, _, _ = T.call_ppc(self.e, entry, [a & MASK for a in args])
        return r3 & MASK if entry == FN_YE else 0       # the other two answer nothing


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.make_blocks()
        fixed = dict(self.blk)
        fixed[L_UPGRADE] = self.g("upgrade")
        for label, addr in self.blk.items():
            lib.tpw_us_setlabel(label, addr)
        CL.host_begin(lib, fixed, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_us_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_us_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def sinit_table(self, word):
        # the static initialiser's table is the arena offset of UpgradeStatusPanelTable() (unit v5L31): the Mac's address when it is that
        return 0x102AF4DC if word == self.lib.tpw_us_sinit_table() else word

    def slot_entry(self, kind):
        b = self.alloc(16)
        self.write(b, struct.pack("<Q", self.lib.tpw_us_fn(kind)))
        return self.ptr(b)

    def ptr(self, addr):
        return self.lib.tpw_us_offset(addr)

    def set_thing(self, addr):
        self.lib.tpw_us_setthing(addr)

    def set_db(self, v):
        self.lib.tpw_us_dbset(self.blk[L_DB], s32(v))

    def call(self, entry, args):
        if entry == FN_UW:
            self.lib.tpw_us_updatewindow(args[0])
            return 0
        if entry == FN_SINIT:
            self.lib.tpw_us_sinit()
            return 0
        return self.lib.tpw_us_yearend(args[0], s32(args[1]), s32(args[2]), s32(args[3])) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_us_addr.restype, lib.tpw_us_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_us_alloc.restype, lib.tpw_us_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_us_offset.restype, lib.tpw_us_offset.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_us_setthing.argtypes = [C.c_uint64]
    lib.tpw_us_setlabel.argtypes = [C.c_int32, C.c_uint64]
    lib.tpw_us_fn.restype, lib.tpw_us_fn.argtypes = C.c_uint64, [C.c_int32]
    lib.tpw_us_dbset.argtypes = [C.c_uint64, C.c_int32]
    lib.tpw_us_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_us_updatewindow.argtypes = [C.c_uint64]
    lib.tpw_us_sinit.argtypes = []
    lib.tpw_us_sinit_table.restype, lib.tpw_us_sinit_table.argtypes = C.c_uint32, []
    lib.tpw_us_yearend.restype, lib.tpw_us_yearend.argtypes = C.c_int32, [C.c_uint64, C.c_int32, C.c_int32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_us_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    b = s.blk
    s.set_thing(b[L_THING])
    s.words(b[L_PANEL] + 0x0C, [s.ptr(b[L_PARENT])])
    s.words(b[L_PANEL] + 0x1C, [s.ptr(b[L_W1C])])
    s.words(b[L_PANEL] + 0x20, [s.ptr(b[L_PTABLE])])
    for label in [L_PARENT, L_W1C] + list(L_WIN.values()):
        s.words(b[label] + 0x118, [b[L_WTABLE]])
    s.words(b[L_WTABLE] + 0x24, [s.slot_entry(0x24) if False else MAGIC[0x24]])
    s.words(b[L_WTABLE] + 0x2C, [MAGIC[0x2C]])
    for kind in (0x1C, 0x20, 0x3C):
        s.words(b[L_PTABLE] + kind, [s.slot_entry(kind)])
    s.set_db(case["db"])
    s.words(s.g("upgrade"), case["upgrade"][:9])
    s.words(s.g("yearend"), [b[L_YEWIN]])


def execute(s, case):
    if case["op"] == "uw":
        return s.call(FN_UW, [s.blk[L_PANEL]])
    if case["op"] == "sinit":
        return s.call(FN_SINIT, [])
    return s.call(FN_YE, [s.blk[L_YEWIN], case["msg"], case["a"], case["b"]])


def observe(s, case, answer):
    return {"answer": answer, "log": s.log(), "yearend": {0: 0, s.blk[L_YEWIN] & MASK: 1}.get(s.rd_word(s.g("yearend")), 2),
            "upgrade": s.rd_words(s.g("upgrade"), 8) + [s.sinit_table(s.rd_word(s.g("upgrade") + 0x20))]}


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


# ---------------------------------------------------------------------------------------------------------------- cases


def w32(r):
    return r.choice([0, 1, 0xFF, 0x100, 0x1FF, 0x80000000, MASK, r.getrandbits(32), r.getrandbits(32)])


def base(r, op):
    return {"op": op, "db": r.choice([0, 1, w32(r)]), "upgrade": [r.getrandbits(32) for _ in range(10)], "msg": 0, "a": 0, "b": 0,
            "queues": {"Win8aNull": [r.choice([0, 0, 1])], "IsClosed": [r.choice([0, 1])],
                       "GetChanceOfWinning": [w32(r)], "GetPricePerUse": [w32(r)], "GetPrizeValue": [w32(r)],
                       "DefaultPanelMsg": [w32(r)]}}


def uw_edges():
    r = random.Random(11)
    for null in (0, 1):
        for closed in (0, 1):
            for db in (0, 1, 0x80000000):
                c = base(r, "uw")
                c["db"] = db
                c["queues"]["Win8aNull"], c["queues"]["IsClosed"] = [null], [closed]
                yield c


def uw_random(r):
    return base(r, "uw")


def sinit_edges():
    r = random.Random(12)
    yield base(r, "sinit")


def sinit_random(r):
    return base(r, "sinit")


MSGS = [0, 1, 4, 5, 6, 0x13, 0x14, 0x15, 0x16, 0x1E, 0xFF, 0x100, 0x101, -1, -5, 0x7FFFFFFF]
ARGS = [-3, -2, -1, 0, 1, 0xADB3, 0xADB4, 0xADB5, 0xADB6, 0xADB7, 0x10000, 0x80000000]


def ye_case(r, msg, a):
    c = base(r, "ye")
    c["msg"], c["a"], c["b"] = msg & MASK, a & MASK, w32(r)
    return c


def ye_edges():
    r = random.Random(13)
    for msg in MSGS:
        for a in ARGS:
            yield ye_case(r, msg, a)


def ye_random(r):
    return ye_case(r, r.choice(MSGS + [r.getrandbits(32)]), r.choice(ARGS + [r.getrandbits(32)]))


TESTS = [
    FnTest("ShowStatusPanel::UpdateWindow", ppc, host, edge=uw_edges, rnd=uw_random),
    FnTest("__sinit_ui_upgradeStatus_cpp", ppc, host, edge=sinit_edges, rnd=sinit_random),
    FnTest("YearEndPanelMsg", ppc, host, edge=ye_edges, rnd=ye_random),
]
