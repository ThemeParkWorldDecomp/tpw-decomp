"""Unit v5935: CFSOnlineSystem::ConnectUploadServer and CFSOnlineSystem::LoggedInRequestFTPDownload, against
libtpw_unit_CFSOnlineSystem_ConnectUploadServer.dylib (tests/unit_CFSOnlineSystem_ConnectUploadServer.cpp), main app
("SimTheme Park"), src/game/online/FS_Online.cpp:
  CFSOnlineSystem::ConnectUploadServer 0x10175808, CFSOnlineSystem::LoggedInRequestFTPDownload 0x10176418

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian) for
the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. Every callee outside the unit
is a fake on both sides that records its arguments (a string by a hash of its code units, an object by its label) and answers from the case's
queues: the string library (working fakes on both sides), the online library's Upload / City / FTPClient, the server and ride lists, the file
tests, the list of the file names (RemoveHead, AddTail), the callback of the park request.

The world of a case: the online system (login data, server and ride lists, upload / city / FTP pointers, the callback words, the list of file
names), the park argument and the system's park (ride bit table, theme, child lock flags, name), the server records, the ride, the global
strings (local path, data folder, separators), the city configuration, gUserInfo, and the answers the fakes will give. What is compared: the
answer, every field the functions write, the list of file names, gUserInfo, and the event log.

No fused operations. Not tested: a NULL mServerList / mRideList / mFTPPark (the original reads through them unconditionally).
"""
import ctypes as C
import random
import struct

from unicorn import UC_HOOK_CODE
from unicorn.ppc_const import UC_PPC_REG_1

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CFSOnlineSystem_ConnectUploadServer"
MASK = 0xFFFFFFFF

FN_CONNECT, FN_REQUEST = 0x10175808, 0x10176418
SIZES = {FN_CONNECT: 328, FN_REQUEST: 2084}
OWN = set(SIZES)

ADDR = dict(
    gTPFileInfo=0x102B3458,
    gLocalPath=0x102B3458,
    gDataFolder=0x103B74C8 + 0x4830,
    gWideSeparator=0x103B74C8 + 0x44C0,
    gF4484=0x103B74C8 + 0x4484,
    gUserInfo=0x10380B34,
    gOnlineConfig=0x102B2B70,
)
MAC = {
    "sys.mLoginPassword": 0x20, "sys.mF34": 0x34, "sys.mF38": 0x38, "sys.mLoginEmail": 0x3C, "sys.mF58": 0x58,
    "sys.mCity": 0xD4, "sys.mCityClock": 0xD8, "sys.mCityListsWanted": 0xDC, "sys.mServerList": 0xFC,
    "sys.mRideList": 0x100, "sys.mUpload": 0x104, "sys.mFTPActive": 0x38C, "sys.mF398": 0x398, "sys.mF39C": 0x39C,
    "sys.mFTP": 0x3A0, "sys.mFTPPark": 0x3A4, "sys.mF3C0": 0x3C0, "sys.mF3C4": 0x3C4, "sys.mF3D8": 0x3D8,
    "sys.mF3DC": 0x3DC, "sys.mFTPParkPending": 0x3F0,
    "park.mParkID": 8, "park.mF4C": 0x4C, "park.mF58": 0x58, "park.mF78": 0x78, "park.mText7C": 0x7C,
    "server.mText24": 0x24, "server.mF3C": 0x3C, "ride.mRideID": 8,
}
LIST = 0x3C8       # the Mac's list header inside the system (head +4, tail +8, count +0xC)

# (name, host kind for tpw_cu_make, Mac size)
WORLD = [("sys", 0, 0x500), ("park", 1, 0x100), ("ftppark", 1, 0x100), ("upsrv", 2, 0x80), ("ftpsrv", 2, 0x80), ("ride", 3, 0x40),
         ("slist", 4, 0x40), ("rlist", 4, 0x40), ("city0", 4, 0x40), ("upload0", 4, 0x40), ("uinfo0", 4, 0x40)]

INTS = ["sys.mF34", "sys.mF38", "sys.mCityClock", "sys.mCityListsWanted", "sys.mF398", "sys.mFTPActive", "sys.mF3D8",
        "sys.mFTPParkPending", "sys.mF3C0", "sys.mF3C4"]
PTRS = ["sys.mUpload", "sys.mCity", "sys.mFTP", "sys.mFTPPark", "sys.mServerList", "sys.mRideList"]


def s32(v):
    return T.s32(v)


def sx16(v):
    v &= 0xFFFF
    return v - 0x10000 if v & 0x8000 else v


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)
_internal = STUBS.internal


def _internal_with_toc(addr, fn):
    """The glue of an imported function saves the TOC pointer at 0x14(r1) and the caller reloads it: do the same here."""
    def wrapped(e):
        e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2) & MASK))
        fn(e)
    _internal(addr, wrapped)


STUBS.internal = _internal_with_toc


def ru(e, a):
    return struct.unpack(">I", e.read(a, 4))[0]


def wu(e, a, v):
    e.write(a, struct.pack(">I", v & MASK))


def H(units):
    h = len(units)
    for c in units:
        h = (h * 131 + c + 1) & 0x7FFFFFFF
    return h


def get_units(e, a, w):
    n = ru(e, a + 4)
    if n == 0 or n > 0x100000:
        return []
    raw = e.read(ru(e, a + 8), n * w)
    return list(raw) if w == 1 else list(struct.unpack(">%dH" % n, raw))


def pack_units(units, w):
    return bytes(u & 0xFF for u in units) if w == 1 else struct.pack(">%dH" % len(units), *units)


def put_units(e, a, w, units):
    buf = e.alloc((len(units) + 1) * w)
    e.write(buf, pack_units(units, w) + bytes(w))
    wu(e, a + 4, len(units))
    wu(e, a + 8, buf)
    wu(e, a + 0xC, len(units))


def init_obj(e, a, w):
    wu(e, a, e.vt[w - 1])
    wu(e, a + 0x10, 0)


def cstr(e, p, w):
    out = []
    while True:
        raw = e.read(p + len(out) * w, w)
        u = raw[0] if w == 1 else struct.unpack(">H", raw)[0]
        if u == 0:
            return out
        out.append(u)


def reg(addr):
    def deco(fn):
        STUBS.internal(addr, fn)
        return fn
    return deco


def mk_ctor(w, kind):
    def fn(e):
        this, arg = e.r(3), e.r(4)
        units = [] if kind == 0 else cstr(e, arg, w) if kind == 1 else get_units(e, arg, w)
        init_obj(e, this, w)
        put_units(e, this, w, units)
        e.cl.rec("StrCtor", (w, kind, H(units)))
    return fn


for _a, _w, _k in [(0x10238458, 1, 0), (0x10238740, 2, 0), (0x102382D8, 1, 1), (0x10238800, 2, 1), (0x10238308, 1, 2),
                   (0x102387A0, 2, 2), (0x10238350, 1, 3)]:
    STUBS.internal(_a, mk_ctor(_w, _k))


def do_dtor(e, w):
    e.cl.rec("StrDtor", (w, H(get_units(e, e.r(3), w))))


STUBS.internal(0x102382F0, lambda e: do_dtor(e, 1))
STUBS.internal(0x10238710, lambda e: do_dtor(e, 2))


def do_append(e, w, kind):
    this, arg = e.r(3), e.r(4)
    a = get_units(e, arg, w) if kind == 0 else cstr(e, arg, w)
    new = get_units(e, this, w) + a
    put_units(e, this, w, new)
    e.cl.rec("StrAppend", (w, kind, H(new), H(a)))


def do_assign(e, w):
    this, arg = e.r(3), e.r(4)
    a = cstr(e, arg, w)
    put_units(e, this, w, a)
    e.cl.rec("StrAssign", (w, 1, H(a), H(a)))


STUBS.internal(0x10238380, lambda e: do_append(e, 1, 0))
STUBS.internal(0x10238788, lambda e: do_append(e, 2, 0))
STUBS.internal(0x1023A8A0, lambda e: do_append(e, 2, 1))


@reg(0x1023BAA8)
def stub_glue(e):
    r12 = e.r(12)
    if r12 == 0xFC000001:
        e.cl.rec("Callback", (s32(e.r(3)), e.r(4) & MASK))
        return
    w = 1 if (r12 >> 16) == 0xFA else 2
    k = r12 & 0xFFFF
    if k == 0x14:
        do_dtor(e, w)
    elif k == 0x20:
        do_assign(e, w)
    elif k == 0x28:
        do_append(e, w, 0)
    elif k == 0x30:
        do_append(e, w, 1)
    else:
        raise AssertionError("glue slot 0x%X width %d" % (k, w))


@reg(0x10005670)
def stub_uni_to_string(e):
    out, src = e.r(3), e.r(4)
    u = get_units(e, src, 2)
    res = [c & 0xFF for c in u]
    put_units(e, out, 1, res)
    e.cl.rec("UniStringToString", (H(u), H(res)))


@reg(0x100057A0)
def stub_string_to_uni(e):
    out, src = e.r(3), e.r(4)
    u = get_units(e, src, 1)
    put_units(e, out, 2, u)
    e.cl.rec("StringToUniString", (H(u), H(u)))


@reg(0x10004BC0)
def stub_adddirsep(e):
    a = e.r(3)
    u = get_units(e, a, 1)
    e.cl.rec("FS_AddDirectorySeparator", (H(u),))
    if not u or u[-1] != 0x2F:
        u = u + [0x2F]
        put_units(e, a, 1, u)


@reg(0x10004D4C)
def stub_getlocalpath(e):
    e.cl.rec("GetLocalPath", (e.cl.lab(e.r(3)),))


def left_mid(name, left):
    def fn(e):
        out, src, n = e.r(3), e.r(4), e.r(5)
        u = get_units(e, src, 1)
        res = u[:n] if left else u[n:]
        put_units(e, out, 1, res)
        wu(e, out, 0)
        wu(e, out + 0xC, 0)
        e.cl.rec(name, (n, H(u), H(res)))
    return fn


STUBS.internal(0x10238F50, left_mid("Left", True))
STUBS.internal(0x10238F80, left_mid("Mid", False))


@reg(0x102386E0)
def stub_exists(e):
    e.cl.rec("LbFile_Exists", (H(cstr(e, e.r(3), 1)),))
    e.set_r(3, e.cl.ret("exists") & 0xFF)


@reg(0x102389F8)
def stub_createdir(e):
    e.cl.rec("LbFile_CreateDirectory", (H(cstr(e, e.r(4), 1)),))


@reg(0x1023A8B8)
def stub_debugstr(e):
    p = e.r(3)
    n = e.read(p, 1)[0]
    e.cl.rec("DebugStr", (n, H(list(e.read(p + 1, n)))))


@reg(0x10000E40)
def stub_udebug(e):
    e.cl.rec("UDebug", (H(cstr(e, e.r(3), 1)),))


@reg(0x1015E9D8)
def stub_lang(e):
    e.cl.rec("GetLanguageId", ())
    e.set_r(3, e.cl.ret("lang") & MASK)


@reg(0x10238A88)
def stub_clock(e):
    e.cl.rec("LbTime_GetClock", ())
    e.set_r(3, e.cl.ret("clock") & MASK)


@reg(0x384B8 + 0x10200000)
def stub_new(e):
    e.set_r(3, e.alloc(e.r(3)))


def lab_event(name, addr):
    def fn(e):
        e.cl.rec(name, (e.cl.lab(e.r(3)),))
    STUBS.internal(addr, fn)


lab_event("ReleaseUploadServerConnection", 0x1017598C)
lab_event("ReleaseFTPDownloadServerConnection", 0x101770DC)
lab_event("CityRelease", 0x10238AA0)


@reg(0x10238AB8)
def stub_city_dtor(e):
    lab = e.cl.lab(e.r(3))
    e.cl.rec("CityDtor", (lab,))
    if s32(e.r(4)) > 0:
        e.cl.rec("delete", (lab,))


@reg(0x10176E90)
def stub_complete(e):
    e.cl.rec("CompleteFTPDownload", (e.cl.lab(e.r(3)), s32(e.r(4))))


def new_event(name, addr):
    def fn(e):
        e.cl.newlab(e.r(3))
        e.cl.rec(name, (e.cl.lab(e.r(3)),))
    STUBS.internal(addr, fn)


new_event("Upload", 0x1023A978)
new_event("City", 0x1023A678)
new_event("UserInfoCtor", 0x1017DE54)


@reg(0x1023A9A8)
def stub_upload_connect(e):
    L = e.cl
    L.rec("UploadConnect", (L.lab(e.r(3)), H(cstr(e, e.r(4), 1)), int(e.r(5) != 0), e.r(6) & 0xFFFF, int(e.r(7) == 0x10267468)))
    e.set_r(3, L.ret("connect") & MASK)


@reg(0x1023A9C0)
def stub_upload_session(e):
    L = e.cl
    L.rec("UploadSetSessionInfo", (L.lab(e.r(3)), sx16(e.r(4)), s32(e.r(5)), s32(e.r(6)), H(cstr(e, e.r(7), 2))))


@reg(0x1023A690)
def stub_city_connect(e):
    L = e.cl
    L.rec("CityConnect", (L.lab(e.r(3)), H(cstr(e, e.r(4), 1)), int(e.r(5) != 0), e.r(6) & 0xFFFF, int(e.r(7) == 0x10267480)))
    e.set_r(3, L.ret("cityconnect") & MASK)


@reg(0x1023A6A8)
def stub_city_session(e):
    L = e.cl
    L.rec("CitySetSessionInfo", (L.lab(e.r(3)), e.r(4) & 0xFFFF, s32(e.r(5)), s32(e.r(6)), H(cstr(e, e.r(7), 2))))


@reg(0x1023A6C0)
def stub_markvisited(e):
    L = e.cl
    L.rec("MarkParkVisited", (L.lab(e.r(3)), int(e.r(4) != 0), s32(e.r(5)), s32(e.r(6))))


@reg(0x1017B134)
def stub_return_upload_server(e):
    e.cl.rec("ReturnUploadServer", (e.cl.lab(e.r(3)),))
    e.set_r(3, e.W["upsrv"])


@reg(0x1017B0B4)
def stub_return_ftp_server(e):
    e.cl.rec("ReturnFTPDownloadServer", (e.cl.lab(e.r(3)),))
    e.set_r(3, e.W["ftpsrv"])


@reg(0x1017C2E4)
def stub_park_assign(e):
    L = e.cl
    L.rec("ParkAssign", (L.lab(e.r(3)), L.lab(e.r(4)), s32(ru(e, e.r(4) + 8))))


@reg(0x10178114)
def stub_removehead(e):
    lst = e.r(3)
    cnt = ru(e, lst + 0xC)
    e.cl.rec("RemoveHead", (cnt,))
    head = ru(e, lst + 4)
    if head == 0:
        e.set_r(3, 0)
        return
    val, nxt = ru(e, head), ru(e, head + 4)
    wu(e, lst + 4, nxt)
    if nxt:
        wu(e, nxt + 8, 0)
    else:
        wu(e, lst + 8, 0)
    wu(e, lst + 0xC, cnt - 1)
    e.set_r(3, val)


def py_addtail(e, lst, val):
    node = e.alloc(12)
    tail = ru(e, lst + 8)
    wu(e, node, val)
    wu(e, node + 4, 0)
    wu(e, node + 8, tail)
    wu(e, tail + 4 if tail else lst + 4, node)
    wu(e, lst + 8, node)
    wu(e, lst + 0xC, ru(e, lst + 0xC) + 1)


STUBS.internal(0x10178200, lambda e: py_addtail(e, e.r(3), e.r(4)))


@reg(0x1017BACC)
def stub_return_ride(e):
    e.cl.rec("ReturnRideInfo", (e.cl.lab(e.r(3)), s32(e.r(4)), s32(e.r(5))))
    e.set_r(3, e.W["ride"])


@reg(0x10176C90)
def stub_getfilepath(e):
    out, ride = e.r(3), e.r(4)
    e.cl.rec("GetFilePath", (e.cl.lab(ride),))
    init_obj(e, out, 2)
    put_units(e, out, 2, [ord("R")] + [ord(c) for c in str(ru(e, ride + 8))])


@reg(0x1017F980)
def stub_onlinepath(e):
    out, info = e.r(3), e.r(4)
    e.cl.rec("ReturnOnlinePath", (e.cl.lab(info),))
    init_obj(e, out, 2)
    put_units(e, out, 2, [ord(c) for c in "S:online"])


@reg(0x1020B368)
def stub_ftp_ctor(e):
    L = e.cl
    this, ctl = e.r(3), e.r(4)
    L.newlab(this)
    L.rec("FTPClient", (L.lab(this), int(ctl != 0), ru(e, ctl) if ctl else 0, e.r(5) & 0xFF))


@reg(0x1020B730)
def stub_ftp_connect(e):
    L = e.cl
    L.rec("FTPClientConnect", (L.lab(e.r(3)), H(cstr(e, e.r(4), 1)), H(cstr(e, e.r(5), 1)), H(cstr(e, e.r(6), 1)), int(e.r(7) != 0)))


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

    def plab(self, a):
        p = self.getptr(a)
        return self.labof(p) if p else 0

    def labels(self):
        d = {self.W[n]: i + 1 for i, (n, _, _) in enumerate(WORLD)}
        d[self.g("gTPFileInfo")] = 40
        return d


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        e.vt = tuple(self.mkvt(b) for b in (0xFA0000, 0xFB0000))
        self.W = e.W = {n: e.alloc(size) for n, _, size in WORLD}
        self.L = e.cl = CL.PpcLog(self.labels(), case["queues"])
        self.saved = STUBS.activate(e)

    def mkvt(self, base):
        a = self.e.alloc(0x40)
        self.e.write(a, b"".join(struct.pack(">I", base | k) for k in range(0, 0x40, 4)))
        return a

    def done(self):
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

    def setptr(self, a, v):
        wu(self.e, a, v)

    def getptr(self, a):
        return ru(self.e, a)

    def labof(self, p):
        return self.L.lab(p)

    def setstr(self, a, wide, units):
        w = 2 if wide else 1
        init_obj(self.e, a, w)
        put_units(self.e, a, w, units)

    def strhash(self, a, wide):
        return H(get_units(self.e, a, 2 if wide else 1))

    def setcity(self, text, port):
        rep = self.e.alloc(0x40)
        data = self.e.alloc(len(text) + 1)
        self.e.write(data, text.encode() + b"\0")
        wu(self.e, rep + 0xC, data)
        c = ADDR["gOnlineConfig"]
        wu(self.e, c + 0xC, rep)
        self.e.write(c + 0x1E, struct.pack(">H", port))

    def setworld(self):
        pass

    def cbword(self):
        return 0xFC000001

    def listadd(self, units):
        obj = self.e.alloc(0x14)
        init_obj(self.e, obj, 2)
        put_units(self.e, obj, 2, units)
        py_addtail(self.e, self.W["sys"] + LIST, obj)

    def listdump(self):
        lst = self.W["sys"] + LIST
        out = [ru(self.e, lst + 0xC)]
        node = ru(self.e, lst + 4)
        while node and len(out) < 65:
            out.append(H(get_units(self.e, ru(self.e, node), 2)))
            node = ru(self.e, node + 4)
        return out

    def call(self, op, park):
        if op == "connect":
            r3, _, _ = T.call_ppc(self.e, FN_CONNECT, [self.W["sys"]])
            return r3 & MASK

        def clear_stack(uc, address, size, user):
            # the original reads three character arrays of its frame it never writes: they must be zero
            sp = uc.reg_read(UC_PPC_REG_1)
            uc.mem_write(sp - 0x800, bytes(0x800))

        hook = self.e.uc.hook_add(UC_HOOK_CODE, clear_stack, begin=FN_REQUEST, end=FN_REQUEST)
        try:
            T.call_ppc(self.e, FN_REQUEST, [self.W["sys"], self.W["park"] if park else 0])
        finally:
            self.e.uc.hook_del(hook)
        return 0


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.W = {n: lib.tpw_cu_make(kind) for n, kind, _ in WORLD}
        self.cache = {}
        lib.tpw_cu_known(0, 0)
        for a, n in self.labels().items():
            lib.tpw_cu_known(a, n)
        CL.host_begin(lib, self.labels(), case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_cu_addr(name.encode())

    def off(self, name):
        if name not in self.cache:
            v = self.lib.tpw_cu_off(name.encode())
            assert v >= 0, name
            self.cache[name] = v
        return self.cache[name]

    def alloc(self, n):
        return self.lib.tpw_cu_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def setptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def getptr(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0]

    def labof(self, p):
        return self.lib.tpw_cu_lab(p)

    def setstr(self, a, wide, units):
        arr = (C.c_uint16 * max(1, len(units)))(*units)
        self.lib.tpw_cu_setstr(a, 1 if wide else 0, arr, len(units))

    def strhash(self, a, wide):
        return self.lib.tpw_cu_strhash(a, 1 if wide else 0)

    def setcity(self, text, port):
        self.lib.tpw_cu_setcity(text.encode(), port)

    def setworld(self):
        self.lib.tpw_cu_world(0, self.W["upsrv"])
        self.lib.tpw_cu_world(1, self.W["ftpsrv"])
        self.lib.tpw_cu_world(2, self.W["ride"])

    def cbword(self):
        return self.lib.tpw_cu_callback_word()

    def listadd(self, units):
        arr = (C.c_uint16 * max(1, len(units)))(*units)
        self.lib.tpw_cu_list_add(self.W["sys"], arr, len(units))

    def listdump(self):
        buf = (C.c_int64 * 64)()
        n = self.lib.tpw_cu_list_dump(self.W["sys"], buf, 64)
        return [self.lib.tpw_cu_list_count(self.W["sys"])] + list(buf)[:n]

    def call(self, op, park):
        if op == "connect":
            return self.lib.tpw_cu_connect(self.W["sys"]) & MASK
        self.lib.tpw_cu_request(self.W["sys"], self.W["park"] if park else 0)
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    f = lambda n, res, args: (setattr(getattr(lib, "tpw_cu_" + n), "restype", res), setattr(getattr(lib, "tpw_cu_" + n), "argtypes", args))
    u16p, i64p = C.POINTER(C.c_uint16), C.POINTER(C.c_int64)
    f("addr", C.c_uint64, [C.c_char_p])
    f("alloc", C.c_uint64, [C.c_uint32])
    f("off", C.c_int64, [C.c_char_p])
    f("make", C.c_uint64, [C.c_int32])
    f("world", None, [C.c_int32, C.c_uint64])
    f("setstr", None, [C.c_uint64, C.c_int32, u16p, C.c_int32])
    f("strhash", C.c_int64, [C.c_uint64, C.c_int32])
    f("setcity", None, [C.c_char_p, C.c_int32])
    f("list_add", None, [C.c_uint64, u16p, C.c_int32])
    f("list_dump", C.c_int32, [C.c_uint64, i64p, C.c_int32])
    f("list_count", C.c_int32, [C.c_uint64])
    f("lab", C.c_int64, [C.c_uint64])
    f("known", None, [C.c_uint64, C.c_int64])
    f("callback_word", C.c_uint32, [])
    f("connect", C.c_int32, [C.c_uint64])
    f("request", None, [C.c_uint64, C.c_uint64])
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    W, o = s.W, s.off
    sysa = W["sys"]
    s.setworld()
    s.words(sysa + o("sys.mF34"), [case["f34"]])
    s.words(sysa + o("sys.mF38"), [case["f38"]])
    s.words(sysa + o("sys.mF58"), [case["flags"]])
    s.setptr(sysa + o("sys.mServerList"), W["slist"])
    s.setptr(sysa + o("sys.mRideList"), W["rlist"])
    s.setptr(sysa + o("sys.mFTPPark"), W["ftppark"])
    s.setptr(sysa + o("sys.mUpload"), W["upload0"] if case["upload0"] else 0)
    s.setptr(sysa + o("sys.mCity"), W["city0"] if case["city0"] else 0)
    s.setptr(sysa + o("sys.mFTP"), 0)
    s.setptr(sysa + o("sys.mF39C"), 0)
    s.words(sysa + o("sys.mF3C0"), [s.cbword() if case["cb"] else 0])
    s.words(sysa + o("sys.mF3C4"), [case["cbctx"]])
    for n, v in (("mCityClock", 0x5A5A0001), ("mCityListsWanted", 0x5A5A0002), ("mF398", 0x5A5A0003), ("mFTPActive", 0x5A5A0004),
                 ("mF3D8", 0x5A5A0005), ("mFTPParkPending", 0x5A5A0006)):
        s.words(sysa + o("sys." + n), [v])
    s.setstr(sysa + o("sys.mLoginPassword"), 1, case["pw"])
    s.setstr(sysa + o("sys.mLoginEmail"), 1, case["email"])
    s.setstr(sysa + o("sys.mF3DC"), 0, [])
    for name, key in (("upsrv", "up"), ("ftpsrv", "ftp")):
        s.setstr(W[name] + o("server.mText24"), 1, case[key + "_text"])
        s.half(W[name] + o("server.mF3C"), case[key + "_port"])
    s.words(W["ride"] + o("ride.mRideID"), [case["rideid"]])
    s.words(W["park"] + o("park.mParkID"), [case["argid"]])
    fp = W["ftppark"]
    s.words(fp + o("park.mParkID"), [case["parkid"]])
    s.words(fp + o("park.mF4C"), [case["theme"]])
    s.write(fp + o("park.mF58"), bytes(case["bits"]))
    s.words(fp + o("park.mF78"), [case["f78"]])
    s.setstr(fp + o("park.mText7C"), 0, case["parktext"])
    s.setstr(s.g("gLocalPath"), 0, case["local"])
    s.setstr(s.g("gDataFolder"), 0, case["datafolder"])
    s.setstr(s.g("gWideSeparator"), 1, case["wsep"])
    s.setstr(s.g("gF4484"), 0, case["f4484"])
    s.setptr(s.g("gUserInfo"), W["uinfo0"] if case["uinfo0"] else 0)
    s.setcity(case["cityserver"], case["cityport"])
    for u in case["pre"]:
        s.listadd(u)


def observe(s, case, answer):
    sysa, o = s.W["sys"], s.off
    out = {"answer": answer}
    for n in INTS:
        out[n] = s.rd_word(sysa + o(n))
    out["sys.mF3C0"] = int(out["sys.mF3C0"] != 0)       # the callback word differs by side (a Mac stand-in, a host function)
    for n in PTRS:
        out[n] = s.plab(sysa + o(n))
    out["mF39C"] = int(s.getptr(sysa + o("sys.mF39C")) != 0)
    out["mF3DC"] = s.strhash(sysa + o("sys.mF3DC"), 0)
    out["list"] = s.listdump()
    out["gUserInfo"] = s.plab(s.g("gUserInfo"))
    out["log"] = s.log()
    return out


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, s.call(case["op"], case["park"]))
    finally:
        s.done()


def ppc(e, case):
    if not getattr(ppc, "shown", False):
        ppc.shown = True
        for a, n in ((0x1024D200, 0x40), (0x1024D220, 0x20), (0x1024D230, 0x10), (0x1024D390, 0x10)):
            print("LIT %X %r" % (a, e.read(a, n)))
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators


def rtext(r, w, lo, hi):
    n = r.randint(lo, hi)
    if w == 1:
        return [r.randint(1, 255) if r.random() < 0.1 else r.randint(0x20, 0x7E) for _ in range(n)]
    return [r.randint(0x20, 0x7E) if r.random() < 0.8 else r.randint(1, 0xFFFF) for _ in range(n)]


def word(r):
    return r.choice([0, 1, 0xFFFFFFFF, 0x80000000, r.getrandbits(32), r.randrange(0, 100)])


def base(r, op):
    c = {"op": op, "f34": word(r), "f38": word(r), "flags": r.choice([0, 4, 8, 12, r.getrandbits(32)]),
         "pw": rtext(r, 2, 0, 12), "email": rtext(r, 2, 0, 20),
         "up_text": rtext(r, 2, 0, 20), "up_port": r.choice([0, 80, 21, 0xFFFF, r.getrandbits(16)]),
         "ftp_text": rtext(r, 2, 0, 20), "ftp_port": r.getrandbits(16),
         "upload0": r.random() < 0.3, "city0": r.random() < 0.5, "cb": r.random() < 0.6, "cbctx": word(r),
         "rideid": r.choice([0, 7, 123456, r.getrandbits(32)]), "argid": word(r), "parkid": r.choice([0, 5, r.getrandbits(31)]),
         "theme": r.randrange(0, 6), "f78": r.choice([0, 1, 0x100]), "parktext": rtext(r, 1, 0, 45),
         "local": r.choice([list(b"/Users/x"), list(b"/Users/x/"), [], list(b"C:")]),
         "datafolder": r.choice([list(b"Data"), [], list(b"d/")]), "wsep": r.choice([[0x2F], [0x5C], [], [0x3A, 0x3A]]),
         "f4484": r.choice([list(b".tpf"), []]), "uinfo0": r.random() < 0.5,
         "cityserver": r.choice(["city.example.com", "", "a"]), "cityport": r.choice([0, 80, 0xFFFF, r.getrandbits(16)]),
         "pre": [rtext(r, 2, 0, 8) for _ in range(r.choice([0, 0, 1, 2, 3]))], "park": r.random() < 0.6,
         "bits": [0] * 32, "exists_p": 0.5}
    return c


def finish(r, c):
    c["queues"] = {
        "connect": [r.choice([1, 1, 0, 2, -1])],
        "lang": [r.choice([0, 1, 7, 0x7FFF, 0x8000, 0xFFFF, 0x12345, 0xFFFFFFFF]) for _ in range(2)],
        "clock": [r.getrandbits(32)],
        "cityconnect": [r.choice([1, 1, 0, 2, -1])],
        "exists": [1 if r.random() < c["exists_p"] else 0 for _ in range(600)],
    }
    return c


def connect_case(r, **kw):
    c = base(r, "connect")
    c.update(kw)
    return finish(r, c)


def request_case(r, **kw):
    c = base(r, "request")
    mode = r.choice(["none", "none", "few", "few", "dense"])
    if mode == "few":
        for _ in range(r.randint(1, 3)):
            c["bits"][r.randrange(32)] |= 1 << r.randrange(8)
    elif mode == "dense":
        for i in r.sample(range(32), 3):       # the event log of a case is capped (512 on the PowerPC side), so a few bytes
            c["bits"][i] = r.getrandbits(8)
    c["exists_p"] = r.choice([0.0, 0.3, 0.7, 0.95, 1.0])
    c["flags"] = r.choice([0, 0, 0, 4, 8, 12, 1 << 20])
    c.update(kw)
    return finish(r, c)


def connect_edges():
    r = random.Random(5935)
    for res in (1, 0, 2, -1, 1):
        yield _with_queue(connect_case(r), "connect", [res])
    for lang in (0, 0x7FFF, 0x8000, 0xFFFF, 0x10001, 0xFFFFFFFF):
        yield _with_queue(_with_queue(connect_case(r), "connect", [1]), "lang", [lang, lang])
    for port in (0, 1, 0xFFFF):
        yield connect_case(r, up_port=port)
    yield connect_case(r, up_text=[], pw=[])
    yield connect_case(r, upload0=True)


def _with_queue(c, key, vals):
    c["queues"][key] = vals
    return c


def connect_random(r):
    return connect_case(r)


def request_edges():
    r = random.Random(5936)
    one = [0] * 32
    one[31] = 1
    one2 = [0] * 32
    one2[0] = 0x80
    edge3 = [0xFF if i in (0, 15, 31) else 0 for i in range(32)]
    for kw in (
        dict(flags=4, f78=1), dict(flags=8, f78=0), dict(flags=8, f78=1), dict(flags=4, f78=0), dict(flags=12, f78=1),
        dict(flags=12, f78=0), dict(flags=0, f78=1), dict(flags=0, f78=0, park=True), dict(flags=0, park=False),
        # branch A: nothing missing
        dict(flags=0, exists_p=1.0, cb=True, city0=True), dict(flags=0, exists_p=1.0, cb=False, city0=True),
        dict(flags=0, exists_p=1.0, cb=True, city0=False), dict(flags=0, exists_p=1.0, cb=False, city0=False, uinfo0=False),
        dict(flags=0, exists_p=1.0, bits=one, city0=False), dict(flags=0, exists_p=1.0, bits=one2, city0=True, pre=[[65, 66]]),
        # branch B: the mF3DC file is missing, or a ride file
        dict(flags=0, exists_p=0.0), dict(flags=0, exists_p=0.0, bits=one), dict(flags=0, exists_p=0.0, bits=one2, uinfo0=False),
        dict(flags=0, exists_p=0.0, parktext=[], local=[]), dict(flags=0, exists_p=0.0, parktext=list(b"x" * 19)),
        dict(flags=0, exists_p=0.0, parktext=list(b"y" * 20)), dict(flags=0, exists_p=0.0, parktext=list(b"z" * 18)),
        dict(flags=0, exists_p=0.0, bits=edge3), dict(flags=0, exists_p=0.5, bits=edge3), dict(flags=0, exists_p=1.0, bits=edge3),
        dict(flags=0, exists_p=1.0, pre=[[1], [2, 3], [4, 5, 6]]),
    ):
        c = request_case(r, **kw)
        yield c
        if kw.get("exists_p") == 1.0 and not kw.get("bits"):
            yield _with_queue(request_case(r, **kw), "cityconnect", [0])
            yield _with_queue(request_case(r, **dict(kw, city0=False)), "cityconnect", [0])
            yield _with_queue(request_case(r, **dict(kw, city0=False)), "cityconnect", [1])
            yield _with_queue(request_case(r, **dict(kw, city0=False, cb=True)), "cityconnect", [2])


def request_random(r):
    return request_case(r)


TESTS = [
    FnTest("CFSOnlineSystem::ConnectUploadServer", ppc, host, edge=connect_edges, rnd=connect_random),
    FnTest("CFSOnlineSystem::LoggedInRequestFTPDownload", ppc, host, edge=request_edges, rnd=request_random),
]
