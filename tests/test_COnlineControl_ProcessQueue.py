"""Unit v5695, COnlineControl::ProcessQueue, AddBuddy and LoadBuddy, against libtpw_unit_COnlineControl_ProcessQueue.dylib
(tests/unit_COnlineControl_ProcessQueue.cpp), main app ("SimTheme Park"),
src/game/online/online.cpp:
  ProcessQueue 0x10214c34, AddBuddy 0x1021baf8, LoadBuddy 0x1021d004

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. Every callee outside
the unit (the mutex, the block-table index, the vote/response/init handlers, the deque pop, the buddy map find and insert, the nocase and
wide string copies, the UI calls, the stream, filebuf, locale and facet calls, the path string calls, sprintf, strcpy, operator new[] and the
exception throw) is a fake on both sides that records its arguments (pointers as labels, texts as length and weighted sum) and answers from
the case's queues.

The world of a case: the COnlineControl block (chat word, player name string, ready byte, the deque of Argument vectors in a block table, the
buddy map's end node), gOnlineControl and gUserInfo (set or NULL), the path text, the stream state words the ifstream constructor leaves
(streambuf, exceptions, state), and the answer queues (find, GetDataForUser, open, close, widen, getline codes). What is compared: the answer,
the event log (which holds the exact sequence and number of every call), the labels of gOnlineControl and gUserInfo, the deque words, the buddy
node's flag bytes, and the stream's state, streambuf and exceptions words.

No fused operations and no floats. Not tested: a deque block size of 0 (the original divides by it unconditionally), a LoadBuddy line
longer than the 32 wide characters the fake getline writes (the fake decides the text, not the original).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_COnlineControl_ProcessQueue"
MASK = 0xFFFFFFFF

# ---- the unit's own functions (address, size from the MANIFEST); OWN runs as the original even where other modules left a stub
FN_PQ, FN_AB, FN_LB = 0x10214C34, 0x1021BAF8, 0x1021D004
SIZES = {FN_PQ: 536, FN_AB: 440, FN_LB: 1128}
OWN = set(SIZES)

# ---- the callees faked on both sides
LOCK, UNLOCK = 0x10200D8C, 0x10200DE0
CDEQUE_INDEX, OPERATOR_NEW, COC_CTOR = 0x101F2004, 0x102384B8, 0x10213870
PROCESS_VOTE, PROCESS_RESPONSE, PROCESS_INIT, POP_FRONT = 0x1021C3CC, 0x10215560, 0x10214E80, 0x1022488C
NOCASE_COPY, TREE_FIND, NOCASE_DTOR, TREE_INSERT = 0x1022213C, 0x1021F8D8, 0x10222020, 0x10222D9C
GET_DATA, UI_ADD_BUDDY, UI_UPDATE = 0x1021B854, 0x101F0B08, 0x101F0AA0
IFSTREAM_CTOR, STRING_CTOR, USERINFO_CTOR, RETURN_PATH = 0x10221E20, 0x1007363C, 0x1017DE54, 0x1017F980
PATH_COPY, PATH_DTOR, PATH_APPEND_STR, PATH_APPEND_TEXT = 0x102387A0, 0x10238710, 0x10238788, 0x1023A8A0
CSTR_ASSIGN, CSTR_APPEND, SPRINTF, FILEBUF_OPEN = 0x101B2440, 0x1020100C, 0x10238578, 0x10221C30
NEW_ARRAY, REFPTR_CTOR, STRCPY, MSL_THROW = 0x10238668, 0x100026AC, 0x10238410, 0x10238680
WSTR_FROM, WSTR_COPY, WSTR_DTOR = 0x10146A14, 0x101F7424, 0x101468F8
IOS_GETLOC, USE_FACET, PTR_GLUE, LOCALE_DTOR = 0x10072A34, 0x1021EB10, 0x1023BAA8, 0x10071F34
GETLINE, FILEBUF_CLOSE, STRING_DTOR = 0x102202B0, 0x10221B40, 0x1000221C
FILEBUF_DTOR, ISTREAM_DTOR, IOS_DTOR = 0x1021CD4C, 0x1021D4A0, 0x1021CBB8

ADDR = dict(gOnlineControl=0x10396D28, gUserInfo=0x10380B34, gTPGlobals=0x103B74C8, gUIInterface=0x10396440,
            ifs=0x102B1E40, iosvtbl=0x1029D084, iosdtor=0x102661D0)
# ---- the host's offsets and sizes the module relies on, checked once against tpw_pq_layout (catches a header that moved)
HOST_LAYOUT = [0x28, 0x38, 0x3C, 0x48, 8, 8, 0x10, 16]

TEXTS = ["alice", "Bob", "", "abcdefghijklmnopqrstuvwxyz01234", "Carol D"]     # the fake getline's lines (code k writes TEXTS[k-1])
NAMES = ["", "x", "Alice", "bob smith", "CAROL", "a long buddy name 0123456789", "été"]


def s32(v):
    return T.s32(v)


def s16(v):
    v &= 0xFFFF
    return v - 0x10000 if v & 0x8000 else v


def s8(v):
    v &= 0xFF
    return v - 0x100 if v & 0x80 else v


def sig(vals):
    return (len(vals), sum((i + 1) * v for i, v in enumerate(vals)) & 0x7FFFFFFF)


def sigw(text):
    return sig([ord(c) for c in text])


def rw(e, a):
    return struct.unpack(">I", e.read(a, 4))[0]


def ww(e, a, v):
    e.write(a, struct.pack(">I", v & MASK))


def cstr(e, a):
    out = []
    for i in range(400):
        b = e.read(a + i, 1)[0]
        if b == 0:
            break
        out.append(b)
    return out


def wtext(e, a):
    out = []
    for i in range(200):
        c = struct.unpack(">H", e.read(a + 2 * i, 2))[0]
        if c == 0:
            break
        out.append(chr(c))
    return "".join(out)


def sigb(e, a):
    return sig(cstr(e, a))


def sigwm(e, a):
    return sigw(wtext(e, a))


ST = {}      # the current PowerPC side (blocks the fakes name)


def rel(a):
    return s32(a - ST["s"].this)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub(addr, fn):
    if addr >= 0x10238000:
        # an imported function: the glue saves the caller's TOC pointer at 0x14(r1) and the caller loads it back after the call
        inner = fn

        def fn(e):
            ww(e, e.r(1) + 0x14, e.r(2))
            inner(e)
    STUBS.internal(addr, fn)
    return fn


def mkrep(e, obj, text):
    data = e.alloc(2 * len(text) + 2)
    e.write(data, text.encode("utf-16-be") + b"\0\0")
    rep = e.alloc(0x20)
    ww(e, rep + 0xC, data)
    ww(e, obj, rep)


def textof(e, obj):
    return wtext(e, rw(e, rw(e, obj) + 0xC))


def rec(e, name, *vals):
    e.cl.rec(name, tuple(vals))


def reg(addr, name, fn=None):
    def deco(f):
        stub(addr, f)
        return f
    return deco


@reg(LOCK, "lock")
def _lock(e):
    rec(e, "lock", rel(e.r(3)))


@reg(UNLOCK, "unlock")
def _unlock(e):
    rec(e, "unlock", rel(e.r(3)))


@reg(CDEQUE_INDEX, "CdequeIndex")
def _cdeque_index(e):
    idx = e.r(4) & MASK
    rec(e, "CdequeIndex", idx)
    e.set_r(3, (rw(e, e.r(3) + 0xC) + 4 * idx) & MASK)


@reg(OPERATOR_NEW, "OperatorNew")
def _operator_new(e):
    size = e.r(3) & MASK
    rec(e, "OperatorNew", size)
    a = e.alloc(size)
    e.write(a, bytes(size))
    e.set_r(3, a)


@reg(COC_CTOR, "COnlineControl")
def _coc_ctor(e):
    rec(e, "COnlineControl", e.cl.newlab(e.r(3)))


@reg(PROCESS_VOTE, "ProcessVoteResponse")
def _vote(e):
    rec(e, "ProcessVoteResponse", e.cl.lab(e.r(3)), s32(e.r(4)))


def log_vector(e, name):
    v = e.r(4)
    cap, size, data = rw(e, v), rw(e, v + 4), rw(e, v + 8)
    command = s32(rw(e, data + 8)) if size != 0 else -1
    rec(e, name, e.cl.lab(e.r(3)), s32(cap), s32(size), command)


stub(PROCESS_RESPONSE, lambda e: log_vector(e, "ProcessResponse"))
stub(PROCESS_INIT, lambda e: log_vector(e, "ProcessInitResponse"))


@reg(GET_DATA, "GetDataForUser")
def _get_data(e):
    rec(e, "GetDataForUser", e.cl.lab(e.r(3)), e.cl.lab(e.r(4)))
    e.set_r(3, e.cl.ret("GetDataForUser") & MASK)


@reg(POP_FRONT, "PopFront")
def _pop_front(e):
    d = e.r(3)
    rec(e, "PopFront", rel(d))
    ww(e, d + 0x14, rw(e, d + 0x14) + 1)
    ww(e, d + 0x18, rw(e, d + 0x18) - 1)


@reg(STRING_CTOR, "StringCtor")
def _string_ctor(e):
    rec(e, "StringCtor", e.cl.newlab(e.r(3)))


@reg(STRING_DTOR, "StringDtor")
def _string_dtor(e):
    rec(e, "StringDtor", e.cl.lab(e.r(3)), s16(e.r(4)))


@reg(WSTR_FROM, "WStringCtorFrom")
def _wstr_from(e):
    obj, text = e.r(3), wtext(e, e.r(4))
    n, sm = sigw(text)
    rec(e, "WStringCtorFrom", e.cl.newlab(obj), n, sm)
    mkrep(e, obj, text)


@reg(WSTR_COPY, "WStringCopy")
def _wstr_copy(e):
    obj, other = e.r(3), e.r(4)
    rec(e, "WStringCopy", e.cl.newlab(obj), e.cl.lab(other), e.r(5) & MASK, e.r(6) & MASK)
    mkrep(e, obj, textof(e, other))


@reg(WSTR_DTOR, "WStringDtor")
def _wstr_dtor(e):
    rec(e, "WStringDtor", e.cl.lab(e.r(3)), s16(e.r(4)))


@reg(NOCASE_COPY, "NocaseWStringCopy")
def _nocase_copy(e):
    rec(e, "NocaseWStringCopy", e.cl.newlab(e.r(3)), e.cl.lab(e.r(4)), e.r(5) & MASK, e.r(6) & MASK)


@reg(NOCASE_DTOR, "NocaseWStringDtor")
def _nocase_dtor(e):
    rec(e, "NocaseWStringDtor", e.cl.lab(e.r(3)), s16(e.r(4)))


@reg(TREE_FIND, "BuddyTreeFind")
def _tree_find(e):
    out, tree = e.r(3), e.r(4)         # an iterator returned through r3
    rec(e, "BuddyTreeFind", rel(tree), e.cl.lab(e.r(5)))
    ww(e, out, ST["s"].node if e.cl.ret("find") != 0 else tree + 4)


@reg(TREE_INSERT, "BuddyTreeInsert")
def _tree_insert(e):
    entry = e.r(5)
    flags = e.read(entry + 4, 2)
    rec(e, "BuddyTreeInsert", rel(e.r(4)), e.cl.lab(entry), flags[0], flags[1])


@reg(UI_ADD_BUDDY, "OnlineAddBuddy")
def _ui_add(e):
    n, sm = sigwm(e, e.r(4))
    rec(e, "OnlineAddBuddy", e.cl.lab(e.r(3)), s32(e.r(5)), n, sm)


@reg(UI_UPDATE, "OnlineUpdateContact")
def _ui_update(e):
    n, sm = sigwm(e, e.r(4))
    rec(e, "OnlineUpdateContact", e.cl.lab(e.r(3)), s32(e.r(5)), n, sm)


@reg(IFSTREAM_CTOR, "IfstreamCtor")
def _ifstream_ctor(e):
    s = e.r(3)
    rec(e, "IfstreamCtor", e.cl.newlab(s), s32(e.r(4)))
    ios = ST["s"].ios
    e.write(ios, bytes(0x60))
    ww(e, s, ios)
    ww(e, ios + 0x24, e.cl.ret("sb0"))
    ww(e, ios + 0x38, e.cl.ret("exc"))
    ww(e, ios + 0x34, e.cl.ret("state0"))


@reg(USERINFO_CTOR, "UserInfoCtor")
def _userinfo_ctor(e):
    rec(e, "UserInfoCtor", e.cl.newlab(e.r(3)))


@reg(RETURN_PATH, "ReturnOnlinePath")
def _return_path(e):
    r = e.r(3)
    rec(e, "ReturnOnlinePath", e.cl.newlab(r), e.cl.lab(e.r(4)))
    pt = ST["s"].pathtext
    ww(e, r + 4, len(wtext(e, pt)))
    ww(e, r + 8, pt)


@reg(PATH_COPY, "PathCopy")
def _path_copy(e):
    a, b = e.r(3), e.r(4)
    rec(e, "PathCopy", e.cl.newlab(a), e.cl.lab(b))
    ww(e, a + 4, rw(e, b + 4))
    ww(e, a + 8, rw(e, b + 8))


@reg(PATH_DTOR, "PathDtor")
def _path_dtor(e):
    rec(e, "PathDtor", e.cl.lab(e.r(3)), s16(e.r(4)))


@reg(PATH_APPEND_STR, "PathAppendString")
def _path_append_string(e):
    rec(e, "PathAppendString", e.cl.lab(e.r(3)), s32(e.r(4) - ADDR["gTPGlobals"]))


@reg(PATH_APPEND_TEXT, "PathAppendText")
def _path_append_text(e):
    n, sm = sigwm(e, e.r(4))
    rec(e, "PathAppendText", e.cl.lab(e.r(3)), n, sm)


@reg(CSTR_ASSIGN, "CharStringAssign")
def _cstr_assign(e):
    n, sm = sigb(e, e.r(4))
    rec(e, "CharStringAssign", e.cl.lab(e.r(3)), e.r(5) & MASK, n, sm)


@reg(CSTR_APPEND, "CharStringAppend")
def _cstr_append(e):
    n, sm = sigb(e, e.r(4))
    rec(e, "CharStringAppend", e.cl.lab(e.r(3)), e.r(5) & MASK, n, sm)


@reg(SPRINTF, "SprintfWide")
def _sprintf(e):
    fn, fs = sigb(e, e.r(4))
    text = wtext(e, e.r(5))
    n, sm = sigw(text)
    rec(e, "SprintfWide", fn, fs, n, sm)
    e.write(e.r(3), text.encode("latin-1", "replace") + b"\0")


@reg(FILEBUF_OPEN, "FilebufOpen")
def _filebuf_open(e):
    n, sm = sigb(e, e.r(4))
    rec(e, "FilebufOpen", s32(e.r(5)), n, sm)
    e.set_r(3, e.cl.ret("FilebufOpen") & MASK)


@reg(FILEBUF_CLOSE, "FilebufClose")
def _filebuf_close(e):
    rec(e, "FilebufClose", 0)
    e.set_r(3, e.cl.ret("FilebufClose") & MASK)


@reg(NEW_ARRAY, "NewArray")
def _new_array(e):
    size = e.r(3) & MASK
    rec(e, "NewArray", size)
    a = e.alloc(size)
    e.write(a, bytes(size))
    e.set_r(3, a)


@reg(REFPTR_CTOR, "RefPtrCtor")
def _refptr_ctor(e):
    rec(e, "RefPtrCtor", 0)
    ww(e, e.r(3), e.r(4))


@reg(STRCPY, "StrCpy")
def _strcpy(e):
    to, frm = e.r(3), e.r(4)
    n, sm = sigb(e, frm)
    rec(e, "StrCpy", n, sm)
    e.write(to, bytes(cstr(e, frm)) + b"\0")
    e.set_r(3, to)


@reg(FILEBUF_DTOR, "FilebufDtor")
def _filebuf_dtor(e):
    rec(e, "FilebufDtor", s16(e.r(4)))


@reg(ISTREAM_DTOR, "IstreamDtor")
def _istream_dtor(e):
    s = e.r(3)
    ios = rw(e, s)
    rec(e, "IstreamDtor", e.cl.lab(s), s16(e.r(4)), s32(rw(e, s + 8) - ADDR["ifs"]), s32(rw(e, ios) - ADDR["ifs"]))


@reg(IOS_DTOR, "IosDtor")
def _ios_dtor(e):
    ios = ST["s"].ios
    consistent = 1 if ((ios + rw(e, ios + 0x44)) & MASK) == e.r(3) else 0
    rec(e, "IosDtor", s16(e.r(4)), consistent)


@reg(IOS_GETLOC, "IosGetloc")
def _ios_getloc(e):
    rec(e, "IosGetloc", e.cl.newlab(e.r(3)), e.cl.lab(e.r(4)))


@reg(USE_FACET, "UseFacet")
def _use_facet(e):
    rec(e, "UseFacet", e.cl.lab(e.r(3)))
    e.set_r(3, ST["s"].facet)


@reg(PTR_GLUE, "CtypeWiden")
def _widen(e):
    rec(e, "CtypeWiden", e.cl.lab(e.r(3)), s8(e.r(4)))
    e.set_r(3, e.cl.ret("widen") & 0xFFFF)


@reg(LOCALE_DTOR, "LocaleDtor")
def _locale_dtor(e):
    rec(e, "LocaleDtor", e.cl.lab(e.r(3)), s16(e.r(4)))


@reg(GETLINE, "Getline")
def _getline(e):
    s, buf = e.r(3), e.r(4)
    rec(e, "Getline", e.cl.lab(s), s32(e.r(5)), e.r(6) & 0xFFFF)
    k = e.cl.ret("getline")
    if k > 0:
        e.write(buf, TEXTS[k - 1].encode("utf-16-be") + b"\0\0")
    else:
        ww(e, rw(e, s) + 0x24, 0)
    e.set_r(3, s)


@reg(MSL_THROW, "MslThrow")
def _throw(e):
    obj = e.r(4)
    n, sm = sigb(e, e.r(3))
    rec(e, "MslThrow", n, sm)
    mn, ms = sigb(e, rw(e, obj + 4))
    rec(e, "MslThrowObject", s32(rw(e, obj) - ADDR["iosvtbl"]), mn, ms, s32(e.r(5) - ADDR["iosdtor"]))


def setup(runner):
    T.install_std_stubs(runner.emu)       # the C library's memset, memcpy, strlen ... (the fakes above override strcpy, sprintf, new[])


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    """What the shared code needs from either memory."""

    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def blocks(self):
        self.this = self.alloc(0xE8C)
        self.other = self.alloc(0x10)
        self.ublk = self.alloc(0x20)

    def text_bytes(self, text):
        return text.encode(self.wenc) + b"\0\0"


class Ppc(Side):
    """One case on the PowerPC side: the fakes active, a fresh log, memory in the emulator's image."""
    fmt = ">I"
    wenc = "utf-16-be"
    ios_state, ios_sb, ios_exc = 0x34, 0x24, 0x38

    def __init__(self, e, case):
        self.e = e
        self.blocks()
        self.ios = e.alloc(0x60)
        self.facet = e.alloc(0x80)
        vt = e.alloc(0x40)
        e.write(self.facet, bytes(0x80))
        ww(e, self.facet, vt)
        self.node = e.alloc(0x40)
        self.pathtext = e.alloc(0x100)
        ST["s"] = self
        fixed = {self.this: 1, ADDR["gUIInterface"]: 2, self.other: 3, self.ios: 4, self.facet: 5, self.ublk: 6}
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return {"node": self.node, "ios": self.ios, "pathtext": self.pathtext}.get(name) or ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def word_for(self, a):
        return a

    def set_ptr(self, a, v):
        ww(self.e, a, v)

    def mkstr(self, text):
        obj = self.alloc(8)
        self.write(obj, bytes(8))
        mkrep(self.e, obj, text)
        return obj, rw(self.e, obj)

    def glab(self, name):
        v = rw(self.e, ADDR[name])
        return self.L.lab(v) if v else 0

    def call(self, fn, args):
        return T.call_ppc(self.e, fn, [a & MASK for a in args])[0] & MASK


class Host(Side):
    """The same on the host library."""
    fmt = "<I"
    wenc = "utf-16-le"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.blocks()
        ST["s"] = self
        fixed = {self.this: 1, self.g("gUIInterface"): 2, self.other: 3, self.g("ios"): 4, self.g("facet"): 5, self.ublk: 6}
        CL.host_begin(lib, {v: k for k, v in fixed.items()}, case["queues"])
        off = lib.layout
        self.ios_state, self.ios_sb, self.ios_exc = off[1], off[0], off[2]

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_pq_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_pq_alloc(n)       # zeroed, 32-bit addressable (platform/arena.cpp)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def word_for(self, a):
        return self.lib.tpw_pq_word(a)

    def set_ptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def mkstr(self, text):
        obj, rep = self.alloc(8), self.alloc(0x20)
        data = self.alloc(2 * len(text) + 2)
        self.write(data, self.text_bytes(text))
        self.set_ptr(rep + 0x10, data)
        self.set_ptr(obj, rep)
        return obj, rep

    def glab(self, name):
        return (self.lib.tpw_pq_label_online if name == "gOnlineControl" else self.lib.tpw_pq_label_user)()

    def call(self, fn, args):
        f = {FN_PQ: self.lib.tpw_pq_ProcessQueue, FN_AB: self.lib.tpw_pq_AddBuddy, FN_LB: self.lib.tpw_pq_LoadBuddy}[fn]
        return f(*args) & MASK


def bind(lib):
    """The host library's entry points, typed once; the layout check runs here."""
    if getattr(lib, "bound", False):
        return
    lib.tpw_pq_addr.restype, lib.tpw_pq_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_pq_alloc.restype, lib.tpw_pq_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_pq_word.restype, lib.tpw_pq_word.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_pq_label_online.restype = C.c_int64
    lib.tpw_pq_label_user.restype = C.c_int64
    lib.tpw_pq_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_pq_ProcessQueue.restype, lib.tpw_pq_ProcessQueue.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_pq_AddBuddy.restype, lib.tpw_pq_AddBuddy.argtypes = C.c_uint32, [C.c_uint64, C.c_uint64, C.c_int32]
    lib.tpw_pq_LoadBuddy.restype, lib.tpw_pq_LoadBuddy.argtypes = C.c_uint32, [C.c_uint64, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_pq_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.layout = list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    """Writes the case's globals and blocks through Side: the same code for both memories (offsets are the Mac's)."""
    this = s.this
    s.write(this, bytes(0xE8C))
    s.write(s.other, bytes(0x10))
    s.write(s.ublk, bytes(0x20))
    s.words(this, [case["chat"]])
    s.set_ptr(s.g("gOnlineControl"), s.other if case.get("pre") else 0)
    s.set_ptr(s.g("gUserInfo"), s.ublk if case.get("preuser") else 0)
    s.write(s.g("ios"), bytes(0x60))
    node = s.g("node")
    s.write(node, bytes(0x40))
    s.write(node + 0x10, bytes([case["n0"], case["n1"], 0x77]))
    s.write(s.g("pathtext"), s.text_bytes(case["path"]))
    s.words(this + 0xE68, [case["bs"]])
    s.words(this + 0xE7C, [case["first"], len(case["recs"])])
    s.write(this + 0x10, bytes([case["ready"]]))
    bs, first = case["bs"], case["first"]
    nslots = (first + len(case["recs"])) // bs + 1
    table = s.alloc(4 * nslots + 4)
    s.write(table, bytes(4 * nslots + 4))
    blocks = [s.alloc(bs * 0xC + 4) for _ in range(nslots)]
    for b in blocks:
        s.write(b, bytes(bs * 0xC + 4))
    s.words(table, [s.word_for(b) for b in blocks])
    s.words(this + 0xE78, [s.word_for(table)])
    for j, rc in enumerate(case["recs"]):
        idx = first + j
        data = 0
        if rc["data"]:
            data = s.alloc(0x30)
            s.write(data, bytes(0x30))
            s.words(data + 8, [rc["cmd0"]])
            s.words(data + 0x14, [rc["cmd1"]])
        s.words(blocks[idx // bs] + (idx % bs) * 0xC, [rc["cap"], rc["size"], s.word_for(data) if data else 0])
    if case["op"] == "load":
        obj, rep = s.mkstr(case["player"])
        s.words(this + 8, [s.word_for(rep)])
        p = s.alloc(len(case["spath"]) + 1)
        s.write(p, case["spath"].encode() + b"\0")
        case["_p"] = p
    if case["op"] == "add":
        obj, rep = s.mkstr(case["name"])
        case["_n"] = obj


def execute(s, case):
    """The call."""
    op = case["op"]
    if op == "pq":
        s.call(FN_PQ, [s.this])
        return 0
    if op == "add":
        return s.call(FN_AB, [s.this, case["_n"], case["flags"]])
    return s.call(FN_LB, [s.this, case["_p"]])


def observe(s, case, answer):
    """Everything the call may change, read back the same way on both sides; pointers as labels."""
    out = {"answer": answer, "gOC": s.glab("gOnlineControl"), "gUI": s.glab("gUserInfo"),
           "deque": s.rd_words(s.this + 0xE68, 1) + s.rd_words(s.this + 0xE7C, 2), "ready": s.read(s.this + 0x10, 1),
           "node": s.read(s.g("node") + 0x10, 3)}
    if case["op"] == "load":
        ios = s.g("ios")
        out["ios"] = [s.rd_word(ios + s.ios_state), s.rd_word(ios + s.ios_sb), s.rd_word(ios + s.ios_exc)]
    out["log"] = s.log()
    return out


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


def base(r, op):
    """The world every case starts from: random globals; the fakes' answer queues cover every branch."""
    return {"op": op, "chat": r.choice([0, 0x1234, 0x5678]), "pre": r.choice([0, 1]), "preuser": r.choice([0, 1]),
            "n0": r.choice([0, 1]), "n1": r.choice([0, 1]), "path": r.choice(["", "Users", "C:Players"]), "bs": r.randrange(1, 5),
            "first": r.randrange(0, 8), "recs": [], "ready": r.choice([0, 1]),
            "queues": {"find": [r.choice([0, 1]) for _ in range(12)], "GetDataForUser": [r.choice([0, 1, -1, 77, r.randrange(1 << 31)]) & MASK for _ in range(12)],
                       "FilebufOpen": [r.choice([0, 1])], "FilebufClose": [r.choice([0, 1])], "widen": [r.choice([10, 10, 0x41, 0x100A]) for _ in range(30)],
                       "getline": [], "sb0": [r.choice([0, 0x1234])], "exc": [r.choice([0, 0, 1, 4, 5, 7, 0x14])],
                       "state0": [r.choice([0, 0, 0, 1, 2, 4, 5])]}}


def rec_of(r, cmd0=None, size=None, data=None):
    return {"cap": r.randrange(1, 1000), "size": r.choice([0, 1, 2, 3]) if size is None else size, "data": r.choice([0, 1, 1]) if data is None else data,
            "cmd0": r.choice([0x2711, 0, 5, 0x2710, 0x2712, r.randrange(1 << 32)]) if cmd0 is None else cmd0, "cmd1": r.choice([0, 7, 0x2711, r.randrange(1 << 32)])}


# ---- ProcessQueue
def pq_case(r, n=None, **kw):
    c = base(r, "pq")
    c["recs"] = [rec_of(r, **kw) for _ in range(r.choice([0, 1, 1, 2, 3]) if n is None else n)]
    for rc in c["recs"]:
        if rc["size"] != 0:
            rc["data"] = 1
    return c


def pq_edges():
    r = random.Random(5695001)
    yield pq_case(r, 0)
    for pre in (0, 1):
        for ready in (0, 1):
            for cmd0 in (0x2711, 0x2710, 0, 0x80002711):
                for size in (0, 1, 2):
                    c = pq_case(r, 1, cmd0=cmd0, size=size)
                    c["pre"], c["ready"] = pre, ready
                    yield c
    for bs in (1, 2, 3, 4):
        for first in (0, 1, bs - 1, bs, 2 * bs + 1):
            c = pq_case(r, 3)
            c["bs"], c["first"] = bs, first
            yield c


def pq_random(r):
    return pq_case(r)


# ---- AddBuddy
def add_case(r, flags=None, find=None):
    c = base(r, "add")
    c["name"] = r.choice(NAMES)
    c["flags"] = r.randrange(0, 8) if flags is None else flags
    if find is not None:
        c["queues"]["find"] = [find]
    return c


def add_edges():
    r = random.Random(5695002)
    for flags in range(8):
        for find in (0, 1):
            yield add_case(r, flags, find)
    for flags in (8, 0x10, 0xFFFFFFFF, 0x80000000, 6, 0x102):
        yield add_case(r, flags, r.choice([0, 1]))


def add_random(r):
    return add_case(r)


# ---- LoadBuddy
def load_case(r, chat=None, lines=None, **kw):
    c = base(r, "load")
    c["player"] = r.choice(NAMES)
    c["spath"] = r.choice(["", "buddies", "x" * 20])
    if chat is not None:
        c["chat"] = chat
    n = r.randrange(0, 5) if lines is None else lines
    c["queues"]["getline"] = [r.randrange(1, 6) for _ in range(n)] + [0]
    for k, v in kw.items():
        c["queues"][k] = [v]
    return c


def load_edges():
    r = random.Random(5695003)
    yield load_case(r, chat=0)
    for opn in (0, 1):
        for cls in (0, 1):
            for sb0 in (0, 0x1234):
                for exc in (0, 4, 5, 1, 0x14):
                    for st0 in (0, 1, 4, 5, 2):
                        yield load_case(r, chat=0x1234, lines=2, FilebufOpen=opn, FilebufClose=cls, sb0=sb0, exc=exc, state0=st0)
    for lines in (0, 1, 3, 5):
        for k in range(1, 6):
            c = load_case(r, chat=0x1234, lines=lines, FilebufOpen=1, FilebufClose=1, sb0=0x1234, exc=0, state0=0)
            c["queues"]["getline"] = [k] * lines + [0]
            yield c


def load_random(r):
    return load_case(r)


TESTS = [
    FnTest("ProcessQueue", ppc, host, edge=pq_edges, rnd=pq_random),
    FnTest("AddBuddy", ppc, host, edge=add_edges, rnd=add_random),
    FnTest("LoadBuddy", ppc, host, edge=load_edges, rnd=load_random),
]
