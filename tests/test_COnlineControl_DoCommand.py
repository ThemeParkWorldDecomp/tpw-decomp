"""Unit v5636, COnlineControl::DoCommand: runs one chat command from the vector of Argument the parser built (the first Argument's command
number picks a case of the jump table at 0x102b1d68), against libtpw_unit_COnlineControl_DoCommand.dylib
(tests/unit_COnlineControl_DoCommand.cpp), main app ("SimTheme Park"),
src/game/online/online.cpp:
  COnlineControl::DoCommand 0x10219bec

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. Every callee is a
fake on both sides: the Chat commands, the string library (BStrVal, the wide and nocase string constructors, assign, destructors, ==, the
tree finds and increment), TbITable::FormatString, Feedback, UDebug, UI_DialogBox, operator new, the online system and master save
accessors and Vote. They record what the real ones read (a string as its length and a hash of its first 32 units, pointers as labels)
and answer from the case's queues or blocks.

The world of a case: the COnlineControl block (Chat word, the player's name, the strings at 0xe20, 0xe24 and 0xe88, the contact set's
nodes, the bytes at 0xe50, 0xe51 and 0xe84, the word at 0xe64), three Arguments and the vector (size 0 to 3, command any number),
gOnlineSystem and gMasterSave (NULL or a block), gText's string table, gOnlineConfig's vote server and port, gVoteSendData, the park,
city and password blocks the fakes answer. What is compared: the bytes at 0xe50 and 0xe51, the word at 0xe64 (as a label), the strings at
0xe20, 0xe24 and 0xe88, the first units of the buffer at 0x220, gOnlineSystem and gMasterSave (as labels) and the event log.

No float work. Not tested: an operator new of the online system that fails (the original then reads the null block's 0xe0); the tree
finds' real search (faked; the answer is the case's).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_COnlineControl_DoCommand"
MASK = 0xFFFFFFFF

# ---- the unit's own functions (address, size from the MANIFEST)
FN_DOCOMMAND = 0x10219BEC
SIZES = {FN_DOCOMMAND: 6960}
OWN = set(SIZES)

# ---- the callees faked on both sides
BSTRVAL, WCTOR, WDTOR, ASSIGN = 0x10217764, 0x10146A14, 0x101468F8, 0x101F7014
NCOPY, NDTOR, EQUAL, SETFIND, BUDDYFIND, INCREMENT = 0x1022213C, 0x10222020, 0x1021F254, 0x1021FD4C, 0x1021F8D8, 0x10152DB0
FORMAT, FEEDBACK, UDEBUG, DIALOG, NEW = 0x10239730, 0x1021B784, 0x10000E40, 0x101F1800, 0x102384B8
NOTAFK, SAY, TELL, SHOUT, RANGE = 0x10239910, 0x1023ABA0, 0x1023ABB8, 0x1023AC00, 0x1023AC18
EAROFF, EARON, UNIGNORE, IGNORE, BLACKMARK = 0x1023AC30, 0x1023AC48, 0x1023AC60, 0x1023AC78, 0x1023AC90
REMOVEBUDDY, ADDBUDDY, PAGE, AFK, GOTO, LOCATE = 0x1023ACA8, 0x1023ACC0, 0x1023ACD8, 0x1023ACF0, 0x1023AD08, 0x1023AD20
SYSCTOR, GETFTP, CITYINFO, PASSWORD, CUSTOMER, PLAYER = 0x1016F7D0, 0x10177654, 0x10178B3C, 0x10171878, 0x1017193C, 0x101718FC
VOTECTOR, DISABLEVOTE, CONNECT, SESSION, SENDVOTE = 0x1023AD38, 0x101E6224, 0x1023AD50, 0x1023AD68, 0x1023AD80
MSCTOR, ISSWEAR, SETSWEAR = 0x101694DC, 0x1016B438, 0x1016B3F4

# ---- the Mac's addresses of the globals (the asm's TOC comments); the host library's come from tpw_dc_addr(name)
ADDR = dict(gOnlineSystem=0x10380AB0, gMasterSave=0x10380A8C, gVoteSendData=0x102B1B58, GotoPark_OK=0x10268990,
            VoteNotifyCallback=0x10268998)
GTEXT_TABLE = 0x1037FF18 + 0x598          # gText.mResourcePointers[19]
GONLINECONFIG = 0x102B2B70                # mVoteServer word at 0x14, mVotePort halfword at 0x22
JUMP_TABLE = 0x102B1D68
VT_UNISTRING = 0x103BCB34                 # the TOC slot of the import __vt__15TbUniStringVTag (holds the resolved vtable)
# ---- the host's offsets and sizes the module relies on, checked once against tpw_dc_layout
HOST_LAYOUT = [16, 24, 4, 8, 12, 4, 8, 0xE8C]


def s32(v):
    return T.s32(v)


def s16(v):
    return ((v & 0xFFFF) ^ 0x8000) - 0x8000


def fnv(units):
    h = 0x811C9DC5
    for u in units:
        h ^= u
        h = (h * 16777619) & MASK
    return h & 0x7FFFFFFF


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def plab(e, p):
    L = e.cl
    return L.news[p] if p in L.news else L.lab(p)


def p_wz(e, a):
    """The wide text at a, up to its 0."""
    out = []
    while len(out) < 0x800:
        u = struct.unpack(">H", e.read(a + 2 * len(out), 2))[0]
        if u == 0:
            break
        out.append(u)
    return out


def p_sig(e, slot):
    rep = e.u32(slot)
    n = e.u32(rep)
    data = e.u32(rep + 12)
    m = min(n, 32)
    units = list(struct.unpack(">%dH" % m, e.read(data, 2 * m))) if m else []
    return (n, fnv(units))


def p_csig(e, a):
    b = bytearray()
    while len(b) < 0x400:
        x = e.read(a + len(b), 1)[0]
        if x == 0:
            break
        b.append(x)
    return (len(b), fnv(b[:32]))


def glue(fn):
    """A cross-TOC call: the glue saves r2 at 0x14(r1) and the caller reloads it."""
    def stub(e):
        e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2)))
        fn(e)
    return stub


def put(e, a, w):
    e.write(a, struct.pack(">I", w & MASK))


def stub_new(e):
    L, size = e.cl, e.r(3)
    if size == 0xC and L.ret("VoteNull"):
        L.rec("new", (size, 0))
        e.set_r(3, 0)
        return
    p = e.alloc(size)
    e.write(p, bytes(size))
    L.news[p] = L.next_new
    L.next_new += 1
    L.rec("new", (size, L.news[p]))
    e.set_r(3, p)


def stub_bstrval(e):
    e.cl.rec("BStrVal", (plab(e, e.r(4)),))
    put(e, e.r(3), e.u32(e.r(4) + 4))


def stub_wctor(e):
    text = e.r(4)
    units = p_wz(e, text)
    rep = e.alloc(16)
    e.write(rep, struct.pack(">4I", len(units), len(units), 1, text))
    e.cl.rec("WCtor", (plab(e, text), len(units), fnv(units[:32])))
    put(e, e.r(3), rep)


def stub_wdtor(e):
    e.cl.rec("WDtor", (*p_sig(e, e.r(3)), s16(e.r(4))))


def stub_assign(e):
    e.cl.rec("Assign", (plab(e, e.r(3)), *p_sig(e, e.r(4)), e.r(5), s32(e.r(6))))
    put(e, e.r(3), e.u32(e.r(4)))


def stub_ncopy(e):
    e.cl.rec("NCopy", (plab(e, e.r(4)), *p_sig(e, e.r(4)), e.r(5), s32(e.r(6))))
    put(e, e.r(3), e.u32(e.r(4)))


def stub_ndtor(e):
    e.cl.rec("NDtor", (*p_sig(e, e.r(3)), s16(e.r(4))))


def stub_equal(e):
    e.cl.rec("Equal", (*p_sig(e, e.r(3)), *p_sig(e, e.r(4))))
    e.set_r(3, e.cl.ret("Equal") & MASK)


def finder(name):
    def stub(e):
        L = e.cl
        L.rec(name, (plab(e, e.r(4)), *p_sig(e, e.r(5))))
        put(e, e.r(3), L.w["found"] if L.ret(name) else e.r(4) + 4)
    return stub


def stub_increment(e):
    p = e.r(3)
    node = e.u32(p)
    e.cl.rec("Inc", (plab(e, node),))
    put(e, p, e.u32(node + 8))


def stub_format(e):
    L, ident = e.cl, e.r(6)
    L.rec("Format", (plab(e, e.r(3)), plab(e, e.r(4)), e.r(5), ident))
    if ident in (0x56, 0x57, 0x7F):
        tag = e.r(7)
        units = p_wz(e, e.u32(tag + 12))
        L.rec("VTag", (e.u32(tag), int(e.u32(tag + 4) == e.u32(VT_UNISTRING)), e.u32(tag + 8), len(units), fnv(units[:32])))
    e.write(e.r(4), struct.pack(">3H", 0x41 + ident % 26, 0x61 + (ident >> 3) % 26, 0))
    e.set_r(3, 0)


def stub_feedback(e):
    e.cl.rec("Feedback", (plab(e, e.r(3)), *p_sig(e, e.r(4))))


def stub_udebug(e):
    e.cl.rec("UDebug", p_csig(e, e.r(3)))


def stub_dialog(e):
    p = e.r(3)
    rgb = e.read(p + 4, 3)
    e.cl.rec("Dialog", (plab(e, e.u32(p)), rgb[0], rgb[1], rgb[2], s32(e.u32(p + 8)), s32(e.u32(p + 12))))
    e.cl.rec("DialogB", (s32(e.r(4)), plab(e, e.r(5)), s32(e.r(6)), plab(e, e.r(7)), s32(e.r(8)), plab(e, e.r(9)), e.r(10) & 0xFF))
    e.set_r(3, 0)


def chat_two(name):
    def stub(e):
        e.cl.rec(name, (plab(e, e.r(3)), plab(e, e.r(4))))
    return stub


def chat_name(name):
    def stub(e):
        e.cl.rec(name, (plab(e, e.r(3)), *p_sig(e, e.r(4)), plab(e, e.r(5))))
    return stub


def chat_text(name):
    def stub(e):
        e.cl.rec(name, (plab(e, e.r(3)), *p_sig(e, e.r(4)), e.r(5) & 0xFF, plab(e, e.r(6))))
    return stub


def stub_tell(e):
    e.cl.rec("Tell", (plab(e, e.r(3)), plab(e, e.r(4)), *p_sig(e, e.r(4)), *p_sig(e, e.r(5)), e.r(6) & 0xFF, plab(e, e.r(7))))


def stub_range(e):
    e.cl.rec("Range", (plab(e, e.r(3)), e.r(4), plab(e, e.r(5))))


def one(name):
    def stub(e):
        e.cl.rec(name, (plab(e, e.r(3)),))
    return stub


def answer_block(name, key):
    def stub(e):
        e.cl.rec(name, (plab(e, e.r(3)),))
        e.set_r(3, e.cl.w[key])
    return stub


def answer_queue(name, key):
    def stub(e):
        e.cl.rec(name, (plab(e, e.r(3)),))
        e.set_r(3, e.cl.ret(key) & MASK)
    return stub


def stub_city(e):
    e.cl.rec("CityInfo", (e.r(3), s32(e.r(4))))
    e.set_r(3, e.cl.w["city"])


def stub_disable(e):
    e.cl.rec("DisableVote", (e.r(3) & 0xFF,))


def stub_connect(e):
    e.cl.rec("Connect", (plab(e, e.r(3)), *p_csig(e, e.r(4)), plab(e, e.r(5)), e.r(6) & 0xFFFF, plab(e, e.r(7))))
    e.set_r(3, e.cl.ret("Connect") & MASK)


def stub_session(e):
    e.cl.rec("Session", (plab(e, e.r(3)), s16(e.r(4)), s32(e.r(5)), s32(e.r(6)), plab(e, e.r(7))))


def stub_sendvote(e):
    d = e.r(5)
    e.cl.rec("SendVote", (plab(e, e.r(3)), s32(e.r(4)), e.u32(d), e.u32(d + 4), e.u32(d + 8), e.u32(d + 12), s32(e.r(6)),
                          plab(e, e.r(7))))


def stub_setswear(e):
    e.cl.rec("SetSwear", (plab(e, e.r(3)), e.r(4) & 0xFF))


for _addr, _fn in [
    (BSTRVAL, stub_bstrval), (WCTOR, stub_wctor), (WDTOR, stub_wdtor), (ASSIGN, stub_assign), (NCOPY, stub_ncopy), (NDTOR, stub_ndtor),
    (EQUAL, stub_equal), (SETFIND, finder("SetFind")), (BUDDYFIND, finder("BuddyFind")), (INCREMENT, stub_increment),
    (FEEDBACK, stub_feedback), (UDEBUG, stub_udebug), (DIALOG, stub_dialog), (SYSCTOR, one("SysCtor")),
    (GETFTP, answer_block("GetFTP", "park")), (CITYINFO, stub_city), (PASSWORD, answer_block("Password", "password")),
    (CUSTOMER, answer_queue("CustomerID", "CustomerID")), (PLAYER, answer_queue("PlayerID", "PlayerID")), (DISABLEVOTE, stub_disable),
    (MSCTOR, one("MSCtor")), (ISSWEAR, answer_queue("IsSwear", "Swear")), (SETSWEAR, stub_setswear),
]:
    STUBS.internal(_addr, _fn)
for _addr, _fn in [
    (NEW, stub_new), (FORMAT, stub_format), (NOTAFK, chat_two("NotAFK")), (SAY, chat_text("Say")), (TELL, stub_tell),
    (SHOUT, chat_text("Shout")), (RANGE, stub_range), (EAROFF, chat_two("EarOff")), (EARON, chat_two("EarOn")), (AFK, chat_two("AFK")),
    (UNIGNORE, chat_name("UnIgnore")), (IGNORE, chat_name("Ignore")), (BLACKMARK, chat_name("Blackmark")),
    (REMOVEBUDDY, chat_name("RemoveBuddy")), (ADDBUDDY, chat_name("AddBuddy")), (PAGE, chat_name("Page")), (GOTO, chat_name("Goto")),
    (LOCATE, chat_name("Locate")), (VOTECTOR, one("VoteCtor")), (CONNECT, stub_connect), (SESSION, stub_session),
    (SENDVOTE, stub_sendvote),
]:
    STUBS.internal(_addr, glue(_fn))


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    """What the shared code needs from either memory."""

    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def rd_u16(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dH" % n, self.read(a, 2 * n))) if n else []

    def wtext(self, text):
        units = [ord(ch) for ch in text] + [0]
        a = self.alloc(2 * len(units))
        self.write(a, struct.pack(self.fmt[0] + "%dH" % len(units), *units))
        return a

    def wstr(self, text):
        """A std::wstring's rep for text; answers its address."""
        data = self.wtext(text)
        rep = self.alloc(24)
        self.words(rep, [len(text), len(text), 1])
        self.ptr(rep + self.REP_DATA, data)
        return rep

    def wsig(self, slot):
        rep = self.unp32(self.rd_word(slot))
        n = self.rd_word(rep)
        return [n, fnv(self.rd_u16(self.rd_ptr(rep + self.REP_DATA), min(n, 32)))]


class Ppc(Side):
    """One case on the PowerPC side: the fakes active, a fresh log, memory in the emulator's image."""
    fmt = ">I"
    REP_DATA = 12

    def __init__(self, e, case):
        self.e = e
        self.case = case
        self.w = {}
        self.saved = None

    def start(self, labels):
        self.L = self.e.cl = CL.PpcLog(dict(labels), self.case["queues"])
        self.L.news = {}
        self.L.next_new = 100
        self.L.w = self.w
        self.saved = STUBS.activate(self.e)

    def done(self):
        if self.saved is not None:
            CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def alloc(self, n):
        a = self.e.alloc(n)
        self.e.write(a, bytes(n))
        return a

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def p32(self, a):
        return a

    def unp32(self, w):
        return w

    def ptr(self, a, p):
        self.words(a, [p])

    def rd_ptr(self, a):
        return self.rd_word(a)

    def setp(self, name, a):
        if name == "table":
            self.words(GTEXT_TABLE, [a])
        else:
            self.w[name] = a

    def server(self, text, port):
        data = self.alloc(len(text) + 1)
        self.write(data, text.encode() + b"\0")
        rep = self.alloc(16)
        self.words(rep, [len(text), len(text), 1, data])
        self.words(GONLINECONFIG + 0x14, [rep])
        self.write(GONLINECONFIG + 0x22, struct.pack(">h", port))

    def lab(self, a):
        return plab(self.e, a)

    def log(self):
        return self.L.log

    def call(self, this, vec):
        T.call_ppc(self.e, FN_DOCOMMAND, [this, vec])


class Host(Side):
    """The same on the host library (32-bit words are offsets from the arena base; native pointers are 8 bytes)."""
    fmt = "<I"
    REP_DATA = 16

    def __init__(self, lib, case):
        self.lib = lib
        self.case = case
        bind(lib)
        self.base = lib.tpw_dc_base()

    def start(self, labels):
        CL.host_begin(self.lib, {v: k for k, v in labels.items()}, self.case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_dc_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_dc_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def p32(self, a):
        return a - self.base if a else 0

    def unp32(self, w):
        return self.base + w if w else 0

    def ptr(self, a, p):
        self.write(a, struct.pack("<Q", p))

    def rd_ptr(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0]

    def setp(self, name, a):
        self.lib.tpw_dc_set(name.encode(), a)

    def server(self, text, port):
        self.lib.tpw_dc_set_server(text.encode(), port)

    def lab(self, a):
        return self.lib.tpw_dc_lab(a)

    def log(self):
        return CL.host_end(self.lib)

    def call(self, this, vec):
        self.lib.tpw_dc_call(this, vec)


def bind(lib):
    """The host library's entry points, typed once; the layout check runs here."""
    if getattr(lib, "bound", False):
        return
    p = "tpw_dc_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "base").restype, getattr(lib, p + "base").argtypes = C.c_uint64, []
    getattr(lib, p + "set").restype, getattr(lib, p + "set").argtypes = None, [C.c_char_p, C.c_uint64]
    getattr(lib, p + "set_server").restype, getattr(lib, p + "set_server").argtypes = None, [C.c_char_p, C.c_int32]
    getattr(lib, p + "lab").restype, getattr(lib, p + "lab").argtypes = C.c_int64, [C.c_uint64]
    getattr(lib, p + "call").restype, getattr(lib, p + "call").argtypes = None, [C.c_uint64, C.c_uint64]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, c):
    """Writes the case's world through Side (the same code for both memories; offsets are the Mac's). Answers the blocks and labels."""
    w = {}
    this = w["this"] = s.alloc(0xE8C)
    chat = s.alloc(16)
    s.words(this, [s.p32(chat)])
    s.words(this + 8, [s.p32(s.wstr(c["me"]))])
    s.words(this + 0xE20, [s.p32(s.wstr(c["e20"])), s.p32(s.wstr(c["e24"]))])
    s.words(this + 0xE88, [s.p32(s.wstr(c["e88"]))])
    s.write(this + 0xE50, bytes([c["e50"], c["e51"]]))
    s.write(this + 0xE84, bytes([c["e84"]]))
    s.words(this + 0xE64, [c["e64"]])
    nodes = [s.alloc(16) for _ in c["keys"]]
    for i, node in enumerate(nodes):
        nxt = nodes[i + 1] if i + 1 < len(nodes) else this + 0xE2C
        s.words(node + 8, [s.p32(nxt), s.p32(s.wstr(c["keys"][i]))])
    s.words(this + 0xE30, [s.p32(nodes[0] if nodes else this + 0xE2C)])
    data = s.alloc(36)
    for i in range(3):
        s.words(data + 12 * i, [0, s.p32(s.wstr(c["args"][i])), c["cmds"][i]])
    vec = w["vec"] = s.alloc(16)
    s.words(vec, [3, c["count"]])
    s.ptr(vec + 8, data)
    table, park, city, found = s.alloc(16), s.alloc(0x50), s.alloc(0x64), s.alloc(16)
    password = s.wtext("secret")
    s.words(park + 8, [c["parkid"]])
    s.words(park + 0x48, [c["cityid"]])
    s.words(city + 0x60, [c["city60"]])
    for name, a in (("table", table), ("park", park), ("city", city), ("password", password), ("found", found)):
        s.setp(name, a)
    labels = {this: 1, table: 2, this + 8: 3, this + 0xE20: 4, this + 0xE24: 5, this + 0xE88: 6, this + 0xE34: 7, this + 0xE44: 8,
              chat: 9, data: 10, data + 12: 11, data + 24: 12, this + 0x220: 13, park: 15, city: 16, password: 17, found: 19,
              s.g("GotoPark_OK"): 20, s.g("VoteNotifyCallback"): 21}
    for i, node in enumerate(nodes):
        labels[node] = 30 + i
        labels[node + 0xC] = 40 + i
    system = 0
    if c["sys"] is not None:
        system = s.alloc(0x404)
        s.words(system + 0xE0, [c["sys"]])
        labels[system] = 14
    s.ptr(s.g("gOnlineSystem"), system)
    save = 0
    if c["ms"]:
        save = s.alloc(0x4C)
        labels[save] = 18
    s.ptr(s.g("gMasterSave"), save)
    s.server(c["server"], c["port"])
    s.words(s.g("gVoteSendData"), c["vote"])
    return w, labels


def observe(s, w):
    """Everything the call may change, read back the same way on both sides; pointers as labels. The log last (it ends the host's session)."""
    this = w["this"]
    out = {
        "flags": list(s.read(this + 0xE50, 2)),
        "vote": s.lab(s.unp32(s.rd_word(this + 0xE64))),
        "e20": s.wsig(this + 0xE20),
        "e24": s.wsig(this + 0xE24),
        "e88": s.wsig(this + 0xE88),
        "buffer": s.rd_u16(this + 0x220, 4),
        "system": s.lab(s.rd_ptr(s.g("gOnlineSystem"))),
        "save": s.lab(s.rd_ptr(s.g("gMasterSave"))),
    }
    out["log"] = s.log()
    return out


def run(s, case):
    try:
        w, labels = build(s, case)
        s.start(labels)
        s.call(w["this"], w["vec"])
        return observe(s, w)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

NAMES = ["Alice", "bob", "Carol the Great", "x", "Dave", "AbCdEfGhIjKlMnOpQrStUvWxYz0123456789"]


def text(r, empty):
    return "" if r.random() < empty else r.choice(NAMES)


def make(r, cmd=None, count=None):
    if cmd is None:
        cmd = r.randrange(0x19) if r.random() < 0.92 else r.choice([0x19, 0x40, 0x7FFFFFFF, 0xFFFFFFFF])
    return {
        "cmd": cmd,
        "count": r.choice([0, 1, 2, 2, 3]) if count is None else count,
        "me": text(r, 0.1), "e20": text(r, 0.4), "e24": text(r, 0.4), "e88": text(r, 0.3),
        "args": [text(r, 0.2), text(r, 0.3), text(r, 0.3)],
        "cmds": [cmd, r.choice([0, 1, 5, 100, 0xFFFFFFFF, r.getrandbits(32)]), r.getrandbits(32)],
        "e50": r.choice([0, 1, 0x80]), "e51": r.choice([0, 1, 0x80]), "e84": r.choice([0, 1, 2]),
        "e64": r.choice([0, 0, 0x1234]),
        "keys": [text(r, 0.1) for _ in range(r.randrange(4))],
        "sys": r.choice([None, r.getrandbits(32), 0]),
        "ms": r.choice([0, 1]),
        "parkid": r.getrandbits(32), "cityid": r.getrandbits(32), "city60": r.choice([0, 0, 1, 0x80000000]),
        "server": r.choice(["vote.bullfrog.co.uk", "", "10.0.0.1"]), "port": r.choice([0, 8080, -1, 0x7FFF, -0x8000]),
        "vote": [r.getrandbits(32) for _ in range(4)],
        "queues": {
            "Equal": [r.choice([0, 1]) for _ in range(6)],
            "SetFind": [r.choice([0, 1])],
            "BuddyFind": [r.choice([0, 1])],
            "Connect": [r.choice([1, 1, 0, -1, 2])],
            "Swear": [r.choice([0, 1, 0x100])],
            "CustomerID": [s32(r.getrandbits(32))],
            "PlayerID": [s32(r.getrandbits(32))],
            "VoteNull": [r.choice([0, 0, 0, 1])],
        },
    }


def edges():
    """Every command (and numbers past the table) with each argument count, several worlds each."""
    r = random.Random(5636)
    for cmd in list(range(0x19)) + [0x19, 0xFFFFFFFF]:
        for count in (0, 1, 2, 3):
            for _ in range(3):
                yield make(r, cmd, count)


def random_case(r):
    return make(r)


TESTS = [
    FnTest("COnlineControl::DoCommand", ppc, host, edge=edges, rnd=random_case),
]
