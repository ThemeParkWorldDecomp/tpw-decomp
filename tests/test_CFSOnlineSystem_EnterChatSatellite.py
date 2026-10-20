"""Unit v5130, the online system's chat satellite, publishing, upload and FTP download entry points and AreWeProcessing, against
libtpw_unit_CFSOnlineSystem_EnterChatSatellite.dylib (tests/unit_CFSOnlineSystem_EnterChatSatellite.cpp), main app ("SimTheme Park"),
src/FS_Online_init_CFSOnlineSystem_EnterChatSatellite.cpp:
  EnterChatSatellite 0x10174be8, SetPublishing 0x10174e4c, RequestUploadPostLoginCallback 0x10175154, CompleteUpload 0x10175c20,
  RequestFTPDownload 0x10175dfc, GotoPlayerRequestFTPDownloadByID 0x10176074, GotoPlayerFTPPostGetParkInfoCallback 0x101762e8,
  AreWeProcessing 0x10171498

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian,
32-bit pointers) for the original, and in the host library's memory (little-endian, 64-bit pointers, host offsets from tpw_cs_off) for the
translation. The callees outside the unit are fakes on both sides that record their arguments: ReturnParkInfo, CreateOnlineParkInfoPanel,
AddUIError, Logout, Upload::Release, the constructor, LoggedInRequestUpload, StopProcessing, CFSOnlinePark::operator=, the post login
callbacks, CompleteFTPDownload, LoggedInRequestFTPDownload, CreateOnlineLoginPanel. The virtual calls (the string's operator= at slot 0x20
and the download globe's slot 0x50) and the stored callback are __ptr_glue calls on the PowerPC side (a stub that tells the target by r12)
and calls through hand made vtables and the lower half of a fake's address (the stored callback word) on the host.

The world of a case: the system block with random words, the lists, the globe and string objects, gOnlineSystem and gLobbyScreens. What is
compared: the answer, every word and pointer (as a label) the functions may write, the 256 halfwords at 0x120, gOnlineSystem and the event
log.

No fused operations. Not tested: a NULL list or screen pointer (both read it unconditionally), SetPublishing with publishing set and NULL
words (the copy reads it).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CFSOnlineSystem_EnterChatSatellite"
MASK = 0xFFFFFFFF

F_ENTER, F_SETPUB, F_UPCB, F_COMPLETE = 0x10174BE8, 0x10174E4C, 0x10175154, 0x10175C20
F_FTP, F_FTPID, F_GOTO, F_ARE = 0x10175DFC, 0x10176074, 0x101762E8, 0x10171498
SIZES = {F_ENTER: 124, F_SETPUB: 164, F_UPCB: 128, F_COMPLETE: 228, F_FTP: 208, F_FTPID: 184, F_GOTO: 72, F_ARE: 208}
OWN = set(SIZES)
ENTRY = {"enter": F_ENTER, "setpub": F_SETPUB, "upcb": F_UPCB, "complete": F_COMPLETE, "ftp": F_FTP, "ftpid": F_FTPID, "goto": F_GOTO,
         "are": F_ARE}

# callees faked on both sides
RETURN_PARK, PANEL, PTR_GLUE, ADD_UI_ERROR, LOGOUT = 0x101791D8, 0x101EABB4, 0x1023BAA8, 0x101F1DF0, 0x10171398
UPLOAD_RELEASE, CTOR, LOGGED_IN_UPLOAD, STOP, PARK_ASSIGN = 0x10238AE8, 0x1016F7D0, 0x10175220, 0x10171404, 0x1017C2E4
REQ_FTP_POST, LOGIN_PANEL, GOTO_ID_POST, COMPLETE_FTP, LOGGED_IN_FTP = 0x10175F24, 0x101E27BC, 0x10176180, 0x10176E90, 0x10176418

# TVectors the Mac passes by address, and the fake targets the glue stub recognises in r12
TV_CANCEL, TV_GENERAL, TV_REQFTP, TV_GOTOID, TV_RAS = 0x10267478, 0x10268590, 0x10267460, 0x10267458, 0x102674B0
VT_STR, VT_GLOBE, CBWORD = 0x7000A000, 0x7000A010, 0x7000A020

ADDR = dict(gOnlineSystem=0x10380AB0, gLobbyScreens=0x102E49B8)

MAC = {"sys.mLogin": 0, "sys.mLoginCallback": 4, "sys.mLoginContext": 8, "sys.mLoggedIn": 0x60, "sys.mNews": 0x6C,
       "sys.mCityList": 0xE0, "sys.mParksTop10": 0xE4, "sys.mParksInCity": 0xE8, "sys.mParksByPlayer": 0xEC, "sys.mParksByID": 0xF0,
       "sys.mParksInCity2": 0xF4, "sys.mThemeList": 0xF8, "sys.mServerList": 0xFC, "sys.mRideList": 0x100, "sys.mUpload": 0x104,
       "sys.mF108": 0x108, "sys.mF10C": 0x10C, "sys.mF120": 0x120, "sys.mF320": 0x320, "sys.mUploadActive": 0x324, "sys.mF374": 0x374,
       "sys.mF378": 0x378, "sys.mF37C": 0x37C, "sys.mF380": 0x380, "sys.mF384": 0x384, "sys.mF388": 0x388, "sys.mFTPActive": 0x38C,
       "sys.mFTPPark": 0x3A4, "sys.mF3A8": 0x3A8, "sys.mF3C0": 0x3C0, "sys.mF3C4": 0x3C4, "sys.size": 0x404, "list.mF0C": 0xC,
       "list.mList": 0x2C, "tl.mCount": 0xC, "scr.mDownloadGlobe": 0xC}
LISTS = ["mCityList", "mParksTop10", "mParksInCity", "mParksByPlayer", "mParksByID", "mParksInCity2", "mThemeList", "mServerList",
         "mRideList"]
SYS_W = ["mF108", "mF320", "mUploadActive", "mF374", "mF378", "mF37C", "mF380", "mF384", "mF388", "mFTPActive", "mF3A8", "mF3C0",
         "mF3C4", "mLoggedIn"]
SYS_P = ["mLogin", "mNews", "mUpload", "mFTPPark", "mLoginContext"]
HOST_LAYOUT = []


def s32(v):
    return T.s32(v)


def text_hash(data):
    h = 0
    for b in data:
        h = (h * 31 + b) & MASK
    return h


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def rec(name, fn):
    def stub(e):
        L = e.cl
        L.rec(name, fn(e, L))
    return stub


def stub_glue(e):
    L = e.cl
    r12 = e.r(12) & MASK
    e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2) & MASK))        # what the real glue does: the TOC goes to 0x14(r1)
    if r12 == VT_STR:
        data = e.cstr(e.r(4))
        if isinstance(data, str):
            data = data.encode("latin1")
        L.rec("StringAssign", (L.lab(e.r(3)), len(data), text_hash(data)))
    elif r12 == VT_GLOBE:
        L.rec("GlobeSlot50", (L.lab(e.r(3)),))
    elif r12 == CBWORD:
        L.rec("Callback", (s32(e.r(3)), e.r(4) & MASK))
    else:
        L.rec("BADGLUE", (r12,))


def stub_return_park(e):
    L = e.cl
    idx = s32(e.r(4))
    L.rec("ReturnParkInfo", (L.lab(e.r(3)), idx))
    e.set_r(3, (0x123400 + idx * 16) & MASK)


STUBS.internal(PTR_GLUE, stub_glue)
STUBS.internal(RETURN_PARK, stub_return_park)
STUBS.internal(PANEL, rec("CreateOnlineParkInfoPanel", lambda e, L: (e.r(3) & MASK, L.lab(e.r(4)), int(e.r(5) == TV_CANCEL),
                                                                      int(e.r(6) == TV_GENERAL), e.r(7) & MASK)))
STUBS.internal(ADD_UI_ERROR, rec("AddUIError", lambda e, L: (s32(e.r(3)), e.r(4) & MASK)))
STUBS.internal(LOGOUT, rec("Logout", lambda e, L: (L.lab(e.r(3)),)))
STUBS.internal(UPLOAD_RELEASE, rec("UploadRelease", lambda e, L: (L.lab(e.r(3)),)))
STUBS.internal(CTOR, CL.recorder("CFSOnlineSystem", "N"))
STUBS.internal(LOGGED_IN_UPLOAD, rec("LoggedInRequestUpload", lambda e, L: (L.lab(e.r(3)),)))
STUBS.internal(STOP, rec("StopProcessing", lambda e, L: (L.lab(e.r(3)),)))
STUBS.internal(PARK_ASSIGN, rec("ParkAssign", lambda e, L: (L.lab(e.r(3)), L.lab(e.r(4)))))
STUBS.internal(REQ_FTP_POST, rec("RequestFTPPostLoginCallback", lambda e, L: (L.lab(e.r(3)), s32(e.r(4)), e.r(5) & MASK)))
STUBS.internal(GOTO_ID_POST, rec("GotoPlayerByIDFTPPostLoginCallback", lambda e, L: (L.lab(e.r(3)), s32(e.r(4)), e.r(5) & MASK)))
STUBS.internal(COMPLETE_FTP, rec("CompleteFTPDownload", lambda e, L: (L.lab(e.r(3)), s32(e.r(4)))))
STUBS.internal(LOGGED_IN_FTP, rec("LoggedInRequestFTPDownload", lambda e, L: (L.lab(e.r(3)), L.lab(e.r(4)))))
STUBS.internal(LOGIN_PANEL, rec("CreateOnlineLoginPanel", lambda e, L: (int(e.r(3) == TV_RAS),)))


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def w32(self, a, v):
        self.write(a, struct.pack(self.fmt, v & MASK))

    def r32(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]

    def wptr(self, a, p):
        self.write(a, struct.pack(self.pfmt, p))

    def rptr(self, a):
        return struct.unpack(self.pfmt, self.read(a, self.psz))[0]

    def halves(self, vals):
        return struct.pack(self.fmt[0] + "%dH" % len(vals), *vals)

    def unhalves(self, data):
        return list(struct.unpack(self.fmt[0] + "%dH" % (len(data) // 2), data))

    def call_op(self, op, *vals):
        raise NotImplementedError


class Ppc(Side):
    fmt, pfmt, psz = ">I", ">I", 4

    def __init__(self, e, case):
        self.e = e
        self.case = case
        self.saved = None

    def begin(self, objs):
        self.L = self.e.cl = CL.PpcLog({addr: label for label, addr in objs.items()}, self.case["queues"])
        self.saved = STUBS.activate(self.e)

    def done(self):
        if self.saved is not None:
            CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def off(self, name):
        return MAC[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def label(self, p):
        return self.L.lab(p)

    def cbword(self):
        return CBWORD

    def cbid(self, p):
        return {0: 0, TV_REQFTP: 1, TV_GOTOID: 2}.get(p, 3)

    def vptr(self, a, which):
        vt = self.alloc(0x80)
        if which == "str":
            self.w32(vt + 0x20, VT_STR)
        else:
            self.w32(vt + 0x50, VT_GLOBE)
        self.w32(a, vt)

    def call_op(self, op, *vals):
        r3, _, _ = T.call_ppc(self.e, ENTRY[op], [v & MASK for v in vals])
        return r3 & MASK


HOST_SIG = {"enter": [C.c_uint64, C.c_int32], "setpub": [C.c_uint64, C.c_int32, C.c_uint64, C.c_uint64],
            "upcb": [C.c_uint64, C.c_int32, C.c_uint64], "complete": [C.c_uint64, C.c_int32],
            "ftp": [C.c_uint64] * 4, "ftpid": [C.c_uint64, C.c_int32, C.c_uint64, C.c_uint64],
            "goto": [C.c_uint64, C.c_int32, C.c_uint64, C.c_uint64], "are": [C.c_uint64]}


class Host(Side):
    fmt, pfmt, psz = "<I", "<Q", 8

    def __init__(self, lib, case):
        self.lib = lib
        self.case = case
        self.cache = {}
        bind(lib)

    def begin(self, objs):
        CL.host_begin(self.lib, dict(objs), self.case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_cs_addr(name.encode())

    def off(self, name):
        if name not in self.cache:
            self.cache[name] = self.lib.tpw_cs_off(name.encode())
            assert self.cache[name] >= 0, name
        return self.cache[name]

    def alloc(self, n):
        return self.lib.tpw_cs_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def label(self, p):
        return self.lib.tpw_cs_lab(p)

    def cbword(self):
        w = self.lib.tpw_cs_thunk()
        assert w != 0
        return w

    def cbid(self, p):
        if p == 0:
            return 0
        return {self.lib.tpw_cs_fn(1): 1, self.lib.tpw_cs_fn(2): 2}.get(p, 3)

    def vptr(self, a, which):
        self.wptr(a, self.lib.tpw_cs_vt(0 if which == "str" else 1))

    def call_op(self, op, *vals):
        r = getattr(self.lib, "tpw_cs_" + op)(*vals)
        return (r or 0) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_cs_addr.restype, lib.tpw_cs_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_cs_alloc.restype, lib.tpw_cs_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_cs_off.restype, lib.tpw_cs_off.argtypes = C.c_int64, [C.c_char_p]
    lib.tpw_cs_vt.restype, lib.tpw_cs_vt.argtypes = C.c_uint64, [C.c_int32]
    lib.tpw_cs_thunk.restype, lib.tpw_cs_thunk.argtypes = C.c_uint32, []
    lib.tpw_cs_fn.restype, lib.tpw_cs_fn.argtypes = C.c_uint64, [C.c_int32]
    lib.tpw_cs_lab.restype, lib.tpw_cs_lab.argtypes = C.c_int64, [C.c_uint64]
    lib.tpw_cs_layout.argtypes = [C.POINTER(C.c_int64)]
    for op, sig in HOST_SIG.items():
        f = getattr(lib, "tpw_cs_" + op)
        f.argtypes = sig
        f.restype = C.c_int32 if op == "are" else None
    out = (C.c_int64 * 32)()
    n = lib.tpw_cs_layout(out)
    assert list(out)[:n] == HOST_LAYOUT
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    """Writes the case's world; returns {label: address} of the objects the log may name."""
    O = {}

    def obj(label, size=0x80):
        a = s.alloc(size)
        O[label] = a
        return a

    sysa = obj(1, s.off("sys.size"))

    def so(n):
        return sysa + s.off("sys." + n)

    for n, v in case["words"].items():
        s.w32(so(n), v)
    s.w32(so("mF374"), s.cbword() if case["cb"] else 0)
    s.wptr(so("mLoginCallback"), 0x1111)
    s.wptr(so("mLoginContext"), 0x2222)
    s.wptr(so("mUpload"), obj(3) if case["upload"] else 0)
    s.wptr(so("mFTPPark"), obj(6))
    s.wptr(so("mLogin"), obj(12) if case["login"] else 0)
    s.wptr(so("mNews"), obj(13) if case["news"] else 0)
    s.write(so("mF120"), s.halves(case["junk"]))
    s.vptr(so("mF10C"), "str")
    O[2] = so("mF10C")
    g = {0: 0, 1: obj(8), 2: sysa}[case["gsys"]]
    s.wptr(s.g("gOnlineSystem"), g)
    op, a = case["op"], case["args"]
    if op == "enter":
        lst, tl = obj(5), obj(10)
        s.wptr(so("mParksInCity2"), lst)
        s.wptr(lst + s.off("list.mList"), tl)
        s.w32(tl + s.off("tl.mCount"), a["count"])
        screens, globe = obj(11), obj(4)
        s.wptr(s.g("gLobbyScreens"), screens)
        s.wptr(screens + s.off("scr.mDownloadGlobe"), globe)
        s.vptr(globe, "globe")
    elif op == "setpub":
        s.words_block = obj(14, 0x200)
        s.write(s.words_block, s.halves(a["halves"]))
        if a["text"] is not None:
            t = obj(15, len(a["text"]) + 1)
            s.write(t, a["text"] + b"\0")
    elif op == "are":
        for i, name in enumerate(LISTS):
            lst = obj(20 + i)
            s.w32(lst + s.off("list.mF0C"), case["lists"][i])
            s.wptr(so(name), lst)
    elif op in ("ftp", "goto"):
        if a.get("park"):
            obj(7)
    return O


def execute(s, case, O):
    op, a = case["op"], case["args"]
    sysa = O[1]
    if op == "enter":
        return s.call_op(op, sysa, a["idx"])
    if op == "setpub":
        return s.call_op(op, sysa, a["publishing"], O.get(15, 0), O[14])
    if op == "upcb":
        return s.call_op(op, sysa, a["result"], a["ctx"])
    if op == "complete":
        return s.call_op(op, sysa, a["failed"])
    if op == "ftp":
        return s.call_op(op, sysa, O[7], a["cb"], a["ctx"])
    if op == "ftpid":
        return s.call_op(op, sysa, a["id"], a["cb"], a["ctx"])
    if op == "goto":
        return s.call_op(op, sysa, a["result"], a["ctx"], O.get(7, 0))
    return s.call_op(op, sysa)


def observe(s, case, O, answer):
    sysa = O[1]

    def so(n):
        return sysa + s.off("sys." + n)

    d = {"answer": answer if case["op"] == "are" else 0}
    for n in SYS_W:
        d[n] = s.r32(so(n))
    d["mF374"] = int(s.r32(so("mF374")) != 0)
    for n in SYS_P:
        d[n] = s.label(s.rptr(so(n)))
    d["mLoginCallback"] = s.cbid(s.rptr(so("mLoginCallback")))
    d["gOnlineSystem"] = s.label(s.rptr(s.g("gOnlineSystem")))
    d["mF120"] = s.unhalves(s.read(so("mF120"), 0x200))
    d["log"] = s.log()
    return d


def run(s, case):
    try:
        O = build(s, case)
        s.begin(O)
        ans = execute(s, case, O)
        return observe(s, case, O, ans)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators


def rw(r):
    return r.choice([0, 0, 1, -1, 2, 0x7FFFFFFF, -0x80000000, r.randrange(1, 100), r.getrandbits(32)]) & MASK


def base(r, op, **args):
    words = {n: (0 if r.random() < 0.4 else r.getrandbits(32)) for n in SYS_W if n != "mF374"}
    return {"op": op, "queues": {}, "words": words, "cb": r.choice([0, 1]), "upload": r.choice([0, 1]), "gsys": r.choice([0, 1, 2]),
            "login": 0, "news": 0, "lists": [0] * 9, "junk": [r.getrandbits(16) for _ in range(256)], "args": args}


def mk(r, op, words=None, **kw):
    args = kw.pop("args")
    c = base(r, op, **args)
    if words:
        c["words"].update(words)
    c.update(kw)
    return c


def enter_edges():
    r = random.Random(11)
    for count in (0, 1, 3, -1):
        for idx in (-2, -1, 0, 1, 2, 3, 4):
            yield mk(r, "enter", args={"count": count, "idx": idx})


def enter_random(r):
    count = r.choice([0, 1, 2, 5, 100, -1, r.randrange(-3, 20)])
    idx = r.choice([0, 1, count - 1, count, count + 1, -1, r.randrange(-5, 25), r.getrandbits(32)])
    return mk(r, "enter", args={"count": count, "idx": idx})


def setpub_case(r, publishing, text):
    return mk(r, "setpub", args={"publishing": publishing, "text": text, "halves": [r.getrandbits(16) for _ in range(256)]})


def setpub_edges():
    r = random.Random(12)
    for publishing in (0, 1, -1, 7):
        for text in (None, b"hello", b""):
            yield setpub_case(r, publishing, text)


def setpub_random(r):
    text = r.choice([None, b"Park " + bytes([65 + r.randrange(26) for _ in range(r.randrange(30))]), b""])
    return setpub_case(r, r.choice([0, 1, -1, r.getrandbits(32)]), text)


def upcb_edges():
    r = random.Random(13)
    for result in (0, 1, -1, 5):
        for gsys in (0, 1, 2):
            c = mk(r, "upcb", args={"result": result, "ctx": rw(r)}, gsys=gsys)
            yield c
    for f37c, f380 in ((0, 0), (5, 0), (0, 6), (5, 6)):
        for f320 in (0, 3):
            yield mk(r, "upcb", args={"result": 1, "ctx": 0}, words={"mF37C": f37c, "mF380": f380, "mF320": f320}, cb=1, upload=1)


def upcb_random(r):
    return mk(r, "upcb", args={"result": r.choice([0, 0, 1, -1, rw(r)]), "ctx": rw(r)})


def complete_edges():
    r = random.Random(14)
    for failed in (0, 1, -1):
        for f37c, f380 in ((0, 0), (5, 0), (0, 6), (5, 6), (-1, 0x80000001)):
            for f320 in (0, 3):
                for cb in (0, 1):
                    for upload in (0, 1):
                        yield mk(r, "complete", args={"failed": failed},
                                 words={"mF37C": f37c, "mF380": f380, "mF320": f320, "mF378": rw(r)}, cb=cb, upload=upload)


def complete_random(r):
    return mk(r, "complete", args={"failed": r.choice([0, 1, -1, rw(r)])},
              words={"mF37C": r.choice([0, rw(r)]), "mF380": r.choice([0, rw(r)]), "mF320": r.choice([0, rw(r)])})


def ftp_case(r, op, loggedin, gsys, idv=0):
    args = {"cb": rw(r), "ctx": rw(r)}
    if op == "ftp":
        args["park"] = 1
    else:
        args["id"] = idv
    return mk(r, op, args=args, words={"mLoggedIn": loggedin}, gsys=gsys)


def ftp_edges(op):
    def gen():
        r = random.Random(15 if op == "ftp" else 16)
        for loggedin in (0, 1, -1):
            for gsys in (0, 1, 2):
                yield ftp_case(r, op, loggedin, gsys, r.randrange(-5, 1000))
    return gen


def ftp_random(op):
    def rnd(r):
        return ftp_case(r, op, r.choice([0, 1, rw(r)]), r.choice([0, 1, 2]), rw(r))
    return rnd


def goto_edges():
    r = random.Random(17)
    for result in (0, 1, -1):
        for park in (0, 1):
            yield mk(r, "goto", args={"result": result, "ctx": rw(r), "park": park})


def goto_random(r):
    return mk(r, "goto", args={"result": r.choice([0, 0, 1, -1, rw(r)]), "ctx": rw(r), "park": r.choice([0, 1])})


def are_case(r, which=None, all_set=False):
    c = mk(r, "are", args={}, words={"mUploadActive": 0, "mFTPActive": 0})
    keys = ["login", "news"] + list(range(9)) + ["up", "ftp"]
    chosen = keys if all_set else ([] if which is None else [which])
    for k in chosen:
        v = rw(r) or 1
        if k in ("login", "news"):
            c[k] = 1
        elif k == "up":
            c["words"]["mUploadActive"] = v
        elif k == "ftp":
            c["words"]["mFTPActive"] = v
        else:
            c["lists"][k] = v
    return c


def are_edges():
    r = random.Random(18)
    yield are_case(r)
    yield are_case(r, all_set=True)
    for k in ["login", "news"] + list(range(9)) + ["up", "ftp"]:
        for _ in range(2):
            yield are_case(r, k)


def are_random(r):
    c = are_case(r)
    for k in ["login", "news"] + list(range(9)) + ["up", "ftp"]:
        if r.random() < 0.08:
            v = rw(r) or 1
            if k in ("login", "news"):
                c[k] = 1
            elif k == "up":
                c["words"]["mUploadActive"] = v
            elif k == "ftp":
                c["words"]["mFTPActive"] = v
            else:
                c["lists"][k] = v
    return c


TESTS = [
    FnTest("CFSOnlineSystem::EnterChatSatellite", ppc, host, edge=enter_edges, rnd=enter_random),
    FnTest("CFSOnlineSystem::SetPublishing", ppc, host, edge=setpub_edges, rnd=setpub_random),
    FnTest("CFSOnlineSystem::RequestUploadPostLoginCallback", ppc, host, edge=upcb_edges, rnd=upcb_random),
    FnTest("CFSOnlineSystem::CompleteUpload", ppc, host, edge=complete_edges, rnd=complete_random),
    FnTest("CFSOnlineSystem::RequestFTPDownload", ppc, host, edge=ftp_edges("ftp"), rnd=ftp_random("ftp")),
    FnTest("CFSOnlineSystem::GotoPlayerRequestFTPDownloadByID", ppc, host, edge=ftp_edges("ftpid"), rnd=ftp_random("ftpid")),
    FnTest("CFSOnlineSystem::GotoPlayerFTPPostGetParkInfoCallback", ppc, host, edge=goto_edges, rnd=goto_random),
    FnTest("CFSOnlineSystem::AreWeProcessing", ppc, host, edge=are_edges, rnd=are_random),
]
