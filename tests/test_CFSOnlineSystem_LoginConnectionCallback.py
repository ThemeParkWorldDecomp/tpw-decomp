"""Unit v5924, the online system's login and news jobs, against libtpw_unit_CFSOnlineSystem_LoginConnectionCallback.dylib
(tests/unit_CFSOnlineSystem_LoginConnectionCallback.cpp), main app ("SimTheme Park"),
src/FS_Online_init_CFSOnlineSystem_LoginConnectionCallback.cpp:
  CFSOnlineSystem::LoginConnectionCallback 0x101715a0, DoLogin 0x10171234, LoggedInGetNews 0x10171eb8,
  NewsConnectionCallback 0x10172030, CompleteNews 0x1017253c

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian, 8 byte pointers, string objects with a vptr) for the translation; the
same code reads both back through per-side member offsets. The callees outside the unit are fakes on both sides that record their
arguments (pointers as labels, texts as a hash of their units) and answer from the case's queues: the online library's Auth and News,
the progress bar, the error dialog, the news panel, SetInitialOnlineValues, the string class (assignment, copy, append, length, the
destructor), the file classes (CDiscFile and the access object's Write), CBalanceTable::Load, CText::GetLanguageId,
CSYS_TPFileInfo::GetDataPath, operator delete, free, ReleaseLoginConnection, ReleaseNewsConnection and CompleteLogin. The Mac's virtual
calls (the strings' operator=, the access object's Write, the kept callback) go through __ptr_glue to TVectors that point at hooked
addresses of functions the unit never runs.

The world of a case: the system object (its words, pointers and six strings), the login or news context, the news text buffer, the
online configuration (servers, ports), the data path, the separator and the override file name, and the queues the fakes answer from.
What is compared: the event log and every member of the system the functions may write, plus the context's use count.

No fused operations. Not tested: NewsConnectionCallback with a NULL buffer for answers 0x32 and 0x33 (the original reads it
unconditionally, so the free of a NULL buffer is unreachable), LoginConnectionCallback answer 0x3c with a NULL context.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CFSOnlineSystem_LoginConnectionCallback"
MASK = 0xFFFFFFFF
TOC = 0x10267CC0

FN_LOGINCB, FN_DOLOGIN, FN_GETNEWS, FN_NEWSCB, FN_COMPLETE = 0x101715A0, 0x10171234, 0x10171EB8, 0x10172030, 0x1017253C
SIZES = {FN_LOGINCB: 392, FN_DOLOGIN: 304, FN_GETNEWS: 320, FN_NEWSCB: 452, FN_COMPLETE: 648}
OWN = set(SIZES)

# ---- the callees faked on both sides (the asm's bl targets that are not OWN)
UDEBUG = 0x10000E40
STR_COPYCTOR, STR_APPEND, STR_LENGTH, STR_DTOR = 0x10238308, 0x10238380, 0x10238320, 0x102382F0
FILE_CTOR, FILE_OPEN, FILE_CLOSE, FILE_DTOR = 0x10239688, 0x102396A0, 0x102396B8, 0x102396D0
AUTH_CTOR, AUTH_CONNECT, AUTH_LOGIN = 0x10239760, 0x10239778, 0x10239790
NEWS_CTOR, NEWS_CONNECT, NEWS_GSD, NEWS_GGN, NEWS_GSN = 0x102395E0, 0x10239670, 0x102396E8, 0x10239700, 0x10239718
NEWS_RELEASE, NEWS_DTOR = 0x10238A40, 0x10238A58
GETLANG, GETDATAPATH, BALANCE_LOAD, SETINITIAL = 0x1015E9D8, 0x10004EC4, 0x10020134, 0x10001494
PROGRESS, ADDERROR, NEWSPANEL = 0x101E01F0, 0x101F1DF0, 0x101E3264
OPDELETE, FREE = 0x10238218, 0x10238A70
REL_LOGIN, REL_NEWS, COMPLETE_LOGIN = 0x10171B68, 0x1017249C, 0x10171C08
# addresses of functions of the same file the unit never runs: the targets of the TVectors the Mac's virtual calls reach
HOOK_ASSIGN, HOOK_WRITE, HOOK_KEPT = 0x101710AC, 0x10171114, 0x10171398

# ---- the Mac's addresses of the globals
GUID, CB_LOGIN, CB_NEWS, TABLE, FILEINFO, CONFIG = 0x102A7248, 0x102674A8, 0x102674A0, 0x102B4D5C, 0x102B3458, 0x102B2B70
SEPARATOR, OVERRIDE = 0x103B74C8 + 0x404C, 0x103B74C8 + 0x413C

# The system's members: (name, Mac offset, kind); the host's offsets come from tpw_cfsol_layout in this order.
# kinds: w word, p pointer, a char string object, u wide string object.
FIELDS = [
    ("mLogin", 0x00, "p"), ("mLoginName", 0x0C, "u"), ("mLoginPassword", 0x20, "u"), ("mF34", 0x34, "w"), ("mF38", 0x38, "w"),
    ("mLoginEmail", 0x3C, "u"), ("mF50", 0x50, "w"), ("mF54", 0x54, "w"), ("mF58", 0x58, "w"), ("mF5C", 0x5C, "w"),
    ("mLoggedIn", 0x60, "w"), ("mLoginDone", 0x64, "w"), ("mLoginErrorID", 0x68, "w"), ("mNews", 0x6C, "p"), ("mF70", 0x70, "w"),
    ("mNewsRequest1", 0x74, "w"), ("mNewsDone1", 0x78, "w"), ("mNewsText", 0x7C, "a"), ("mNewsRequest2", 0x90, "w"),
    ("mNewsDone2", 0x94, "w"), ("mF98", 0x98, "u"), ("mNewsRequest3", 0xAC, "w"), ("mNewsDone3", 0xB0, "w"), ("mFB4", 0xB4, "u"),
    ("mLoginCallbackKept", 0xC8, "p"), ("mLoginContextKept", 0xCC, "p"), ("mNewsFinished", 0xD0, "w"),
]
MAC = {n: o for n, o, k in FIELDS}
KIND = {n: k for n, o, k in FIELDS}
IDX = {n: i for i, (n, o, k) in enumerate(FIELDS)}
N_FIELDS = len(FIELDS)
LAYOUT_TAIL = N_FIELDS   # then: sizeof(system), string mLength offset, string mData offset, sizeof(string), login port, news port
MAC_SYS_SIZE = 0x404

WORDS = ["mF34", "mF38", "mF50", "mF54", "mF58", "mF5C", "mLoggedIn", "mLoginDone", "mLoginErrorID", "mF70", "mNewsRequest1",
         "mNewsDone1", "mNewsRequest2", "mNewsDone2", "mNewsRequest3", "mNewsDone3", "mNewsFinished"]
STRINGS = ["mLoginName", "mLoginPassword", "mLoginEmail", "mNewsText", "mF98", "mFB4"]

PERSIST = {}      # the Mac's persistent TVectors, vtables and access object (made in setup)
CUR = {}          # the Mac side of the case being run (the stubs read it)
LAY = []          # the host's layout numbers
SEEN = set()


# ---------------------------------------------------------------------------------------------------------------- texts and hashes

def fnv(data):
    h = 2166136261
    for b in data:
        h = ((h ^ b) * 16777619) & MASK
    return h


def hash_units(units, kind):
    if kind == "a":
        return fnv(units)
    out = []
    for u in units:
        out += [u & 0xFF, (u >> 8) & 0xFF]
    return fnv(out)


def enc(text):
    return [ord(c) for c in text]


def pack_units(units, kind, endian):
    """The units and a terminator, in the side's byte order."""
    if kind == "a":
        return bytes(units) + b"\0\0"
    return struct.pack(endian + "%dH" % (len(units) + 1), *(list(units) + [0]))


# ---------------------------------------------------------------------------------------------------------------- the Mac's memory helpers

def rd32(e, a):
    return struct.unpack(">I", e.read(a, 4))[0]


def rd_units(e, a, kind, cap=4096):
    if a == 0:
        return []
    out = []
    w = 1 if kind == "a" else 2
    while len(out) < cap:
        raw = e.read(a + w * len(out), w)
        u = raw[0] if w == 1 else struct.unpack(">H", raw)[0]
        if u == 0:
            break
        out.append(u)
    return out


def str_set(e, obj, kind, units):
    data = pack_units(units, kind, ">")
    buf = e.alloc(len(data) + 8)
    e.write(buf, data)
    e.write(obj + 4, struct.pack(">I", len(units)))
    e.write(obj + 8, struct.pack(">I", buf))


def str_kind(e, obj):
    return "u" if rd32(e, obj) == PERSIST["vt_w"] else "a"


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub(addr):
    def deco(fn):
        STUBS.internal(addr, fn)
        return fn
    return deco


@stub(UDEBUG)
def stub_udebug(e):
    text = bytes(rd_units(e, e.r(3), "a"))
    if text not in SEEN:
        SEEN.add(text)
        print("UDebug text:", text)
    e.cl.rec("UDebug", (hash_units(list(text), "a"),))


@stub(HOOK_ASSIGN)
def stub_assign(e):
    L = e.cl
    this, text = e.r(3) & MASK, e.r(4) & MASK
    kind = str_kind(e, this)
    units = rd_units(e, text, kind)
    L.rec("AssignW" if kind == "u" else "AssignA", (L.lab(this), hash_units(units, kind)))
    str_set(e, this, kind, units)
    e.set_r(3, this)


@stub(STR_COPYCTOR)
def stub_strcopy(e):
    L = e.cl
    this, src = e.r(3) & MASK, e.r(4) & MASK
    units = rd_units(e, rd32(e, src + 8), "a")
    L.rec("StrCopy", (L.lab(this), hash_units(units, "a")))
    e.write(this, struct.pack(">I", PERSIST["vt_a"]))
    str_set(e, this, "a", units)
    e.set_r(3, this)


@stub(STR_APPEND)
def stub_strappend(e):
    L = e.cl
    this, src = e.r(3) & MASK, e.r(4) & MASK
    add = rd_units(e, rd32(e, src + 8), "a")
    L.rec("StrAppend", (L.lab(this), hash_units(add, "a")))
    cur = rd_units(e, rd32(e, this + 8), "a")
    str_set(e, this, "a", cur + add)
    e.set_r(3, this)


@stub(STR_LENGTH)
def stub_strlen(e):
    e.set_r(3, rd32(e, (e.r(3) & MASK) + 4))


@stub(STR_DTOR)
def stub_strdtor(e):
    e.cl.rec("StrDtor", (e.cl.lab(e.r(3) & MASK),))


@stub(FILE_CTOR)
def stub_file_ctor(e):
    this = e.r(3) & MASK
    e.cl.rec("FileCtor", (e.cl.lab(this),))
    e.write(this + 4, struct.pack(">I", PERSIST["acc"] if CUR["acc"] else 0))
    e.set_r(3, this)


@stub(FILE_OPEN)
def stub_file_open(e):
    L = e.cl
    name = rd_units(e, e.r(5) & MASK, "a")
    L.rec("FileOpen", (L.lab(e.r(4) & MASK), hash_units(name, "a"), e.r(6) & MASK))


@stub(FILE_CLOSE)
def stub_file_close(e):
    e.cl.rec("FileClose", (e.cl.lab(e.r(3) & MASK),))


@stub(FILE_DTOR)
def stub_file_dtor(e):
    e.cl.rec("FileDtor", (e.cl.lab(e.r(3) & MASK),))


@stub(HOOK_WRITE)
def stub_write(e):
    L = e.cl
    this, data, size = e.r(3) & MASK, e.r(4) & MASK, e.r(5) & MASK
    L.rec("FileWrite", (L.lab(this), hash_units(list(e.read(data, size)), "a"), size))
    e.set_r(3, L.ret("FileWrite") & MASK)


@stub(HOOK_KEPT)
def stub_kept(e):
    e.cl.rec("KeptCallback", (s32(e.r(3)), e.r(4) & MASK))


@stub(AUTH_CTOR)
def stub_auth_ctor(e):
    e.cl.newlab(e.r(3) & MASK)
    e.cl.rec("AuthCtor", (e.cl.lab(e.r(3) & MASK),))


@stub(AUTH_CONNECT)
def stub_auth_connect(e):
    L = e.cl
    server = rd_units(e, e.r(4) & MASK, "a")
    L.rec("AuthConnect", (L.lab(e.r(3) & MASK), hash_units(server, "a"), e.r(5) & MASK, e.r(6) & 0xFFFF, L.lab(e.r(7) & MASK)))
    e.set_r(3, L.ret("AuthConnect") & MASK)


@stub(AUTH_LOGIN)
def stub_auth_login(e):
    L = e.cl
    name = rd_units(e, e.r(5) & MASK, "u")
    pw = rd_units(e, e.r(6) & MASK, "u")
    L.rec("AuthLogin", (L.lab(e.r(3) & MASK), e.r(4) & 0xFFFF, hash_units(name, "u"), hash_units(pw, "u"), e.r(7) & MASK))


@stub(NEWS_CTOR)
def stub_news_ctor(e):
    e.cl.newlab(e.r(3) & MASK)
    e.cl.rec("NewsCtor", (e.cl.lab(e.r(3) & MASK),))


@stub(NEWS_DTOR)
def stub_news_dtor(e):
    lab = e.cl.lab(e.r(3) & MASK)
    e.cl.rec("NewsDtor", (lab, s32(e.r(4))))
    e.cl.rec("delete", (lab,))      # the host's operator delete (the shim's) records it after the deleting destructor


@stub(NEWS_RELEASE)
def stub_news_release(e):
    e.cl.rec("NewsRelease", (e.cl.lab(e.r(3) & MASK),))


@stub(NEWS_CONNECT)
def stub_news_connect(e):
    L = e.cl
    server = rd_units(e, e.r(4) & MASK, "a")
    L.rec("NewsConnect", (L.lab(e.r(3) & MASK), hash_units(server, "a"), e.r(5) & MASK, e.r(6) & 0xFFFF, L.lab(e.r(7) & MASK)))
    e.set_r(3, L.ret("NewsConnect") & MASK)


@stub(NEWS_GSD)
def stub_news_gsd(e):
    L = e.cl
    L.rec("GetGameServerDetails", (L.lab(e.r(3) & MASK), e.r(4) & 0xFFFF, L.lab(e.r(5) & MASK), e.r(6) & MASK))
    e.set_r(3, L.ret("GetGameServerDetails") & MASK)


@stub(NEWS_GGN)
def stub_news_ggn(e):
    L = e.cl
    L.rec("GetGameNews", (L.lab(e.r(3) & MASK), e.r(4) & 0xFFFF, L.lab(e.r(5) & MASK), e.r(6) & MASK))
    e.set_r(3, L.ret("GetGameNews") & MASK)


@stub(NEWS_GSN)
def stub_news_gsn(e):
    L = e.cl
    L.rec("GetServerNews", (L.lab(e.r(3) & MASK), e.r(4) & 0xFFFF, e.r(5) & MASK))
    e.set_r(3, L.ret("GetServerNews") & MASK)


@stub(GETLANG)
def stub_getlang(e):
    e.cl.rec("GetLanguageId", ())
    e.set_r(3, e.cl.ret("GetLanguageId") & MASK)


@stub(GETDATAPATH)
def stub_getdatapath(e):
    e.cl.rec("GetDataPath", (e.cl.lab(e.r(3) & MASK),))
    e.set_r(3, CUR["dp"])


@stub(BALANCE_LOAD)
def stub_balance_load(e):
    L = e.cl
    path = rd_units(e, e.r(4) & MASK, "a")
    L.rec("BalanceLoad", (L.lab(e.r(3) & MASK), hash_units(path, "a"), e.r(5) & MASK))
    e.set_r(3, L.ret("BalanceLoad") & MASK)


@stub(SETINITIAL)
def stub_setinitial(e):
    e.cl.rec("SetInitialOnlineValues", ())


@stub(PROGRESS)
def stub_progress(e):
    e.cl.rec("CreateUIProgressBar", (s32(e.r(3)),))


@stub(ADDERROR)
def stub_adderror(e):
    e.cl.rec("AddUIError", (s32(e.r(3)), e.r(4) & MASK))


@stub(NEWSPANEL)
def stub_newspanel(e):
    e.cl.rec("CreateOnlineNewsPanel", ())


@stub(OPDELETE)
def stub_opdelete(e):
    e.cl.rec("OperatorDelete", (e.cl.lab(e.r(3) & MASK),))


@stub(FREE)
def stub_free(e):
    e.cl.rec("Free", (e.cl.lab(e.r(3) & MASK),))


@stub(REL_LOGIN)
def stub_rel_login(e):
    e.cl.rec("ReleaseLoginConnection", (e.cl.lab(e.r(3) & MASK),))


@stub(REL_NEWS)
def stub_rel_news(e):
    e.cl.rec("ReleaseNewsConnection", (e.cl.lab(e.r(3) & MASK),))


@stub(COMPLETE_LOGIN)
def stub_complete_login(e):
    e.cl.rec("CompleteLogin", (e.cl.lab(e.r(3) & MASK), s32(e.r(4))))


def setup(runner):
    T.install_std_stubs(runner.emu)
    emu = runner.emu

    def tvector(code):
        return T.persist_alloc(emu, 8, struct.pack(">II", code, TOC))

    tv_assign, tv_write, tv_kept = tvector(HOOK_ASSIGN), tvector(HOOK_WRITE), tvector(HOOK_KEPT)
    slots = [0] * 16
    slots[8] = tv_assign           # offset 0x20: operator=(const Char*)
    PERSIST["vt_a"] = T.persist_alloc(emu, 0x40, struct.pack(">16I", *slots))
    PERSIST["vt_w"] = T.persist_alloc(emu, 0x40, struct.pack(">16I", *slots))
    slots = [0] * 16
    slots[4] = tv_write            # offset 0x10: Write
    vt_acc = T.persist_alloc(emu, 0x40, struct.pack(">16I", *slots))
    PERSIST["acc"] = T.persist_alloc(emu, 0x10, struct.pack(">4I", vt_acc, 0, 0, 0))
    PERSIST["kept"] = tv_kept


# ---------------------------------------------------------------------------------------------------------------- the two sides

class Side:
    """What the shared code needs from either memory."""

    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def setw(self, base, name, v):
        self.words(base + self.off(name), [v])

    def getw(self, base, name):
        return self.rd_word(base + self.off(name))

    def setp(self, base, name, v):
        self.write(base + self.off(name), self.pack_ptr(v))

    def getp(self, base, name):
        return self.unpack_ptr(self.read(base + self.off(name), self.psz))

    def mkbuf(self, units, kind):
        data = pack_units(units, kind, self.fmt[0])
        a = self.alloc(len(data) + 8)
        self.write(a, data)
        return a

    def rd_units(self, p, kind, cap=700):
        out = []
        w = 1 if kind == "a" else 2
        while p and len(out) < cap:
            raw = self.read(p + w * len(out), w)
            u = raw[0] if w == 1 else struct.unpack(self.fmt[0] + "H", raw)[0]
            if u == 0:
                break
            out.append(u)
        return tuple(out)


class Ppc(Side):
    """One case on the PowerPC side: the fakes active, a fresh log, memory in the emulator's image."""
    fmt = ">I"
    psz = 4

    def __init__(self, e, case):
        self.e = e
        self.blk = {n: e.alloc(sz) for n, sz in BLOCKS}
        self.blk["news"] = e.alloc(0x10)
        CUR.clear()
        CUR.update(dp=self.blk["dp"], acc=case["acc"])
        fixed = {self.blk["sys"]: 1, self.blk["ctx"]: 2, self.blk["buf"]: 3, GUID: 4, CB_LOGIN: 5, CB_NEWS: 6, TABLE: 7,
                 PERSIST["acc"]: 8, FILEINFO: 9, self.blk["news"]: 10}
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def off(self, name):
        return MAC[name]

    def pack_ptr(self, v):
        return struct.pack(">I", v & MASK)

    def unpack_ptr(self, raw):
        return struct.unpack(">I", raw)[0]

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def kept_ptr(self):
        return PERSIST["kept"]

    def sep_addr(self):
        return SEPARATOR

    def ovr_addr(self):
        return OVERRIDE

    def mkstr(self, obj, kind, units, length=None):
        buf = self.mkbuf(units, kind)
        vptr = PERSIST["vt_w"] if kind == "u" else PERSIST["vt_a"]
        self.words(obj, [vptr, len(units) if length is None else length, buf, 0, 0])

    def rd_str(self, obj, kind):
        return (self.rd_word(obj + 4), self.rd_units(self.rd_word(obj + 8), kind))

    def set_config(self, login, lport, news, nport):
        for slot, text in ((8, login), (0x10, news)):
            data = self.mkbuf(enc(text), "a")
            rep = self.alloc(0x20)
            self.words(rep, [len(text), 0, 0, data])
            self.words(CONFIG + slot, [rep])
        self.write(CONFIG + 0x1C, struct.pack(">h", s16(lport)))
        self.write(CONFIG + 0x20, struct.pack(">h", s16(nport)))

    def call(self, entry, args):
        T.call_ppc(self.e, entry, [a & MASK for a in args])


def s16(v):
    v &= 0xFFFF
    return v - 0x10000 if v & 0x8000 else v


class Host(Side):
    """The same on the host library."""
    fmt = "<I"
    psz = 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.blk = {n: lib.tpw_cfsol_alloc(sz) for n, sz in BLOCKS}
        self.blk["news"] = lib.tpw_cfsol_rawnew(16)
        self.acc = lib.tpw_cfsol_acc()
        self.dp_string = None
        lib.tpw_cfsol_ctl(1, 1 if case["acc"] else 0)
        fixed = {1: self.blk["sys"], 2: self.blk["ctx"], 3: self.blk["buf"], 4: self.g("guid"), 5: self.g("cbLogin"),
                 6: self.g("cbNews"), 7: self.g("table"), 8: self.acc, 9: self.g("fileinfo"), 10: self.blk["news"]}
        CL.host_begin(lib, fixed, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_cfsol_addr(name.encode())

    def off(self, name):
        return LAY[IDX[name]]

    def pack_ptr(self, v):
        return struct.pack("<Q", v)

    def unpack_ptr(self, raw):
        return struct.unpack("<Q", raw)[0]

    def alloc(self, n):
        return self.lib.tpw_cfsol_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def kept_ptr(self):
        return self.g("keptcb")

    def sep_addr(self):
        return self.g("separator")

    def ovr_addr(self):
        return self.g("override")

    def mkstr(self, obj, kind, units, length=None):
        buf = self.mkbuf(units, kind)
        self.lib.tpw_cfsol_mkstr(obj, 1 if kind == "u" else 0, buf, len(units) if length is None else length)

    def rd_str(self, obj, kind):
        length = struct.unpack("<I", self.read(obj + LAY[N_FIELDS + 1], 4))[0]
        data = struct.unpack("<Q", self.read(obj + LAY[N_FIELDS + 2], 8))[0]
        return (length, self.rd_units(data, kind))

    def set_config(self, login, lport, news, nport):
        self.lib.tpw_cfsol_cfg(login.encode(), s16(lport), news.encode(), s16(nport))

    def call(self, entry, args):
        getattr(self.lib, "tpw_cfsol_" + entry)(*args)


BLOCKS = [("sys", MAC_SYS_SIZE), ("ctx", 0x80), ("buf", 0x400), ("dp", 0x40), ("x1", 0x10)]


def bind(lib):
    """The host library's entry points, typed once; the layout is read here."""
    if getattr(lib, "bound", False):
        return
    u64, i32, u32 = C.c_uint64, C.c_int32, C.c_uint32
    lib.tpw_cfsol_addr.restype, lib.tpw_cfsol_addr.argtypes = u64, [C.c_char_p]
    lib.tpw_cfsol_alloc.restype, lib.tpw_cfsol_alloc.argtypes = u64, [u32]
    lib.tpw_cfsol_rawnew.restype, lib.tpw_cfsol_rawnew.argtypes = u64, [u32]
    lib.tpw_cfsol_acc.restype, lib.tpw_cfsol_acc.argtypes = u64, []
    lib.tpw_cfsol_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_cfsol_mkstr.restype, lib.tpw_cfsol_mkstr.argtypes = None, [u64, i32, u64, i32]
    lib.tpw_cfsol_cfg.restype, lib.tpw_cfsol_cfg.argtypes = None, [C.c_char_p, i32, C.c_char_p, i32]
    lib.tpw_cfsol_ctl.restype, lib.tpw_cfsol_ctl.argtypes = None, [i32, u64]
    for name, argtypes in (("login_cb", [u64, i32, i32, u64, u64]), ("dologin", [u64, u64, u64]), ("getnews", [u64]),
                           ("news_cb", [u64, i32, i32, u64, u64]), ("completenews", [u64])):
        fn = getattr(lib, "tpw_cfsol_" + name)
        fn.restype, fn.argtypes = None, argtypes
    out = (C.c_int64 * 64)()
    n = lib.tpw_cfsol_layout(out)
    LAY[:] = list(out)[:n]
    assert len(LAY) == N_FIELDS + 6, LAY
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world

ENTRY = {"login_cb": FN_LOGINCB, "dologin": FN_DOLOGIN, "getnews": FN_GETNEWS, "news_cb": FN_NEWSCB, "completenews": FN_COMPLETE}


def build(s, case):
    """Writes the case's system, context, configuration and strings through Side: the same code for both memories (Mac offsets)."""
    b = s.blk
    sysobj = b["sys"]
    for n in WORDS:
        s.setw(sysobj, n, case["words"][n])
    s.setp(sysobj, "mLogin", b["x1"] if case["ptrs"][0] else 0)
    s.setp(sysobj, "mNews", b["news"] if case["ptrs"][1] else 0)
    s.setp(sysobj, "mLoginCallbackKept", s.kept_ptr() if case["ptrs"][2] else 0)
    s.setp(sysobj, "mLoginContextKept", case["ctxkept"])
    for n in STRINGS:
        kind = KIND[n]
        units = enc(case["str"][n]) if kind == "u" else list(case["str"][n].encode("latin-1"))
        s.mkstr(sysobj + s.off(n), kind, units, case["newslen"] if n == "mNewsText" else None)
    login, lport, news, nport = case["cfg"]
    s.set_config(login, lport, news, nport)
    s.mkstr(s.sep_addr(), "a", list(case["sep"].encode("latin-1")))
    s.mkstr(s.ovr_addr(), "a", list(case["ovr"].encode("latin-1")))
    s.mkstr(b["dp"], "a", list(case["dp"].encode("latin-1")))
    if isinstance(s, Host):
        s.lib.tpw_cfsol_ctl(0, b["dp"])
    # the context
    cx, ctx = case["ctx"], b["ctx"]
    if case["op"] == "login_cb":
        s.words(ctx, [cx["w0"], cx["w4"]])
        s.write(ctx + 0x30, pack_units(enc(cx["email"]), "u", s.fmt[0]))
        s.words(ctx + 0x58, [cx["w58"], cx["w5c"], cx["w60"], cx["w64"], cx["count"]])
    elif case["op"] == "news_cb":
        a = case["args"][0]
        if a == 0x32:
            units = ([cx["bom"]] if cx["bom"] else []) + enc(cx["text"])
            s.write(b["buf"], pack_units(units, "u", s.fmt[0]))
        elif a == 0x33:
            s.write(b["buf"], pack_units(list(cx["text"].encode("latin-1")), "a", s.fmt[0]))
        s.write(ctx, s.pack_ptr(b["buf"]))
        s.words(ctx + 8, [cx["count"]])
    # DoLogin's arguments
    if case["op"] == "dologin":
        b["name"] = s.mkbuf(enc(case["name"]), "u")
        b["pw"] = s.mkbuf(enc(case["pw"]), "u")


def execute(s, case):
    b, a = s.blk, case["args"]
    op = case["op"]
    if op in ("login_cb", "news_cb"):
        d = b["ctx"] if case["dflag"] else 0
        s.call(ENTRY[op] if isinstance(s, Ppc) else op, [b["sys"], a[0], a[1], case["c"], d] if isinstance(s, Ppc)
               else [b["sys"], s32(a[0]), s32(a[1]), case["c"], d])
    elif op == "dologin":
        s.call(ENTRY[op] if isinstance(s, Ppc) else op, [b["sys"], b["name"], b["pw"]])
    else:
        s.call(ENTRY[op] if isinstance(s, Ppc) else op, [b["sys"]])


def observe(s, case):
    """Everything the call may change, read back the same way on both sides; pointers as flags, strings as (length, units)."""
    b = s.blk
    out = {}
    for n, mac, kind in FIELDS:
        if kind == "w":
            out[n] = s.getw(b["sys"], n)
        elif kind == "p":
            v = s.getp(b["sys"], n)
            out[n] = v if n == "mLoginContextKept" else v != 0
        else:
            out[n] = s.rd_str(b["sys"] + s.off(n), kind)
    out["ctx8"] = s.rd_word(b["ctx"] + 8)
    out["ctx68"] = s.rd_word(b["ctx"] + 0x68)
    out["log"] = s.log()
    return out


def run(s, case):
    try:
        build(s, case)
        execute(s, case)
        o = observe(s, case)
        return o
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

NARROW = "abcXYZ019:. _-é"
WIDE = "abcXYZ019:. _-éあ€"
SERVERS = ["login.example.com", "10.1.2.3", "a", "news.host.net", ""]
PATHS = ["Data", "Mac HD:Games:TPW:Data", "", "Déjà"]
LANGS = [0x409, 0x40C, 7, 0, 0x10409, 0xFFFF0407, 0xFFFF, 0x8000]
IDS = [0xFFFFFFFF, 0, 1, 7, 0x55, 0x80000000, 0x12345678]


def rtext(r, alphabet, hi=10):
    return "".join(r.choice(alphabet) for _ in range(r.randrange(0, hi + 1)))


def rword(r):
    return r.choice([0, 1, 2, 0x4B, 0x37, 0xFFFFFFFF, 0x80000000, r.getrandbits(32)])


def base(r, op):
    """The world every case starts from: random members, texts, configuration, the fakes' answer queues covering every branch."""
    langs = [r.choice(LANGS) for _ in range(3)]
    c = {"op": op, "words": {n: rword(r) for n in WORDS}, "ptrs": [r.choice([0, 1]) for _ in range(3)],
         "ctxkept": r.choice([0, 1, 0x1234, 0xFFFFFFFF, r.getrandbits(32)]),
         "str": {n: rtext(r, WIDE if KIND[n] == "u" else NARROW) for n in STRINGS},
         "cfg": (r.choice(SERVERS), r.choice([80, 0x1F90, 0xFFFF, 0x8001, r.getrandbits(16)]), r.choice(SERVERS),
                 r.choice([81, 0x1F91, 0xFFFE, 0x8002, r.getrandbits(16)])),
         "sep": r.choice([":", "/", "::", ""]), "ovr": r.choice(["onlineoverride.sam", "x", "é.sam"]), "dp": r.choice(PATHS),
         "acc": r.choice([0, 1]), "c": r.choice([0, 1, 0x7777, r.getrandbits(32)]), "dflag": 1,
         "name": rtext(r, WIDE, 12), "pw": rtext(r, WIDE, 12), "args": [0, 0],
         "ctx": {"w0": rword(r), "w4": rword(r), "email": rtext(r, WIDE, 20), "w58": rword(r), "w5c": rword(r), "w60": rword(r),
                 "w64": rword(r), "count": r.choice([1, 2, 5, 0, 0x80000001]), "bom": 0, "text": rtext(r, WIDE, 20)},
         "queues": {"AuthConnect": [r.choice([1, 1, 0, -1, 2])], "NewsConnect": [r.choice([1, 1, 0, -1, 2])],
                    "GetLanguageId": langs, "GetGameServerDetails": [r.choice(IDS)], "GetGameNews": [r.choice(IDS)],
                    "GetServerNews": [r.choice(IDS)], "BalanceLoad": [r.choice([1, 1, 0, 2, -1])],
                    "FileWrite": [r.choice([-1, 0, 5, 40, 0x7FFFFFFF])]}}
    c["newslen"] = len(c["str"]["mNewsText"]) + r.choice([0, 0, 0, 1, -1]) if c["str"]["mNewsText"] else 0
    return c


# ---- LoginConnectionCallback
LOGIN_A = [-8, -7, -6, -5, -4, -3, -2, -1, 0, 1, 0x3B, 0x3C, 0x3D, 0x100, -0x100, 0x7FFFFFFF, -0x80000000]


def login_case(r, a=None, count=None, dflag=None):
    c = base(r, "login_cb")
    c["args"] = [r.choice(LOGIN_A) if a is None else a, r.choice([0, 1, -1, r.getrandbits(31)])]
    if count is not None:
        c["ctx"]["count"] = count
    c["dflag"] = 1 if (c["args"][0] == 0x3C or r.random() < 0.7) else 0
    if dflag is not None:
        c["dflag"] = dflag
    return c


def login_edges():
    r = random.Random(5924001)
    for a in LOGIN_A:
        for count in ([1, 2, 0, 0x80000001] if a == 0x3C else [1]):
            yield login_case(r, a, count)
    for a in (-4, -3, 0):
        yield login_case(r, a, 1, 0)


def login_random(r):
    return login_case(r)


# ---- DoLogin
def dologin_case(r, connect=None):
    c = base(r, "dologin")
    if connect is not None:
        c["queues"]["AuthConnect"] = [connect]
    return c


def dologin_edges():
    r = random.Random(5924002)
    for connect in (1, 0, -1, 2, 1, 0):
        for ptrs in ((0, 0, 0), (1, 1, 1)):
            c = dologin_case(r, connect)
            c["ptrs"] = list(ptrs)
            yield c
    c = dologin_case(r, 1)
    c["name"], c["pw"] = "", ""
    yield c


def dologin_random(r):
    return dologin_case(r)


# ---- LoggedInGetNews (a failed connection runs CompleteNews, the original, with the case's world)
def getnews_case(r, connect=None):
    c = base(r, "getnews")
    if connect is not None:
        c["queues"]["NewsConnect"] = [connect]
    return c


def getnews_edges():
    r = random.Random(5924003)
    for connect in (1, 0, -1, 2):
        for ptrs in ((0, 0, 0), (1, 1, 1), (0, 1, 0), (1, 0, 1)):
            for finished in (0, 1):
                c = getnews_case(r, connect)
                c["ptrs"] = list(ptrs)
                c["words"]["mNewsFinished"] = finished
                yield c


def getnews_random(r):
    return getnews_case(r)


# ---- NewsConnectionCallback
NEWS_A = [-8, -7, -6, -5, -4, -3, -2, -1, 0, 1, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x3C, 0x100, -0x100]


def news_case(r, a=None, which=None, count=None, bom=None, req1=None):
    c = base(r, "news_cb")
    a = r.choice(NEWS_A) if a is None else a
    w = c["words"]
    if which == 2:
        b = w["mNewsRequest2"]
    elif which == 3:
        b = w["mNewsRequest3"]
        if w["mNewsRequest2"] == b:
            w["mNewsRequest2"] ^= 1
    elif which == 4:
        b = r.choice(IDS)
        while b in (w["mNewsRequest2"], w["mNewsRequest3"]):
            b ^= 0x100
    elif which == 5:
        b = w["mNewsRequest2"] = w["mNewsRequest3"]      # both requests equal: the first test wins
    else:
        b = r.choice([w["mNewsRequest2"], w["mNewsRequest3"], r.choice(IDS)])
    c["args"] = [a, b]
    if count is not None:
        c["ctx"]["count"] = count
    c["ctx"]["bom"] = r.choice([0, 0xFFFE, 0xFEFF]) if bom is None else bom
    if req1 is not None:
        w["mNewsRequest1"] = req1
    c["ctx"]["text"] = rtext(r, WIDE if a == 0x32 else NARROW, 20)
    return c


def news_edges():
    r = random.Random(5924004)
    for a in NEWS_A:
        if a in (0x32, 0x33):
            continue
        yield news_case(r, a)
    for req1 in (0, 1, 0xFFFFFFFF):
        for count in (1, 2):
            yield news_case(r, 0x33, count=count, req1=req1)
    for which in (2, 3, 4, 5):
        for count in (1, 2, 0, 0x80000001):
            for bom in (0, 0xFFFE, 0xFEFF, 0x0041):
                yield news_case(r, 0x32, which, count, bom)


def news_random(r):
    return news_case(r)


# ---- CompleteNews
def complete_case(r, finished=None, kept=None, mnews=None, acc=None, write=None, load=None, f70=None):
    c = base(r, "completenews")
    w = c["words"]
    if finished is not None:
        w["mNewsFinished"] = finished
    if kept is not None:
        c["ptrs"][2] = kept
    if mnews is not None:
        c["ptrs"][1] = mnews
    if acc is not None:
        c["acc"] = acc
    if write is not None:
        c["queues"]["FileWrite"] = [write]
    if load is not None:
        c["queues"]["BalanceLoad"] = [load]
    if f70 is not None:
        w["mF70"] = f70
    return c


def complete_edges():
    r = random.Random(5924005)
    for finished in (0x4B, 0x4F, 0x37):
        for kept in (0, 1):
            for mnews in (0, 1):
                yield complete_case(r, finished, kept, mnews)
    for kept in (0, 1):
        for mnews in (0, 1):
            for f70 in (0, 1):
                yield complete_case(r, 0, kept, mnews, 0, None, None, f70)
    for mnews in (0, 1):
        for f70 in (0, 1):
            for write in (-1, 0, 5):
                for load in (1, 0, 2):
                    yield complete_case(r, 0, 1, mnews, 1, write, load, f70)


def complete_random(r):
    return complete_case(r)


TESTS = [
    FnTest("CFSOnlineSystem::LoginConnectionCallback", ppc, host, edge=login_edges, rnd=login_random),
    FnTest("CFSOnlineSystem::DoLogin", ppc, host, edge=dologin_edges, rnd=dologin_random),
    FnTest("CFSOnlineSystem::LoggedInGetNews", ppc, host, edge=getnews_edges, rnd=getnews_random),
    FnTest("CFSOnlineSystem::NewsConnectionCallback", ppc, host, edge=news_edges, rnd=news_random),
    FnTest("CFSOnlineSystem::CompleteNews", ppc, host, edge=complete_edges, rnd=complete_random),
]
