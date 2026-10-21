"""Unit v5285, COnlineControl's avatar moves, chat text and contact lookups, against libtpw_unit_COnlineControl_CreateVisitor.dylib
(tests/unit_COnlineControl_CreateVisitor.cpp), main app ("SimTheme Park"),
src/game/online/online.cpp:
  CreateVisitor 0x102182c0, SetAvatarDestination 0x10218140, TeleportAvatar 0x1021802c, IsThisMe 0x10217f34, ReceiveText 0x10217dbc,
  Feedback 0x1021b784, GetDataForUser 0x1021b854, AddContact 0x1021b950.

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside
the unit (GetThingId, SetDestination, Teleport, NoLongerAFK, Chat::MovePlayer, operator new, the COnlinePerson constructor, GetParkEntrancePos,
the master save constructor and IsSwearFilterOn, SwearFilter::Filter, the UI calls, the string constructors, copies and destructors, the
nocase compare, the two tree finds and the set insert) are fakes on both sides that record their arguments (texts as length and hash) and
answer from the case's queues. Operator new answers one preallocated block, so the objects the code makes are named in the log.

The world of a case: a COnlineControl block (chat word at 0, peep word at 4), the world and master save globals, the text objects (a Mac
string is a word to a rep whose word at 0xc is the character pointer; the host uses a std::u16string), the two find nodes with their flag
bytes. What is compared: the answer, the event log, the master save global (empty, the old one or the new block) and the rest of the object.

No fused operations. Not tested: nothing the original cannot take.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_COnlineControl_CreateVisitor"
MASK = 0xFFFFFFFF

F_CREATE, F_SETDEST, F_TELE, F_ISME, F_RECV, F_FEED, F_GETDATA, F_ADD = (
    0x102182C0, 0x10218140, 0x1021802C, 0x10217F34, 0x10217DBC, 0x1021B784, 0x1021B854, 0x1021B950)
SIZES = {F_CREATE: 184, F_SETDEST: 148, F_TELE: 180, F_ISME: 200, F_RECV: 232, F_FEED: 92, F_GETDATA: 180, F_ADD: 96}
OWN = set(SIZES)

ADDR = dict(world=0x1037EBFC, gms=0x10380A8C, ui=0x10396440)
HOST_LAYOUT = [0xE8C]
OBJ_SIZE = 0xE90


def s32(v):
    return T.s32(v)


def th(chars):
    h = 0
    for c in chars:
        h = (h * 31 + c) & MASK
    return h


def tinfo(chars):
    return (len(chars), th(chars))


def wtext_at(e, a):
    out = []
    for i in range(200):
        c = struct.unpack(">H", e.read(a + 2 * i, 2))[0]
        if c == 0:
            break
        out.append(c)
    return out


def strobj_text(e, so):
    rep = e.u32(so)
    return wtext_at(e, e.u32(rep + 0xC))


def ppc_mkstr(e, chars):
    data = e.alloc(2 * len(chars) + 2, b"".join(struct.pack(">H", c) for c in chars) + b"\0\0")
    rep = e.alloc(0x10, struct.pack(">IIII", len(chars), len(chars) + 1, 1, data))
    return e.alloc(4, struct.pack(">I", rep))


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def internal(addr):
    def deco(fn):
        STUBS.internal(addr, fn)
        return fn
    return deco


@internal(0x1012E1F8)
def st_get_thing_id(e):
    L = e.cl
    L.rec("GetThingId", (L.lab(e.r(4)),))
    e.write(e.r(3), struct.pack(">H", L.ret("GetThingId") & 0xFFFF))


@internal(0x10101AE0)
def st_park_entrance(e):
    L = e.cl
    L.rec("GetParkEntrancePos", (L.lab(e.r(4)), s32(e.r(5))))
    e.write(e.r(3), struct.pack(">H", L.ret("GetParkEntrancePos") & 0xFFFF))


@internal(0x102197EC)
def st_set_destination(e):
    L = e.cl
    L.rec("SetDestination", (L.lab(e.r(3)), e.r(4) >> 16, e.r(5) >> 16))
    e.set_r(3, 1 if L.ret("SetDestination") else 0)


@internal(0x10219A10)
def st_teleport(e):
    L = e.cl
    L.rec("Teleport", (L.lab(e.r(3)), e.r(4) >> 16, e.r(5) >> 16))
    e.set_r(3, 1 if L.ret("Teleport") else 0)


@internal(0x10219634)
def st_no_longer_afk(e):
    L = e.cl
    L.rec("NoLongerAFK", (L.lab(e.r(3)), e.r(4) & 0xFF))


def imp(name):
    def deco(fn):
        STUBS.imp(name, fn)
        return fn
    return deco


@imp("MovePlayer__4ChatFiiiPv")
def st_move_player(e):
    L = e.cl
    L.rec("MovePlayer", (L.lab(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6)), L.lab(e.r(7))))


@imp("__nw__FUl")
def st_new(e):
    L = e.cl
    L.rec("OperatorNew", (e.r(3),))
    e.set_r(3, L.ret("new") & MASK)


@internal(0x10212D98)
def st_person_ctor(e):
    L = e.cl
    L.rec("PersonCtor", (L.lab(e.r(3)), e.r(4) >> 16, e.r(5), L.lab(e.r(6)), e.r(7) & 0xFF))


@internal(0x101694DC)
def st_master_ctor(e):
    L = e.cl
    L.rec("MasterSaveCtor", (L.lab(e.r(3)),))


@internal(0x1016B438)
def st_swear_on(e):
    L = e.cl
    L.rec("IsSwearFilterOn", (L.lab(e.r(3)),))
    e.set_r(3, 1 if L.ret("IsSwearFilterOn") else 0)


@internal(0x1020D2CC)
def st_filter(e):
    L = e.cl
    L.rec("Filter", (L.lab(e.r(3)), L.lab(e.r(4))) + tinfo(strobj_text(e, e.r(4))) + (e.r(5) & 0xFF,))
    e.set_r(3, 1 if L.ret("Filter") else 0)


@internal(0x101F0978)
def st_receive_str(e):
    L = e.cl
    L.rec("OnlineReceiveStr", (L.lab(e.r(3)),) + tinfo(wtext_at(e, e.r(4))) + (s32(e.r(5)),))


@internal(0x101F09DC)
def st_add_contact(e):
    L = e.cl
    L.rec("OnlineAddContact", (L.lab(e.r(3)),) + tinfo(wtext_at(e, e.r(4))) + (s32(e.r(5)),))


@internal(0x10146A14)
def st_wstring_ctor_from(e):
    L = e.cl
    L.rec("WStringCtorFrom", tinfo(wtext_at(e, e.r(4))))


@internal(0x101468F8)
def st_wstring_dtor(e):
    L = e.cl
    L.rec("WStringDtor", (T.s16(e.r(4)),))


@internal(0x101F7424)
def st_wstring_copy(e):
    L = e.cl
    src = strobj_text(e, e.r(4))
    pos, count = e.r(5), e.r(6)
    L.rec("WStringCopy", (L.lab(e.r(4)), pos, count) + tinfo(src))
    new = ppc_mkstr(e, src[pos:pos + count])
    e.write(e.r(3), e.read(new, 4))


@internal(0x1022213C)
def st_nocase_copy(e):
    L = e.cl
    L.rec("NocaseWStringCopy", (L.lab(e.r(4)), e.r(5), e.r(6)))


@internal(0x10222020)
def st_nocase_dtor(e):
    L = e.cl
    L.rec("NocaseWStringDtor", (T.s16(e.r(4)),))


@internal(0x1021F254)
def st_nocase_eq(e):
    L = e.cl
    L.rec("NocaseEq", (L.lab(e.r(3)), L.lab(e.r(4))))
    e.set_r(3, 1 if L.ret("NocaseEq") else 0)


@internal(0x1021F8D8)
def st_buddy_find(e):
    L = e.cl
    L.rec("BuddyFind", (L.lab(e.r(4)), L.lab(e.r(5))))
    e.write(e.r(3), struct.pack(">I", L.ret("BuddyFind") & MASK))


@internal(0x1021FD4C)
def st_nocase_find(e):
    L = e.cl
    L.rec("NocaseFind", (L.lab(e.r(4)), L.lab(e.r(5))))
    e.write(e.r(3), struct.pack(">I", L.ret("NocaseFind") & MASK))


@internal(0x10223A24)
def st_set_insert(e):
    L = e.cl
    L.rec("SetInsert", (L.lab(e.r(4)), L.lab(e.r(5))) + tinfo(strobj_text(e, e.r(5))))


def setup(runner):
    pass


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def wbuf(self, chars):
        a = self.alloc(2 * len(chars) + 2)
        self.write(a, b"".join(struct.pack(self.fmt[0] + "H", c) for c in chars) + b"\0\0")
        return a

    def make_objects(self, case):
        """Every object the log names, allocated before the log starts: the same order on both sides."""
        o = self.o = {}
        o["obj"] = self.alloc(OBJ_SIZE)
        o["chat"] = self.alloc(0x40)
        o["thing"] = self.alloc(0x40)
        o["ms0"] = self.alloc(0x50)
        o["newobj"] = self.alloc(0x3B0)
        o["key"] = self.alloc(0x10)
        o["n0"] = self.alloc(0x20)
        o["n1"] = self.alloc(0x20)
        o["world"] = self.alloc(0x400)
        o["str"] = self.mkstr(case["text"])
        o["buf"] = self.wbuf(case["text"])

    def label_map(self):
        o = self.o
        return {1: o["obj"], 2: o["chat"], 3: o["thing"], 4: self.g("ui"), 5: o["ms0"], 6: o["newobj"], 7: o["str"], 8: o["key"],
                9: o["world"] + 0x2D8, 10: o["obj"] + 8, 11: o["obj"] + 0xE28, 12: o["obj"] + 0xE34, 13: o["obj"] + 0xE38,
                14: o["obj"] + 0xE44, 15: o["obj"] + 0xE48, 16: o["n0"], 17: o["n1"]}

    def queues(self, case):
        o = self.o
        q = {k: list(v) for k, v in case["queues"].items()}
        q["new"] = [o["newobj"]]
        q["BuddyFind"] = [o["obj"] + 0xE38 if case["f0"] == 0 else o["n0"]]
        q["NocaseFind"] = [o["obj"] + 0xE48 if case["f1"] == 0 else o["n1"]]
        return q


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.make_objects(case)
        fixed = {a: l for l, a in self.label_map().items()}
        self.L = e.cl = CL.PpcLog(fixed, self.queues(case))
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def ptrword(self, p):
        return p

    def putptr(self, a, p):
        self.words(a, [p])

    def getptr(self, a):
        return self.rd_words(a, 1)[0]

    def g(self, name):
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n, bytes(n))

    def mkstr(self, chars):
        return ppc_mkstr(self.e, chars)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, entry, args):
        r3, _, _ = T.call_ppc(self.e, entry, [a & MASK for a in args])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.make_objects(case)
        CL.host_begin(lib, self.label_map(), self.queues(case))

    def done(self):
        pass

    def ptrword(self, p):
        return self.lib.tpw_cv_handle(p)

    def putptr(self, a, p):
        self.write(a, struct.pack("<Q", p))

    def getptr(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0]

    def g(self, name):
        return self.lib.tpw_cv_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_cv_alloc(n)

    def mkstr(self, chars):
        arr = (C.c_uint16 * (len(chars) + 1))(*chars)
        return self.lib.tpw_cv_mkstr(arr, len(chars))

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)


HOST_FN = {}


def bind(lib):
    if getattr(lib, "bound", False):
        return
    U64, U32, I32 = C.c_uint64, C.c_uint32, C.c_int32

    def fn(name, res, args):
        f = getattr(lib, "tpw_cv_" + name)
        f.restype, f.argtypes = res, args
        return f

    fn("addr", U64, [C.c_char_p])
    fn("alloc", U64, [U32])
    fn("handle", U32, [U64])
    fn("mkstr", U64, [C.POINTER(C.c_uint16), U32])
    lib.tpw_cv_layout.argtypes = [C.POINTER(C.c_int64)]
    HOST_FN["set_avatar_destination"] = fn("set_avatar_destination", U32, [U64, U32])
    HOST_FN["teleport_avatar"] = fn("teleport_avatar", U32, [U64, U32])
    HOST_FN["is_this_me"] = fn("is_this_me", U32, [U64, U64])
    HOST_FN["receive_text"] = fn("receive_text", None, [U64, U64, I32])
    HOST_FN["feedback"] = fn("feedback", None, [U64, U64])
    HOST_FN["get_data_for_user"] = fn("get_data_for_user", U32, [U64, U64])
    HOST_FN["add_contact"] = fn("add_contact", None, [U64, U64, I32])
    HOST_FN["create_visitor"] = fn("create_visitor", None, [U64, U64, I32, I32, I32, I32])
    out = (C.c_int64 * 32)()
    n = lib.tpw_cv_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    o = s.o
    s.putptr(s.g("world"), o["world"])
    s.putptr(s.g("gms"), o["ms0"] if case["ms"] else 0)
    s.words(o["obj"], [s.ptrword(o["chat"]), s.ptrword(o["thing"])])
    s.write(o["n0"] + 0x10, bytes([case["b10"], case["b11"]]))
    s.write(o["n1"] + 0x10, bytes([0xFF, 0xFF]))


def execute(s, case):
    o = s.o
    op, a = case["op"], case["args"]
    if op == "setdest":
        cell = a[0] & 0xFFFF
        if isinstance(s, Ppc):
            s.call(F_SETDEST, [o["obj"], cell << 16])
        else:
            HOST_FN["set_avatar_destination"](o["obj"], cell)
        return 0
    if op == "teleport":
        cell = a[0] & 0xFFFF
        if isinstance(s, Ppc):
            s.call(F_TELE, [o["obj"], cell << 16])
        else:
            HOST_FN["teleport_avatar"](o["obj"], cell)
        return 0
    if op == "isme":
        return s.call(F_ISME, [o["obj"], o["buf"]]) & 0xFF if isinstance(s, Ppc) else HOST_FN["is_this_me"](o["obj"], o["buf"])
    if op == "recv":
        if isinstance(s, Ppc):
            s.call(F_RECV, [o["obj"], o["str"], a[0]])
        else:
            HOST_FN["receive_text"](o["obj"], o["str"], s32(a[0]))
        return 0
    if op == "feedback":
        if isinstance(s, Ppc):
            s.call(F_FEED, [o["obj"], o["str"]])
        else:
            HOST_FN["feedback"](o["obj"], o["str"])
        return 0
    if op == "getdata":
        return s.call(F_GETDATA, [o["obj"], o["key"]]) if isinstance(s, Ppc) else HOST_FN["get_data_for_user"](o["obj"], o["key"])
    if op == "add":
        if isinstance(s, Ppc):
            s.call(F_ADD, [o["obj"], o["str"], a[0]])
        else:
            HOST_FN["add_contact"](o["obj"], o["str"], s32(a[0]))
        return 0
    if op == "create":
        x, y, avatar, extra = a
        if isinstance(s, Ppc):
            s.call(F_CREATE, [o["obj"], o["str"], x, y, avatar, extra])
        else:
            HOST_FN["create_visitor"](o["obj"], o["str"], s32(x), s32(y), s32(avatar), s32(extra))
        return 0
    raise AssertionError(op)


def observe(s, case, answer):
    o = s.o
    gms = s.getptr(s.g("gms"))
    kind = 0 if gms == 0 else 1 if gms == o["ms0"] else 2 if gms == o["newobj"] else 3
    return {"answer": answer, "log": s.log(), "gms": kind, "world": s.getptr(s.g("world")) == o["world"],
            "rest": s.read(o["obj"] + 8, OBJ_SIZE - 8),
            "head": s.rd_words(o["obj"], 2) == [s.ptrword(o["chat"]), s.ptrword(o["thing"])]}


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

CELLS = [0, 1, 2, 0x7F, 0x80, 0x81, 0xFF, 0x100, 0x101, 0x3FFF, 0x4000, 0x7FFF, 0x8000, 0x8001, 0xFFFF]
COORDS = [0, 1, -1, 255, 256, 257, -255, -256, -257, 511, 512, 0x7FFF, 0x8000, 0xFFFF, 0x10000, 0x7FFFFFFF, -0x80000000, 0x1234567, -0x1234567]


def rtext(r):
    return [r.choice([r.randrange(0x41, 0x7B), r.randrange(1, 0x10000)]) for _ in range(r.choice([0, 1, 2, 3, 5, 8]))]


def base(r, op, args):
    c = {"op": op, "args": args, "text": rtext(r), "ms": r.choice([0, 1]), "f0": r.choice([0, 1]), "f1": r.choice([0, 1]),
         "b10": r.choice([0, 1, 2, 0x80, 0xFF]), "b11": r.choice([0, 1, 2, 0x80, 0xFF]),
         "queues": {"GetThingId": [r.randrange(0x10000)], "SetDestination": [r.choice([0, 1])], "Teleport": [r.choice([0, 1])],
                    "GetParkEntrancePos": [r.randrange(0x10000)], "IsSwearFilterOn": [r.choice([0, 1])], "NocaseEq": [r.choice([0, 1])],
                    "Filter": [r.choice([0, 1])]}}
    return c


def with_(c, **kw):
    for k, v in kw.items():
        if k in c:
            c[k] = v
        else:
            c["queues"][k] = [v]
    return c


def rcell(r):
    return r.choice(CELLS + [r.randrange(0x10000)])


def rcoord(r):
    return r.choice(COORDS + [r.randrange(-0x40000, 0x40000), T.s32(r.getrandbits(32))])


def gen(op, argfn, edge_variants):
    def edges():
        r = random.Random(sum(map(ord, op)) * 7 + 1)
        for variant in edge_variants(r):
            yield variant

    def rnd(r):
        return base(r, op, argfn(r))

    return edges, rnd


def edges_cell(op):
    def f(r):
        for cell in CELLS:
            for ans in (0, 1):
                key = "SetDestination" if op == "setdest" else "Teleport"
                yield with_(base(r, op, [cell]), **{key: ans})
    return f


def edges_isme(r):
    for ans in (0, 1):
        for n in (0, 1, 4):
            yield with_(base(r, "isme", []), NocaseEq=ans, text=[0x61] * n)


def edges_recv(r):
    for ms in (0, 1):
        for sw in (0, 1):
            for colour in (0, 7, -1, 0x7FFFFFFF):
                yield with_(base(r, "recv", [colour & MASK]), ms=ms, IsSwearFilterOn=sw)


def edges_feed(r):
    for ms in (0, 1):
        for sw in (0, 1):
            for n in (0, 1, 6):
                yield with_(base(r, "feedback", []), ms=ms, IsSwearFilterOn=sw, text=[0x62] * n)


def edges_getdata(r):
    for f0 in (0, 1):
        for f1 in (0, 1):
            for b10 in (0, 1, 0x80):
                for b11 in (0, 1, 0xFF):
                    yield with_(base(r, "getdata", []), f0=f0, f1=f1, b10=b10, b11=b11)


def edges_add(r):
    for flag in (0, 1, -1, 12345):
        for n in (0, 1, 7):
            yield with_(base(r, "add", [flag & MASK]), text=[0x63] * n)


def edges_create(r):
    for x in COORDS:
        for y in COORDS:
            yield base(r, "create", [x & MASK, y & MASK, r.getrandbits(32), r.getrandbits(32)])
    for x in (0, 5, -7):
        yield base(r, "create", [x & MASK, MASK, 0x12345678, 9])


def rnd_create_args(r):
    y = MASK if r.random() < 0.25 else rcoord(r) & MASK
    return [rcoord(r) & MASK, y, r.getrandbits(32), r.getrandbits(32)]


def make_test(name, op, argfn, edge_fn):
    edges = lambda: (c for c in edge_fn(random.Random(len(op) * 31 + 5)))
    return FnTest(name, ppc, host, edge=edges, rnd=lambda r: base(r, op, argfn(r)))


TESTS = [
    make_test("COnlineControl::CreateVisitor", "create", rnd_create_args, edges_create),
    make_test("COnlineControl::SetAvatarDestination", "setdest", lambda r: [rcell(r)], edges_cell("setdest")),
    make_test("COnlineControl::TeleportAvatar", "teleport", lambda r: [rcell(r)], edges_cell("teleport")),
    make_test("COnlineControl::IsThisMe", "isme", lambda r: [], edges_isme),
    make_test("COnlineControl::ReceiveText", "recv", lambda r: [r.choice([0, 7, MASK, r.getrandbits(32)])], edges_recv),
    make_test("COnlineControl::Feedback", "feedback", lambda r: [], edges_feed),
    make_test("COnlineControl::GetDataForUser", "getdata", lambda r: [], edges_getdata),
    make_test("COnlineControl::AddContact", "add", lambda r: [r.choice([0, 1, MASK, r.getrandbits(32)])], edges_add),
]
