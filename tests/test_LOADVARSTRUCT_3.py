"""Unit v5960, the action history load, the tag table lookup and the tag system's message receiver, against
libtpw_unit_LOADVARSTRUCT_3.dylib (tests/unit_LOADVARSTRUCT_3.cpp), main app ("SimTheme Park"),
src/game/Thing.cpp, src/game/buildings/TrainingCentre.cpp:
  LOADVARSTRUCT<CThoughts::EventHistType> 0x1012f5a8, CTagTable::GetElement 0x1012dd94, CTagSystem::ReceiveMessage 0x1012c90c

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. The callees outside the unit (WriteSerialiseLog,
LbFile_Read, GAME_ASSERTMSG, CTRACE, TRACE, UDebug, FormatTagText, UIInterface::MessageTag and the message's GetType) are fakes on both
sides that record their arguments and answer from the case's queues; CTagSystem::ReceiveMessage runs CTagTable::GetElement as the original.

The world of a case: the tag table (0x17e elements, ids mostly the slot number, some swapped or broken), the TbError zero word, a message
of a type the case names, the file read's answers. What is compared: the answer, the 4 bytes of the history entry, the table index
GetElement answers, and the event log (every argument, the tag parameters in full, the text pointer as an offset from the tag system).

No fused operations. Not tested: GetElement with an id from 0x180 up or negative (it reads past the table, whose neighbours are
other globals).

Unit v5L39: an image test of the tag table as loaded (tests/imagedata.py); the other cases write their own tables.
"""
import ctypes as C
import random
import struct

import calllog as CL
import imagedata
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_LOADVARSTRUCT_3"
MASK = 0xFFFFFFFF

FN_LOAD, FN_GET, FN_RECV = 0x1012F5A8, 0x1012DD94, 0x1012C90C
SIZES = {FN_LOAD: 244, FN_GET: 144, FN_RECV: 760}
OWN = set(SIZES)

WSL, LBREAD, ASSERT, CTRACE, TRACE, UDEBUG = 0x1015B3DC, 0x10238848, 0x1000DDE8, 0x1006FCBC, 0x1006FCFC, 0x10000E40
FORMAT, MSGTAG, GETTYPE = 0x1012CC3C, 0x101EFEE8, 0x100F93B0

ADDR = dict(table=0x102A12C0, tbzero=0x102A4400, saveload=0x1037FEFC, ui=0x10396440)
HOST_LAYOUT = [0x20, 0x17E * 0x20, 4, 0x80E, 0xC]

CUR = {}   # the PowerPC side's addresses of this case's tag system


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def lb_effect(e, log):
    """LbFile_Read(result*, file, buffer, length, transferred*): the file's four bytes, the count and the TbError."""
    L = e.cl
    e.write(e.r(5), struct.pack(">I", L.ret("data") & MASK))
    e.write(e.r(7), struct.pack(">I", L.ret("count") & MASK))
    res = e.r(3)
    e.write(res, struct.pack(">I", L.ret("result") & MASK))
    return res


STUBS.internal(WSL, CL.recorder("WriteSerialiseLog", "Lisi"))
STUBS.internal(LBREAD, CL.recorder("LbFile_Read", "xixux", ret=lb_effect))
STUBS.internal(ASSERT, CL.recorder("GAME_ASSERTMSG", "is"))
STUBS.internal(CTRACE, CL.recorder("CTRACE", "isi"))
STUBS.internal(TRACE, CL.recorder("TRACE", "si"))
STUBS.internal(UDEBUG, CL.recorder("UDebug", "s"))


def stub_gettype(e):
    e.cl.rec("GetType", (0,))
    e.set_r(3, e.cl.ret("type") & MASK)


def stub_format(e):
    L = e.cl
    elem = e.r(4)
    index = (elem - ADDR["table"]) // 0x20 if elem else -1
    L.rec("FormatTagText", (1 if e.r(3) == CUR["this"] else -1, index, (e.r(5) >> 16) & 0xFFFF, e.r(6), 1 if e.r(7) else 0))
    e.set_r(3, 1)


def stub_msgtag(e):
    L = e.cl
    p = e.r(4)
    w = struct.unpack(">IIIIhxxIIHxxIBxxx", e.read(p, 0x28))
    text = (w[2] - CUR["this"]) if w[2] else -9999
    L.rec("MessageTag", (L.lab(e.r(3)), w[0], s32(w[1]), text, s32(w[3]), w[4], s32(w[5]), s32(w[6]), w[7], s32(w[8]), w[9]))


STUBS.internal(GETTYPE, stub_gettype)
STUBS.internal(FORMAT, stub_format)
STUBS.internal(MSGTAG, stub_msgtag)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def cstring(self, text):
        a = self.alloc(len(text) + 1)
        self.write(a, text.encode() + b"\0")
        return a

    def table(self, rows):
        """rows: 0x17e tuples (id, w4, h8, w0c, w10, w14, w18, w1c); the halfword at +8 in the side's own order."""
        f = self.fmt[0] + "IIHHIIIII"
        self.write(self.g("table"), b"".join(struct.pack(f, r[0] & MASK, r[1], r[2] & 0xFFFF, 0, *r[3:]) for r in rows))

    def message(self, case):
        m = case["msg"]
        if m is None:
            return 0
        return self.make_msg(*m)


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({ADDR["saveload"]: 1, ADDR["ui"]: 2}, case["queues"])
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

    def make_msg(self, kind, a, thing, time, value):
        vt = self.alloc(0x20)
        tv = self.alloc(8)
        self.words(tv, [GETTYPE, self.e.r(2)])
        self.words(vt + 8, [tv])
        m = self.alloc(0x30)
        if kind == 0:
            self.words(m, [vt, 0, a, (thing & 0xFFFF) << 16, time, value])
        else:
            self.words(m, [vt, 0, a])
        return m

    def call_load(self, file, buf, name, nolog):
        r3, _, _ = T.call_ppc(self.e, FN_LOAD, [file, buf, name, nolog & MASK])
        return r3 & MASK

    def call_get(self, id_):
        r3, _, _ = T.call_ppc(self.e, FN_GET, [0, id_ & MASK])
        return r3 & MASK

    def call_recv(self, this, msg):
        CUR["this"] = this
        T.call_ppc(self.e, FN_RECV, [this, msg])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {1: self.g("saveload"), 2: self.g("ui")}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_x_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_x_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def make_msg(self, kind, a, thing, time, value):
        return self.lib.tpw_x_msg(kind, s32(a), thing & 0xFFFF, s32(time), value & MASK)

    def call_load(self, file, buf, name, nolog):
        return self.lib.tpw_x_load(file, buf, name, s32(nolog)) & MASK

    def call_get(self, id_):
        return self.lib.tpw_x_get(s32(id_))

    def call_recv(self, this, msg):
        self.lib.tpw_x_recv(this, msg)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    getattr(lib, "tpw_x_addr").restype, getattr(lib, "tpw_x_addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, "tpw_x_alloc").restype, getattr(lib, "tpw_x_alloc").argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_x_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_x_msg.restype, lib.tpw_x_msg.argtypes = C.c_uint64, [C.c_int32, C.c_int32, C.c_uint32, C.c_int32, C.c_uint32]
    lib.tpw_x_load.restype, lib.tpw_x_load.argtypes = C.c_int32, [C.c_uint64, C.c_uint64, C.c_uint64, C.c_int32]
    lib.tpw_x_get.restype, lib.tpw_x_get.argtypes = C.c_uint64, [C.c_int32]
    lib.tpw_x_recv.restype, lib.tpw_x_recv.argtypes = None, [C.c_uint64, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_x_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    s.words(s.g("tbzero"), [case["tbzero"]])
    s.table(case["rows"])


def run(s, case, fn):
    try:
        build(s, case)
        return fn(s, case)
    finally:
        s.done()


def do_load(s, case):
    buf = s.alloc(8)
    s.words(buf, [case["buf"]])
    name = s.cstring(case["name"])
    ans = s.call_load(case["file"], buf, name, case["nolog"])
    # After a good read the two halves as values (the host keeps the file's little-endian order, the Mac swaps each half into its
    # own); after a failed one the buffer holds the file's bytes as read, the same on both sides.
    raw = s.read(buf, 4)
    return {"answer": ans, "log": s.log(), "buf": list(struct.unpack(s.fmt[0] + "2H", raw)) if ans == 0 else raw}


def do_get(s, case):
    p = s.call_get(case["id"])
    return {"answer": (p - s.g("table")) // 0x20 if p else -1, "log": s.log()}


def do_recv(s, case):
    this = s.alloc(0x80E)
    msg = s.message(case)
    s.call_recv(this, msg)
    return {"log": s.log(), "this": s.read(this, 0x80E)}


def ppc(op):
    return lambda e, case: run(Ppc(e, case), case, op)


def host(op):
    return lambda lib, case: run(Host(lib, case), case, op)


# ---------------------------------------------------------------------------------------------------------------- case generators

NAMES = ["mActionHistIndex", "mEvents", "x"]


def make_rows(r, swaps=(), broken=()):
    rows = []
    for i in range(0x17E):
        rows.append([i, r.getrandbits(32), r.randrange(-0x8000, 0x8000), r.getrandbits(32), r.getrandbits(32), r.getrandbits(32),
                     r.choice([0, 1, 5, 300, r.getrandbits(32)]), r.getrandbits(32)])
    for a, b in swaps:
        rows[a][0], rows[b][0] = rows[b][0], rows[a][0]
    for a in broken:
        rows[a][0] = 0x7F000000 + a
    return rows


def base(r, swaps=(), broken=()):
    return {"rows": make_rows(r, swaps, broken), "tbzero": 0, "queues": {}, "msg": None}


def random_perturb(r):
    k = r.random()
    if k < 0.5:
        return (), ()
    if k < 0.7:
        return [tuple(r.sample(range(0x17E), 2)) for _ in range(r.randrange(1, 4))], ()
    return (), [r.randrange(0x17E) for _ in range(r.randrange(1, 4))]


# ---- LOADVARSTRUCT
def load_case(r, result=None, count=4, tbzero=0, nolog=None):
    c = base(r)
    c["tbzero"] = tbzero
    c["file"] = r.randrange(0x1000, 0x7000)
    c["buf"] = r.getrandbits(32)
    c["name"] = r.choice(NAMES)
    c["nolog"] = r.choice([0, 1, 2, -1]) if nolog is None else nolog
    c["queues"] = {"data": [r.getrandbits(32)], "count": [count], "result": [r.choice([0, 1, -1, 5, -0x80000000]) if result is None else result]}
    return c


def load_edges():
    r = random.Random(5960)
    for tb in (0, 1, 0x80000000):
        for result in (0, 1, -1, 0x7FFFFFFF, -0x80000000):
            for count in (4, 0, 3, 5):
                for nolog in (0, 1):
                    yield load_case(r, result, count, tb, nolog)


def load_random(r):
    return load_case(r, count=r.choice([4, 4, 4, 0, 3]), tbzero=r.choice([0, 0, 1, r.getrandbits(32)]))


# ---- GetElement
def get_case(r, id_=None, swaps=(), broken=()):
    c = base(r, swaps, broken)
    c["id"] = r.choice([0, 1, 0x17D, 0x17E, 0x17F, r.randrange(0x17E)]) if id_ is None else id_
    return c


def get_edges():
    r = random.Random(5961)
    for i in (0, 1, 0x17B, 0x17C, 0x17D, 0x17E, 0x17F):
        yield get_case(r, i)
    yield get_case(r, 5, broken=[5])           # not found
    yield get_case(r, 0, broken=[0])
    yield get_case(r, 0x17D, broken=[0x17D])
    yield get_case(r, 7, swaps=[(7, 100)])     # found by the search, in slot 100
    yield get_case(r, 0, swaps=[(0, 0x17D)])
    yield get_case(r, 0x17D, swaps=[(0, 0x17D)])
    yield get_case(r, 0x17E, swaps=[(0, 0x17D)])


def get_random(r):
    s, b = random_perturb(r)
    return get_case(r, None, s, b)


# ---- ReceiveMessage
def recv_case(r, typ, a, swaps=(), broken=()):
    c = base(r, swaps, broken)
    c["queues"] = {"type": [typ]}
    thing = r.choice([0, 1, 0xFFFF, r.getrandbits(16)])
    time = r.choice([-1, -5, 0, 1, 77, -0x80000000, r.randrange(-100, 1000)])
    value = r.choice([0, 1, 0xFFFFFFFF, r.getrandbits(32)])
    kind = {0x18: 0, 0x13: 1}.get(typ, 2)
    c["msg"] = (kind, a, thing, time, value)
    return c


def recv_edges():
    r = random.Random(5962)
    for tag in (0, 1, 0x17D, 0x17E, 0x17F):
        for typ in (0x18, 0x19):
            yield recv_case(r, typ, tag)
    for tag in (5, 0x17D):
        for typ in (0x18, 0x19):
            yield recv_case(r, typ, tag, broken=[tag])
    for code in range(-1, 12):
        yield recv_case(r, 0x13, code)
    for code in (7, 8, 9):
        yield recv_case(r, 0x13, code, broken=[{7: 0x17B, 8: 0x17C, 9: 0x17D}[code]])
    for typ in (0, 1, 0x12, 0x14, 0x17, 0x1A, 0x1B, 0x100, -1):
        yield recv_case(r, typ, 3)


def recv_random(r):
    typ = r.choice([0x18, 0x18, 0x19, 0x13, 0x13, r.randrange(0, 0x30)])
    if typ == 0x13:
        a = r.randrange(5, 12)
    else:
        a = r.choice([r.randrange(0x17E), r.randrange(0x17E), 0x17E, 0x17F])
    s, b = random_perturb(r)
    return recv_case(r, typ, a, s, b)


TESTS = [
    FnTest("LOADVARSTRUCT<CThoughts::EventHistType>", ppc(do_load), host(do_load), edge=load_edges, rnd=load_random),
    FnTest("CTagTable::GetElement", ppc(do_get), host(do_get), edge=get_edges, rnd=get_random),
    FnTest("CTagSystem::ReceiveMessage", ppc(do_recv), host(do_recv), edge=recv_edges, rnd=recv_random),
    imagedata.table_test("the tag table", [("tags", ADDR["table"], "IIIIIIII", 0x17E)]),
]
