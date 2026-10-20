"""Unit v5903, the buddy / ignore list helpers of COnlineControl, GotoFailed, GotoStoredPerson, pair<nocase_string,BuddyInfo>::~pair and
basic_ofstream<wchar_t>::~basic_ofstream, against libtpw_unit_std_basic_ofstream_Dtor_basic_ofstream_3.dylib
(tests/unit_std_basic_ofstream_Dtor_basic_ofstream_3.cpp), main app, src/COnlineControl_..._ProcessQueue_std_basic_ofstream_Dtor_basic_ofstream_3.cpp:
  ~basic_ofstream 0x1021ce74, GotoStoredPerson 0x1021c330, GotoFailed 0x1021c254, UpdateIgnoreListToServer 0x1021c190,
  UpdateBuddyListToServer 0x1021c0ec, UpdateBuddyList 0x1021c02c, ~pair 0x1021bd24

A case is a world and a call, built by one piece of Python code in emulator memory (big-endian, Mac offsets) and in the host library's
memory. The callees outside the unit are fakes on both sides with the same events: the wstring constructor/copy/destructor (a string object
is one word holding the text pointer), Chat::Goto/Ignore/AddBuddy, TbITable::FormatString, ReceiveText, the tree increment (the next node
is the first word of a node), GetDataForUser, OnlineAddBuddy, filebuf close, locale and ios_base destructors, operator delete. Pointers
are logged as ids and texts as a hash of their characters.

The world: the COnlineControl block (chat word, person, feedback buffer, flag byte, buddy and ignore node chains with key reps), or the
ofstream object with its virtual-base table. Compared: the event log, the feedback buffer, the flag byte, the stream's stored vtable words
(as symbolic names), the base offsets, the answer.

No fused operations. Not tested: the second null test of the pair destructor and the ofstream's this+8 / +0x24 / +0x58 null tests
(they cannot be false for a non-null object).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std_basic_ofstream_Dtor_basic_ofstream_3"
MASK = 0xFFFFFFFF

FN = dict(ofs=0x1021ce74, goto_stored=0x1021c330, goto_failed=0x1021c254, upd_ignore=0x1021c190, upd_buddy_srv=0x1021c0ec,
          upd_buddy=0x1021c02c, pair=0x1021bd24)
SIZES = {0x1021ce74: 324, 0x1021c330: 100, 0x1021c254: 168, 0x1021c190: 132, 0x1021c0ec: 100, 0x1021c02c: 136, 0x1021bd24: 100}
OWN = set(SIZES)

ADDR = dict(ui=0x10396440, files=0x103bd0bc, textres=0x1037ff18 + 0x598, vtOfs=0x102b1f50, vtOfsB=0x102b1f08, vtC=0x102b1e60,
            vtD=0x102b1eb0, vtE=0x102b1f20)
SYMS = ["vtOfs", "vtOfsB", "vtC", "vtD", "vtE", "files"]
HOST_LAYOUT = [8, 0x10, 0x18, 0x28, 0x38, 0x40, 0x48]
MAC_OFS = dict(slot=4, fb=8, loc=0x24, file=0x2c, ios=0x58)
HOST_OFS = dict(slot=8, fb=0x10, loc=0x18, file=0x28, ios=0x38)

CUR = {"obj": 0}


def s32(v):
    return T.s32(v)


def thash(chars):
    h = len(chars)
    for c in chars:
        h = (h * 31 + c) & 0x7FFFFFFF
    return h


def rd_wstr(e, a):
    chars = []
    while len(chars) < 0x420:
        c = struct.unpack(">H", e.read(a + 2 * len(chars), 2))[0]
        if c == 0:
            break
        chars.append(c)
    return chars


def str_word(e, a):
    return rd_wstr(e, e.u32(a))


def key_hash(e, a):
    rep = e.u32(a)
    return thash(rd_wstr(e, e.u32(rep + 0xC)))


def oid(p):
    o = CUR["obj"]
    if p == o:
        return 0
    if p == o + MAC_OFS["fb"]:
        return 1
    if p == o + MAC_OFS["loc"]:
        return 2
    if p == o + MAC_OFS["ios"]:
        return 3
    return -1


def isobj(p):
    return 1 if p == CUR["obj"] else 0


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_ctor(e):
    e.cl.rec("WStringCtorFrom", (thash(rd_wstr(e, e.r(4))),))
    e.write(e.r(3), struct.pack(">I", e.r(4)))


def stub_copy(e):
    e.cl.rec("WStringCopy", (thash(str_word(e, e.r(4))), e.r(5), s32(e.r(6))))
    e.write(e.r(3), e.read(e.r(4), 4))


def stub_dtor(e):
    e.cl.rec("WStringDtor", (thash(str_word(e, e.r(3))), s32(e.r(4))))


def stub_delete(e):
    e.cl.rec("Delete", (oid(e.r(3)),))


def stub_nocase_dtor(e):
    e.cl.rec("NocaseDtor", (oid(e.r(3)), s32(e.r(4))))


def stub_ui(e):
    e.cl.rec("OnlineAddBuddy", (1 if e.r(3) == ADDR["ui"] else 0, thash(rd_wstr(e, e.r(4))), s32(e.r(5))))


def stub_goto(e):
    e.cl.rec("ChatGoto", (e.r(3),thash(str_word(e, e.r(4))), isobj(e.r(5))))


def stub_add_buddy(e):
    e.cl.rec("ChatAddBuddy", (e.r(3), key_hash(e, e.r(4)), isobj(e.r(5))))


def stub_ignore(e):
    e.cl.rec("ChatIgnore", (e.r(3), key_hash(e, e.r(4)), isobj(e.r(5))))


def stub_inc(e):
    e.cl.rec("TreeIncrement", (0,))
    node = e.u32(e.r(3))
    e.write(e.r(3), struct.pack(">I", e.u32(node)))


def stub_close(e):
    e.cl.rec("close", (oid(e.r(3)),))
    e.set_r(3, 1)


def stub_locale(e):
    e.cl.rec("LocaleDtor", (oid(e.r(3)), s32(e.r(4))))


def stub_ios(e):
    e.cl.rec("IosBaseDtor", (oid(e.r(3)), s32(e.r(4))))


def stub_data(e):
    e.cl.rec("GetDataForUser", (isobj(e.r(3)), key_hash(e, e.r(4))))
    e.set_r(3, e.cl.ret("GetDataForUser") & MASK)


def stub_receive(e):
    e.cl.rec("ReceiveText", (isobj(e.r(3)), thash(str_word(e, e.r(4))), s32(e.r(5))))


def stub_format(e):
    e.cl.rec("FormatString", (e.r(3), s32(e.r(5)), e.r(6), 1 if e.r(4) == CUR["obj"] + 0x220 else 0))
    k = e.cl.ret("FormatString")
    n = k % 5 + 1
    chars = [0x41 + (k + i * 3) % 26 for i in range(n)] + [0]
    e.write(e.r(4), struct.pack(">%dH" % len(chars), *chars))
    e.set_r(3, 0)


for _addr, _fn in ((0x10146A14, stub_ctor), (0x101F7424, stub_copy), (0x101468F8, stub_dtor), (0x10238218, stub_delete),
                   (0x10222020, stub_nocase_dtor), (0x101F0B08, stub_ui), (0x1023AD08, stub_goto), (0x1023ACC0, stub_add_buddy),
                   (0x1023AC78, stub_ignore), (0x10152DB0, stub_inc), (0x10221B40, stub_close), (0x10071F34, stub_locale),
                   (0x10238950, stub_ios), (0x1021B854, stub_data), (0x10217DBC, stub_receive), (0x10239730, stub_format)):
    STUBS.internal(_addr, _fn)


def setup(runner):
    pass


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]

    def wchars(self, a, chars):
        self.write(a, struct.pack(self.fmt[0] + "%dH" % len(chars), *chars))

    def rd_chars(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dH" % n, self.read(a, 2 * n)))

    def putp(self, a, v):
        self.write(a, struct.pack(self.pfmt, v & ((1 << (8 * self.PS)) - 1)))

    def getp(self, a):
        return struct.unpack(self.pfmt, self.read(a, self.PS))[0]


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    PS = 4
    OFS = MAC_OFS
    TBLOFF = 0x44

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        if name == "files":
            return self.e.u32(0x10260CA4)   # the TOC slot of the __files import: the address the code compares with
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def setobj(self, a):
        CUR["obj"] = a

    def ptr(self, a):
        return a

    def call(self, op, a, flag=0):
        args = [a] if op not in ("ofs", "pair") else [a, flag & MASK]
        r3, _, _ = T.call_ppc(self.e, FN[op], args)
        return r3 & MASK


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"
    PS = 8
    OFS = HOST_OFS
    TBLOFF = 0x40

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.cache = {}
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        if name not in self.cache:
            self.cache[name] = self.lib.tpw_ofs3_addr(name.encode())
        return self.cache[name]

    def alloc(self, n):
        return self.lib.tpw_ofs3_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def setobj(self, a):
        self.lib.tpw_ofs3_setobj(a)

    def ptr(self, a):
        return self.lib.tpw_ofs3_offset(a)

    def call(self, op, a, flag=0):
        f = getattr(self.lib, "tpw_ofs3_" + op)
        if op in ("ofs", "pair"):
            return f(a, s32(flag))
        f(a)
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_ofs3_"
    lib.tpw_ofs3_addr.restype, lib.tpw_ofs3_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_ofs3_alloc.restype, lib.tpw_ofs3_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_ofs3_setobj.argtypes = [C.c_uint64]
    lib.tpw_ofs3_offset.restype, lib.tpw_ofs3_offset.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_ofs3_layout.argtypes = [C.POINTER(C.c_int64)]
    for n in ("goto_stored", "goto_failed", "upd_ignore", "upd_buddy_srv", "upd_buddy"):
        getattr(lib, p + n).argtypes = [C.c_uint64]
    for n in ("ofs", "pair"):
        getattr(lib, p + n).restype, getattr(lib, p + n).argtypes = C.c_uint64, [C.c_uint64, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_ofs3_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def mk_list(s, texts, end):
    nxt = s.ptr(end)
    for t in reversed(texts):
        node = s.alloc(0x20)
        rep = s.alloc(0x10)
        data = s.alloc(2 * len(t) + 4)
        s.wchars(data, t + [0])
        s.words(rep + 0xC, [s.ptr(data)])
        s.words(node, [nxt])
        s.words(node + 0xC, [s.ptr(rep)])
        nxt = s.ptr(node)
    return nxt


def build_list_world(s, case):
    oc = s.alloc(0x1000)
    s.setobj(oc)
    s.words(oc, [case["chat"]])
    s.wchars(oc + 0x220, case["buf"] + [0])
    s.write(oc + 0xE84, bytes([case["flag84"]]))
    person = s.alloc(0x20)
    text = s.alloc(2 * len(case["person"]) + 4)
    s.wchars(text, case["person"] + [0])
    s.words(person + 0xC, [s.ptr(text)])
    s.words(oc + 0xE88, [s.ptr(person)])
    s.words(oc + 0xE40, [mk_list(s, case["buddies"], oc + 0xE38)])
    s.words(oc + 0xE4C, [mk_list(s, case["ignores"], oc + 0xE48)])
    s.putp(s.g("textres"), 0x7000 + case["tbl"])
    return {"oc": oc}


def build_ofs(s, case):
    obj = s.alloc(0x100)
    tbl = s.alloc(0x48)
    s.setobj(obj)
    o = s.OFS
    s.putp(obj, tbl)
    s.putp(obj + o["slot"], 0x1111)
    s.putp(obj + o["fb"], 0x2222)
    s.putp(obj + o["ios"], 0x3333)
    files = s.g("files")
    s.putp(obj + o["file"], [files, files + 0x54, files + 0xA8, files + 0xFC, 0, 0x6666][case["file"]])
    s.putp(tbl, 0x4444)
    s.putp(tbl + s.TBLOFF, 0x5555)
    return {"oc": 0 if case["null"] else obj, "obj": obj, "tbl": tbl}


def build_pair(s, case):
    obj = s.alloc(0x20)
    s.setobj(obj)
    return {"oc": 0 if case["null"] else obj, "obj": obj}


def build(s, case):
    return {"ofs": build_ofs, "pair": build_pair}.get(case["op"], build_list_world)(s, case)


def sym(s, v):
    for i, n in enumerate(SYMS):
        d = v - s.g(n)
        if 0 <= d < (0x100 if n == "files" else 0x10):
            return 1000000 + i * 256 + d
    return v


def observe(s, case, st, ans):
    op = case["op"]
    d = {"log": s.log()}
    if op == "pair":
        d["ret"] = ans == st["oc"]
    elif op == "ofs":
        o, obj, tbl = s.OFS, st["obj"], st["tbl"]
        base = s.getp(tbl + s.TBLOFF)
        if base >= 1 << (8 * s.PS - 1):
            base -= 1 << (8 * s.PS)
        if base == 0x5555:
            tgt = "unchanged"
        else:
            t = base + tbl - obj
            tgt = "fb" if t == o["fb"] else "ios" if t == o["ios"] else ("raw", t)
        d["ret"] = ans == st["oc"]
        d["slot"] = sym(s, s.getp(obj + o["slot"]))
        d["fb"] = sym(s, s.getp(obj + o["fb"]))
        d["file"] = sym(s, s.getp(obj + o["file"]))
        d["ios"] = sym(s, s.getp(obj + o["ios"]))
        d["first"] = sym(s, s.getp(tbl))
        d["base"] = tgt
    else:
        oc = st["oc"]
        d["buf"] = s.rd_chars(oc + 0x220, 0x400)
        d["b84"] = s.read(oc + 0xE84, 1)
        d["chat"] = s.rd_word(oc)
    return d


def run(s, case):
    try:
        st = build(s, case)
        return observe(s, case, st, s.call(case["op"], st["oc"], case.get("flag", 0) & 0xFFFF | (case.get("garb", 0) & 0xFFFF) << 16))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators


def rtext(r, lo=0, hi=7):
    return [r.choice([0x41, 0x62, 0x20, 0x1234, 0x7A, 0xFF, 0x8000]) for _ in range(r.randint(lo, hi))]


def lcase(r, op, nb=None, ni=None):
    nb = r.choice([0, 1, 2, 3, 5]) if nb is None else nb
    ni = r.choice([0, 1, 2, 3, 5]) if ni is None else ni
    return {"op": op, "chat": r.randrange(1, 1 << 31), "buf": rtext(r, 0, 9), "flag84": r.choice([0, 1, 0xFF]),
            "person": rtext(r, 0, 8), "buddies": [rtext(r) for _ in range(nb)], "ignores": [rtext(r) for _ in range(ni)],
            "tbl": r.randrange(0, 100),
            "queues": {"GetDataForUser": [r.choice([0, 1, 5, 0x7FFFFFFF, 0xFFFFFFFF, r.randrange(1 << 32)]) for _ in range(max(nb, 1))],
                       "FormatString": [r.randrange(0, 100)]}}


def list_edges(op):
    def gen():
        r = random.Random(sum(map(ord, op)))
        for nb, ni in ((0, 0), (1, 1), (2, 3), (5, 2), (0, 4), (4, 0)):
            yield lcase(r, op, nb, ni)
    return gen


def list_random(op):
    return lambda r: lcase(r, op)


def fcase(r, op, flag=None, garb=None, file=None, null=None):
    return {"op": op, "queues": {"x": [0]}, "flag": r.choice([0, 1, -1, 2, 0x7FFF, -0x8000, 3]) if flag is None else flag,
            "garb": r.choice([0, 0, 1, 0xFFFF, 0x1234]) if garb is None else garb,
            "file": r.randrange(6) if file is None else file, "null": r.random() < 0.05 if null is None else null}


def ofs_edges():
    r = random.Random(11)
    for f in (0, 1, -1, 2, 0x7FFF, -0x8000):
        for k in range(6):
            yield fcase(r, "ofs", f, 0, k, False)
    yield fcase(r, "ofs", 0, 1, 0, False)
    yield fcase(r, "ofs", 1, 0xFFFF, 1, False)
    yield fcase(r, "ofs", 5, 0, 0, True)


def pair_edges():
    r = random.Random(12)
    for f in (0, 1, -1, 2, 0x7FFF, -0x8000):
        yield fcase(r, "pair", f, 0, 0, False)
    yield fcase(r, "pair", 0, 1, 0, False)
    yield fcase(r, "pair", 1, 0xFFFF, 0, True)


TESTS = [
    FnTest("std::basic_ofstream<wchar_t,std::char_traits<wchar_t>>::~basic_ofstream", ppc, host, edge=ofs_edges,
           rnd=lambda r: fcase(r, "ofs")),
    FnTest("COnlineControl::GotoStoredPerson", ppc, host, edge=list_edges("goto_stored"), rnd=list_random("goto_stored")),
    FnTest("COnlineControl::GotoFailed", ppc, host, edge=list_edges("goto_failed"), rnd=list_random("goto_failed")),
    FnTest("COnlineControl::UpdateIgnoreListToServer", ppc, host, edge=list_edges("upd_ignore"), rnd=list_random("upd_ignore")),
    FnTest("COnlineControl::UpdateBuddyListToServer", ppc, host, edge=list_edges("upd_buddy_srv"), rnd=list_random("upd_buddy_srv")),
    FnTest("COnlineControl::UpdateBuddyList", ppc, host, edge=list_edges("upd_buddy"), rnd=list_random("upd_buddy")),
    FnTest("std::pair<const_nocase_string,BuddyInfo>::~pair", ppc, host, edge=pair_edges, rnd=lambda r: fcase(r, "pair")),
]
