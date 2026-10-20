"""Unit v5762, CFSOnlineSystemParkList_InWorldByPlayerName::RequestListFromServer, against
libtpw_unit_CFSOnlineSystemParkList_InWorldByPlayerName_Requ.dylib, main app:
  RequestListFromServer 0x101796cc

A case is a player name (UTF-16 text, its pointer stored at +0x3c of the object on the PowerPC side; the host builds its object
around the same pointer), a city block and the unused argument. The callees UDebug and City::GetParkInWorldByName are fakes on both
sides recording their arguments; the second answers from the case's queue. Compared: the answer and the event log.
No fused operations. Not tested: a NULL city (read unconditionally by the callee glue).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CFSOnlineSystemParkList_InWorldByPlayerName_Requ"
MASK = 0xFFFFFFFF

FN = 0x101796CC
UDEBUG = 0x10000E40
GETPARK = 0x102392B0

STUBS = CL.Stubs()
STUBS.real.add(FN)
STUBS.internal(UDEBUG, CL.recorder("UDebug", "s"))
STUBS.imp("GetParkInWorldByName__4CityFPvPwUli", CL.recorder("GetParkInWorldByName", "LiSii", ret="i"))


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def put_name(self, text):
        data = b"".join(struct.pack(self.fmt[0] + "H", ord(ch)) for ch in text + "\0")
        a = self.alloc(len(data))
        self.write(a, data)
        return a


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.city = e.alloc(16)
        self.L = e.cl = CL.PpcLog({self.city: 1}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def alloc(self, n):
        return self.e.alloc(n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, name, unused):
        obj = self.e.alloc(0x48)
        self.write(obj + 0x3C, struct.pack(">I", name))
        r3, _, _ = T.call_ppc(self.e, FN, [obj, self.city, unused & MASK])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.city = lib.tpw_parkreq_alloc(16)
        CL.host_begin(lib, {1: self.city}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_parkreq_alloc(n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, name, unused):
        return self.lib.tpw_parkreq_run(self.city, name, unused & MASK) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_parkreq_alloc.restype, lib.tpw_parkreq_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_parkreq_run.restype, lib.tpw_parkreq_run.argtypes = C.c_uint32, [C.c_uint64, C.c_uint64, C.c_uint32]
    lib.bound = True


def run(s, case):
    try:
        name = s.put_name(case["name"])
        answer = s.call(name, case["unused"])
        return {"answer": answer, "log": s.log()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def make(r, name=None, unused=None, answer=None):
    if name is None:
        name = "".join(chr(r.choice([0x41 + r.randrange(26), 0x61 + r.randrange(26), 0x3A9, 0x4E2D])) for _ in range(r.randrange(0, 12)))
    if unused is None:
        unused = r.choice([0, 1, MASK, 0x80000000, r.getrandbits(32)])
    if answer is None:
        answer = r.choice([0, 1, -1, r.getrandbits(32)])
    return {"name": name, "unused": unused, "queues": {"GetParkInWorldByName": [answer & MASK]}}


def edges():
    r = random.Random(5762)
    for name in ("", "A", "Player One"):
        for unused in (0, 1, MASK, 0x80000000):
            yield make(r, name, unused, r.choice([0, 7, MASK]))


def rnd(r):
    return make(r)


TESTS = [
    FnTest("CFSOnlineSystemParkList_InWorldByPlayerName::RequestListFromServer", ppc, host, edge=edges, rnd=rnd),
]
