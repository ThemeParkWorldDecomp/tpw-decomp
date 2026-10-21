"""Unit v5016, the online client's object, against libtpw_unit_COnlineControl_COnlineControl.dylib
(tests/unit_COnlineControl_COnlineControl.cpp), main app ("SimTheme Park"), src/game/online/online.cpp:
  COnlineControl::COnlineControl 0x10213870, COnlineControl::~COnlineControl 0x10213dd4, COnlineControl::InitClient 0x102143c4,
  COnlineControl::InitGameWorld 0x10214678

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees
outside the unit (the string, deque, set, mutex and chat library, the online system's getters, the mouse modes, the map's entrance,
TeleportAvatar, rand, swprintf and wcslen) are fakes on both sides that record their arguments (pointers as labels, texts as hashes, the
strings built on the stack as the token their constructor wrote) and answer from the case's queues. swprintf and wcslen are the real
ones on the host (platform/msl.cpp) and small Python versions on the PowerPC side.

The world of a case: the object (0xe8c bytes, pattern filled), its string reps (a string is one pointer to a block with the character
pointer at +0xc), the online system, the chat object, the mouse, the world and gOnlineConfig's chat server and port. What is compared: the
object's bytes (except the pointer words and the format buffer, which are compared as labels and text), the globals, and the event log.
No fused operations. Not tested: the destructor's delete of the object itself (the hidden flag; the host's caller frees it) and a NULL this.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_COnlineControl_COnlineControl"
MASK = 0xFFFFFFFF

FN_CT, FN_DT, FN_IC, FN_IG = 0x10213870, 0x10213dd4, 0x102143c4, 0x10214678
SIZES = {FN_CT: 364, FN_DT: 444, FN_IC: 640, FN_IG: 404}
OWN = set(SIZES)

ADDR = dict(gOnlineSystem=0x10380ab0, gMouse=0x102dc1f4, gMouseWord1E0=0x102dc1e0, gMouseWord1E4=0x102dc1e4, gGameWorld=0x1037ebfc)
CONFIG = 0x102b2b70
FMT_TABLE = 0x1025179c
CALLBACK_TV = 0x10268948
HOST_LAYOUT = [0xe8c]
OBJ = 0xe90

PSTATE = {}


def s32(v):
    return T.s32(v)


def s16(v):
    return T.s16(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes


def u16s(e, a, limit=0x800):
    out = []
    for i in range(limit):
        c = struct.unpack(">H", e.read(a + 2 * i, 2))[0]
        if c == 0:
            break
        out.append(c)
    return out


def cbytes(e, a, limit=0x800):
    out = []
    for i in range(limit):
        c = e.read(a + i, 1)[0]
        if c == 0:
            break
        out.append(c)
    return out


def H(chars):
    h = len(chars) & MASK
    for c in chars:
        h = (h * 31 + c) & MASK
    return h


def S(name, spec, ret=None):
    """A recording fake. spec, one letter per argument register from r3: L label, i s32, u u32, h s16, H high half, b byte,
    c narrow text hash, w wide text hash, W the word the address holds. ret: a queue key or fn(e, log) -> r3."""

    def fn(e):
        L = e.cl
        vals = []
        for k, c in enumerate(spec):
            r = e.r(3 + k) & MASK
            if c == "L":
                vals.append(L.lab(r))
            elif c == "i":
                vals.append(s32(r))
            elif c == "u":
                vals.append(r)
            elif c == "h":
                vals.append(s16(r & 0xFFFF))
            elif c == "H":
                vals.append(r >> 16)
            elif c == "b":
                vals.append(r & 0xFF)
            elif c == "c":
                vals.append(H(cbytes(e, r)))
            elif c == "w":
                vals.append(H(u16s(e, r)))
            elif c == "W":
                vals.append(e.u32(r))
        L.rec(name, tuple(vals))
        if ret is not None:
            v = L.ret(ret) if isinstance(ret, str) else ret(e, L)
            e.set_r(3, v & MASK)

    return fn


def stub_new(e):
    size = e.r(3) & MASK
    e.cl.rec("new", (size,))
    e.set_r(3, e.alloc(max(size, 4)))


def stub_delete(e):
    p = e.r(3) & MASK
    e.cl.rec("delete", (e.cl.lab(p), p if p < 0x10000 else 0))


def stub_wstr_from(e):
    p = e.r(3) & MASK
    token = H(u16s(e, e.r(4) & MASK))
    e.cl.rec("wstr_from", (e.cl.lab(p), token))
    e.write(p, struct.pack(">I", token))


def stub_wstr_assign(e):
    n = e.r(5) & MASK
    chars = u16s(e, e.r(4) & MASK)[:n]
    e.cl.rec("wstr_assign", (e.cl.lab(e.r(3) & MASK), n, H(chars) if len(chars) == n else H(chars)))


def stub_str_assign(e):
    other = e.r(4) & MASK
    text = cbytes(e, e.u32(e.u32(other) + 0xC))
    e.cl.rec("str_assign", (e.cl.lab(e.r(3) & MASK), H(text), e.r(5) & MASK, s32(e.r(6))))


def stub_deque_ctor(e):
    p = e.r(3) & MASK
    e.cl.rec("deque_ctor", (e.cl.lab(p),))
    e.write(p + 0x10, struct.pack(">I", e.cl.ret("deque10") & MASK))


def stub_connect(e):
    L = e.cl
    server = e.r(4) & MASK
    L.rec("Connect", (L.lab(e.r(3) & MASK), H(cbytes(e, server)), e.r(5) & 0xFFFF, 1 if (e.r(6) & MASK) == CALLBACK_TV else 0,
                      L.lab(e.r(7) & MASK)))
    e.set_r(3, L.ret("Connect") & MASK)


def stub_create_player(e):
    L = e.cl
    L.rec("CreatePlayer", (L.lab(e.r(3) & MASK), e.u32(e.r(4) & MASK), s32(e.r(5)), s32(e.r(6)), e.u32(e.r(7) & MASK),
                           L.lab(e.r(8) & MASK)))


def stub_entrance(e):
    L = e.cl
    L.rec("GetParkEntrancePos", (L.lab(e.r(4) & MASK), s32(e.r(5))))
    e.write(e.r(3) & MASK, struct.pack(">H", L.ret("entrance") & 0xFFFF))


def stub_rand(e):
    e.set_r(3, e.cl.ret("rand") & MASK)


def stub_wcslen(e):
    e.set_r(3, len(u16s(e, e.r(3) & MASK)))


def format_text(e, fmt, arg):
    out = []
    i = 0
    while i < len(fmt):
        c = fmt[i]
        if c != "%":
            out.append(ord(c))
            i += 1
            continue
        j = i + 1
        zero = False
        width = 0
        if fmt[j] == "0":
            zero = True
            j += 1
        while fmt[j].isdigit():
            width = width * 10 + int(fmt[j])
            j += 1
        wide = False
        if fmt[j] == "l":
            wide = True
            j += 1
        conv = fmt[j]
        if conv in "di":
            text = str(s32(arg))
        elif conv == "u":
            text = str(arg & MASK)
        elif conv in "xX":
            text = ("%x" if conv == "x" else "%X") % (arg & MASK)
        elif conv == "s" and not wide:
            text = "".join(chr(b) for b in cbytes(e, arg & MASK))
        elif conv == "s":
            text = "".join(chr(b) for b in u16s(e, arg & MASK))
        else:
            raise AssertionError("format " + fmt)
        if len(text) < width:
            text = text.rjust(width, "0" if zero else " ")
        out.extend(ord(t) for t in text)
        i = j + 1
    return out


def stub_swprintf(e):
    buf, n, fmtaddr, arg = e.r(3) & MASK, e.r(4) & MASK, e.r(5) & MASK, e.r(6) & MASK
    fmt = "".join(chr(c) for c in u16s(e, fmtaddr))
    chars = format_text(e, fmt, arg)[: max(n - 1, 0)]
    e.write(buf, b"".join(struct.pack(">H", c) for c in chars) + b"\0\0")
    e.set_r(3, len(chars))


STUBS = CL.Stubs()
STUBS.real.update(OWN)

TOC = 0x10267CC0
# The glue functions save r2 in the caller's frame (0x14(r1)) and the caller loads it back afterwards: the fakes do the save.
GLUE = {0x102384B8, 0x10238218, 0x10239148, 0x10239160, 0x102389C8, 0x1023A9F0, 0x1023AA08, 0x1023B0C8, 0x1023AC18, 0x1023B158,
        0x1023AA50, 0x1023B320}


def glue(fn):
    def wrapped(e):
        e.write((e.r(1) + 0x14) & MASK, struct.pack(">I", e.r(2) & MASK))
        fn(e)

    return wrapped


for _addr, _fn in [
    (0x102384b8, stub_new), (0x10238218, stub_delete), (0x10239148, stub_rand), (0x10239160, stub_swprintf), (0x102389c8, stub_wcslen),
    (0x101e0b84, S("wstr_ctor", "L")), (0x10146a14, stub_wstr_from), (0x101468f8, S("wstr_dtor", "Lh")),
    (0x101b2510, stub_wstr_assign), (0x1007363c, S("str_ctor", "L")), (0x1000221c, S("str_dtor", "Lh")),
    (0x10001c80, stub_str_assign), (0x10225938, stub_deque_ctor), (0x10224fbc, S("deque_copy", "LL")),
    (0x1022462c, S("deque_td", "L")), (0x1016985c, S("list_dtor", "Lh")), (0x1021425c, S("nset_dtor", "Lh")),
    (0x10214148, S("map_dtor", "Lh")), (0x10213fbc, S("wset_dtor", "Lh")), (0x10200ca4, S("mutex_ctor", "L")),
    (0x10200d04, S("mutex_dtor", "Lh")), (0x1021e068, S("chatdata_ctor", "L")), (0x1020ca80, S("ip_ctor", "L")),
    (0x1016f3c8, S("ip_startup", "L", "Startup")), (0x1016f448, S("ip_shutdown", "L")),
    (0x1023a9f0, CL.recorder("Chat", "N")), (0x1023aa08, stub_connect), (0x1023b0c8, stub_create_player),
    (0x1023ac18, S("hearing", "LuL")), (0x1023b158, S("enum", "LL")), (0x1023aa50, S("chat_release", "L")),
    (0x1023b320, S("chat_dtor", "Lh")), (0x1021c75c, S("SaveBuddy", "Lc")), (0x1021d74c, S("SaveIgnore", "Lc")),
    (0x10171834, S("cust_name", "L", lambda e, L: PSTATE["custName"])),
    (0x10171878, S("password", "L", lambda e, L: PSTATE["password"])),
    (0x101718fc, S("player_id", "L", "pid")), (0x1017193c, S("cust_id", "L", "cid")), (0x101719bc, S("avatar", "L", "avatar")),
    (0x1016f7d0, CL.recorder("CFSOnlineSystem", "N")), (0x10000e40, S("UDebug", "c")),
    (0x10101ae0, stub_entrance), (0x10212d98, CL.recorder("COnlinePerson", "NwuLb")),
    (0x1021802c, S("Teleport", "LH")), (0x1007faac, CL.recorder("CMMOnline", "N")), (0x1007ea78, CL.recorder("CMMDefault", "N")),
    (0x1007e2e0, S("AssignMouseMode", "LL")),
]:
    STUBS.internal(_addr, glue(_fn) if _addr in GLUE else _fn)


def setup(runner):
    pass


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def pw(self, a, ptr):
        self.words(a, [self.to_word(ptr)])

    def rpw(self, a):
        return self.from_word(self.rd_word(a))

    def to_word(self, ptr):
        return ptr

    def from_word(self, w):
        return w

    def rd16(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dH" % n, self.read(a, 2 * n)))

    def wtext(self, text):
        data = [ord(c) for c in text] + [0]
        a = self.alloc(2 * len(data))
        self.write(a, struct.pack(self.fmt[0] + "%dH" % len(data), *data))
        return a

    def ctext(self, text):
        a = self.alloc(len(text) + 1)
        self.write(a, text.encode("latin-1") + b"\0")
        return a

    def rep(self, textaddr):
        r = self.alloc(0x20)
        self.pw(r + 0xC, textaddr)
        return r

    def make_objs(self):
        o = dict(O=self.alloc(OBJ), sys=self.alloc(0x40), chat=self.alloc(0x40), world=self.alloc(0x300), mouse=self.alloc(0x10))
        self.objs = o
        return o

    def fixed(self):
        o = self.objs
        base = o["O"]
        pairs = [(base, 1), (base + 8, 2), (base + 0xC, 3), (base + 0x18, 4), (base + 0x1C, 5), (base + 0xE20, 6), (base + 0xE24, 7),
                 (base + 0xE28, 8), (base + 0xE34, 9), (base + 0xE44, 10), (base + 0xE54, 11), (base + 0xE68, 12), (base + 0xE88, 13),
                 (o["sys"], 14), (o["world"] + 0x2D8, 15), (o["mouse"], 16), (o["chat"], 17)]
        return pairs


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.make_objs()
        self.L = e.cl = CL.PpcLog({a: l for a, l in self.fixed()}, case["queues"])
        self.saved = STUBS.activate(e)
        PSTATE.clear()

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

    def put_ptr(self, name, v):
        self.words(self.g(name), [v])

    def get_ptr(self, name):
        return self.rd_word(self.g(name))

    def fake_ptr(self, name, v):
        PSTATE[name] = v

    def set_config(self, text, port):
        self.words(CONFIG + 4, [self.rep(self.ctext(text))])
        self.write(CONFIG + 0x1A, struct.pack(">H", port & 0xFFFF))

    def label(self, ptr):
        return self.L.lab(ptr)

    def log(self):
        return self.L.log

    def call(self, fn, args):
        self.e.set_r(2, TOC)
        T.call_ppc(self.e, fn, args)


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.make_objs()
        CL.host_begin(lib, {l: a for a, l in self.fixed()}, case["queues"])

    def done(self):
        pass

    def to_word(self, ptr):
        return self.lib.tpw_ocs_handle(ptr)

    def from_word(self, w):
        return self.lib.tpw_ocs_deref(w)

    def g(self, name):
        return self.lib.tpw_ocs_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_ocs_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def put_ptr(self, name, v):
        self.write(self.g(name), struct.pack("<Q", v))

    def get_ptr(self, name):
        return struct.unpack("<Q", self.read(self.g(name), 8))[0]

    def fake_ptr(self, name, v):
        self.put_ptr(name, v)

    def set_config(self, text, port):
        self.lib.tpw_ocs_set_config(text.encode("latin-1"), port)

    def label(self, ptr):
        return self.lib.tpw_ocs_lab(ptr)

    def log(self):
        return CL.host_end(self.lib)

    def call(self, fn, args):
        {FN_CT: self.lib.tpw_ocs_ctor, FN_DT: self.lib.tpw_ocs_dtor, FN_IC: self.lib.tpw_ocs_init_client,
         FN_IG: self.lib.tpw_ocs_init_world}[fn](args[0])


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_ocs_addr.restype, lib.tpw_ocs_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_ocs_alloc.restype, lib.tpw_ocs_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_ocs_lab.restype, lib.tpw_ocs_lab.argtypes = C.c_int32, [C.c_uint64]
    lib.tpw_ocs_handle.restype, lib.tpw_ocs_handle.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_ocs_deref.restype, lib.tpw_ocs_deref.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_ocs_set_config.restype, lib.tpw_ocs_set_config.argtypes = None, [C.c_char_p, C.c_int32]
    lib.tpw_ocs_layout.argtypes = [C.POINTER(C.c_int64)]
    for n in ("ctor", "dtor", "init_client", "init_world"):
        f = getattr(lib, "tpw_ocs_" + n)
        f.restype, f.argtypes = None, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_ocs_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    o = s.objs
    O = o["O"]
    s.write(O, b"\xAA" * 0xE8C)
    if case["op"] == "ct":
        s.set_config(case["server"], case["port"])
        return
    s.write(O + 0x10, bytes(case["w10"]))
    s.write(O + 0x13, bytes([case["b13"]]))
    s.words(O + 0xE78, [case["w78"]])
    s.pw(O, o["chat"] if case["chat"] else 0)
    s.words(O + 4, [0])
    if case["op"] == "dt":
        return
    s.pw(O + 8, s.rep(s.wtext(case["wname"])))
    s.pw(O + 0xC, s.rep(s.ctext(case["server"])))
    s.set_config(case["server"], case["port"])
    s.put_ptr("gOnlineSystem", o["sys"] if case["sys"] else 0)
    s.fake_ptr("custName", s.wtext(case["cust"]))
    s.fake_ptr("password", s.wtext(case["pass"]))
    s.put_ptr("gGameWorld", o["world"])
    s.put_ptr("gMouse", o["mouse"] if case["mouse"] else 0)
    s.words(s.g("gMouseWord1E0"), [case["m0"]])
    s.words(s.g("gMouseWord1E4"), [case["m1"]])


def execute(s, case):
    s.call({"ct": FN_CT, "dt": FN_DT, "ic": FN_IC, "ig": FN_IG}[case["op"]], [s.objs["O"], 0])


def observe(s, case):
    o = s.objs
    O = o["O"]
    blk = bytearray(s.read(O, 0xE8C))
    for a in (0, 4, 8, 0xC):
        blk[a:a + 4] = b"\0\0\0\0"
    blk[0xE78:0xE7C] = b"\0\0\0\0"
    blk[0x220:0x620] = bytes(0x400)
    chars = s.rd16(O + 0x220, 0x400)
    text = chars[: chars.index(0)] if 0 in chars else chars
    out = {"blk": bytes(blk), "w78": s.rd_word(O + 0xE78),"text": text, "chat": s.label(s.rpw(O)), "person": s.label(s.rpw(O + 4))}
    if case["op"] in ("ic", "ig"):
        out["sys"] = s.label(s.get_ptr("gOnlineSystem"))
        out["mouse"] = s.label(s.get_ptr("gMouse"))
        out["m"] = s.rd_words(s.g("gMouseWord1E0"), 1) + s.rd_words(s.g("gMouseWord1E4"), 1)
    out["log"] = s.log()
    return out


def run(s, case):
    try:
        build(s, case)
        s.call_done = execute(s, case)
        return observe(s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

NAMES = ["", "a", "Bob", "Player One", "é中", "x" * 40]


def rtext(r):
    k = r.random()
    if k < 0.3:
        return r.choice(NAMES)
    return "".join(chr(r.choice([0x41, 0x61, 0x30, 0x5F, 0xE9, 0x4E2D])) for _ in range(r.randrange(1, 12)))


def ascii_text(r):
    return "".join(r.choice("abcdefgh.0123456789") for _ in range(r.randrange(0, 20)))


def base(r, op):
    c = {"op": op, "dump": False}
    c["queues"] = {"rand": [r.choice([0, 1, 0x7FFFFFFF, r.randrange(0, 0x7FFFFFFF)])], "deque10": [r.choice([0, 0x1234, 0x12345678])],
                   "Startup": [r.choice([0, 1, -1, 5])], "Connect": [r.choice([0, 1, 2, -1])], "pid": [r.randrange(-5, 1 << 31)],
                   "cid": [r.randrange(-5, 1 << 31)], "avatar": [r.choice([0, 1, r.randrange(0, 1 << 32)])],
                   "entrance": [r.choice([0, 1, 0xFFFF, r.randrange(0, 0x10000)])]}
    c["server"] = ascii_text(r)
    c["port"] = r.choice([0, 1, 0x7FFF, -1, 0x8000, 0xFFFF, r.randrange(0, 0x10000)])
    c["wname"] = rtext(r)
    c["cust"] = rtext(r)
    c["pass"] = rtext(r)
    c["chat"] = r.choice([True, False])
    c["sys"] = r.choice([True, False])
    c["mouse"] = r.choice([True, False])
    c["w10"] = [r.randrange(0, 256) for _ in range(3)]
    c["b13"] = r.choice([0, 1, 2, 0xFF, r.randrange(0, 256)])
    c["w78"] = r.choice([0, 0x77, 0x12345678, r.randrange(0, 1 << 32)])
    c["m0"] = r.randrange(0, 1 << 32)
    c["m1"] = r.randrange(0, 1 << 32)
    return c


def make(op, seed):
    def gen(r, **kw):
        c = base(r, op)
        c.update(kw)
        return c

    def edges():
        r = random.Random(seed)
        for kw in EDGES[op]:
            yield gen(r, **kw)

    def rnd(r):
        return gen(r)

    return edges, rnd


EDGES = {
    "ct": [dict(dump=True), dict(server=""), dict(server="chat.example.com")],
    "dt": [dict(chat=True, b13=1, w78=0), dict(chat=False, b13=1, w78=0x77), dict(chat=True, b13=0, w78=0x12345678),
           dict(chat=False, b13=2, w78=0), dict(chat=False, b13=0xFF, w78=5)],
    "ic": [dict(sys=False, chat=False), dict(sys=True, chat=True), dict(sys=False, chat=True), dict(sys=True, chat=False),
           dict(queues_override=True)],
    "ig": [dict(sys=False, mouse=False, chat=True), dict(sys=True, mouse=True, chat=True), dict(sys=False, mouse=True, chat=False),
           dict(sys=True, mouse=False, chat=False)],
}


def _fix_edges():
    # InitClient edges that pin the queues: every Startup and Connect answer
    base_edges = EDGES["ic"][:-1]
    for st in (0, 1, -1):
        for cn in (0, 1):
            base_edges.append(dict(sys=False, chat=False, startq=st, connq=cn))
    EDGES["ic"] = base_edges


_fix_edges()


def make_ic():
    def gen(r, **kw):
        c = base(r, "ic")
        if "startq" in kw:
            c["queues"]["Startup"] = [kw.pop("startq")]
            c["queues"]["Connect"] = [kw.pop("connq")]
        c.update(kw)
        return c

    def edges():
        r = random.Random(77)
        for kw in EDGES["ic"]:
            yield gen(r, **dict(kw))

    def rnd(r):
        return gen(r)

    return edges, rnd


ct_e, ct_r = make("ct", 11)
dt_e, dt_r = make("dt", 12)
ic_e, ic_r = make_ic()
ig_e, ig_r = make("ig", 14)

TESTS = [
    FnTest("COnlineControl::COnlineControl", ppc, host, edge=ct_e, rnd=ct_r),
    FnTest("COnlineControl::~COnlineControl", ppc, host, edge=dt_e, rnd=dt_r),
    FnTest("COnlineControl::InitClient", ppc, host, edge=ic_e, rnd=ic_r),
    FnTest("COnlineControl::InitGameWorld", ppc, host, edge=ig_e, rnd=ig_r),
]
