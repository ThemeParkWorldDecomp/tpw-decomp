"""Unit v5655, the FTP download job of the online system, against libtpw_unit_CFSOnlineSystem_CompleteFTPDownload.dylib
(tests/unit_CFSOnlineSystem_CompleteFTPDownload.cpp), main app ("SimTheme Park"),
src/game/online/FS_Online.cpp, src/game/online/FS_OnlineList.cpp:
  CFSOnlineSystem::CompleteFTPDownload 0x10176e90, FTPDownloadCallbackFn 0x101771a4, RequestNextFileForFTPDownload 0x101772ec,
  Instance 0x10178618

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. Every callee
outside the unit is a fake on both sides that records its arguments: the interface error, the city connection (its constructor, Connect
with the server text, port and callback, SetSessionInfo with the password text, MarkParkVisited, Release, destructor), the clock, the
language, Logout, the release of the download connection, the FTP client (Download with both texts, Disconnect), the server, city and
list lookups, the strings (constructors, copies, appends, destructors, each logging the text it holds; their buffers really hold the
text so that the texts the client gets are the ones the code built), the string conversions, the directory separator, the caller's
callback and the debug texts.

The world of a case: the system object (words at the Mac's offsets), the park, the server record, the city record, the list head item,
the globals (the config's city server and port, the local path, the data folder, the online system pointer). What is compared: the
answer, the words the functions may change, the event log.

No fused operations. Not tested: the Release block of CompleteFTPDownload at 0x10176f14 (reached only with a city connection that the
branch before has just proved null, so it is dead); a RequestNextFileForFTPDownload whose list head is NULL (it reads through it).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CFSOnlineSystem_CompleteFTPDownload"
MASK = 0xFFFFFFFF

FN_COMPLETE, FN_CALLBACK, FN_NEXT, FN_INSTANCE = 0x10176E90, 0x101771A4, 0x101772EC, 0x10178618
SIZES = {FN_COMPLETE: 528, FN_CALLBACK: 264, FN_NEXT: 724, FN_INSTANCE: 100}
OWN = set(SIZES)

# ---- the Mac's addresses of the globals
ADDR = dict(gOnlineSystem=0x10380AB0)
CONFIG = 0x102B2B70            # gOnlineConfig: +0xc the city server's string holder, +0x1e the city port
LOCAL_PATH_DATA = 0x102B3458 + 8       # gTPFileInfo.mLocalPath, its data pointer
DATA_FOLDER_DATA = 0x103B74C8 + 0x4830 + 8     # gTPGlobals.mDataFolder, its data pointer
CITY_PORT = CONFIG + 0x1E
CITY_CB = 0x10267480           # the TVector of FSOnlineCityServerConnectionCallback
CB_CODE = 0x1017178C           # an unused function of the image: the caller's callback lands here (a stub)
DEL_CODE = 0x10171980          # the same for the deleting destructor of the list item

# (Mac offset, host id) of the fields the module sets and reads, per object kind
SYS = dict(f34=(0x34, 0), f38=(0x38, 1), pw=(0x28, 2), city=(0xD4, 3), clock=(0xD8, 4), wanted=(0xDC, 5), clist=(0xE0, 6),
           slist=(0xFC, 7), f384=(0x384, 8), f388=(0x388, 9), done=(0x390, 10), result=(0x394, 11), f398=(0x398, 12),
           ftp=(0x3A0, 13), park=(0x3A4, 14), f3c0=(0x3C0, 15), f3c4=(0x3C4, 16), count=(0x3D4, 17), due=(0x3D8, 18),
           save=(0x3E4, 19), pending=(0x3F0, 20), list=(0x3C8, 21))
PARK = dict(id=(0x08, 0), f48=(0x48, 1), name=(0x84, 2))
SERVER = dict(t=(0x74, 0))
CITYREC = dict(n=(0x4C, 0))
GLOBALS = dict(local=(LOCAL_PATH_DATA, 0), data=(DATA_FOLDER_DATA, 1), port=(CITY_PORT, 2))
TABLES = {0: SYS, 1: PARK, 2: SERVER, 3: CITYREC, 4: GLOBALS}

OBJECTS = ["sys", "park", "server", "cityrec", "slist", "clist", "ftp", "city0"]
LABELS = {"sys": 1, "park": 2, "server": 3, "cityrec": 4, "slist": 6, "clist": 7, "ftp": 8, "city0": 9}
LIST_LABEL = 5

CUR = {}


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def reg(addr, fn):
    """A fake at a glue address: the glue the original goes through saves the TOC in the caller's frame (0x14(r1)), and the caller
    reloads r2 from there after the call."""

    def stub(e):
        e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2)))
        fn(e)

    STUBS.internal(addr, stub)


def fake(addr, name, spec="", ret=None):
    reg(addr, CL.recorder(name, spec, ret=ret))


IN = {}


def inner(name, spec):
    IN[name] = CL.recorder(name, spec)


def rd_text(e, a, wide):
    if a == 0:
        return ""
    if wide:
        data = e.read(a, 600)
        out = []
        for i in range(0, 600, 2):
            ch = (data[i] << 8) | data[i + 1]
            if ch == 0:
                break
            out.append(chr(ch))
        return "".join(out)
    return e.cstr(a)


def obj_text(e, obj, wide):
    return rd_text(e, e.u32(obj + 8), wide)


def init_obj(e, obj, wide):
    e.write(obj, struct.pack(">III", 0, 0, e.alloc(520)))


def put_text(e, obj, text, wide):
    data = e.u32(obj + 8)
    e.write(data, text.encode("utf-16-be") + b"\0\0" if wide else text.encode("latin-1") + b"\0")
    e.write(obj + 4, struct.pack(">I", len(text)))


def log_r4(e, name, value):
    old = e.r(4)
    e.set_r(4, value)
    IN[name](e)
    e.set_r(4, old)


for _n, _s in (("WCtor", ""), ("CCtor", ""), ("WDtor", "xS"), ("CDtor", "xs"), ("CCopy", "xs"), ("WAssign", "xS"),
               ("WAppendChar", "xu"), ("WAppendText", "xS"), ("WAppendStr", "xS"), ("CAppendText", "xs"), ("CAppendStr", "xs"),
               ("UniToStr", "xS"), ("StrToUni", "xs"), ("AddSeparator", "xs")):
    inner(_n, _s)


def f_wctor(e):
    this = e.r(3)
    IN["WCtor"](e)
    init_obj(e, this, True)


def f_cctor(e):
    this = e.r(3)
    IN["CCtor"](e)
    init_obj(e, this, False)


def f_ccopy(e):
    this, src = e.r(3), e.r(4)
    text = obj_text(e, src, False)
    log_r4(e, "CCopy", e.u32(src + 8))
    init_obj(e, this, False)
    put_text(e, this, text, False)


def f_wdtor(e):
    log_r4(e, "WDtor", e.u32(e.r(3) + 8))


def f_cdtor(e):
    log_r4(e, "CDtor", e.u32(e.r(3) + 8))


def f_wassign(e):
    this = e.r(3)
    text = rd_text(e, e.r(4), True)
    IN["WAssign"](e)
    put_text(e, this, text, True)


def f_wappendchar(e):
    this, p = e.r(3), e.r(4)
    ch = struct.unpack(">H", e.read(p, 2))[0]
    log_r4(e, "WAppendChar", ch)
    put_text(e, this, obj_text(e, this, True) + chr(ch), True)


def f_wappendtext(e):
    this = e.r(3)
    add = rd_text(e, e.r(4), True)
    IN["WAppendText"](e)
    put_text(e, this, obj_text(e, this, True) + add, True)


def f_wappendstr(e):
    this, src = e.r(3), e.r(4)
    add = obj_text(e, src, True)
    log_r4(e, "WAppendStr", e.u32(src + 8))
    put_text(e, this, obj_text(e, this, True) + add, True)


def f_cappendtext(e):
    this = e.r(3)
    add = rd_text(e, e.r(4), False)
    IN["CAppendText"](e)
    put_text(e, this, obj_text(e, this, False) + add, False)


def f_cappendstr(e):
    this, src = e.r(3), e.r(4)
    add = obj_text(e, src, False)
    log_r4(e, "CAppendStr", e.u32(src + 8))
    put_text(e, this, obj_text(e, this, False) + add, False)


def f_unitostr(e):
    out, src = e.r(3), e.r(4)
    text = obj_text(e, src, True)
    log_r4(e, "UniToStr", e.u32(src + 8))
    put_text(e, out, text, False)


def f_strtouni(e):
    out, src = e.r(3), e.r(4)
    text = obj_text(e, src, False)
    log_r4(e, "StrToUni", e.u32(src + 8))
    put_text(e, out, text, True)


def f_addsep(e):
    p = e.r(3)
    text = obj_text(e, p, False)
    log_r4(e, "AddSeparator", e.u32(p + 8))
    put_text(e, p, text + "/", False)


for _a, _f in ((0x10238740, f_wctor), (0x10238458, f_cctor), (0x10238308, f_ccopy), (0x10238710, f_wdtor),
               (0x102382F0, f_cdtor), (0x10239328, f_wassign), (0x1023A6D8, f_wappendchar), (0x1023A8A0, f_wappendtext),
               (0x10238788, f_wappendstr), (0x10238398, f_cappendtext), (0x10238380, f_cappendstr), (0x10005670, f_unitostr),
               (0x100057A0, f_strtouni), (0x10004BC0, f_addsep), (DEL_CODE, f_wdtor)):
    reg(_a, _f)

fake(0x10000E40, "UDebug", "s")
fake(0x101F1DF0, "AddUIError", "iL")
fake(0x1023A678, "CityCtor", "N")
fake(0x10238AB8, "CityDtor", "Li")
fake(0x10238AA0, "CityRelease", "L")
fake(0x10238A88, "GetClock", "", ret="i")
fake(0x1015E9D8, "GetLanguageId", "", ret="i")
fake(0x1023A6A8, "CitySetSession", "LHiiS")
fake(0x1023A6C0, "CityMark", "Liii")
fake(0x10171398, "Logout", "L")
fake(0x101770DC, "ReleaseFTP", "L")
fake(0x1016F7D0, "SysCtor", "N")
fake(0x1020C064, "FTPDownload", "Lss")
fake(0x1020BEB8, "FTPDisconnect", "L")
fake(0x1017B0B4, "ReturnFTPDownloadServer", "L", ret=lambda e, log: CUR["server"])
fake(0x10178B3C, "ReturnCityInfo", "Li", ret=lambda e, log: CUR["cityrec"])
fake(0x10178114, "RemoveHead", "L", ret=lambda e, log: CUR["item"])
fake(0x10004D4C, "GetLocalPath", "", ret=lambda e, log: 0x102B3458)
fake(CB_CODE, "Callback", "ii")

_connect = CL.recorder("CityConnect", "LsiHx", ret="i")


def f_connect(e):
    cb = e.r(7)
    _connect(e)
    e.cl.rec("ConnectCb", (1 if cb == CITY_CB else 0,))


reg(0x1023A690, f_connect)


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

    def fa(self, kind, base, key):
        return self.field(kind, base, key)

    def cstr(self, text):
        a = self.alloc(len(text) + 8)
        self.write(a, text.encode("latin-1") + b"\0")
        return a

    def wstr(self, text):
        a = self.alloc(2 * len(text) + 8)
        self.write(a, text.encode(self.wenc) + b"\0\0")
        return a

    def make_objects(self):
        self.o = {n: self.alloc(0x2000 if n in ("sys", "park", "server", "cityrec") else 0x40) for n in OBJECTS}


class Ppc(Side):
    fmt = ">I"
    wenc = "utf-16-be"

    def __init__(self, e, case):
        self.e = e
        self.make_objects()
        fixed = {self.o[n]: LABELS[n] for n in OBJECTS}
        fixed[self.o["sys"] + 0x3C8] = LIST_LABEL
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def field(self, kind, base, key):
        off, _ = TABLES[kind][key]
        return off if kind == 4 else base + off

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def ptr(self, a, v):
        self.words(a, [v])

    def half(self, a, v):
        self.write(a, struct.pack(">H", v & 0xFFFF))

    def log(self):
        return self.L.log

    def cbword(self):
        a = self.alloc(8)
        self.words(a, [CB_CODE, 0])
        return a

    def set_config(self, c):
        holder = self.alloc(0x20)
        self.words(CONFIG + 0xC, [holder])
        self.words(holder + 0xC, [self.cstr(c["cityserver"])])

    def make_item(self, c):
        item, vtbl, tvec = self.alloc(0x20), self.alloc(0x40), self.alloc(8)
        self.words(tvec, [DEL_CODE, 0])
        self.words(vtbl + 0x14, [tvec])
        self.words(item, [vtbl, len(c["item"]), self.wstr(c["item"])])
        CUR["item"] = item
        CUR["server"] = self.o["server"]
        CUR["cityrec"] = self.o["cityrec"]

    def call(self, entry, args):
        return T.call_ppc(self.e, entry, [a & MASK for a in args])[0] & MASK


class Host(Side):
    fmt = "<I"
    wenc = "utf-16-le"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.make_objects()
        self.item = lib.tpw_ftpdl_item_new(case["item"].encode())      # made before the log starts
        fixed = {LABELS[n]: self.o[n] for n in OBJECTS}
        fixed[LIST_LABEL] = self.field(0, self.o["sys"], "list")
        CL.host_begin(lib, fixed, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_ftpdl_addr(name.encode())

    def field(self, kind, base, key):
        return self.lib.tpw_ftpdl_faddr(kind, base, TABLES[kind][key][1])

    def alloc(self, n):
        return self.lib.tpw_ftpdl_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def ptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def half(self, a, v):
        self.write(a, struct.pack("<H", v & 0xFFFF))

    def log(self):
        return CL.host_end(self.lib)

    def cbword(self):
        return self.lib.tpw_ftpdl_cbword()

    def set_config(self, c):
        self.lib.tpw_ftpdl_set_cityserver(c["cityserver"].encode())

    def make_item(self, c):
        self.lib.tpw_ftpdl_world(self.o["server"], self.o["cityrec"], self.item)

    def call(self, entry, args):
        if entry == FN_INSTANCE:
            return self.lib.tpw_ftpdl_instance()
        sysp = args[0]
        if entry == FN_COMPLETE:
            self.lib.tpw_ftpdl_complete(sysp, s32(args[1]))
        elif entry == FN_CALLBACK:
            self.lib.tpw_ftpdl_callback(sysp, s32(args[1]), s32(args[2]))
        elif entry == FN_NEXT:
            self.lib.tpw_ftpdl_next(sysp)
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_ftpdl_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "faddr").restype, getattr(lib, p + "faddr").argtypes = C.c_uint64, [C.c_int32, C.c_uint64, C.c_int32]
    getattr(lib, p + "set_cityserver").argtypes = [C.c_char_p]
    getattr(lib, p + "item_new").restype, getattr(lib, p + "item_new").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "world").argtypes = [C.c_uint64, C.c_uint64, C.c_uint64]
    getattr(lib, p + "cbword").restype = C.c_uint32
    getattr(lib, p + "complete").argtypes = [C.c_uint64, C.c_int32]
    getattr(lib, p + "callback").argtypes = [C.c_uint64, C.c_int32, C.c_int32]
    getattr(lib, p + "next").argtypes = [C.c_uint64]
    getattr(lib, p + "instance").restype = C.c_uint64
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world

OBS = ["f34", "f38", "f384", "f388", "done", "result", "f398", "f3c0", "f3c4", "count", "due", "pending", "clock", "wanted"]


def norm(v, same):
    v &= MASK
    return 0 if v == 0 else (1 if v == same else 2)


def build(s, c):
    o = s.o
    f = lambda key: s.fa(0, o["sys"], key)
    s.words(f("f34"), [c["f34"]])
    s.words(f("f38"), [c["f38"]])
    s.ptr(f("pw"), s.wstr(c["pw"]))
    s.ptr(f("city"), o["city0"] if c["city"] else 0)
    s.words(f("clock"), [c["clock0"]])
    s.words(f("wanted"), [c["wanted0"]])
    s.ptr(f("clist"), o["clist"])
    s.ptr(f("slist"), o["slist"])
    s.words(f("f384"), [c["f384"]])
    s.words(f("f388"), [c["f388"]])
    s.words(f("done"), [c["done"]])
    s.words(f("result"), [c["result"]])
    s.words(f("f398"), [c["f398"]])
    s.ptr(f("ftp"), o["ftp"] if c["ftp"] else 0)
    s.ptr(f("park"), o["park"])
    s.words(f("f3c0"), [s.cbword() if c["cb"] else 0])
    s.words(f("f3c4"), [c["f3c4"]])
    s.words(f("count"), [c["count"]])
    s.words(f("due"), [c["due"]])
    s.ptr(f("save"), s.cstr(c["save"]))
    s.words(f("pending"), [c["pending"]])
    s.words(s.fa(1, o["park"], "id"), [c["parkid"]])
    s.words(s.fa(1, o["park"], "f48"), [c["f48"]])
    s.ptr(s.fa(1, o["park"], "name"), s.cstr(c["parkname"]))
    s.ptr(s.fa(2, o["server"], "t"), s.wstr(c["server"]))
    s.ptr(s.fa(3, o["cityrec"], "n"), s.wstr(c["cityname"]))
    s.ptr(s.fa(4, 0, "local"), s.cstr(c["local"]))
    s.ptr(s.fa(4, 0, "data"), s.cstr(c["data"]))
    s.half(s.fa(4, 0, "port"), c["port"])
    s.set_config(c)
    s.ptr(s.g("gOnlineSystem"), o["city0"] if c["gos"] else 0)
    s.make_item(c)


ENTRY = {"complete": FN_COMPLETE, "callback": FN_CALLBACK, "next": FN_NEXT, "instance": FN_INSTANCE}


def execute(s, c):
    args = [s.o["sys"]] + list(c["args"]) if c["op"] != "instance" else []
    return s.call(ENTRY[c["op"]], args)


def observe(s, c, answer):
    o = s.o
    out = {"log": s.log()}
    for key in OBS:
        out[key] = s.rd_word(s.fa(0, o["sys"], key))
    out["f3c0"] = int(out["f3c0"] != 0)      # the callback word is a TVector on the Mac and a function on the host
    out["city"] = norm(s.rd_word(s.fa(0, o["sys"], "city")), o["city0"] & MASK)
    gos = s.rd_word(s.g("gOnlineSystem"))
    out["gos"] = norm(gos, o["city0"] & MASK)
    if c["op"] == "instance":
        out["answer"] = norm(answer, o["city0"] & MASK)
        out["answer_is_global"] = int((answer & MASK) == gos)
    return out


def run(s, c):
    try:
        build(s, c)
        ans = execute(s, c)
        return observe(s, c, ans)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

NAMES = ["a.dat", "ride1", "", "park7.tpw", "Z"]
PATHS = ["C:data", "/home/x", "", "d"]


def base(r, op):
    return {
        "op": op, "args": [], "f34": r.choice([0, 5, -3, 77]), "f38": r.choice([0, 9, -1]), "f384": r.choice([0, 1, 2]),
        "f388": r.choice([0, 1, 4]), "f398": r.choice([0, 0, 0x1C, -7]), "done": r.choice([0, 1]), "result": r.choice([0, 1, 3]),
        "pending": r.choice([0, 1]), "count": r.choice([0, 1, 3]), "due": r.choice([0, 1, 0x100, -5]), "cb": r.random() < 0.6,
        "f3c4": r.randrange(0, 2 ** 31), "ftp": r.random() < 0.95, "city": r.random() < 0.5, "parkid": r.choice([0, 12, -2, 70000]),
        "f48": r.choice([0, 3, -1, 9]), "pw": r.choice(NAMES), "cityserver": r.choice(["city.example.com", "", "10.0.0.1"]),
        "port": r.choice([0, 80, 0x1234, 0xFFFF]), "save": r.choice(PATHS), "parkname": r.choice(NAMES),
        "server": r.choice(["ftp.example.com", "", "h"]), "cityname": r.choice(NAMES), "item": r.choice(NAMES),
        "local": r.choice(PATHS), "data": r.choice(PATHS), "clock0": r.randrange(0, 2 ** 31), "wanted0": r.choice([0, 1, 5]),
        "gos": r.random() < 0.5,
        "queues": {"CityConnect": [r.choice([1, 1, 0, 2, -1])], "GetClock": [r.randrange(0, 2 ** 31)],
                   "GetLanguageId": [r.choice([0, 1, 3, 0x10005])]},
    }


def complete_case(r, failed=None, f398=None, f388=None, city=None, cb=None, connect=None):
    c = base(r, "complete")
    c["args"] = [r.choice([0, 0, 1, -1, 7]) if failed is None else failed]
    for k, v in (("f398", f398), ("f388", f388), ("city", city), ("cb", cb)):
        if v is not None:
            c[k] = v
    if connect is not None:
        c["queues"]["CityConnect"] = [connect]
    return c


def complete_edges():
    r = random.Random(5655)
    for failed in (0, 1, -1):
        for f398 in (0, 9):
            for f388 in (0, 1):
                for city in (False, True):
                    for cb in (False, True):
                        for connect in (1, 0, 2):
                            yield complete_case(r, failed, f398, f388, city, cb, connect)


def complete_random(r):
    return complete_case(r)


def callback_case(r, state=None, error=None, ftp=None):
    c = base(r, "callback")
    c["args"] = [r.randrange(-3, 8) if state is None else state, r.choice([0, 0, 3, -1, 0x28]) if error is None else error]
    if ftp is not None:
        c["ftp"] = ftp
    return c


def callback_edges():
    r = random.Random(5656)
    for state in range(-3, 8):
        for error in (0, 3, -1):
            for ftp in (True, False):
                yield callback_case(r, state, error, ftp)


def callback_random(r):
    return callback_case(r)


def next_case(r, count=None, due=None):
    c = base(r, "next")
    c["ftp"] = True
    if count is not None:
        c["count"] = count
    if due is not None:
        c["due"] = due
    return c


def next_edges():
    r = random.Random(5657)
    for count in (0, 1, 2):
        for due in (0, 1, 0x01000000, -1):
            for _ in range(2):
                yield next_case(r, count, due)


def next_random(r):
    return next_case(r)


def instance_case(r, gos=None):
    c = base(r, "instance")
    if gos is not None:
        c["gos"] = gos
    return c


def instance_edges():
    r = random.Random(5658)
    for gos in (False, True):
        yield instance_case(r, gos)


def instance_random(r):
    return instance_case(r)


TESTS = [
    FnTest("CFSOnlineSystem::CompleteFTPDownload", ppc, host, edge=complete_edges, rnd=complete_random),
    FnTest("CFSOnlineSystem::FTPDownloadCallbackFn", ppc, host, edge=callback_edges, rnd=callback_random),
    FnTest("CFSOnlineSystem::RequestNextFileForFTPDownload", ppc, host, edge=next_edges, rnd=next_random),
    FnTest("CFSOnlineSystem::Instance", ppc, host, edge=instance_edges, rnd=instance_random),
]
