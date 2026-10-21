"""Unit v5043, the online upload job and the city unpublish request, against libtpw_unit_CFSOnlineSystem_LoggedInRequestUpload.dylib
(tests/unit_CFSOnlineSystem_LoggedInRequestUpload.cpp), main app ("SimTheme Park"), src/game/online/FS_Online.cpp:
  UnpublishPark 0x10174ca0, RequestUpload 0x10174fe8, LoggedInRequestUpload 0x10175220, UploadServerConnectionCallback 0x101753dc,
  ReturnUploadFilename 0x10175608

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside
the unit (the debug log, the string class, operator new / delete / free, wcscpy, the City and Upload connections, the theme list, the other
upload job steps, the user info, the text class, the clock) are fakes on both sides that record their arguments and answer from the case.

The world of a case: an online system (the integer words, the pointers, the three string members with their data, the 256 halfwords),
the globals the code reads (the online system pointer, the user info pointer, the text resource word, the online configuration's city
server and port) and the arguments. What is compared: the event log (every call with its arguments, the park file info handed to
UploadParkRequest as hashes), every integer word of the system, its pointers (as labels), the two global pointers and the upload context.

No fused operations. Not tested: a NULL context for the granted answer (the original reads its use count unconditionally), names longer
than 19 characters or descriptions longer than 255 (they would overflow the info record), the dead initial city release of UnpublishPark
(the original reaches it only with a connection it just tested absent).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CFSOnlineSystem_LoggedInRequestUpload"
MASK = 0xFFFFFFFF

UNPUBLISH, REQUEST, LOGGEDIN, CALLBACK, FILENAME = 0x10174CA0, 0x10174FE8, 0x10175220, 0x101753DC, 0x10175608
SIZES = {UNPUBLISH: 372, REQUEST: 288, LOGGEDIN: 380, CALLBACK: 480, FILENAME: 260}
OWN = set(SIZES)

ADDR = dict(
    gOnlineSystem=0x10380AB0, gUserInfo=0x10380B34, gText=0x1037FF18, gTextRes=0x1037FF18 + 0x564,
    wideSep=0x103B74C8 + 0x44C0, f4498=0x103B74C8 + 0x4498, tail=0x10380AD4,
    cbUpload=0x10267470, cbCity=0x10267480, cbRas=0x102674B0, gOnlineConfig=0x102B2B70,
)

# the members the module touches: name, the Mac offset (the order is tpw_v_layout's)
FIELDS = [
    ("loginCallback", 0x04), ("loginContext", 0x08), ("passData", 0x28), ("f34", 0x34), ("f38", 0x38), ("flags", 0x58),
    ("loggedIn", 0x60), ("city", 0xD4), ("cityClock", 0xD8), ("listsWanted", 0xDC), ("themeList", 0xF8), ("upload", 0x104),
    ("f10CData", 0x114), ("f120", 0x120), ("f320", 0x320), ("active", 0x324), ("done", 0x328), ("failed", 0x32C),
    ("s330", 0x330), ("d330", 0x338), ("f344", 0x344), ("s348", 0x348), ("d348", 0x350), ("s35C", 0x35C), ("d35C", 0x364),
    ("f370", 0x370), ("f374", 0x374), ("f378", 0x378), ("f37C", 0x37C), ("f380", 0x380),
]
MACOFF = dict(FIELDS)
PTRS = ["loginCallback", "loginContext", "passData", "city", "themeList", "upload", "f10CData", "d330", "d348", "d35C"]
INTS = ["f34", "f38", "flags", "loggedIn", "cityClock", "listsWanted", "f320", "active", "done", "failed", "f344", "f370",
        "f374", "f378", "f37C", "f380"]

# the labels the log names: number, and where the object is (a block of the case or a global)
LABELS = dict(sys=1, ctx=2, ctxbuf=3, city=4, upload=5, theme=6, uinfo=7, out=8, gText=9, wideSep=10, tail=11, f4498=12,
              s330=14, s348=15, s35C=16, nameArg=17, descArg=18, fileArg=19, cbUpload=20, cbRas=21, cbCity=22, osys=23,
              d330=24, d348=25, d35C=26, passData=27, f10CData=28)
GLOBAL_LABELS = ["gText", "wideSep", "tail", "f4498", "cbUpload", "cbRas", "cbCity"]
OBJ_LABELS = ["s330", "s348", "s35C"]
# blocks of a case: name, size
BLOCKS = [("ctx", 16), ("ctxbuf", 16), ("city", 0x20), ("upload", 0x20), ("theme", 0x20), ("uinfo", 0x20), ("out", 0x40),
          ("osys", 0x20), ("vptr", 0x80), ("nameArg", 0x240), ("descArg", 0x240), ("fileArg", 0x240), ("d330", 0x240),
          ("d348", 0x240), ("d35C", 0x240), ("passData", 0x240), ("f10CData", 0x240), ("cfgrep", 0x40), ("cfgstr", 0x40)]
WIDE = {"nameArg", "descArg", "d348", "d35C", "passData"}

HOSTOFF = {}
CUR = {}


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)

# The import glue of the Mac saves the TOC pointer in the caller's frame (stw r2, 0x14(r1)) and the code reloads it after the call;
# a fake that stands for a glue does the same.
_stub_internal = STUBS.internal


def _internal(addr, fn):
    if addr >= 0x10238000:
        inner = fn

        def fn(e, inner=inner):
            e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2)))
            inner(e)
    _stub_internal(addr, fn)


STUBS.internal = _internal


def rd_wide(e, a):
    out = b""
    while True:
        h = e.read(a, 2)
        if h == b"\0\0":
            return out
        out += h
        a += 2


def stub_new(e):
    # the host's operator new is the harness's (it records nothing in this library); the constructor fakes that follow
    # (City_ctor, CFSOnlineSystem_ctor, CFSUserInfo_ctor) show what was made
    n = e.r(3)
    e.set_r(3, e.alloc(n if n else 4))


_rec_fmt_n = CL.recorder("Format", "Ls", None)
_rec_fmt_w = CL.recorder("Format", "LS", None)


def stub_glue(e):
    (_rec_fmt_n if e.r(3) == CUR["sys"] + 0x330 else _rec_fmt_w)(e)


_rec_wcscpy = CL.recorder("wcscpy", "xS", None)


def stub_wcscpy(e):
    dst, src = e.r(3), e.r(4)
    _rec_wcscpy(e)
    e.write(dst, rd_wide(e, src) + b"\0\0")
    e.set_r(3, dst)


_rec_upreq = CL.recorder("UploadParkRequest", "LsxL", None)


def h16(vals, until_nul):
    h = 0
    for v in vals:
        if until_nul and v == 0:
            break
        h = (h * 31 + v) & MASK
    return h


def stub_upreq(e):
    info = e.r(5)
    _rec_upreq(e)
    d = e.read(info, 1070)
    mf00 = struct.unpack(">i", d[0:4])[0]
    name = struct.unpack(">20H", d[6:46])
    desc = struct.unpack(">256H", d[46:558])
    words = struct.unpack(">256H", d[558:1070])
    e.cl.rec("ParkFileInfo", (mf00, d[4], d[5], h16(name, True), h16(desc, True), h16(words, False)))


STUBS.internal(0x10000E40, CL.recorder("UDebug", "s", None))
STUBS.internal(0x10175808, CL.recorder("ConnectUploadServer", "L", "i"))
STUBS.internal(0x10175C20, CL.recorder("CompleteUpload", "Li", None))
STUBS.internal(0x10175A18, CL.recorder("InitUpload", "L", None))
STUBS.internal(0x10175154, CL.recorder("RequestUploadPostLoginCallback", "LiL", None))
STUBS.internal(0x1017A4C0, CL.recorder("ReturnOnlineThemeID", "Ls", "b"))
STUBS.internal(0x1016F7D0, CL.recorder("CFSOnlineSystem_ctor", "N", None))
STUBS.internal(0x1017DE54, CL.recorder("CFSUserInfo_ctor", "N", None))
STUBS.internal(0x1017F858, CL.recorder("ReturnUsersPathWithTheme", "xL", None))
STUBS.internal(0x1015E9D8, CL.recorder("GetLanguageId", "L", "i"))
STUBS.internal(0x101E27BC, CL.recorder("CreateOnlineLoginPanel", "L", None))
STUBS.internal(0x10238C80, stub_wcscpy)
STUBS.internal(0x1023A9D8, stub_upreq)
STUBS.internal(0x1023A990, CL.recorder("UploadParkFile", "LsL", None))
STUBS.internal(0x10238A70, CL.recorder("free", "L", None))
STUBS.internal(0x10238218, CL.recorder("delete", "L", None))
STUBS.internal(0x102384B8, stub_new)
STUBS.internal(0x1023BAA8, stub_glue)
STUBS.internal(0x10238740, CL.recorder("StrCtor", "N", None))
STUBS.internal(0x102387A0, CL.recorder("StrCopy", "LL", None))
STUBS.internal(0x10238710, CL.recorder("StrDtor", "Li", None))
STUBS.internal(0x10238788, CL.recorder("StrAppend", "LL", None))
STUBS.internal(0x10239850, CL.recorder("StrAssign", "LL", None))
STUBS.internal(0x1023A678, CL.recorder("City_ctor", "N", None))
STUBS.internal(0x1023A690, CL.recorder("City_Connect", "LsLHL", "i"))
STUBS.internal(0x1023A6A8, CL.recorder("City_SetSessionInfo", "LHiiS", None))
STUBS.internal(0x1023A900, CL.recorder("City_UnpublishPark", "LLii", None))
STUBS.internal(0x10238AA0, CL.recorder("City_Release", "L", None))
STUBS.internal(0x10238AB8, CL.recorder("City_dtor", "Li", None))
STUBS.internal(0x10238A88, CL.recorder("LbTime_GetClock", "", "i"))


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

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def text(self, a, s, wide):
        if wide:
            self.write(a, b"".join(struct.pack(self.fmt[0] + "H", ord(c)) for c in s) + b"\0\0")
        else:
            self.write(a, s.encode() + b"\0")


class Ppc(Side):
    fmt = ">I"
    off = MACOFF

    def __init__(self, e, case):
        self.e = e
        self.case = case
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def newsys(self):
        return self.e.alloc(0x404)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def ptr(self, a, v):
        self.words(a, [v])

    def rd_ptr(self, a):
        return self.rd_word(a)

    def start(self, labels):
        self.L = self.e.cl = CL.PpcLog({addr: n for n, addr in labels.items()}, self.case["queues"])

    def log(self):
        return self.L.log

    def setcity(self, server, port):
        base = ADDR["gOnlineConfig"]
        self.ptr(base + 0xC, self.B["cfgrep"])
        self.ptr(self.B["cfgrep"] + 0xC, self.B["cfgstr"])
        self.text(self.B["cfgstr"], server, False)
        self.half(base + 0x1E, port)

    def call(self, addr, args):
        r3, _, _ = T.call_ppc(self.e, addr, [a & MASK for a in args])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        self.case = case
        bind(lib)
        self.off = HOSTOFF

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_v_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_v_alloc(n)

    def newsys(self):
        return self.lib.tpw_v_newsys()

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def ptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def rd_ptr(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0]

    def start(self, labels):
        CL.host_begin(self.lib, {n: addr for n, addr in labels.items()}, self.case["queues"])

    def log(self):
        return CL.host_end(self.lib)

    def setcity(self, server, port):
        self.lib.tpw_v_setcity(server.encode(), port)

    def call(self, addr, args):
        lib = self.lib
        if addr == UNPUBLISH:
            lib.tpw_v_unpublish(args[0], s32(args[1]))
        elif addr == REQUEST:
            lib.tpw_v_request(args[0], args[1], s32(args[2]), args[3], args[4], s32(args[5]), args[6], args[7])
        elif addr == LOGGEDIN:
            lib.tpw_v_loggedin(args[0])
        elif addr == CALLBACK:
            lib.tpw_v_callback(args[0], s32(args[1]), 0, 0, args[4])
        else:
            lib.tpw_v_filename(args[1], args[0])
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    u64, i32, u32 = C.c_uint64, C.c_int32, C.c_uint32
    lib.tpw_v_addr.restype, lib.tpw_v_addr.argtypes = u64, [C.c_char_p]
    lib.tpw_v_alloc.restype, lib.tpw_v_alloc.argtypes = u64, [u32]
    lib.tpw_v_newsys.restype, lib.tpw_v_newsys.argtypes = u64, []
    lib.tpw_v_setcity.restype, lib.tpw_v_setcity.argtypes = None, [C.c_char_p, u32]
    lib.tpw_v_layout.restype, lib.tpw_v_layout.argtypes = i32, [C.POINTER(C.c_int64)]
    lib.tpw_v_unpublish.restype, lib.tpw_v_unpublish.argtypes = None, [u64, i32]
    lib.tpw_v_request.restype, lib.tpw_v_request.argtypes = None, [u64, u64, i32, u64, u64, i32, u64, u64]
    lib.tpw_v_loggedin.restype, lib.tpw_v_loggedin.argtypes = None, [u64]
    lib.tpw_v_callback.restype, lib.tpw_v_callback.argtypes = None, [u64, i32, i32, u64, u64]
    lib.tpw_v_filename.restype, lib.tpw_v_filename.argtypes = None, [u64, u64]
    out = (C.c_int64 * 40)()
    n = lib.tpw_v_layout(out)
    assert n == len(FIELDS) + 1, n
    HOSTOFF.clear()
    HOSTOFF.update({name: out[i] for i, (name, _) in enumerate(FIELDS)})
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    sysa = s.newsys()
    B = {"sys": sysa}
    for name, size in BLOCKS:
        B[name] = s.alloc(size)
    s.B = B
    CUR["sys"] = sysa
    labels = {}
    for name in B:
        if name in LABELS:
            labels[LABELS[name]] = B[name]
    for name in GLOBAL_LABELS:
        labels[LABELS[name]] = s.g(name)
    for name in OBJ_LABELS:
        labels[LABELS[name]] = sysa + s.off[name]
    if isinstance(s, Host):
        labels = {n: a for n, a in labels.items()}
    else:
        labels = {n: a for n, a in labels.items()}
    s.labels = labels
    s.start(labels)

    def addr_of(name):
        if name is None:
            return 0
        if name in B:
            return B[name]
        return s.g(name)

    o = s.off
    for name in INTS:
        s.words(sysa + o[name], [case["ints"][name]])
    for name in PTRS:
        s.ptr(sysa + o[name], addr_of(case["ptrs"].get(name)))
    s.write(sysa + o["f120"], b"".join(struct.pack(s.fmt[0] + "H", v) for v in case["f120"]))
    if isinstance(s, Ppc):
        for name in OBJ_LABELS:
            s.words(sysa + o[name], [B["vptr"]])
    for name, text in case["texts"].items():
        s.text(B[name], text, name in WIDE)
    # the context: [buffer, 0, count]
    s.ptr(B["ctx"], B["ctxbuf"] if case["ctxbuf"] else 0)
    s.words(B["ctx"] + 8, [case["ctxcount"]])
    # the globals
    s.ptr(s.g("gOnlineSystem"), addr_of(case["gos"]))
    s.ptr(s.g("gUserInfo"), addr_of(case["guser"]))
    s.ptr(s.g("gTextRes"), case["res"])
    s.setcity(case["server"], case["port"])


def execute(s, case):
    B, sysa = s.B, s.B["sys"]
    op = case["op"]
    if op == "UnpublishPark":
        return s.call(UNPUBLISH, [sysa, case["park"]])
    if op == "RequestUpload":
        return s.call(REQUEST, [sysa, B["fileArg"], case["a344"], B["nameArg"], B["descArg"], case["a370"], case["acb"], case["actx"]])
    if op == "LoggedInRequestUpload":
        return s.call(LOGGEDIN, [sysa])
    if op == "UploadServerConnectionCallback":
        return s.call(CALLBACK, [sysa, case["result"], 0, 0, B["ctx"]])
    return s.call(FILENAME, [B["out"], sysa])


def norm(s, v):
    if v == 0:
        return 0
    for n, a in s.labels.items():
        if a == v:
            return n
    return 99


def observe(s, case, answer):
    sysa, o = s.B["sys"], s.off
    ints = {n: s.rd_word(sysa + o[n]) for n in INTS}
    ptrs = {n: norm(s, s.rd_ptr(sysa + o[n])) for n in PTRS}
    return {
        "log": s.log(), "ints": ints, "ptrs": ptrs,
        "gos": norm(s, s.rd_ptr(s.g("gOnlineSystem"))), "guser": norm(s, s.rd_ptr(s.g("gUserInfo"))),
        "ctx": [s.rd_ptr(s.B["ctx"]) != 0, s.rd_word(s.B["ctx"] + 8)],
    }


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

LET = "abcdefghijklmnopqrstuvwxyz0123456789 _-"


def rs(r, n):
    return "".join(r.choice(LET) for _ in range(n))


def w32(r):
    return r.choice([0, 1, 0x7FFFFFFF, 0x80000000, MASK, r.randrange(1 << 32), r.randrange(100)])


def base(r, op):
    ints = {n: w32(r) for n in INTS}
    ints["flags"] = r.choice([0, 0x10, 0xFFFFFFEF, MASK, 0x1F])
    ints["loggedIn"] = r.choice([0, 1, 5])
    ints["f370"] = r.choice([0, 1, 255, 0x1234, MASK])
    ptrs = {"loginCallback": r.choice([None, "cbUpload"]), "loginContext": r.choice([None, "ctx"]), "city": r.choice([None, "city"]),
            "themeList": "theme", "upload": r.choice([None, "upload"]), "passData": "passData", "f10CData": "f10CData",
            "d330": "d330", "d348": "d348", "d35C": "d35C"}
    return {
        "op": op, "ints": ints, "ptrs": ptrs, "f120": [r.choice([0, 1, 0xFFFF, r.randrange(65536)]) for _ in range(256)],
        "texts": {"nameArg": rs(r, r.choice([0, 1, 19])), "descArg": rs(r, r.choice([0, 1, 255, r.randrange(256)])),
                  "fileArg": rs(r, r.randrange(1, 30)), "d330": rs(r, r.randrange(30)), "d348": rs(r, r.choice([0, 5, 19])),
                  "d35C": rs(r, r.choice([0, 7, 255])), "passData": rs(r, r.randrange(12)), "f10CData": rs(r, r.randrange(1, 12))},
        "ctxbuf": r.choice([True, False]), "ctxcount": r.choice([1, 2, 0, 0x80000001, MASK, 5]),
        "gos": r.choice([None, "osys"]), "guser": r.choice([None, "uinfo"]), "res": r.choice([0, 0x12345678, 0x7FFFFFF0]),
        "server": rs(r, r.randrange(0, 23)), "port": r.choice([0, 0x7FFF, 0x8000, 0xFFFF, r.randrange(65536)]),
        "park": r.choice([0, 1, -1, r.randrange(1, 1000000)]), "a344": r.choice([0, 7, -1, 0x7FFFFFFF]),
        "a370": r.choice([0, 1, 255, 0x1234, -1]), "acb": r.choice([0, 0x1234, 0x7FFF0000]), "actx": r.choice([0, 0x4321, 0x7FFF0000]),
        "result": 0,
        "queues": {
            "ConnectUploadServer": [r.choice([0, 1])], "ReturnOnlineThemeID": [r.choice([0, 1, 7, 255])],
            "City_Connect": [r.choice([0, 1, 2, -1])], "LbTime_GetClock": [r.randrange(1 << 31)],
            "GetLanguageId": [r.randrange(1 << 31) if r.random() < 0.5 else r.choice([0, 1, 5, 0x10005, 0xFFFF])],
        },
    }


def mk(op, seed, n, **over):
    r = random.Random(seed)
    for _ in range(n):
        c = base(r, op)
        c.update(over)
        yield c


def rnd_of(op, seed_base):
    def gen(r):
        c = base(r, op)
        if op == "UploadServerConnectionCallback":
            c["result"] = r.choice(RESULTS) if r.random() < 0.7 else r.randrange(-0x60, 0x60)
        return c
    return gen


RESULTS = [r for r in range(-0x2A, 0x53)] + [-0x28, -0x14, 0x7FFFFFFF, -0x80000000, 0x50, 0x51, 0x4F]


def edges_unpublish():
    r = random.Random(11)
    for city in (None, "city"):
        for conn in (0, 1, 2, -1):
            for port in (0, 0x7FFF, 0x8000, 0xFFFF):
                c = base(r, "UnpublishPark")
                c["ptrs"]["city"] = city
                c["queues"]["City_Connect"] = [conn]
                c["port"] = port
                yield c


def edges_request():
    r = random.Random(12)
    for logged in (0, 1, 5):
        for gos in (None, "osys"):
            c = base(r, "RequestUpload")
            c["ints"]["loggedIn"] = logged
            c["gos"] = gos
            yield c


def edges_loggedin():
    r = random.Random(13)
    for flags in (0, 0x10, 0xFFFFFFEF, MASK):
        for upload in (None, "upload"):
            for conn in (0, 1):
                for nl, dl in ((0, 0), (19, 255), (3, 40)):
                    c = base(r, "LoggedInRequestUpload")
                    c["ints"]["flags"] = flags
                    c["ptrs"]["upload"] = upload
                    c["queues"]["ConnectUploadServer"] = [conn]
                    c["texts"]["d348"] = rs(r, nl)
                    c["texts"]["d35C"] = rs(r, dl)
                    c["queues"]["ReturnOnlineThemeID"] = [r.choice([0, 255, 3])]
                    c["ints"]["f370"] = r.choice([0, 255, 0x1FF, MASK])
                    yield c


def edges_callback():
    r = random.Random(14)
    for res in RESULTS:
        c = base(r, "UploadServerConnectionCallback")
        c["result"] = res
        yield c
    for count in (1, 2, 0, 0x80000001, MASK):
        for buf in (True, False):
            c = base(r, "UploadServerConnectionCallback")
            c["result"] = 0x50
            c["ctxcount"] = count
            c["ctxbuf"] = buf
            yield c


def edges_filename():
    r = random.Random(15)
    for g in ("uinfo", None):
        for _ in range(2):
            c = base(r, "ReturnUploadFilename")
            c["guser"] = g
            yield c


def rnd_unpublish(r):
    return base(r, "UnpublishPark")


def rnd_request(r):
    return base(r, "RequestUpload")


def rnd_loggedin(r):
    return base(r, "LoggedInRequestUpload")


def rnd_callback(r):
    c = base(r, "UploadServerConnectionCallback")
    c["result"] = r.choice(RESULTS) if r.random() < 0.7 else r.choice([r.randrange(-0x60, 0x60), s32(r.randrange(1 << 32))])
    return c


def rnd_filename(r):
    return base(r, "ReturnUploadFilename")


TESTS = [
    FnTest("UnpublishPark", ppc, host, edge=edges_unpublish, rnd=rnd_unpublish),
    FnTest("RequestUpload", ppc, host, edge=edges_request, rnd=rnd_request),
    FnTest("LoggedInRequestUpload", ppc, host, edge=edges_loggedin, rnd=rnd_loggedin),
    FnTest("UploadServerConnectionCallback", ppc, host, edge=edges_callback, rnd=rnd_callback),
    FnTest("ReturnUploadFilename", ppc, host, edge=edges_filename, rnd=rnd_filename),
]
