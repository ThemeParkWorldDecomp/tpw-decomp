"""CFSUserInfo::GameLogout (main app 0x1017f61c, src/FS_UserInfo_CFSUserInfo_GameLogout.cpp), against
libtpw_unit_CFSUserInfo_GameLogout.dylib (tests/unit_CFSUserInfo_GameLogout.cpp).

Every callee (the slot and save paths, the string class, UniStringToString, the two saves, the master save destructor, SaveGlobal) is a
recording fake on both sides. The world: a user info block with a random logged-in slot (mGameUser), a master save present or not.
Compared: the call log (with the texts the saves receive), mGameUser, and whether gMasterSave is NULL after the call.
No fused operations. Not tested: nothing skipped.
"""
import ctypes as C
import random

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CFSUserInfo_GameLogout"
MASK = 0xFFFFFFFF

FN = 0x1017F61C
OWN = {FN: 424}

G_TP = 0x103B74C8
G_SETTINGS = 0x1038071C
G_MASTER = 0x10380A8C

W_CTOR, W_COPY, W_DTOR, W_ASSIGN, W_APPEND = 0x10238740, 0x102387A0, 0x10238710, 0x10239850, 0x10238788
N_CTOR, N_DTOR = 0x10238458, 0x102382F0
RET_SLOT, RET_SAVE, SAVE_MASTER = 0x1017EBF0, 0x1017E838, 0x1017FA50
UNI, AB_SAVE, OB_SAVE, MASTER_DTOR, SAVE_GLOBAL = 0x10005670, 0x102067CC, 0x1020461C, 0x1016997C, 0x101671EC

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def uni(e):
    L = e.cl
    L.rec("UniToStr", (L.lab(e.r(3)), L.lab(e.r(4))))
    n = getattr(L, "uni_calls", 0)
    L.uni_calls = n + 1
    text = [b"slotpath\0", b"settings\0", b"extra\0"][min(n, 2)]
    e.write(e.r(3) + 8, (e.alloc(16, text)).to_bytes(4, "big"))
    e.set_r(3, 1)


def glue(fn):
    """The glue saves the caller's TOC at 0x14(r1); the caller reloads it after the call."""
    def run_glue(e):
        e.write(e.r(1) + 0x14, e.r(2).to_bytes(4, "big"))
        fn(e)
    return run_glue


STUBS.internal(W_CTOR, glue(CL.recorder("WCtor", "N")))
STUBS.internal(W_COPY, glue(CL.recorder("WCopy", "NL")))
STUBS.internal(W_DTOR, glue(CL.recorder("WDtor", "L")))
STUBS.internal(W_ASSIGN, glue(CL.recorder("WAssign", "LL")))
STUBS.internal(W_APPEND, glue(CL.recorder("WAppend", "LL")))
STUBS.internal(N_CTOR, glue(CL.recorder("NCtor", "N")))
STUBS.internal(N_DTOR, glue(CL.recorder("NDtor", "L")))
STUBS.internal(RET_SLOT, CL.recorder("RetSlotPath", "NLi"))
STUBS.internal(RET_SAVE, CL.recorder("RetSavePath", "NL"))
STUBS.internal(SAVE_MASTER, CL.recorder("SaveMaster", "L"))
STUBS.internal(UNI, uni)
STUBS.internal(AB_SAVE, CL.recorder("AddressBookSave", "Ls"))
STUBS.internal(OB_SAVE, CL.recorder("OutboxSave", "L"))
STUBS.internal(MASTER_DTOR, CL.recorder("MasterDtor", "D"))
STUBS.internal(SAVE_GLOBAL, CL.recorder("SaveGlobal", "LT"))


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(int(w & MASK).to_bytes(4, self.order) for w in ws))

    def rd_word(self, a):
        return int.from_bytes(self.read(a, 4), self.order)


class Ppc(Side):
    order = "big"
    ug = 0x60

    def __init__(self, e, case):
        self.e = e
        self.obj = e.alloc(0x200)
        self.master = e.alloc(64) if case["master"] else 0
        e.write(G_MASTER, self.master.to_bytes(4, "big"))
        fixed = {self.obj: 1, self.obj + 0xC4: 2, self.obj + 0x64: 3, G_TP + 0x44C0: 5, G_TP + 0x454C: 6, G_SETTINGS: 7}
        if self.master:
            fixed[self.master] = 4
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def master_set(self):
        return int(self.e.u32(G_MASTER) != 0)

    def call(self):
        T.call_ppc(self.e, FN, [self.obj])


class Host(Side):
    order = "little"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.master = lib.tpw_gl_setup(1 if case["master"] else 0)
        self.obj = lib.tpw_gl_alloc(0x1000)
        lay = lib.layout
        self.ug = lay[0]
        a = lambda n: lib.tpw_gl_addr(n)
        fixed = {1: self.obj, 2: self.obj + lay[2], 3: self.obj + lay[1], 5: a(b"sep"), 6: a(b"cfg"), 7: a(b"settings")}
        if self.master:
            fixed[4] = self.master
        CL.host_begin(lib, fixed, case["queues"])

    def done(self):
        pass

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def master_set(self):
        return self.lib.tpw_gl_master_set()

    def call(self):
        self.lib.tpw_gl_logout(self.obj)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_gl_addr.restype, lib.tpw_gl_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_gl_alloc.restype, lib.tpw_gl_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_gl_setup.restype, lib.tpw_gl_setup.argtypes = C.c_uint64, [C.c_int32]
    lib.tpw_gl_master_set.restype = C.c_int32
    lib.tpw_gl_logout.argtypes = [C.c_uint64]
    lib.tpw_gl_layout.argtypes = [C.POINTER(C.c_int64)]
    out = (C.c_int64 * 32)()
    n = lib.tpw_gl_layout(out)
    lib.layout = list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        s.words(s.obj + s.ug, [case["user"]])
        s.call()
        return {"log": s.log(), "user": s.rd_word(s.obj + s.ug), "master": s.master_set()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def mk(user, master):
    return {"user": user & MASK, "master": master, "queues": {}}


def edges():
    for u in (0, 1, 2, 3, -1, 4, 0x7FFFFFFF):
        for m in (0, 1):
            yield mk(u, m)


def rnd(r):
    return mk(r.choice([0, 1, 2, 3, -1, r.randrange(-100, 100)]), r.choice([0, 1]))


TESTS = [FnTest("CFSUserInfo::GameLogout", ppc, host, edge=edges, rnd=rnd)]
