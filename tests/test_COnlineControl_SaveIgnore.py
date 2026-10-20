"""Unit v5235: COnlineControl::SaveIgnore and LoadIgnore, against libtpw_unit_COnlineControl_SaveIgnore.dylib
(tests/unit_COnlineControl_SaveIgnore.cpp), main app ("SimTheme Park"),
src/COnlineControl_COnlineControl_COnlineControl_Locate_COnlineControl_WantToShutDown_COnlineControl_SaveIgnore.cpp:
  COnlineControl::SaveIgnore 0x1021d74c, COnlineControl::LoadIgnore 0x1021db98

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. Every callee
outside the unit (stream, filebuf, string, tree, user info, chat) is a fake on both sides that records its arguments (texts included)
and answers from the case's queues or memory.

The world of a case: the COnlineControl block (chat word, player name, the ignore tree), gUserInfo, the stream's ios block (state,
exception mask, stream buffer), the path text the string fake hands out, the lines getline hands out, the queue answers (new, open,
file, close, widen). What is compared: the answer (low byte), the event log, gUserInfo (set or not), the ios block (vtable word relative
to its global, the words up to 0x44; the word at 0x44 is checked by the ios destructor fake) and the line index.

No fused operations. Not tested: a getline answer of NULL (the original then reads address 0x34).
"""
import ctypes as C
import random
import string
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_COnlineControl_SaveIgnore"
MASK = 0xFFFFFFFF
FN_SAVE, FN_LOAD = 0x1021d74c, 0x1021db98
OWN = {FN_SAVE, FN_LOAD}

ADDR = dict(userinfo=0x10380b34, tpg=0x103b74c8, ofsvtbl=0x102b1f50, ifsvtbl=0x102b1e40, ifv=0x1029d084, dv=0x102661d0)
HOST_LAYOUT = [0x28, 0x38, 0x3C, 0x48, 0x24, 0x10, 8]
TVEC = 0x12345670


def s32(v):
    return T.s32(v)


def _b(x):
    return x if isinstance(x, bytes) else x.encode("latin-1")


def wstr(e, a):
    out = []
    while len(out) < 300:
        c = struct.unpack(">H", e.read(a, 2))[0]
        if c == 0:
            break
        out.append(chr(c))
        a += 2
    return "".join(out)


def enc_be(text):
    return text.encode("utf-16-be") + b"\0\0"


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)
R = CL.recorder


def ctx_block(e):
    return e.u32(ADDR["ctx"])


def ctx_self(e):
    return e.u32(ADDR["ctx"] + 4)


def with_regs(rec, regs):
    """Runs the recorder with some registers replaced by computed values, then restores them."""
    def f(e):
        saved = {r: e.r(r) for r in (3, 4, 5, 6)}
        new = {r: fn(e, saved) & MASK for r, fn in regs.items()}
        for r, v in new.items():
            e.set_r(r, v)
        rec(e)
        for r, v in saved.items():
            e.set_r(r, v)
    return f


def fake(addr, fn):
    if 0x10238000 <= addr < 0x1023b000:
        inner = fn

        def fn(e, inner=inner):
            # the import glue saves the TOC pointer in the caller's frame; the caller reloads r2 from there
            e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2)))
            inner(e)
    STUBS.internal(addr, fn)


def st_strlen(e):
    e.set_r(3, len(_b(e.cstr(e.r(3)))))


rec_strcpy = R("strcpy", "xs")


def st_strcpy(e):
    rec_strcpy(e)
    d, src = e.r(3), e.r(4)
    e.write(d, _b(e.cstr(src)) + b"\0")
    e.set_r(3, d)


def ctor_stub(name):
    rec = R(name, "xi")

    def f(e):
        rec(e)
        e.cl.stream = e.r(3)
        e.write(e.r(3), struct.pack(">I", ctx_block(e)))
    return f


def st_retpath(e):
    e.cl.sret = e.r(3)
    e.cl.rec("ReturnOnlinePath", (1 if e.r(4) else 0,))


def st_pathcopy(e):
    L = e.cl
    L.rec("PathCopy", (1 if e.r(4) == getattr(L, "sret", -1) else 0,))
    L.path = e.r(3)
    e.write(e.r(3) + 8, struct.pack(">I", ADDR["pathbuf"]))


def st_pathdtor(e):
    L = e.cl
    r3 = e.r(3)
    which = 1 if r3 == getattr(L, "sret", -1) else (2 if r3 == getattr(L, "path", -1) else 0)
    L.rec("PathDtor", (which, s32(e.r(4))))


def st_appstr(e):
    L = e.cl
    L.rec("PathAppendString", (1 if e.r(3) == getattr(L, "path", -1) else 0, s32(e.r(4) - ADDR["tpg"])))


rec_sprintf = R("sprintf", "xsS")


def st_sprintf(e):
    rec_sprintf(e)
    text = wstr(e, e.r(5))
    e.write(e.r(3), _b(text) + b"\0")


def st_open(e):
    L = e.cl
    rec = R("open", "xsu")
    rec(e)
    e.write(e.r(3) + 0x24, struct.pack(">I", L.ret("file") & MASK))
    e.set_r(3, L.ret("open") & MASK)


def st_newarr(e):
    L = e.cl
    rec = R("NewArray", "u")
    n = e.r(3)
    rec(e)
    L.newarr = e.alloc(max(4, n))
    e.set_r(3, L.newarr)


def st_refptr(e):
    e.cl.rec("RefPtr", ())
    e.write(e.r(3), struct.pack(">I", e.r(4)))


rec_throw = R("throw", "s")


def st_throw(e):
    rec_throw(e)
    obj = e.r(4)
    e.cl.rec("throwobj", (s32(e.u32(obj) - ADDR["ifv"]), s32(e.r(5) - ADDR["dv"]),
                          1 if e.u32(obj + 4) == getattr(e.cl, "newarr", -1) else 0))


def st_newop(e):
    L = e.cl
    rec = R("OperatorNew", "u")
    n = e.r(3)
    rec(e)
    e.set_r(3, e.alloc(max(4, n)) if L.ret("new") else 0)


def dtor_stream(name, key, slot=4):
    def f(e):
        stream = e.r(3)
        B = e.u32(stream)
        e.cl.rec(name, (s32(e.u32(stream + slot) - ADDR[key]), s32(e.u32(B) - ADDR[key] - 0xC), s32(e.r(4))))
    return f


def st_iosdtor(e):
    B = ctx_block(e)
    ok = s32(e.u32(B + 0x44)) == s32(e.r(3) - B)
    e.cl.rec("IosDtor", (1 if ok else 0, s32(e.r(4))))


rec_swprintf = R("swprintf", "xuSS")


def st_swprintf(e):
    rec_swprintf(e)
    fmt, arg, n = wstr(e, e.r(5)), wstr(e, e.r(6)), e.r(4)
    out = fmt.replace("%ls", arg)[: max(n - 1, 0)]
    e.write(e.r(3), enc_be(out))


rec_oswrite = R("OstreamWrite", "xS")


def st_oswrite(e):
    stream = e.r(3)
    rec_oswrite(e)
    e.set_r(3, stream)


def st_incr(e):
    e.cl.rec("TreeIncrement", ())
    p = e.r(3)
    e.write(p, struct.pack(">I", e.u32(e.u32(p))))


def st_getloc(e):
    e.cl.rec("getloc", (1 if e.r(4) == ctx_block(e) else 0,))
    e.write(e.r(3), struct.pack(">II", 0x11, 0x22))


def st_usefacet(e):
    w0, w1 = e.u32(e.r(3)), e.u32(e.r(3) + 4)
    e.cl.rec("UseFacet", (w0, w1))
    e.set_r(3, ADDR["facet"])


def st_ptrglue(e):
    ok = e.r(12) == TVEC and e.r(3) == ADDR["facet"]
    e.cl.rec("widen", (1 if e.r(3) == ADDR["facet"] else 0, s32(e.r(4)), 1 if ok else 0))
    e.set_r(3, e.cl.ret("widen") & 0xFFFF)


def st_locdtor(e):
    w0, w1 = e.u32(e.r(3)), e.u32(e.r(3) + 4)
    e.cl.rec("LocaleDtor", (w0, w1, s32(e.r(4))))


def st_getline(e):
    L = e.cl
    stream, buf, n = e.r(3), e.r(4), s32(e.r(5))
    L.rec("getline", (1 if stream == getattr(L, "stream", -1) else 0, n, e.r(6) & 0xFFFF))
    lines = ADDR["lines"]
    cnt, idx = e.u32(lines), e.u32(lines + 4)
    if idx < cnt:
        text = e.read(lines + 8 + 64 * idx, 64)
        chars = [struct.unpack(">H", text[i:i + 2])[0] for i in range(0, 64, 2)]
        out = []
        for c in chars:
            if c == 0 or len(out) + 1 >= n:
                break
            out.append(c)
        e.write(buf, struct.pack(">%dH" % (len(out) + 1), *out, 0))
        e.write(lines + 4, struct.pack(">I", idx + 1))
    else:
        B = e.u32(stream)
        e.write(B + 0x34, struct.pack(">I", e.u32(B + 0x34) | 4))
    e.set_r(3, stream)


def st_wctor(e):
    rec = R("WStringCtor", "xS")
    rec(e)
    text = wstr(e, e.r(4))
    p = e.alloc(2 * len(text) + 2)
    e.write(p, enc_be(text))
    e.write(e.r(3), struct.pack(">I", p))


def st_wcopy(e):
    rec = with_regs(R("WStringCopy", "xSui"), {4: lambda e, s: e.u32(s[4])})
    dst, src = e.r(3), e.r(4)
    rec(e)
    e.write(dst, struct.pack(">I", e.u32(src)))


fake(0x10221f20, ctor_stub("OfsCtor"))
fake(0x10221e20, ctor_stub("IfsCtor"))
fake(0x1007363c, R("StringCtor", ""))
fake(0x102384b8, st_newop)
fake(0x1017de54, R("UserInfoCtor", ""))
fake(0x1017f980, st_retpath)
fake(0x102387a0, st_pathcopy)
fake(0x10238710, st_pathdtor)
fake(0x10238788, st_appstr)
fake(0x1023a8a0, R("PathAppendText", "xS"))
fake(0x10238650, st_strlen)
fake(0x101b2440, R("CharStringAssign", "xsu"))
fake(0x1020100c, R("CharStringAppend", "xsu"))
fake(0x10238578, st_sprintf)
fake(0x10221c30, st_open)
fake(0x10221b40, R("close", "", ret="i"))
fake(0x10238668, st_newarr)
fake(0x100026ac, st_refptr)
fake(0x10238410, st_strcpy)
fake(0x10238680, st_throw)
fake(0x1000221c, R("StringDtor", "xi"))
fake(0x1021cd4c, R("FilebufDtor", "xi"))
fake(0x1021cc68, dtor_stream("OstreamDtor", "ofsvtbl"))
fake(0x1021d4a0, dtor_stream("IstreamDtor", "ifsvtbl", 8))
fake(0x1021cbb8, st_iosdtor)
fake(0x10239160, st_swprintf)
fake(0x1021f374, st_oswrite)
fake(0x10152db0, st_incr)
fake(0x10072a34, st_getloc)
fake(0x1021eb10, st_usefacet)
fake(0x1023baa8, st_ptrglue)
fake(0x10071f34, st_locdtor)
fake(0x102202b0, st_getline)
fake(0x10146a14, st_wctor)
fake(0x101f7424, st_wcopy)
fake(0x101468f8, with_regs(R("WStringDtor", "Si"), {3: lambda e, s: e.u32(s[3])}))
fake(0x10219034, with_regs(R("IgnoreOn", "uS"), {3: lambda e, s: 1 if s[3] == ctx_self(e) else 0, 4: lambda e, s: e.u32(s[4])}))
fake(0x1023ac78, with_regs(R("ChatIgnore", "uSu"), {4: lambda e, s: e.u32(s[4]), 5: lambda e, s: 1 if s[5] == ctx_self(e) else 0}))


def setup(runner):
    e = runner.emu
    ADDR["ctx"] = T.persist_alloc(e, 16)
    ADDR["lines"] = T.persist_alloc(e, 8 + 64 * 8)
    ADDR["pathbuf"] = T.persist_alloc(e, 128)
    vt = T.persist_alloc(e, 0x40)
    ADDR["facet"] = T.persist_alloc(e, 8)
    e.write(ADDR["facet"], struct.pack(">I", vt))
    e.write(vt + 0x2C, struct.pack(">I", TVEC))


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def o(self, mac):
        """The side's offset of a Mac offset in the ios block (host pointers are 8 bytes)."""
        return mac if mac < 4 else mac + self.shift

    def enc(self, text):
        return text.encode("utf-16-be" if self.fmt[0] == ">" else "utf-16-le") + b"\0\0"


class Ppc(Side):
    fmt = ">I"
    shift = 0
    psz = 4
    rep_off = 0xC

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def h(self, a):
        return a

    def g(self, name):
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def putp(self, a, v):
        self.write(a, struct.pack(">I", v & MASK))

    def getp(self, a):
        return self.rd_word(a)

    def log(self):
        return self.L.log

    def call(self, op, this, path):
        self.e.set_r(13, 0x10388A14)   # small-data base: gUserInfo 0x10380b34 = r13 - 0x7ee0
        r3, _, _ = T.call_ppc(self.e, FN_SAVE if op == "save" else FN_LOAD, [this & MASK, path & MASK])
        return r3 & 0xFF


class Host(Side):
    fmt = "<I"
    shift = 4
    psz = 8
    rep_off = 0x10

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def h(self, a):
        return self.lib.tpw_ign_handle(a)

    def g(self, name):
        return self.lib.tpw_ign_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_ign_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def putp(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def getp(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0]

    def log(self):
        return CL.host_end(self.lib)

    def call(self, op, this, path):
        fn = self.lib.tpw_ign_save if op == "save" else self.lib.tpw_ign_load
        return fn(this, path) & 0xFF


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_ign_"
    lib.tpw_ign_addr.restype, lib.tpw_ign_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_ign_alloc.restype, lib.tpw_ign_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_ign_handle.restype, lib.tpw_ign_handle.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_ign_layout.argtypes = [C.POINTER(C.c_int64)]
    for n in ("save", "load"):
        f = getattr(lib, p + n)
        f.restype, f.argtypes = C.c_uint32, [C.c_uint64, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_ign_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def make_text(s, text):
    a = s.alloc(2 * len(text) + 2)
    s.write(a, s.enc(text))
    return a


def make_rep(s, text):
    t = make_text(s, text)
    rep = s.alloc(0x20)
    s.putp(rep + s.rep_off, t)
    return rep


def build(s, case):
    op = case["op"]
    s.putp(s.g("userinfo"), case["ui"])
    B = s.alloc(0x60)
    s.words(B + s.o(0x24), [case["rdbuf"]])
    s.words(B + s.o(0x34), [case["state"], case["exc"]])
    this = s.alloc(0xE90)
    s.words(this, [case["chat"]])
    s.words(this + 8, [s.h(make_rep(s, case["name"]))])
    sentinel = this + 0xE48
    nodes = [s.alloc(0x10) for _ in case["nodes"]]
    for i, (node, text) in enumerate(zip(nodes, case["nodes"])):
        s.words(node, [s.h(nodes[i + 1] if i + 1 < len(nodes) else sentinel)])
        s.words(node + 0xC, [s.h(make_rep(s, text))])
    s.words(this + 0xE4C, [s.h(nodes[0] if nodes else sentinel)])
    ctx = s.g("ctx")
    s.putp(ctx, B)
    s.putp(ctx + s.psz, this)
    s.write(s.g("pathbuf"), s.enc(case["pathtext"]))
    lines = s.g("lines")
    s.words(lines, [len(case["lines"]), 0])
    for i, text in enumerate(case["lines"]):
        s.write(lines + 8 + 64 * i, s.enc(text))
    path = s.alloc(len(case["param"]) + 1)
    s.write(path, case["param"].encode("ascii") + b"\0")
    return {"B": B, "this": this, "path": path}


def execute(s, case, w):
    return s.call(case["op"], w["this"], w["path"])


def observe(s, case, w, answer):
    key = "ofsvtbl" if case["op"] == "save" else "ifsvtbl"
    v0 = s.getp(w["B"])
    ui = s.getp(s.g("userinfo"))
    if case["ui"] == 0:
        ui = 1 if ui else 0
    return {"answer": answer, "log": s.log(), "ui": ui,
            "vt": 0 if v0 == 0 else (v0 - s.g(key)) & MASK,
            "B": s.rd_words(w["B"] + s.o(4), 16),
            "idx": s.rd_word(s.g("lines") + 4)}


def run(s, case):
    try:
        w = build(s, case)
        return observe(s, case, w, execute(s, case, w))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

ALPHA = string.ascii_letters + string.digits + " _.-"


def text(r, lo=0, hi=12):
    return "".join(r.choice(ALPHA) for _ in range(r.randrange(lo, hi + 1)))


def base(r, op):
    return {"op": op, "ui": r.choice([0, 0x4444]), "chat": r.choice([0, 0x7777, 0x7777]),
            "name": text(r, 1), "pathtext": text(r, 1, 30), "param": text(r, 0, 12),
            "state": r.choice([0, 0, 0, 1, 2, 4, 8]), "exc": r.choice([0, 0, 1, 4, 5, 7]),
            "rdbuf": r.choice([0, 0x5555, 0x5555]),
            "queues": {"new": [r.choice([0, 1, 1])], "open": [r.choice([0, 1, 1])], "file": [r.choice([0, 0x20000, 0x20000])],
                       "close": [r.choice([0, 1, 1])], "widen": [r.choice([10, 13, 0, 0xABCD, 65])]},
            "lines": [text(r, 0, 20) for _ in range(r.randrange(0, 5))],
            "nodes": [text(r, 1, 12) for _ in range(r.randrange(0, 5))]}


def edges(op, seed):
    r = random.Random(seed)
    for opn in (0, 1):
        for fl in (0, 0x20000):
            for cl in (0, 1):
                for st, ex, rb in ((0, 0, 0x5555), (0, 4, 0x5555), (0, 1, 0), (1, 0, 0), (0, 0, 0), (2, 5, 0x5555), (4, 4, 0x5555)):
                    for chat in ((0x7777, 0) if op == "load" else (0x7777,)):
                        c = base(r, op)
                        c["queues"]["open"], c["queues"]["file"], c["queues"]["close"] = [opn], [fl], [cl]
                        c["state"], c["exc"], c["rdbuf"], c["chat"] = st, ex, rb, chat
                        if c["queues"]["new"] == [0] and fl:
                            c["ui"] = 0
                        yield c
    for ui in (0, 0x4444):
        for nw in (0, 1):
            c = base(r, op)
            c["ui"], c["queues"]["new"] = ui, [nw]
            yield c


def save_case(r):
    return base(r, "save")


def load_case(r):
    return base(r, "load")


TESTS = [
    FnTest("COnlineControl::SaveIgnore", ppc, host, edge=lambda: edges("save", 5235), rnd=save_case),
    FnTest("COnlineControl::LoadIgnore", ppc, host, edge=lambda: edges("load", 5236), rnd=load_case),
]
