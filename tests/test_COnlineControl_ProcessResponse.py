"""Unit v5638, the chat server's responses, against libtpw_unit_COnlineControl_ProcessResponse.dylib
(tests/unit_COnlineControl_ProcessResponse.cpp), main app ("SimTheme Park"),
src/COnlineControl_COnlineControl_COnlineControl_Locate_COnlineControl_WantToShutDown_COnlineControl_ProcessResponse.cpp:
  COnlineControl::ProcessResponse 0x10215560

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. Every callee is a
fake on both sides: the string members (BStrVal, StrVal, the copies, the constructor from the buffer, the destructors) build MSL reps in
the emulator and std::u16string objects on the host, and record their texts; the tree finds answer the node or the end node from the
case's queues; the COnlineControl members, the UI updates, FormatString (with each tag's magic, vtable, index and text), CTRACE, UDebug,
the peep members, SCR_SetFloatVariable, operator new and the online system's members record their arguments.

The world of a case: the COnlineControl object (the own avatar word at +4, a handle on the host; the goto flag byte at +0xe84), six
Arguments (char string index, wide string index, number), the vector (capacity, size, data), gText's table pointer, gOnlineSystem (NULL or
an existing system), two person blocks (MapPos bytes at +4/+6, the script word at +0xc), the buddy, ignore and contact nodes, the park info
(its id word at +8). What is compared: the whole object (the +4 handle aside), the buffer at +0x220 as UTF-16 units, the flag bytes at
+0xe84, the node and person blocks, gOnlineSystem (as a label) and the event log.

No fused operations. Not tested: Arguments past the sixth (no case reads them) and texts outside ASCII (the host log narrows them).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_COnlineControl_ProcessResponse"
MASK = 0xFFFFFFFF

# ---- the unit's own function
FN = 0x10215560
OWN = {FN}

# ---- the Mac's addresses of the globals
ADDR = dict(gText=0x1037ff18, gOnlineSystem=0x10380ab0, ui=0x10396440, vt=0x103bcb34, callback=0x10268988)
# ---- the host's sizes and offsets the module relies on (tpw_pr_layout)
HOST_LAYOUT = [0xE8C, 12, 4, 8, 4, 8]

OBJ_SIZE = 0xE8C
N_ARGS = 6
VALID = [11, 12, 15, 16, 17, 18, 19, 22, 24, 26, 28, 30, 31, 34, 35, 36, 37, 38, 39, 42, 46, 47, 56, 57, 58, 59, 60, 61, 63, 64,
         65, 66, 68, 69, 71, 79, 81, 82]
TAGS = {0x58: 2, 0x5d: 2, 0x59: 2, 0x5b: 2, 0x5a: 2, 0x5c: 2, 0x98: 3, 0x68: 0, 0x60: 0, 0x61: 0, 0x79: 0, 0x7a: 0, 0x5f: 0}
WIDE = ["alice", "Bob", "", "carol99", "Dave Smith", "x" * 60, "Eve", "frank_the_tank"]
CHARS = ["123", "-45", "  7x", "", "abc", "2147483647", "-2147483648", "0"]


def s32(v):
    return T.s32(v)


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def c_atoi(text):
    """C atoi on the texts the module uses (in int range)."""
    i, n = 0, len(text)
    while i < n and text[i] in " \t\n\v\f\r":
        i += 1
    sign = 1
    if i < n and text[i] in "+-":
        sign = -1 if text[i] == "-" else 1
        i += 1
    v = 0
    while i < n and text[i].isdigit():
        v = v * 10 + int(text[i])
        i += 1
    return sign * v


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)
CUR = {}   # the running case's texts and blocks (PowerPC side)


def u16z(e, a):
    out = []
    while a and len(out) < 0x400:
        c = struct.unpack(">H", e.read(a, 2))[0]
        if c == 0:
            break
        out.append(chr(c))
        a += 2
    return "".join(out)


def wtext(e, slot):
    rep = e.u32(slot)
    if rep == 0:
        return ""
    return e.read(e.u32(rep + 0xC), 2 * e.u32(rep)).decode("utf-16-be")


def make_wrep(e, text):
    data = text.encode("utf-16-be") + b"\0\0"
    rep = e.alloc(16 + len(data))
    e.write(rep, struct.pack(">IIII", len(text), len(text), 0, rep + 16) + data)
    return rep


def wide_of(index):
    return CUR["wide"][index] if index < len(CUR["wide"]) else ""


def chars_of(index):
    return CUR["chars"][index] if index < len(CUR["chars"]) else ""


def st(addr, fn):
    STUBS.internal(addr, fn)


def glue(addr, fn):
    """A glue import faked at its address: the glue saves the TOC at 0x14(r1), which the caller reloads after the call."""
    def wrapped(e):
        e.write((e.r(1) + 0x14) & MASK, struct.pack(">I", e.r(2) & MASK))
        fn(e)
    STUBS.internal(addr, wrapped)


def stub_bstrval(e):
    L = e.cl
    text = wide_of(e.u32(e.r(4) + 4))
    e.write(e.r(3), struct.pack(">I", make_wrep(e, text)))
    L.rec("BStrVal", (L.newlab(e.r(3)), L.lab(e.r(4))), (text,))


def stub_strval(e):
    L = e.cl
    text = chars_of(e.u32(e.r(4)))
    data = text.encode("latin-1") + b"\0"
    rep = e.alloc(16 + len(data))
    e.write(rep, struct.pack(">IIII", len(text), len(text), 0, rep + 16) + data)
    e.write(e.r(3), struct.pack(">I", rep))
    L.rec("StrVal", (L.newlab(e.r(3)), L.lab(e.r(4))), (text,))


def copy_stub(name):
    def fn(e):
        L = e.cl
        text = wtext(e, e.r(4))
        e.write(e.r(3), struct.pack(">I", make_wrep(e, text)))
        L.rec(name, (L.newlab(e.r(3)), L.lab(e.r(4)), e.r(5) & MASK, e.r(6) & MASK), (text,))
    return fn


def dtor_stub(name):
    def fn(e):
        L = e.cl
        L.rec(name, (L.lab(e.r(3)), T.s16(e.r(4))), (wtext(e, e.r(3)),))
    return fn


def stub_wctor(e):
    L = e.cl
    text = u16z(e, e.r(4))
    e.write(e.r(3), struct.pack(">I", make_wrep(e, text)))
    L.rec("WCtor", (L.newlab(e.r(3)), L.lab(e.r(4))), (text,))


def stub_sdtor(e):
    L = e.cl
    L.rec("SDtor", (L.lab(e.r(3)), T.s16(e.r(4))))


def stub_wassign(e):
    L = e.cl
    L.rec("WAssign", (L.lab(e.r(3)), L.lab(e.r(4)), e.r(5) & MASK, e.r(6) & MASK), (wtext(e, e.r(4)),))


def stub_wequal(e):
    L = e.cl
    L.rec("WEqual", (L.lab(e.r(3)), L.lab(e.r(4))), (wtext(e, e.r(3)),))
    e.set_r(3, L.ret("WEqual") & MASK)


def find_stub(name, node_key):
    def fn(e):
        L = e.cl
        L.rec(name, (L.lab(e.r(4)), L.lab(e.r(5))), (wtext(e, e.r(5)),))
        node = CUR[node_key] if L.ret(name) == 1 else (e.r(4) + 4) & MASK
        e.write(e.r(3), struct.pack(">I", node))
    return fn


def stub_ninsert(e):
    L = e.cl
    L.rec("NInsert", (L.lab(e.r(4)), L.lab(e.r(5))), (wtext(e, e.r(5)),))


def stub_nerase(e):
    L = e.cl
    L.rec("NErase", (L.lab(e.r(3)), L.lab(e.r(4))))


def ui_stub(name):
    def fn(e):
        L = e.cl
        L.rec(name, (L.lab(e.r(3)), s32(e.r(5))), (u16z(e, e.r(4)),))
    return fn


def stub_earmuffs(e):
    e.cl.rec("Earmuffs", (1 if e.r(3) & 0xFF else 0,))


def name_stub(name, extra=0, answer=None):
    """A COnlineControl member taking (this, the name string, `extra` ints)."""
    def fn(e):
        L = e.cl
        ints = (L.lab(e.r(3)), L.lab(e.r(4))) + tuple(s32(e.r(5 + i)) for i in range(extra))
        L.rec(name, ints, (wtext(e, e.r(4)),))
        if answer is not None:
            e.set_r(3, answer(e, L) & MASK)
    return fn


def person_answer(e, L):
    k = L.ret("GetPerson")
    return CUR["person"] if k == 1 else CUR["other"] if k == 2 else 0


def stub_this_only(name):
    def fn(e):
        e.cl.rec(name, (e.cl.lab(e.r(3)),))
    return fn


def stub_thingid(e):
    L = e.cl
    L.rec("GetThingId", (L.lab(e.r(4)),))
    e.write(e.r(3), struct.pack(">H", L.ret("ThingId") & 0xFFFF))


def pos_stub(name):
    def fn(e):
        L = e.cl
        L.rec(name, (L.lab(e.r(3)), (e.r(4) >> 16) & 0xFFFF, (e.r(5) >> 16) & 0xFFFF, (e.r(6) >> 16) & 0xFFFF))
        e.set_r(3, 0)
    return fn


def stub_teleport_avatar(e):
    L = e.cl
    L.rec("TeleportAvatar", (L.lab(e.r(3)), (e.r(4) >> 16) & 0xFFFF))


def stub_ctrace(e):
    e.cl.rec("CTRACE", (e.r(3) & MASK, s32(e.r(5)), s32(e.r(6))), (e.cstr(e.r(4)),))


def stub_udebug(e):
    e.cl.rec("UDebug", (), (e.cstr(e.r(3)),))


def stub_hide(e):
    e.cl.rec("Hide", (e.cl.lab(e.r(3)),))


def stub_draw(e):
    L = e.cl
    L.rec("Draw", (L.lab(e.r(3)), fb(e.f(1)), fb(e.f(2)), fb(e.f(3)), s32(e.r(7)), s32(e.r(8))))


def stub_scr(e):
    e.cl.rec("SCRSetF", (s32(e.r(3)), e.r(4) & MASK, fb(e.f(1))))


def stub_format(e):
    L = e.cl
    ident = e.r(6) & MASK
    L.rec("Format", (L.lab(e.r(3)), L.lab(e.r(4)), e.r(5) & MASK, ident))
    for i in range(TAGS.get(ident, 1)):
        t = e.r(7 + i)
        L.rec("Tag", (e.u32(t), L.lab(e.u32(t + 4)), e.u32(t + 8)), (u16z(e, e.u32(t + 12)),))
    e.write(e.r(4), ("F%x" % ident).encode("utf-16-be") + b"\0\0")
    e.set_r(3, 0)


def stub_atoi(e):
    e.set_r(3, c_atoi(e.cstr(e.r(3))) & MASK)


def stub_new(e):
    L = e.cl
    size = e.r(3) & MASK
    a = e.alloc(max(size, 4))
    L.rec("new", (size, L.newlab(a)))
    e.set_r(3, a)


def stub_parkinfo(e):
    e.cl.rec("ParkInfo", (e.cl.lab(e.r(3)),))
    e.set_r(3, CUR["info"])


def stub_gotoreq(e):
    L = e.cl
    L.rec("GotoReq", (L.lab(e.r(3)), s32(e.r(4)), L.lab(e.r(5)), L.lab(e.r(6))))


st(0x10217764, stub_bstrval)
st(0x10217684, stub_strval)
st(0x1021f130, stub_wequal)
st(0x101468f8, dtor_stub("WDtor"))
st(0x101f7424, copy_stub("WCopy"))
st(0x10146a14, stub_wctor)
st(0x1022213c, copy_stub("NCopy"))
st(0x10222020, dtor_stub("NDtor"))
st(0x1000221c, stub_sdtor)
st(0x101f7014, stub_wassign)
st(0x1021f8d8, find_stub("BuddyFind", "bnode"))
st(0x1021fae4, find_stub("WSetFind", "cnode"))
st(0x1021fd4c, find_stub("NSetFind", "inode"))
st(0x102243c0, stub_ninsert)
st(0x1022419c, stub_nerase)
st(0x101f0bc8, ui_stub("UIBuddy"))
st(0x101f0aa0, ui_stub("UIContact"))
st(0x101e6168, stub_earmuffs)
st(0x102182c0, name_stub("CreateVisitor", 4))
st(0x1021b950, name_stub("AddContact", 1))
st(0x1021c0ec, stub_this_only("UpdBuddyList"))
st(0x1021c190, stub_this_only("UpdIgnoreList"))
st(0x1021b784, name_stub("Feedback"))
st(0x10217dbc, name_stub("ReceiveText", 1))
st(0x1021b854, name_stub("GetData", 0, lambda e, L: L.ret("GetData")))
st(0x102183f4, name_stub("DestroyVisitor"))
st(0x1021ba28, name_stub("RemoveContact"))
st(0x1021baf8, name_stub("AddBuddy", 1, lambda e, L: L.ret("AddBuddy")))
st(0x1021bdcc, name_stub("RemoveBuddy"))
st(0x1021969c, name_stub("GetPerson", 0, person_answer))
st(0x1012e1f8, stub_thingid)
st(0x10219908, pos_stub("SetDest"))
st(0x10219af4, pos_stub("Teleport"))
st(0x1021802c, stub_teleport_avatar)
st(0x1006fcbc, stub_ctrace)
st(0x10000e40, stub_udebug)
st(0x10114758, stub_hide)
st(0x10114624, stub_draw)
st(0x100d95a0, stub_scr)
glue(0x10239730, stub_format)
glue(0x10238590, stub_atoi)
glue(0x102384b8, stub_new)
st(0x1016f7d0, stub_this_only("SysCtor"))
st(0x10177654, stub_parkinfo)
st(0x10176074, stub_gotoreq)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.case = case
        self.saved = None

    def begin(self, W):
        """The globals, the own avatar word and the log."""
        e, case = self.e, self.case
        CUR.clear()
        CUR.update(wide=case["wide"], chars=case["chars"], **W)
        e.write(ADDR["gText"] + 0x598, struct.pack(">I", W["table"]))
        e.write(ADDR["gOnlineSystem"], struct.pack(">I", W["sys"]))
        me = [0, W["person"], W["other"]][case["me"]]
        self.words(W["this"] + 4, [me])
        vt = e.u32(0x10261348)   # the TOC slot of the import __vt__15TbUniStringVTag, as the loader resolved it
        fixed = {a: l for l, a in labels(W, ADDR["ui"], vt, ADDR["callback"]).items()}
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        if self.saved is not None:
            CL.Stubs.deactivate(self.e, self.saved)

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def sys_label(self):
        return self.L.lab(self.e.u32(ADDR["gOnlineSystem"]))

    def log(self):
        return self.L.log

    def call(self, W, case):
        vec = self.e.alloc(12)
        self.words(vec, [case["cap"], case["size"], W["args"]])
        T.call_ppc(self.e, FN, [W["this"], vec])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        self.case = case
        bind(lib)

    def begin(self, W):
        lib, case = self.lib, self.case
        for i in range(16):
            lib.tpw_pr_set_text(0, i, (case["wide"][i] if i < len(case["wide"]) else "").encode())
            lib.tpw_pr_set_text(1, i, (case["chars"][i] if i < len(case["chars"]) else "").encode())
        for i, k in enumerate(["", "person", "other", "bnode", "inode", "cnode", "info"]):
            lib.tpw_pr_set_ptr(i, W[k] if k else 0)
        lib.tpw_pr_set_world(W["table"], W["sys"])
        self.words(W["this"] + 4, [case["me"]])
        g = lambda n: lib.tpw_pr_addr(n.encode())
        CL.host_begin(lib, labels(W, g("ui"), g("vt"), g("callback")), case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_pr_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def sys_label(self):
        return self.lib.tpw_pr_system_label()

    def log(self):
        return CL.host_end(self.lib)

    def call(self, W, case):
        self.lib.tpw_pr_process(W["this"], case["cap"] & MASK, case["size"] & MASK, W["args"])


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_pr_addr.restype, lib.tpw_pr_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_pr_alloc.restype, lib.tpw_pr_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_pr_layout.restype, lib.tpw_pr_layout.argtypes = C.c_int32, [C.POINTER(C.c_int64)]
    lib.tpw_pr_set_text.restype, lib.tpw_pr_set_text.argtypes = None, [C.c_int32, C.c_int32, C.c_char_p]
    lib.tpw_pr_set_ptr.restype, lib.tpw_pr_set_ptr.argtypes = None, [C.c_int32, C.c_uint64]
    lib.tpw_pr_set_world.restype, lib.tpw_pr_set_world.argtypes = None, [C.c_uint64, C.c_uint64]
    lib.tpw_pr_system_label.restype, lib.tpw_pr_system_label.argtypes = C.c_int64, []
    lib.tpw_pr_process.restype = None
    lib.tpw_pr_process.argtypes = [C.c_uint64, C.c_uint32, C.c_uint32, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_pr_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def labels(W, ui, vt, callback):
    """{label: address} of every object the log names."""
    t = W["this"]
    out = {1: t, 2: t + 8, 3: t + 0x220, 4: t + 0xE24, 5: t + 0xE28, 6: t + 0xE34, 7: t + 0xE44, 8: W["table"], 9: ui, 10: vt,
           11: callback, 12: W["person"], 13: W["bnode"], 14: t + 0xE2C, 15: t + 0xE38, 16: t + 0xE48, 17: W["inode"],
           18: W["cnode"], 19: W["info"], 31: W["other"]}
    for i in range(N_ARGS):
        out[20 + i] = W["args"] + 12 * i
    if W["sys"]:
        out[30] = W["sys"]
    return out


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    W = {}
    W["this"] = s.alloc(OBJ_SIZE)
    s.write(W["this"], bytes(OBJ_SIZE))
    s.write(W["this"] + 0xE84, bytes([case["e84"], 0, 0, 0]))
    for k in ("table", "bnode", "inode", "cnode", "info"):
        W[k] = s.alloc(0x20)
    for k in ("person", "other"):
        W[k] = s.alloc(0x10)
        s.write(W[k], bytes(case[k + "_bytes"]))
        s.words(W[k] + 0xC, [case[k + "_scr"]])
    for k in ("bnode", "inode", "cnode"):
        s.write(W[k], bytes(case["nodes"][k]))
    s.words(W["info"] + 8, [case["park"]])
    W["args"] = s.alloc(12 * N_ARGS)
    for i, (sw, ww, cmd) in enumerate(case["args"]):
        s.words(W["args"] + 12 * i, [sw, ww, cmd])
    W["sys"] = s.alloc(0x404) if case["sys"] else 0
    s.begin(W)
    return W


def observe(s, W):
    t = W["this"]
    words = s.rd_words(t, OBJ_SIZE // 4)
    obj = [w for i, w in enumerate(words) if i != 1 and not (0x220 // 4 <= i < 0xA20 // 4) and i != 0xE84 // 4]
    out = {
        "obj": obj,
        "buf": list(struct.unpack(s.fmt[0] + "1024H", s.read(t + 0x220, 0x800))),
        "e84": s.read(t + 0xE84, 4),
        "nodes": [s.read(W[k], 0x20) for k in ("bnode", "inode", "cnode")],
        "people": [s.read(W[k], 0xC) for k in ("person", "other")],
        "scr": [s.rd_words(W[k] + 0xC, 1)[0] for k in ("person", "other")],
        "info": s.rd_words(W["info"], 8),
        "sys": s.sys_label(),
    }
    out["log"] = s.log()
    return out


def run(s, case):
    try:
        W = build(s, case)
        s.call(W, case)
        return observe(s, W)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

XY = [0, 1, 255, 256, 257, -1, -255, -256, -257, 511, 0x7FFFFFFF, -0x80000000, 0x12345]


def rnum(r):
    k = r.random()
    if k < 0.3:
        return r.choice([0, 1, 2, 3, 4, -1])
    if k < 0.6:
        return r.choice(XY)
    return r.getrandbits(32)


def base(r, cmd):
    queues = {
        "BuddyFind": [r.choice([0, 1])], "WSetFind": [r.choice([0, 1])], "NSetFind": [r.choice([0, 1])],
        "WEqual": [r.choice([0, 1, 0x100])], "GetPerson": [r.choice([0, 1, 1, 2])], "AddBuddy": [r.choice([0, 1, 0x100])],
        "GetData": [r.getrandbits(32)], "ThingId": [r.getrandbits(16)],
    }
    args = [[r.randrange(len(CHARS)), r.randrange(len(WIDE)), rnum(r) & MASK] for _ in range(N_ARGS)]
    args[0][2] = cmd & MASK
    c = {
        "queues": queues, "args": args, "size": r.choice([0, 1, 2, 3, 4, 6, 0xFFFFFFFF]), "cap": 8,
        "wide": list(WIDE), "chars": list(CHARS), "me": r.choice([0, 0, 1, 2]), "e84": r.choice([0, 0, 1, 2]),
        "sys": r.choice([0, 1]), "nodes": {k: [r.getrandbits(8) for _ in range(0x20)] for k in ("bnode", "inode", "cnode")},
        "person_bytes": [r.getrandbits(8) for _ in range(0x10)], "other_bytes": [r.getrandbits(8) for _ in range(0x10)],
        "person_scr": r.getrandbits(32), "other_scr": r.getrandbits(32),
    }
    park = c_atoi(CHARS[args[4][0]]) & MASK
    c["park"] = park if r.random() < 0.5 else (park + r.choice([1, -1, 0x10000])) & MASK
    return c


def edges():
    r = random.Random(5638)
    for cmd in list(range(0, 0x56)) + [0xFFFFFFFF, 0x80000000, 0x7FFFFFFF, 0x100, 0x5B]:
        yield base(r, cmd)
    for size in (0, 1, 2, 3, 0xFFFFFFFF):
        for n in (0, 1, 0x100):
            for cmd in (15, 16):
                c = base(r, cmd)
                c["size"] = size
                c["args"][1][2] = n
                yield c
    for size in (0, 1, 2, 0xFFFFFFFF):
        c = base(r, 42)
        c["size"] = size
        yield c
    for size in (2, 3, 0xFFFFFFFF):
        for n in (0, 1):
            for found in (0, 1):
                for answer in (0, 1, 0x100):
                    c = base(r, 56)
                    c["size"] = size
                    c["args"][2][2] = n
                    c["queues"]["WSetFind"] = [found]
                    c["queues"]["AddBuddy"] = [answer]
                    yield c
    for person in (0, 1, 2):
        for me in (0, 1, 2):
            for z in (0, 1, 2, 3, 4, -1):
                c = base(r, 34)
                c["queues"]["GetPerson"] = [person]
                c["me"] = me
                c["args"][4][2] = z & MASK
                yield c
    for x in XY:
        for z in (0, 1, 3):
            c = base(r, 34)
            c["queues"]["GetPerson"] = [1]
            c["me"] = 0
            c["args"][2][2] = x & MASK
            c["args"][3][2] = (-x - 1) & MASK
            c["args"][4][2] = z
            yield c
    for cmd in (35, 36, 37, 38):
        for b in (0, 1):
            for w in (0, 1):
                c = base(r, cmd)
                c["queues"]["BuddyFind"] = [b]
                c["queues"]["WSetFind"] = [w]
                yield c
    for eq in (0, 1, 0x100, 0x1FE):
        c = base(r, 39)
        c["queues"]["WEqual"] = [eq]
        yield c
    for cmd in (58, 59):
        for f in (0, 1):
            c = base(r, cmd)
            c["queues"]["NSetFind"] = [f]
            yield c
    for e84 in (0, 1, 2, 0xFF):
        for sys in (0, 1):
            for same in (0, 1):
                for person in (0, 1):
                    for ci in range(len(CHARS)):
                        c = base(r, 82)
                        c["e84"], c["sys"] = e84, sys
                        c["args"][4][0] = ci
                        park = c_atoi(CHARS[ci]) & MASK
                        c["park"] = park if same else (park ^ 1)
                        c["queues"]["GetPerson"] = [person]
                        yield c
    for cmd in VALID:
        for wi in range(len(WIDE)):
            c = base(r, cmd)
            for a in c["args"]:
                a[1] = wi
            yield c


def rand_case(r):
    k = r.random()
    if k < 0.85:
        cmd = r.choice(VALID)
    elif k < 0.95:
        cmd = r.randrange(0, 0x60)
    else:
        cmd = r.getrandbits(32)
    return base(r, cmd)


TESTS = [FnTest("COnlineControl::ProcessResponse", ppc, host, edge=edges, rnd=rand_case)]
