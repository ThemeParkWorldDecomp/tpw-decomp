"""Unit v51125, sixteen small leaves of the main app ("SimTheme Park"), against libtpw_unit_std_exception_what.dylib
(tests/unit_std_exception_what.cpp), src/std_exception_what.cpp (and sAlignedBox::Byteswap in src/m3d2_load.cpp):
  std::exception::what 0x100025a8, std::logic_error::what 0x10002570, std::length_error::~length_error 0x100024ac,
  SamsUtilities::UResourceId::~UResourceId 0x10001ad0, CMapLinkBase::~CMapLinkBase 0x10007b04, sAlignedBox::Byteswap 0x1005e52c,
  SamsUtilities::USwap32<unsigned char*> 0x1005e5b4, USwapInplace16<unsigned short> 0x1005e610, USwapInplace32<unsigned long> 0x1005e658,
  TbTimeStamp::TbTimeStamp 0x1013dca8, std::__tree<unsigned short>::destroy 0x1016e65c, the three CFSOnlineSystem*List destructors
  0x101701c8, 0x10170834, 0x10170ca8, std::list<std::string>::clear 0x1017fc04, FeatureStatusPanel::DeInitViewWindow 0x1019e314.

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian,
4-byte pointers) for the original, and in the host library's memory (little-endian, 8-byte pointers) for the translation; the offsets of
each side are in a table. The callees outside the unit (operator delete, the string destructors, the ref-counted pointer destructor, the
record and list destructors, the lists' RemoveHead, the ushort set's value destroy and node deallocate, UIMain_DestroyRidePreview) are
fakes on both sides that record their arguments (objects as labels) and answer from the case (RemoveHead answers the next queued label).

The world of a case: the objects of the case (a message object, a tree of nodes, an online list with its answer record, infos and typed
list, a ring of string nodes, raw bytes to swap), a delete flag (the low 16 bits; the original gets random upper bits too, it sign-extends
the halfword). What is compared: the answer, the event log, and every word the function may write (the vtable word of the length_error by
name, the lists' fields, counts and answer record, the ring's links and size, the swapped bytes, the time stamp).

No fused operations. Not tested: the null `this` of the swaps, the tree's destroy and the list clear (the original reads through it); a
negative node count of the typed list (the loop would not end); CMapLinkBase's stored vtable word 0x10278c9c is only checked on the
original (the translation keeps the C++ vtable).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std_exception_what"
MASK = 0xFFFFFFFF

FN = dict(
    exc_what=0x100025A8, logic_what=0x10002570, length_dtor=0x100024AC, uresid_dtor=0x10001AD0, cmap_dtor=0x10007B04,
    box_swap=0x1005E52C, swap32=0x1005E5B4, swap16=0x1005E610, swapinplace32=0x1005E658, ts_ctor=0x1013DCA8,
    tree_destroy=0x1016E65C, city_dtor=0x101701C8, park1_dtor=0x10170834, park2_dtor=0x10170CA8, list_clear=0x1017FC04,
    deinit=0x1019E314,
)
OWN = set(FN.values())

# ---- the callees faked on both sides
OP_DELETE, USTRING_DTOR, RC_DTOR, STRING_DTOR = 0x10238218, 0x10238290, 0x100025E0, 0x1000221C
REMOVE_HEAD_CITY, REMOVE_HEAD_PARK, VDTOR, DESTROY_INFOS = 0x10177E2C, 0x10177B98, 0x1023BAA8, 0x10238AD0
US_DESTROY, US_DEALLOC, DESTROY_RIDE_PREVIEW = 0x1016E988, 0x1016E8B0, 0x101815D4

ADDR = dict(vtblA=0x10278B74, vtblB=0x10278BB0, vtblC=0x10278BC0)
VTBL_CMAP = 0x10278C9C

KEYS = ["exc_rc", "exc_size", "node_right", "node_parent", "node_value", "node_size", "sl_f24", "sl_f28", "sl_list", "sl_rec",
        "sl_size", "rec_infos", "rec_ref", "rec_size", "tb_count", "tb_size", "str_prev", "str_next", "sn_next", "sn_str", "sn_size",
        "ts_size"]
HOST_LAYOUT = [8, 24, 8, 16, 24, 32, 40, 44, 48, 56, 72, 8, 20, 24, 24, 32, 8, 16, 8, 16, 24, 8]
MAC_LAYOUT = [4, 8, 4, 8, 0xC, 0x10, 0x24, 0x28, 0x2C, 0x30, 0x34, 4, 0xC, 0x10, 0xC, 0x10, 4, 8, 4, 8, 0x10, 8]
HOST_OFF = dict(zip(KEYS, HOST_LAYOUT), node_left=0, sn_prev=0, hd=8)
MAC_OFF = dict(zip(KEYS, MAC_LAYOUT), node_left=0, sn_prev=0, hd=4)

CUR = {"objs": {}}          # label -> Mac address of the case being run (what the RemoveHead stub answers)
SENTINEL = 200


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_delete(e):
    L = e.cl
    L.rec("Delete", (L.lab(e.r(3)),))


def stub_ustring(e):
    L = e.cl
    L.rec("UStringDtor", (L.lab(e.r(3)), s32(e.r(4))))


def stub_rc(e):
    L = e.cl
    L.rec("RcDtor", (L.lab(e.r(3) - 4), s32(e.r(4))))


def stub_string(e):
    L = e.cl
    L.rec("StringDtor", (L.lab(e.r(3) - 8), s32(e.r(4))))


def stub_remove_head(e):
    L = e.cl
    this = e.r(3)
    L.rec("RemoveHead", (L.lab(this),))
    count = struct.unpack(">I", e.read(this + 0xC, 4))[0]
    e.write(this + 0xC, struct.pack(">I", (count - 1) & MASK))
    e.set_r(3, CUR["objs"].get(L.ret("RemoveHead") & 255, 0))


def stub_vdtor(e):
    L = e.cl
    L.rec("VDtor", (L.lab(e.r(3)), s32(e.r(4))))


def stub_infos(e):
    L = e.cl
    L.rec("DestroyInfos", (L.lab(e.r(3)), e.r(4) & MASK))


def stub_us_destroy(e):
    L = e.cl
    L.rec("UsDestroy", (L.lab(e.r(3)), L.lab(e.r(4) - 0xC)))


def stub_us_dealloc(e):
    L = e.cl
    L.rec("UsDealloc", (L.lab(e.r(3) - 4), L.lab(e.r(4)), e.r(5) & MASK))


def stub_ride(e):
    e.cl.rec("DestroyRidePreview", ())


STUBS.internal(OP_DELETE, stub_delete)
STUBS.internal(USTRING_DTOR, stub_ustring)
STUBS.internal(RC_DTOR, stub_rc)
STUBS.internal(STRING_DTOR, stub_string)
STUBS.internal(REMOVE_HEAD_CITY, stub_remove_head)
STUBS.internal(REMOVE_HEAD_PARK, stub_remove_head)
STUBS.internal(VDTOR, stub_vdtor)
STUBS.internal(DESTROY_INFOS, stub_infos)
STUBS.internal(US_DESTROY, stub_us_destroy)
STUBS.internal(US_DEALLOC, stub_us_dealloc)
STUBS.internal(DESTROY_RIDE_PREVIEW, stub_ride)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    """What the shared code needs from either memory: objects by label, pointers, words, bytes."""

    def new(self, label, size=0x80):
        a = self.alloc(size)
        self.labels[label] = a
        return a

    def at(self, label):
        return self.labels[label] if label else 0

    def wp(self, a, v):
        self.write(a, struct.pack(self.pf, v))

    def rp(self, a):
        return struct.unpack(self.pf, self.read(a, self.pw))[0]

    def w32(self, a, v):
        self.write(a, struct.pack(self.f32, v & MASK))

    def r32(self, a):
        return struct.unpack(self.f32, self.read(a, 4))[0]

    def wl(self, a, label):
        self.wp(a, self.at(label))

    def tolabel(self, v):
        if v == 0:
            return 0
        for lab, addr in self.labels.items():
            if addr == v:
                return lab
        return ("raw", v)

    def rl(self, a):
        return self.tolabel(self.rp(a))


class Ppc(Side):
    """One case on the PowerPC side: memory in the emulator's image; the fakes are switched on by start()."""
    pf, pw, f32 = ">I", 4, ">I"
    off = MAC_OFF

    def __init__(self, e, case):
        self.e = e
        self.case = case
        self.labels = {}
        self.saved = None

    def start(self):
        e = self.e
        e.cl = self.L = CL.PpcLog({addr: lab for lab, addr in self.labels.items()}, self.case["queues"])
        CUR["objs"] = dict(self.labels)
        self.saved = STUBS.activate(e)

    def done(self):
        if self.saved is not None:
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

    def call(self, op, a, b, hi):
        r3, _, _ = T.call_ppc(self.e, FN[op], [a & MASK, ((hi << 16) | b) & MASK])
        return r3 & MASK


class Host(Side):
    """The same on the host library."""
    pf, pw, f32 = "<Q", 8, "<I"
    off = HOST_OFF

    def __init__(self, lib, case):
        self.lib = lib
        self.case = case
        self.labels = {}
        bind(lib)

    def start(self):
        CL.host_begin(self.lib, dict(self.labels), self.case["queues"])
        for lab, addr in self.labels.items():
            self.lib.tpw_stdexc_label(lab, addr)

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_stdexc_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_stdexc_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, op, a, b, hi):
        return getattr(self.lib, "tpw_stdexc_" + op)(a, b)


def bind(lib):
    """The host library's entry points, typed once; the layout check runs here."""
    if getattr(lib, "bound", False):
        return
    lib.tpw_stdexc_addr.restype, lib.tpw_stdexc_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_stdexc_alloc.restype, lib.tpw_stdexc_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_stdexc_label.restype, lib.tpw_stdexc_label.argtypes = None, [C.c_int32, C.c_uint64]
    lib.tpw_stdexc_layout.argtypes = [C.POINTER(C.c_int64)]
    for op in FN:
        f = getattr(lib, "tpw_stdexc_" + op)
        f.restype, f.argtypes = C.c_uint64, [C.c_uint64, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_stdexc_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world and the ops
# Every op builder: (side, spec) -> (a, b, observe) with the objects written; observe() reads back what the call may change.


def junk(r_bytes, n):
    return bytes((r_bytes * 7 + i * 13 + 5) & 255 for i in range(n))


def op_exc_what(s, c):
    a = 0 if c["null"] else s.new(1)
    return a, 0, lambda: {}


def op_logic_what(s, c):
    a = s.new(1)
    s.new(2)
    s.wl(a + s.off["exc_rc"], c["msg"])
    return a, 0, lambda: {}


def op_length_dtor(s, c):
    if c["null"]:
        return 0, c["flag"], lambda: {}
    a = s.new(1)
    s.new(2)
    s.wp(a, 0x1234)
    s.wl(a + s.off["exc_rc"], 2)

    def observe():
        v = s.rp(a)
        name = {s.g("vtblA"): "A", s.g("vtblB"): "B", s.g("vtblC"): "C"}.get(v, "other")
        return {"vtbl": name, "msg": s.rl(a + s.off["exc_rc"])}
    return a, c["flag"], observe


def op_plain_dtor(s, c):
    """uresid_dtor / cmap_dtor: nothing is written but the Mac's stored vtable word (checked on the original only)."""
    if c["null"]:
        return 0, c["flag"], lambda: {}
    a = s.new(1)
    s.write(a, junk(c["flag"], 16))
    return a, c["flag"], lambda: {"bytes": s.read(a + 8, 8)}


def op_box_swap(s, c):
    a = s.new(1)
    s.write(a, c["bytes"] + b"\xa5" * 8)
    return a, 0, lambda: {"bytes": s.read(a, 32)}


def op_swap32(s, c):
    return c["word"], 0, lambda: {}


def op_swap_inplace(s, c):
    a = s.new(1)
    s.write(a, c["bytes"])
    return a, 0, lambda: {"bytes": s.read(a, 8)}


def op_ts_ctor(s, c):
    a = s.new(1)
    s.write(a, c["bytes"])
    return a, 0, lambda: {"bytes": s.read(a, 16)}


def op_tree_destroy(s, c):
    tree = s.new(1)
    nodes = []

    def flatten(t, parent):
        if t is None:
            return
        nodes.append((t, parent))
        flatten(t[0], t)
        flatten(t[1], t)
    flatten(c["tree"], None)
    lab = {}
    for i, (t, _) in enumerate(nodes):
        lab[id(t)] = 2 + i
        s.new(2 + i, 0x20)
    o = s.off
    for t, parent in nodes:
        a = s.at(lab[id(t)])
        s.wl(a + o["node_left"], lab[id(t[0])] if t[0] is not None else 0)
        s.wl(a + o["node_right"], lab[id(t[1])] if t[1] is not None else 0)
        s.wl(a + o["node_parent"], lab[id(parent)] if parent is not None else 0)
    return tree, s.at(2), lambda: {}


def op_list_dtor(s, c):
    if c["null"]:
        return 0, c["flag"], lambda: {}
    o = s.off
    me, tb = s.new(1), s.new(2)
    s.w32(me + o["sl_f24"], c["f24"])
    s.w32(me + o["sl_f28"], c["f28"])
    s.wl(me + o["sl_list"], 2)
    s.w32(tb + o["tb_count"], len(c["records"]))
    s.new(30)                                   # the vtable the original reads (slot +8) before its glue call, which is faked
    s.wl(tb, 30)
    for lab in c["records"]:
        if lab:
            s.new(lab)
            s.wl(s.at(lab), 30)
    rec = c["rec"]
    rec_addr = 0
    if rec is not None:
        rec_addr = s.new(3)
        s.wl(me + o["sl_rec"], 3)
        s.w32(rec_addr + o["rec_ref"], rec["ref"])
        if rec["infos"]:
            s.new(4)
            s.wl(rec_addr + o["rec_infos"], 4)

    def observe():
        d = {"f24": s.r32(me + o["sl_f24"]), "f28": s.r32(me + o["sl_f28"]), "list": s.rl(me + o["sl_list"]),
             "rec": s.rl(me + o["sl_rec"]), "count": s.r32(tb + o["tb_count"])}
        if rec_addr:
            d["ref"] = s.r32(rec_addr + o["rec_ref"])
            d["infos"] = s.rl(rec_addr + o["rec_infos"])
        return d
    return me, c["flag"], observe


def op_list_clear(s, c):
    o = s.off
    me = s.new(1)
    head = me + o["hd"]
    s.labels[SENTINEL] = head
    n = c["n"]
    nodes = [s.new(10 + i, 0x40) for i in range(n)]
    s.w32(me, c["size"])
    ring = [head] + nodes
    for i, a in enumerate(nodes):
        s.wp(a + o["sn_prev"], ring[i])
        s.wp(a + o["sn_next"], nodes[i + 1] if i + 1 < n else head)
        s.write(a + o["sn_str"], junk(i + 3, 8))
    s.wp(head + o["sn_prev"] - o["sn_prev"], nodes[-1] if n else head)
    s.wp(head + (o["str_next"] - o["str_prev"]), nodes[0] if n else head)

    def observe():
        d = {"size": s.r32(me), "prev": s.rl(head), "next": s.rl(head + (o["str_next"] - o["str_prev"]))}
        for i, a in enumerate(nodes):
            d["n%d" % i] = (s.rl(a + o["sn_prev"]), s.rl(a + o["sn_next"]))
        return d
    return me, 0, observe


def op_deinit(s, c):
    return (0 if c["null"] else s.new(1)), 0, lambda: {}


BUILD = dict(
    exc_what=op_exc_what, logic_what=op_logic_what, length_dtor=op_length_dtor, uresid_dtor=op_plain_dtor, cmap_dtor=op_plain_dtor,
    box_swap=op_box_swap, swap32=op_swap32, swap16=op_swap_inplace, swapinplace32=op_swap_inplace, ts_ctor=op_ts_ctor,
    tree_destroy=op_tree_destroy, city_dtor=op_list_dtor, park1_dtor=op_list_dtor, park2_dtor=op_list_dtor, list_clear=op_list_clear,
    deinit=op_deinit,
)
# how the answer is read: "str" the 10 bytes at it, "ptr" a label, "val" the word, "none" not compared (void)
KIND = dict(
    exc_what="str", logic_what="ptr", length_dtor="ptr", uresid_dtor="ptr", cmap_dtor="ptr", box_swap="none", swap32="val",
    swap16="none", swapinplace32="none", ts_ctor="ptr", tree_destroy="none", city_dtor="ptr", park1_dtor="ptr", park2_dtor="ptr",
    list_clear="none", deinit="none",
)


def run(s, c):
    try:
        op = c["op"]
        a, b, observe = BUILD[op](s, c)
        s.start()
        ans = s.call(op, a, b, c.get("hi", 0))
        kind = KIND[op]
        if kind == "str":
            ans = s.read(ans, 10)
        elif kind == "ptr":
            ans = s.tolabel(ans & (MASK if s.pw == 4 else (1 << 64) - 1))
        elif kind == "val":
            ans &= MASK
        else:
            ans = 0
        d = {"answer": ans, "log": s.log()}
        d.update(observe())
        if op == "cmap_dtor" and s.pw == 4 and a:
            d["vtbl_checked"] = s.r32(a) == VTBL_CMAP      # only the original stores it; the host side below answers the same
        elif op == "cmap_dtor":
            d["vtbl_checked"] = True
        return d
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

FLAGS = [0xFFFF, 0, 1, 2, 0x7FFF, 0x8000, 0xFFFE, 0x8001, 0x0100]


def case(op, **kw):
    c = {"op": op, "queues": {}}
    c.update(kw)
    return c


def flagged(r, op, f=None, null=None, **kw):
    if f is None:
        f = r.choice(FLAGS + [r.randrange(65536)])
    if null is None:
        null = r.random() < 0.15
    return case(op, flag=f, hi=r.choice([0, 0xFFFF, r.randrange(65536)]), null=null, **kw)


def flag_edges(op, **kw):
    for f in FLAGS:
        for null in (False, True):
            yield case(op, flag=f, hi=0, null=null, **kw)
            yield case(op, flag=f, hi=0xFFFF, null=null, **kw)
            yield case(op, flag=f, hi=0x7FFF, null=null, **kw)


def simple_edges(op, make):
    r = random.Random(sum(map(ord, op)))
    for i in range(12):
        yield make(r)


# ---- std::exception::what, std::logic_error::what
def exc_what_case(r):
    return case("exc_what", null=r.random() < 0.3)


def logic_what_case(r):
    return case("logic_what", msg=r.choice([0, 2]))


def length_dtor_case(r):
    return flagged(r, "length_dtor")


def uresid_case(r):
    return flagged(r, "uresid_dtor")


def cmap_case(r):
    return flagged(r, "cmap_dtor")


def rbytes(r, n):
    return bytes(r.randrange(256) for _ in range(n))


def box_case(r):
    return case("box_swap", bytes=rbytes(r, 24))


def swap32_case(r):
    return case("swap32", word=r.choice([0, MASK, 0x80000000, 0x000000FF, 0x01020304, r.getrandbits(32), r.getrandbits(32)]))


def swap_inplace_case(op):
    def make(r):
        return case(op, bytes=r.choice([b"\x00" * 8, b"\xff" * 8, b"\x01\x02\x03\x04\x05\x06\x07\x08", rbytes(r, 8), rbytes(r, 8)]))
    return make


def ts_case(r):
    return case("ts_ctor", bytes=r.choice([b"\x00" * 16, b"\xff" * 16, rbytes(r, 16), rbytes(r, 16)]))


def deinit_case(r):
    return case("deinit", null=r.random() < 0.3)


# ---- the tree
def gen_tree(r, depth, p=0.62):
    if depth == 0:
        return [None, None, 0]
    return [gen_tree(r, depth - 1, p) if r.random() < p else None, gen_tree(r, depth - 1, p) if r.random() < p else None, 0]


def tree_case(r):
    return case("tree_destroy", tree=gen_tree(r, r.randrange(0, 6), r.choice([0.4, 0.6, 0.85, 1.0])))


def full(d):
    return [full(d - 1), full(d - 1), 0] if d else [None, None, 0]


def tree_edges():
    leaf = lambda: [None, None, 0]
    shapes = [leaf(), [leaf(), None, 0], [None, leaf(), 0], [leaf(), leaf(), 0], full(2), full(3), full(4), full(5)]
    chain_l, chain_r, zig = leaf(), leaf(), leaf()
    for _ in range(5):
        chain_l, chain_r = [chain_l, None, 0], [None, chain_r, 0]
        zig = [zig, None, 0] if _ % 2 else [None, zig, 0]
    shapes += [chain_l, chain_r, zig, [[leaf(), None, 0], [None, leaf(), 0], 0], [[None, [leaf(), leaf(), 0], 0], None, 0]]
    for t in shapes:
        yield case("tree_destroy", tree=t)


# ---- the lists
def list_spec(r, op, n=None, rec="rand", f=None, null=None):
    n = r.choice([0, 0, 1, 2, 3, 4]) if n is None else n
    records = [r.choice([0, 20 + i, 20 + i]) for i in range(n)]
    if rec == "rand":
        rec = r.choice([None, {"ref": 1, "infos": True}, {"ref": 1, "infos": False}, {"ref": 2, "infos": True},
                        {"ref": 0, "infos": True}, {"ref": r.getrandbits(32), "infos": r.random() < 0.5}])
    c = flagged(r, op, f=f, null=null, records=records, rec=rec, f24=r.getrandbits(32), f28=r.getrandbits(32))
    c["queues"] = {"RemoveHead": list(records)}
    return c


def list_case(op):
    return lambda r: list_spec(r, op)


def list_edges(op):
    r = random.Random(sum(map(ord, op)))
    for f in FLAGS:
        yield list_spec(r, op, n=r.choice([0, 1, 3]), f=f, null=False)
        yield list_spec(r, op, f=f, null=True)
    for rec in (None, {"ref": 1, "infos": True}, {"ref": 1, "infos": False}, {"ref": 2, "infos": True}, {"ref": 0, "infos": True},
                {"ref": MASK, "infos": True}, {"ref": 0x80000000, "infos": True}):
        for n in (0, 1, 2, 4):
            yield list_spec(r, op, n=n, rec=rec, f=1, null=False)
    c = list_spec(r, op, n=3, rec=None, f=1, null=False)
    c["records"] = [0, 0, 0]
    c["queues"] = {"RemoveHead": [0, 0, 0]}
    yield c


# ---- std::list<std::string>::clear
def clear_case(r):
    n = r.choice([0, 1, 2, 3, 4, 4])
    return case("list_clear", n=n, size=r.choice([n, n, 0, 1, n + 1, MASK, r.getrandbits(32)]))


def clear_edges():
    for n in (0, 1, 2, 3, 4):
        for size in (n, 0, 1, 5, MASK, 0x80000000):
            yield case("list_clear", n=n, size=size)


def rnd_of(make):
    return lambda r: make(r)


def edge_of(op, make):
    return lambda: simple_edges(op, make)


def fn_test(name, op, make, edges=None):
    return FnTest(name, ppc, host, edge=edges if edges is not None else edge_of(op, make), rnd=rnd_of(make))


TESTS = [
    fn_test("std::exception::what", "exc_what", exc_what_case),
    fn_test("std::logic_error::what", "logic_what", logic_what_case),
    fn_test("std::length_error::~length_error", "length_dtor", length_dtor_case, lambda: flag_edges("length_dtor")),
    fn_test("SamsUtilities::UResourceId::~UResourceId", "uresid_dtor", uresid_case, lambda: flag_edges("uresid_dtor")),
    fn_test("CMapLinkBase::~CMapLinkBase", "cmap_dtor", cmap_case, lambda: flag_edges("cmap_dtor")),
    fn_test("sAlignedBox::Byteswap", "box_swap", box_case),
    fn_test("SamsUtilities::USwap32<unsigned_char*>", "swap32", swap32_case),
    fn_test("SamsUtilities::USwapInplace16<unsigned_short>", "swap16", swap_inplace_case("swap16")),
    fn_test("SamsUtilities::USwapInplace32<unsigned_long>", "swapinplace32", swap_inplace_case("swapinplace32")),
    fn_test("TbTimeStamp::TbTimeStamp", "ts_ctor", ts_case),
    fn_test("std::__tree<unsigned_short,std::less<unsigned_short>,std::allocator<unsigned_short>>::destroy", "tree_destroy", tree_case,
            tree_edges),
    fn_test("CFSOnlineSystemList<TbPtrList<CFSOnlineCity>,ccCityList>::~CFSOnlineSystemList", "city_dtor", list_case("city_dtor"),
            lambda: list_edges("city_dtor")),
    fn_test("CFSOnlineSystemParkList_InWorldByParkID::~CFSOnlineSystemParkList_InWorldByParkID", "park1_dtor", list_case("park1_dtor"),
            lambda: list_edges("park1_dtor")),
    fn_test("CFSOnlineSystemParkList_InCityTop10::~CFSOnlineSystemParkList_InCityTop10", "park2_dtor", list_case("park2_dtor"),
            lambda: list_edges("park2_dtor")),
    fn_test("std::list<std::basic_string<char,std::char_traits<char>,std::allocator<char>>,"
            "std::allocator<std::basic_string<char,std::char_traits<char>,std::allocator<char>>>>::clear", "list_clear", clear_case,
            clear_edges),
    fn_test("FeatureStatusPanel::DeInitViewWindow", "deinit", deinit_case),
]
