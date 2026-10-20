"""Unit v5342: COnlineControl::SaveBuddy and COnlineControl::ProcessVoteResponse, against libtpw_unit_COnlineControl_SaveBuddy.dylib
(tests/unit_COnlineControl_SaveBuddy.cpp), main app ("SimTheme Park"), src/COnlineControl_..._SaveBuddy.cpp:
  COnlineControl::SaveBuddy 0x1021c75c (the host answer comes from OnlineSaveBuddy), COnlineControl::ProcessVoteResponse 0x1021c3cc

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. Every callee outside
the unit (the stream, filebuf and string constructors and destructors, the path string, the user info, sprintf/swprintf, the stream write,
the tree increment, new, new[], __throw, TbITable::FormatString, the wide string copies, ReceiveText, UDebug) is a fake on both sides that
records its arguments (stack objects as "is it the object made earlier", pointers into the world as labels) and answers from the case.

The world of SaveBuddy: the object (name at 0x8, buddy map header 0xe38, first node 0xe40; nodes hold a key whose data pointer is a wide
string), gUserInfo (0 or an existing block), the ios block the stream constructor fake points the stream at (state, buffer, exception mask),
the path data the path copy fake hands out. The world of ProcessVoteResponse: the object, the string table in the text object's slot at 0x598.
What is compared: the answer, the log, gUserInfo, the ios block words, the exception message buffer, the done-voting byte at 0xe60 and the
chat buffer at 0x220. No fused operations. Not tested: operator new returning NULL (the original never gets it).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_COnlineControl_SaveBuddy"
MASK = 0xFFFFFFFF

FN_SAVE, FN_VOTE = 0x1021C75C, 0x1021C3CC
SIZES = {FN_SAVE: 1064, FN_VOTE: 852}
OWN = set(SIZES)

ADDR = dict(gUserInfo=0x10380B34, gTPGlobals=0x103B74C8, gText=0x1037FF18, gOfsWVtbl=0x102B1F50,
            gIosFailureDtorVec=0x102661D0, gIosFailureVtbl=0x1029D084)
MAC_IOS = dict(streambuf=0x24, state=0x34, exc=0x38, base=0x44)
HOST_LAYOUT_FIXED = [0x28, 0x38, 0x3C, 0x48, 0x24]      # IgnoreIos mStreamBuf, mState, mExceptions, mBaseOffset; IgnoreFilebuf mFile
HOST = {}                                                # filled by bind: the text object's slot offset

ST = {}                                                  # per case, the PowerPC side's remembered objects


def s16(v):
    return T.s16(v)


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def rec(e, name, ints=(), texts=()):
    if texts:
        e.cl.rec(name, tuple(ints), tuple(texts))
    else:
        e.cl.rec(name, tuple(ints))


def wstr(e, a):
    out = []
    for i in range(300):
        c = struct.unpack(">H", e.read(a + 2 * i, 2))[0]
        if not c:
            break
        out.append(chr(c))
    return "".join(out)


def w32(e, a, v):
    e.write(a, struct.pack(">I", v & MASK))


def wwrite(e, a, text):
    e.write(a, b"".join(struct.pack(">H", ord(c)) for c in text) + b"\0\0")


def st_ofs_ctor(e):
    rec(e, "ofs_ctor", (s32(e.r(4)),))
    ST["stream"] = e.r(3)
    w32(e, e.r(3), ST["W"]["ios"])


def st_user_ctor(e):
    rec(e, "user_ctor", (e.cl.lab(e.r(3)),))


def st_online_path(e):
    rec(e, "online_path", (e.cl.lab(e.r(4)),))


def st_path_copy(e):
    rec(e, "path_copy")
    ST["full"] = e.r(3)
    w32(e, e.r(3) + 8, ST["W"]["pathdata"])


def st_path_dtor(e):
    rec(e, "path_dtor", (s16(e.r(4)), int(e.r(3) == ST.get("full"))))


def st_path_apps(e):
    rec(e, "path_apps", (int(e.r(3) == ST.get("full")), e.r(4) - ADDR["gTPGlobals"]))


def st_path_appt(e):
    rec(e, "path_appt", (int(e.r(3) == ST.get("full")),), (wstr(e, e.r(4)),))


def st_str_ctor(e):
    rec(e, "str_ctor")
    ST["name"] = e.r(3)


def st_str_dtor(e):
    rec(e, "str_dtor", (int(e.r(3) == ST.get("name")), s16(e.r(4))))


def st_str_assign(e):
    rec(e, "str_assign", (int(e.r(3) == ST.get("name")), e.r(5)), (e.cstr(e.r(4)),))


def st_str_append(e):
    rec(e, "str_append", (int(e.r(3) == ST.get("name")), e.r(5)), (e.cstr(e.r(4)),))


def st_sprintf(e):
    t = wstr(e, e.r(5))
    rec(e, "sprintf", (), (e.cstr(e.r(4)), t))
    e.write(e.r(3), t[:259].encode("latin-1") + b"\0")


def st_open(e):
    buf = e.r(3)
    ST["buf"] = buf
    rec(e, "open", (e.r(5),), (e.cstr(e.r(4)),))
    w32(e, buf + 0x24, e.cl.ret("file"))
    e.set_r(3, e.cl.ret("open") & MASK)


def st_close(e):
    rec(e, "close", (int(e.r(3) == ST.get("buf")),))
    e.set_r(3, e.cl.ret("close") & MASK)


def st_new(e):
    rec(e, "new", (e.r(3),))
    e.set_r(3, ST["W"]["new"])


def st_new_array(e):
    rec(e, "new_array", (e.r(3),))
    e.set_r(3, ST["W"]["arr"])


def st_refptr(e):
    rec(e, "refptr", (int(e.r(4) == ST["W"]["arr"]),))
    w32(e, e.r(3), e.r(4))


def st_throw(e):
    obj = e.r(4)
    msg = e.u32(obj + 4)
    rec(e, "throw", (int(e.u32(obj) == ADDR["gIosFailureVtbl"]), int(e.r(5) == ADDR["gIosFailureDtorVec"]), int(msg == ST["W"]["arr"])),
        (e.cstr(e.r(3)), e.cstr(msg)))


def st_buf_dtor(e):
    rec(e, "buf_dtor", (int(e.r(3) == ST.get("buf")), s16(e.r(4))))


def st_os_dtor(e):
    rec(e, "os_dtor", (int(e.r(3) == ST.get("stream")), s16(e.r(4)), int(e.u32(e.r(3) + 4) == ADDR["gOfsWVtbl"])))


def st_ios_dtor(e):
    block = ST["W"]["ios"]
    rec(e, "ios_dtor", (s16(e.r(4)), int(e.u32(block + 0x44) == ((e.r(3) - block) & MASK)), int(e.u32(block) == ADDR["gOfsWVtbl"] + 0xC)))


def st_swprintf(e):
    t = wstr(e, e.r(6))
    rec(e, "swprintf", (e.r(4),), (wstr(e, e.r(5)), t))
    wwrite(e, e.r(3), t[:31])


def st_os_write(e):
    rec(e, "os_write", (int(e.r(3) == ST.get("stream")),), (wstr(e, e.r(4)),))


def st_increment(e):
    node = e.u32(e.r(3))
    rec(e, "increment", (e.cl.lab(node),))
    w32(e, e.r(3), e.u32(node + 4))


def st_format(e):
    rec(e, "format", (e.cl.lab(e.r(3)), e.r(4) - ST["this"], e.r(5), e.r(6)))
    wwrite(e, e.r(4), "T%d" % e.r(6))
    e.set_r(3, 0)


def st_wstr_from(e):
    rec(e, "wstr_from", (e.r(4) - ST["this"],))
    ST["first"] = e.r(3)
    w32(e, e.r(3), e.r(4))


def st_wstr_copy(e):
    rec(e, "wstr_copy", (int(e.r(4) == ST.get("first")), e.r(5), e.r(6)))
    ST["second"] = e.r(3)
    w32(e, e.r(3), e.u32(e.r(4)))


def st_receive(e):
    data = e.u32(e.r(4))
    rec(e, "receive", (e.cl.lab(e.r(3)), int(e.r(4) == ST.get("second")), data - ST["this"], e.r(5)), (wstr(e, data),))


def st_wstr_dtor(e):
    a = e.r(3)
    rec(e, "wstr_dtor", (2 if a == ST.get("second") else (1 if a == ST.get("first") else 0), s16(e.r(4))))


def st_udebug(e):
    rec(e, "udebug", (), (e.cstr(e.r(3)),))


for _addr, _fn in [(0x10221F20, st_ofs_ctor), (0x1017DE54, st_user_ctor), (0x1017F980, st_online_path), (0x102387A0, st_path_copy),
                   (0x10238710, st_path_dtor), (0x10238788, st_path_apps), (0x1023A8A0, st_path_appt), (0x1007363C, st_str_ctor),
                   (0x1000221C, st_str_dtor), (0x101B2440, st_str_assign), (0x1020100C, st_str_append), (0x10238578, st_sprintf),
                   (0x10221C30, st_open), (0x10221B40, st_close), (0x102384B8, st_new), (0x10238668, st_new_array),
                   (0x100026AC, st_refptr), (0x10238680, st_throw), (0x1021CD4C, st_buf_dtor), (0x1021CC68, st_os_dtor),
                   (0x1021CBB8, st_ios_dtor), (0x10239160, st_swprintf), (0x1021F374, st_os_write), (0x10152DB0, st_increment),
                   (0x10239730, st_format), (0x10146A14, st_wstr_from), (0x101F7424, st_wstr_copy), (0x10217DBC, st_receive),
                   (0x101468F8, st_wstr_dtor), (0x10000E40, st_udebug)]:
    STUBS.internal(_addr, (lambda f: lambda e: (w32(e, e.r(1) + 0x14, e.r(2)), f(e)))(_fn))   # a glue call saves the TOC at 0x14(r1)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def wstr(self, a, text):
        self.write(a, b"".join(struct.pack(self.fmt[0] + "H", ord(c)) for c in text) + b"\0\0")

    def cstr(self, a):
        return self.read(a, 64).split(b"\0")[0].decode("latin-1")


class Ppc(Side):
    fmt = ">I"
    ios = MAC_IOS
    text_off = 0x598

    def __init__(self, e, case):
        self.e = e
        ST.clear()
        self.fixed = {}
        self.W = build(self, case)
        ST["W"] = self.W
        ST["this"] = self.W["this"]
        self.L = e.cl = CL.PpcLog(self.fixed, case["queues"])
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

    def ptr(self, v):
        return v

    def putptr(self, a, v):
        self.words(a, [v])

    def getptr(self, a):
        return self.rd_word(a)

    def setworld(self, ws):
        pass

    def log(self):
        return self.L.log

    def call_save(self, this, path):
        r3, _, _ = T.call_ppc(self.e, FN_SAVE, [this, path])
        return r3 & MASK

    def call_vote(self, this, code):
        T.call_ppc(self.e, FN_VOTE, [this, code & MASK])
        return 0


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.base = lib.tpw_bud_addr(b"arena")
        self.ios = HOST["ios"]
        self.text_off = HOST["text_off"]
        lib.tpw_bud_reset()
        self.fixed = {}
        self.W = build(self, case)
        CL.host_begin(lib, {label: addr for addr, label in self.fixed.items()}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_bud_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_bud_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def ptr(self, v):
        return (v - self.base) & MASK if v else 0

    def putptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def getptr(self, a):
        lo, hi = self.rd_words(a, 2)
        return lo | (hi << 32)

    def setworld(self, ws):
        self.words(self.g("world"), ws)

    def log(self):
        return CL.host_end(self.lib)

    def call_save(self, this, path):
        return self.lib.tpw_bud_save(this, path) & MASK

    def call_vote(self, this, code):
        self.lib.tpw_bud_vote(this, s32(code))
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_bud_addr.restype, lib.tpw_bud_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_bud_alloc.restype, lib.tpw_bud_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_bud_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_bud_reset.argtypes = []
    lib.tpw_bud_save.restype, lib.tpw_bud_save.argtypes = C.c_uint32, [C.c_uint64, C.c_uint64]
    lib.tpw_bud_vote.restype, lib.tpw_bud_vote.argtypes = None, [C.c_uint64, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_bud_layout(out)
    got = list(out)[:n]
    assert got[:5] == HOST_LAYOUT_FIXED and got[6] == 8, got
    HOST["ios"] = dict(streambuf=got[0], state=got[1], exc=got[2], base=got[3])
    HOST["text_off"] = got[5]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    W = {}
    this = W["this"] = s.alloc(0xE90)
    W["ios"] = s.alloc(0x80)
    W["new"] = s.alloc(0x20)
    W["old"] = s.alloc(0x20)
    W["arr"] = s.alloc(0x40)
    W["table"] = s.alloc(0x20)
    W["pathdata"] = s.alloc(0x200)
    s.fixed.update({W["this"]: 3, W["ios"]: 2, W["new"]: 1, W["old"]: 4, W["arr"]: 6, W["table"]: 5})
    if case["op"] == "save":
        s.wstr(W["pathdata"], case["pdata"])
        rep = s.alloc(0x20)
        namew = s.alloc(0x80)
        s.wstr(namew, case["name"])
        s.words(rep + 0xC, [s.ptr(namew)])
        s.words(this + 8, [s.ptr(rep)])
        sentinel = this + 0xE38
        nodes = []
        for text in case["nodes"]:
            node = s.alloc(0x20)
            key = s.alloc(0x20)
            data = s.alloc(0x80)
            s.wstr(data, text)
            s.words(key + 0xC, [s.ptr(data)])
            s.words(node + 0xC, [s.ptr(key)])
            nodes.append(node)
        for i, node in enumerate(nodes):
            s.words(node + 4, [s.ptr(nodes[i + 1] if i + 1 < len(nodes) else sentinel)])
            s.fixed[node] = 10 + i
        s.words(this + 0xE40, [s.ptr(nodes[0] if nodes else sentinel)])
        o = s.ios
        s.words(W["ios"] + o["streambuf"], [case["sbuf"]])
        s.words(W["ios"] + o["state"], [case["state"]])
        s.words(W["ios"] + o["exc"], [case["exc"]])
        W["path"] = s.alloc(0x80)
        s.write(W["path"], case["path"].encode("latin-1") + b"\0")
        s.putptr(s.g("gUserInfo"), W["old"] if case["ugi"] else 0)
    else:
        s.putptr(s.g("gText") + s.text_off, W["table"])
    s.setworld([s.ptr(W["ios"]), s.ptr(W["pathdata"]), s.ptr(W["new"]), s.ptr(W["arr"]), 0, 0, 0, 0])
    return W


def execute(s, case):
    if case["op"] == "save":
        return s.call_save(s.W["this"], s.W["path"])
    return s.call_vote(s.W["this"], case["code"])


def observe(s, case, answer):
    W = s.W
    out = {"answer": answer, "log": s.log()}
    if case["op"] == "save":
        v = s.getptr(s.g("gUserInfo"))
        out["user"] = 0 if v == 0 else (1 if v == W["new"] else (4 if v == W["old"] else 99))
        o = s.ios
        out["ios"] = [s.rd_word(W["ios"] + o[k]) for k in ("streambuf", "state", "exc")]
        out["msg"] = s.cstr(W["arr"])
    else:
        out["flag"] = s.read(W["this"] + 0xE60, 1)
        buf = struct.unpack(s.fmt[0] + "1024H", s.read(W["this"] + 0x220, 2048))
        text = []
        for c in buf:
            if not c:
                break
            text.append(chr(c))
        out["buf"] = "".join(text)
        out["rest"] = [c for c in buf[len(text) + 1:] if c]
    return out


def run(s, case):
    try:
        return observe(s, case, execute(s, case))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

LETTERS = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789:/\\ ._"


def rtext(r, hi=12):
    return "".join(r.choice(LETTERS) for _ in range(r.randrange(0, hi + 1)))


def save_case(r, **kw):
    c = dict(op="save", ugi=r.choice([0, 1]), pdata=rtext(r, 20), name=rtext(r), path=rtext(r),
             nodes=[rtext(r, 40) for _ in range(r.choice([0, 1, 2, 3, 5]))],
             state=r.choice([0, 1, 2, 4, 5]), exc=r.randrange(8), sbuf=r.choice([0, 0x1234]),
             queues={"open": [r.choice([0, 1])], "file": [r.choice([0, 0x77])], "close": [r.choice([0, 1])]})
    for k in ("open", "file", "close"):
        if k in kw:
            c["queues"][k] = [kw.pop(k)]
    c.update(kw)
    return c


def save_edges():
    r = random.Random(4242)
    for ugi in (0, 1):
        for op in (0, 1):
            for fl in (0, 0x55):
                for cl in (0, 1):
                    for exc in (0, 1, 4, 5, 7):
                        for sbuf in (0, 9):
                            yield save_case(r, ugi=ugi, open=op, file=fl, close=cl, exc=exc, sbuf=sbuf)
    yield save_case(r, nodes=[], pdata="", name="", path="")
    yield save_case(r, nodes=["x"], file=1, open=1, close=1, exc=0)


def save_random(r):
    return save_case(r)


CODES = [0, -2, -0x1E, -0x1F, -3, 1, -1, -4, -5, -6, -0x1D, -0x20, -0x21, 2, 3, 100, -100, 0x7FFFFFFF, -0x80000000, -29, -31, -32, -7]


def vote_case(r, code):
    return dict(op="vote", code=code, queues={})


def vote_edges():
    r = random.Random(77)
    for code in CODES:
        yield vote_case(r, code)


def vote_random(r):
    k = r.random()
    if k < 0.5:
        return vote_case(r, r.randrange(-40, 6))
    return vote_case(r, T.s32(r.getrandbits(32)))


TESTS = [
    FnTest("COnlineControl::SaveBuddy", ppc, host, edge=save_edges, rnd=save_random),
    FnTest("COnlineControl::ProcessVoteResponse", ppc, host, edge=vote_edges, rnd=vote_random),
]
