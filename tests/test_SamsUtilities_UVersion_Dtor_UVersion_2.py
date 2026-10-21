"""Unit v51042, against libtpw_unit_SamsUtilities_UVersion_Dtor_UVersion_2.dylib (tests/unit_SamsUtilities_UVersion_Dtor_UVersion_2.cpp),
main app ("SimTheme Park"), src/game/ui/InterfaceTextObjectSprite.cpp, src/game/ui/ui_Postcard.cpp, src/game/ui/ui_loansPanel.cpp, src/game/ui/ui_main.cpp, src/game/ui/ui_parkStatus.cpp:
  SamsUtilities::UVersion::~UVersion 0x10187710, UIMain_SetObjectStateText 0x10181018, InterfaceRankTextObject::Change 0x1018fb08,
  LenderListCallback 0x101a4bec, LendersItemsCallback 0x101a4ce4, ThoughtMsg 0x101ab968, NameEditMsg 0x101afea4

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. The callees outside the unit (the string
destructor, operator delete, the window's SetFontColour / CallDefaultWindowHandler, the two virtuals through __ptr_glue, TbITable's
GetConstString, UIMain_ConvertMulti, swprintf, the text sprite's Change, FillLenderList, UpdateListHeadings, NewEntryInAddressBook) are
fakes on both sides that record their arguments and answer from the case's queues.

The world of a case: the state colour and string tables, the gText table pointers (+0x564, +0x56c), the thought state, the lender mode,
a window with the Mac's vtable at +0x118, a UVersion block, a rank text object and a change block. What is compared: the answer (the
address of the thought state, or the object, as a name), the globals read back and the whole event log.

No fused operations. Not tested: the rank format at 0x1024dd1a+0x16 is read by the original from the image and by the host from a
copy (its text is compared through the swprintf log); a pass-through Change with flag 0x100 (the fake would read the text through the
change block); a state outside 0..26 (reads past the tables).

A last test (unit v5L39), "the object state tables": the host's colour and string id tables as the library was loaded against the program
image's words (tests/imagedata.py), one case per entry. The cases above write their own tables; before v5L39 the host had only the first
colour and no string ids.
"""
import ctypes as C
import random
import struct

import calllog as CL
import imagedata
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_SamsUtilities_UVersion_Dtor_UVersion_2"
MASK = 0xFFFFFFFF

FN_UVDTOR, FN_STATE, FN_RANK = 0x10187710, 0x10181018, 0x1018FB08
FN_LENDLIST, FN_LENDITEMS, FN_THOUGHT, FN_NAMEEDIT = 0x101A4BEC, 0x101A4CE4, 0x101AB968, 0x101AFEA4
SIZES = {FN_UVDTOR: 112, FN_STATE: 148, FN_RANK: 156, FN_LENDLIST: 184, FN_LENDITEMS: 192, FN_THOUGHT: 116, FN_NAMEEDIT: 132}
OWN = set(SIZES)

TV_GET, TV_SET = 0x20000028, 0x2000002C
TB6, TB8 = 0x5A5A0006, 0x5A5A0008

ADDR = dict(colours=0x102A7780, strings=0x102A77EC, thought=0x10394A20, mode=0x1039497C, tb6=0x1037FF18 + 0x564, tb8=0x1037FF18 + 0x56C)
HOST_LAYOUT = [27 * 4, 8, 0]


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)

STUBS.internal(0x10238290, CL.recorder("UStringDtor", "Lx"))
STUBS.internal(0x10238218, CL.recorder("OperatorDelete", "L"))
STUBS.internal(0x101DA3AC, CL.recorder("SetFontColour", "Liiii"))
STUBS.internal(0x101DE318, CL.recorder("CallDefault", "Liii", ret="i"))
STUBS.internal(0x10238B78, CL.recorder("GetConstString", "uu", ret="i"))
STUBS.internal(0x10180334, CL.recorder("ConvertMulti", "u", ret="i"))
STUBS.internal(0x101A4A38, CL.recorder("FillLenderList", "L"))
STUBS.internal(0x101A4230, CL.recorder("UpdateListHeadings", ""))
STUBS.internal(0x101B0168, CL.recorder("NewEntryInAddressBook", ""))


def wide(e, addr, n):
    """Up to n UTF-16 (big-endian) units from addr, stopping at the terminator."""
    out = []
    for i in range(n):
        c = struct.unpack(">H", e.read(addr + 2 * i, 2))[0]
        if c == 0:
            break
        out.append(c)
    return out


def stub_glue(e):
    """__ptr_glue: the virtual calls of a window (r12 = the table entry)."""
    L = e.cl
    r12 = e.r(12) & MASK
    e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2) & MASK))   # the glue saves the TOC where the caller reloads it
    if r12 == TV_SET:
        L.rec("WindowSetText", (L.lab(e.r(3)), e.r(4) & MASK))
    elif r12 == TV_GET:
        L.rec("WindowGetValue", (L.lab(e.r(3)),))
        e.set_r(3, L.ret("WindowGetValue") & MASK)
    else:
        raise AssertionError("unknown ptr_glue target %x" % r12)


def stub_swprintf(e):
    L = e.cl
    buf, size, fmt, value = e.r(3) & MASK, e.r(4) & MASK, e.r(5) & MASK, e.r(6) & MASK
    f = wide(e, fmt, 9)
    L.rec("swprintf", tuple([size, value] + f + [0] * (9 - len(f))))
    chars = f[: max(size - 1, 0)] if size else []
    e.write(buf, b"".join(struct.pack(">H", c) for c in wide(e, fmt, 0x20)[: max(size - 1, 0)]) + b"\0\0")


def stub_change(e):
    L = e.cl
    this, flags, change = e.r(3) & MASK, e.r(4) & MASK, e.r(5) & MASK
    c = [0] * 8
    if flags == 0x100 and change:
        style = e.u32(change + 0x10)
        text = e.u32(style) if style else 0
        if text:
            w = wide(e, text, 8)      # up to the terminator (the rest of the buffer is not written)
            c[: len(w)] = w
    L.rec("Change", tuple([L.lab(this), flags, L.lab(change)] + c))


STUBS.internal(0x1023BAA8, stub_glue)
STUBS.internal(0x10239160, stub_swprintf)
STUBS.internal(0x1018D798, stub_change)


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

    def __init__(self, e, case):
        self.e = e
        self.win = e.alloc(0x200)
        self.uv = e.alloc(0x40)
        self.rank = e.alloc(0x200)
        self.change = e.alloc(0x40)
        self.L = e.cl = CL.PpcLog({self.win: 1, self.uv: 2, self.uv + 0xC: 3, self.uv + 0x18: 4, self.rank: 5, self.change: 6},
                                  case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def ptr(self, name, v):
        self.words(ADDR[name], [v])

    def vtable(self):
        tbl = self.e.alloc(0x40)
        self.words(tbl + 0x28, [TV_GET])
        self.words(tbl + 0x2C, [TV_SET])
        self.words(self.win + 0x118, [tbl])

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, fn, args):
        r3, _, _ = T.call_ppc(self.e, fn, [a & MASK for a in args])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.win = lib.tpw_v51042_alloc(0x200)
        self.uv = lib.tpw_v51042_alloc(0x100)
        self.rank = lib.tpw_v51042_alloc(0x200)
        self.change = lib.tpw_v51042_alloc(0x40)
        CL.host_begin(lib, {1: self.win, 2: self.uv, 3: lib.tpw_v51042_uversion_string(self.uv, 0),
                            4: lib.tpw_v51042_uversion_string(self.uv, 1), 5: self.rank, 6: self.change}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_v51042_addr(name.encode())

    def ptr(self, name, v):
        C.memmove(self.g(name), struct.pack("<Q", v & MASK), 8)

    def vtable(self):
        pass

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, fn, args):
        name = {FN_UVDTOR: "uversion_dtor", FN_STATE: "state_text", FN_RANK: "rank_change", FN_LENDLIST: "lender_list",
                FN_LENDITEMS: "lenders_items", FN_THOUGHT: "thought", FN_NAMEEDIT: "name_edit"}[fn]
        return (getattr(self.lib, "tpw_v51042_" + name)(*args) or 0) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_v51042_addr.restype, lib.tpw_v51042_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_v51042_alloc.restype, lib.tpw_v51042_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_v51042_uversion_string.restype, lib.tpw_v51042_uversion_string.argtypes = C.c_uint64, [C.c_uint64, C.c_int32]
    lib.tpw_v51042_layout.argtypes = [C.POINTER(C.c_int64)]
    U, I = C.c_uint64, C.c_int32
    lib.tpw_v51042_uversion_dtor.restype, lib.tpw_v51042_uversion_dtor.argtypes = U, [U, I]
    lib.tpw_v51042_state_text.restype, lib.tpw_v51042_state_text.argtypes = None, [U, I]
    lib.tpw_v51042_rank_change.restype, lib.tpw_v51042_rank_change.argtypes = None, [U, C.c_uint32, U]
    for n in ("lender_list", "lenders_items", "thought", "name_edit"):
        f = getattr(lib, "tpw_v51042_" + n)
        f.restype, f.argtypes = C.c_uint32, [U, I, I, I]
    out = (C.c_int64 * 32)()
    n = lib.tpw_v51042_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    s.words(s.g("colours"), case["colours"])
    s.words(s.g("strings"), case["strings"])
    s.words(s.g("thought"), case["thought"])
    s.words(s.g("mode"), [case["mode"]])
    s.ptr("tb6", TB6)
    s.ptr("tb8", TB8)
    s.vtable()
    s.words(s.change, [case["change0"]])


def execute(s, case):
    op, a = case["op"], case["args"]
    if op == "uv":
        obj = 0 if a[0] == 0 else s.uv
        ans = s.call(FN_UVDTOR, [obj, a[1]])
        return "NULL" if ans == 0 else ("SELF" if ans == (s.uv & MASK) else ans)
    if op == "state":
        s.call(FN_STATE, [s.win, a[0]])
        return 0
    if op == "rank":
        s.call(FN_RANK, [s.rank, a[0], s.change])
        return 0
    fn = {"lendlist": FN_LENDLIST, "lenditems": FN_LENDITEMS, "thought": FN_THOUGHT, "nameedit": FN_NAMEEDIT}[op]
    ans = s.call(fn, [s.win] + list(a))
    if op == "thought" and ans == (s.g("thought") & MASK):
        return "STATE"
    return ans


def observe(s, case, answer):
    return {"answer": answer, "log": s.log(), "thought": s.rd_words(s.g("thought"), 2), "mode": s.rd_word(s.g("mode"))}


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


def u32r(r):
    return r.choice([0, 1, 0xFFFFFFFF, 0x80000000, r.getrandbits(32), r.getrandbits(32), r.randrange(1, 5000)])


def base(r, op, args):
    return {"op": op, "args": args,
            "queues": {"CallDefault": [u32r(r)], "GetConstString": [u32r(r)], "ConvertMulti": [u32r(r)],
                       "WindowGetValue": [r.choice([0, 1, -1, 5, 0x7FFFFFFF, 0x80000000, r.randrange(0, 100), r.getrandbits(32)])]},
            "colours": [r.getrandbits(32) for _ in range(27)], "strings": [u32r(r) for _ in range(27)],
            "thought": [r.getrandbits(32), r.getrandbits(32)], "mode": r.choice([0, 1, r.getrandbits(32)]),
            "change0": r.choice([0, 1, 5, 99, 0x7FFFFFFE, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, r.getrandbits(32)])}


def pick_msg(r, specials):
    return r.choice(specials + [0, 1, -1, 0x14, 0x17, 0x100, 0x102, r.randrange(-5, 0x2000), r.getrandbits(32)])


# ---- UVersion
def uv_args(r, obj=None, flag=None):
    return [r.choice([0, 1]) if obj is None else obj, r.choice([-1, 0, 1, 2, 0x7FFF, -0x8000, 0x10000, 0x10001, 0x18000, r.getrandbits(32)])
            if flag is None else flag]


def uv_edges():
    r = random.Random(11)
    for obj in (0, 1):
        for flag in (-1, 0, 1, 2, 0x7FFF, 0x8000, 0x10000, 0x10001, 0x18000, 0xFFFF):
            yield base(r, "uv", uv_args(r, obj, flag))


def uv_random(r):
    return base(r, "uv", uv_args(r))


# ---- UIMain_SetObjectStateText
def state_edges():
    r = random.Random(12)
    for st in range(27):
        yield base(r, "state", [st])


def state_random(r):
    return base(r, "state", [r.randrange(27)])


# ---- InterfaceRankTextObject::Change
def rank_flags(r):
    return r.choice([0x80, 0x80, 0x180, 0xFFFFFFFF, 0x81, 0x80 | r.getrandbits(32), r.getrandbits(32) & ~0x180, 0, 1, 0x40, 0x7F])


def rank_edges():
    r = random.Random(13)
    for flags in (0x80, 0x180, 0x1000080, 0, 0x7F, 0x100 >> 1, 1, 0xFFFFFF7F):
        for v in (0, 5, 0x7FFFFFFE, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 100000):
            c = base(r, "rank", [flags])
            c["change0"] = v
            yield c


def rank_random(r):
    flags = rank_flags(r)
    if flags & 0x80 == 0:
        flags &= ~0x100
    return base(r, "rank", [flags])


# ---- the message functions
def msg_edges(op, key, specials, first_args):
    r = random.Random(key)
    for m in specials:
        for a in first_args:
            for b in (-1, 0, 1, 2, 0x7FFFFFFF, 0x80000000):
                yield base(r, op, [m, a, b])


def msg_random(op, specials, aspecials):
    def gen(r):
        return base(r, op, [pick_msg(r, specials), r.choice(aspecials + [r.getrandbits(32)]), r.choice([-1, 0, 1, 2, r.getrandbits(32)])])
    return gen


def lend_edges():
    return msg_edges("lendlist", 14, [0x101, 0x100, 0x102, 0x15, 0, 0x403], [0x12C4BF, 0x12C4C0, 0x12C4BE, 0, -1, 0x2C4BF, 0x112C4BF])


def lenditems_edges():
    return msg_edges("lenditems", 15, [0x403, 0x402, 0x404, 0x1E, 0, -1, 0x1403], [0, 1])


def thought_edges():
    return msg_edges("thought", 16, [0xC, 0xB, 0xD, 0, -1, 0x10C], [0, 1, -1])


def nameedit_edges():
    return msg_edges("nameedit", 17, [0x1000B, 0x1000A, 0x1000C, 0xB, 0, -1, 0x11000B], [0xD, 0xC, 0xE, 0, -1, 0x1000D])


TESTS = [
    FnTest("SamsUtilities::UVersion::~UVersion", ppc, host, edge=uv_edges, rnd=uv_random),
    FnTest("UIMain_SetObjectStateText", ppc, host, edge=state_edges, rnd=state_random),
    FnTest("InterfaceRankTextObject::Change", ppc, host, edge=rank_edges, rnd=rank_random),
    FnTest("LenderListCallback", ppc, host, edge=lend_edges, rnd=msg_random("lendlist", [0x101, 0x15], [0x12C4BF, 0, 1])),
    FnTest("LendersItemsCallback", ppc, host, edge=lenditems_edges, rnd=msg_random("lenditems", [0x403, 0x1E], [0, 1])),
    FnTest("ThoughtMsg", ppc, host, edge=thought_edges, rnd=msg_random("thought", [0xC], [0, 1])),
    FnTest("NameEditMsg", ppc, host, edge=nameedit_edges, rnd=msg_random("nameedit", [0x1000B], [0xD, 0xC])),
    imagedata.table_test("the object state tables", [("colours", ADDR["colours"], "I", 27), ("strings", ADDR["strings"], "I", 27)]),
]
