"""Unit v5549, CreateRestartParkSaveGame 0x10237304, against libtpw_unit_CreateRestartParkSaveGame.dylib
(tests/unit_CreateRestartParkSaveGame.cpp), main app ("SimTheme Park"), src/game/GameSys.cpp.

A case is a world and a call. The callees outside the unit (CDiscFile ctor/dtor/Open/Close, the wide and narrow string ctors, dtors and
operator+=, CFSUserInfo ctor and ReturnUsersPathWithTheme, UniStringToString, CFSLoadSave::SaveHidden) are fakes on both sides that record
their arguments (objects as labels; Open's name as a hash of its text) and answer from the case's queues.

The world of a case: gUserInfo null or set; the Open answer (negative, zero, positive); the text UniStringToString leaves in the name.
Compared: the event log (order, the labels of the three appended globals, the SaveHidden argument, the Open mode) and gUserInfo (null,
the preset object, or a new one). Not compared: the size operator new gets (0xd4 on the Mac, the host's own size); the ctor event proves the
allocation happened.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CreateRestartParkSaveGame"
MASK = 0xFFFFFFFF

FN = 0x10237304
SIZES = {FN: 364}
OWN = set(SIZES)

ADDR = dict(gUserInfo=0x10380b34, sep=0x103b74c8 + 0x44c0, restart=0x103b74c8 + 0x4524, ints=0x103b74c8 + 0x44ac, loadsave=0x10396a7c)
HOST_LAYOUT = []
NAMES = ["", "restart", "data:users:a:land:restart.INTS", "x"]
LABELS = {"sep": 1, "restart": 2, "ints": 3, "loadsave": 4}


def hash_name(s):
    if isinstance(s, str):
        s = s.encode("latin-1")
    h = 0
    for b in s:
        h = (h * 31 + b) & MASK
    return h & 0x7FFFFFFF


STUBS = CL.Stubs()
STUBS.real.update(OWN)

STUBS.imp("__ct__Q33bfl10FileSystem9CDiscFileFv", CL.recorder("DiscCtor", "N"))
STUBS.imp("__dt__Q33bfl10FileSystem9CDiscFileFv", CL.recorder("DiscDtor", "L"))
STUBS.imp("Close__Q33bfl10FileSystem5CFileFv", CL.recorder("Close", "L"))
STUBS.internal(0x1017de54, CL.recorder("CFSUserInfo", "N"))
STUBS.internal(0x1017f858, CL.recorder("Users", "xL"))
STUBS.imp("__ct__26TbDynamicStringTemplate<w>FRC26TbDynamicStringTemplate<w>", CL.recorder("WCopy", "NL"))
STUBS.imp("__dt__26TbDynamicStringTemplate<w>Fv", CL.recorder("WDtor", "L"))
STUBS.imp("__apl__26TbDynamicStringTemplate<w>FRC15TbStringBase<w>", CL.recorder("WAppend", "LL"))
STUBS.imp("__ct__26TbDynamicStringTemplate<c>Fv", CL.recorder("SCtor", "N"))
STUBS.imp("__dt__26TbDynamicStringTemplate<c>Fv", CL.recorder("SDtor", "L"))
STUBS.internal(0x101fc568, CL.recorder("SaveHidden", "LL"))


def stub_uni(e):
    L = e.cl
    L.rec("Uni", (L.lab(e.r(3)), L.lab(e.r(4))))
    text = NAMES[L.ret("Uni")].encode()
    buf = e.alloc(len(text) + 1, text + b"\0")
    e.write(e.r(3) + 4, struct.pack(">I", len(text)))
    e.write(e.r(3) + 8, struct.pack(">I", buf))
    e.set_r(3, 0)


def stub_open(e):
    L = e.cl
    L.rec("Open", (L.lab(e.r(4)), hash_name(e.cstr(e.r(5))), e.r(6) & MASK))
    e.write(e.r(3), struct.pack(">I", L.ret("Open") & MASK))


def stub_new(e):
    """operator new: never answers NULL; the size (0xd4 on the Mac) is not compared, see the docstring."""
    e.set_r(3, e.alloc(max(e.r(3), 4)))


STUBS.imp("__nw__FUl", stub_new)
STUBS.internal(0x10005670, stub_uni)
STUBS.imp("Open__Q33bfl10FileSystem9CDiscFileFPCcUi", stub_open)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.obj = e.alloc(0xd4)
        fixed = {ADDR[k]: v for k, v in LABELS.items()}
        fixed[self.obj] = 5
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def set_ptr(self, a, v):
        self.write(a, struct.pack(">I", v))

    def get_ptr(self, a):
        return struct.unpack(">I", self.read(a, 4))[0]

    def log(self):
        return self.L.log

    def call(self):
        T.call_ppc(self.e, FN, [])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.obj = lib.tpw_crp_alloc(0xd4)
        fixed = {v: self.g(k) for k, v in LABELS.items()}
        fixed[5] = self.obj
        CL.host_begin(lib, fixed, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_crp_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def set_ptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def get_ptr(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0]

    def log(self):
        return CL.host_end(self.lib)

    def call(self):
        self.lib.tpw_crp_call()


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_crp_addr.restype, lib.tpw_crp_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_crp_alloc.restype, lib.tpw_crp_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_crp_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_crp_call.restype, lib.tpw_crp_call.argtypes = None, []
    out = (C.c_int64 * 32)()
    n = lib.tpw_crp_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        g = s.g("gUserInfo")
        s.set_ptr(g, s.obj if case["have"] else 0)
        s.call()
        p = s.get_ptr(g)
        user = 0 if p == 0 else (5 if p == s.obj else 1)
        return {"log": s.log(), "user": user}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def mk(have, name, opened):
    return {"have": have, "queues": {"Uni": [name], "Open": [opened]}}


OPENS = [-1, 0, 1, -0x7FFFFFFF, 0x7FFFFFFF, 12345, -2]


def edges():
    for have in (0, 1):
        for name in range(len(NAMES)):
            for o in OPENS:
                yield mk(have, name, o & MASK if o < 0 else o)


def rnd(r):
    return mk(r.choice([0, 1]), r.randrange(len(NAMES)), r.choice(OPENS) & MASK)


TESTS = [
    FnTest("CreateRestartParkSaveGame", ppc, host, edge=edges, rnd=rnd),
]
