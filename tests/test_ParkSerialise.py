"""Differential test for unit w162, CParkAnalyser::Serialise 0x100e3004 (4,432 bytes), against libtpw_unit_ParkSerialise.dylib
(tests/unit_ParkSerialise.cpp), main app ("SimTheme Park"), src/game/park/analyser.cpp.

A case is an analyser image of 0x21c48 bytes with 0x40 guard bytes on each side, filled with random bytes (the same bytes on both sides), and a call
(save, version, unused). Every callee is a fake on both sides that writes nothing: CThing::Serialise 0x1012e010, CParkAnalyserCell::Serialise 0x100e60a0,
CParkAnalyserVal<int>::Serialise 0x100ef278, the six CHistory instances (0x100f0754, 0x100f0414, 0x100efe8c, 0x100efbbc, 0x100ef644, 0x100f0b80),
LOADVAR / SAVEVAR<int> 0x1000ef18 / 0x1000f4b0 and LOADVARSTRUCT / SAVEVARSTRUCT<TbTimeStamp> 0x100f1468 / 0x100f1b1c. Each logs its kind and arguments
(member calls: the object's offset, the file's label, save, version, unused; VAR calls: the file, the value's offset, noLog, the name text). One call
counter shared by all fakes (restarted per case): the call whose number equals the case's `fail` answers the case's `failValue`, every other call 0.
A full run is 1,091 calls (0 CThing, 1 to 5 the five ints, 6 to 1029 the cells, 1030 to 1090 the rest).

Compared: the answer, the call log and every byte of the image and its guards (the function writes nothing itself). Not tested: the callees' own work.
"""
import ctypes as C

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_ParkSerialise"
MASK = 0xFFFFFFFF
INT_MIN, INT_MAX = -0x80000000, 0x7FFFFFFF
GUARD = 0x40
SIZE = 0x21C48
FILE = 0x1000          # the file argument (a label on both sides)
ADDR = 0x100E3004
HOST_LAYOUT = [0x21C48, 0x20, 0x1F068, 0x1F100, 0x216FC, 0x21C0C, 0x21C44]

# ---- the member callees faked on both sides: address -> event name
MEMBERS = {
    0x1012E010: "CThing::Serialise", 0x100E60A0: "CParkAnalyserCell::Serialise", 0x100EF278: "CParkAnalyserVal<int>::Serialise",
    0x100F0754: "CHistory<int,144>::Serialise", 0x100F0414: "CHistory<float,144>::Serialise", 0x100EFE8C: "CHistory<int,50>::Serialise",
    0x100EFBBC: "CHistory<int,5>::Serialise", 0x100EF644: "CHistory<unsigned char,50>::Serialise", 0x100F0B80: "CHistory<ObjectId,3>::SerialiseStruct",
}
# ---- the VAR callees
VARS = {
    0x1000EF18: "LOADVAR<int>", 0x1000F4B0: "SAVEVAR<int>",
    0x100F1468: "LOADVARSTRUCT<TbTimeStamp>", 0x100F1B1C: "SAVEVARSTRUCT<TbTimeStamp>",
}

# the failing call numbers outside the cell loop, and inside it
OUTSIDE = list(range(0, 6)) + list(range(1030, 1091))
CELLS = [6 + k for k in (0, 1, 2, 511, 1022, 1023)]


def s32(v):
    v &= MASK
    return v - (1 << 32) if v & 0x80000000 else v


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.add(ADDR)
STATE = {"count": 0, "fail": -1, "value": 0}


def answer(e):
    number = STATE["count"]
    STATE["count"] += 1
    e.set_r(3, (STATE["value"] if number == STATE["fail"] else 0) & MASK)


def member_stub(name):
    def stub(e):
        L = e.cl
        L.rec(name, (e.r(3) - L.base, L.lab(e.r(4)), s32(e.r(5)), s32(e.r(6)), e.r(7) & MASK), ())
        answer(e)
    return stub


def var_stub(name):
    def stub(e):
        L = e.cl
        L.rec(name, (L.lab(e.r(3)), e.r(4) - L.base, s32(e.r(6))), (e.cstr(e.r(5))[:159],))
        answer(e)
    return stub


for _addr, _name in MEMBERS.items():
    STUBS.internal(_addr, member_stub(_name))
for _addr, _name in VARS.items():
    STUBS.internal(_addr, var_stub(_name))


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Ppc:
    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({FILE: 1})
        STATE.update(count=0, fail=case["fail"], value=case["fail_value"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def start(self, obj, case):
        self.L.base = obj

    def call(self, obj, case):
        r3, _, _ = T.call_ppc(self.e, ADDR, [obj, FILE, case["save"] & MASK, case["version"] & MASK, case["unused"] & MASK])
        return r3 & MASK

    def log(self):
        return self.L.log


class Host:
    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {1: FILE})

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_pw_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def start(self, obj, case):
        self.lib.tpw_pw_setup(obj, case["fail"], s32(case["fail_value"]))

    def call(self, obj, case):
        return self.lib.tpw_pw_serialise(obj, s32(case["save"]), s32(case["version"]), case["unused"] & MASK) & MASK

    def log(self):
        return CL.host_end(self.lib)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_pw_alloc.restype, lib.tpw_pw_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_pw_setup.restype, lib.tpw_pw_setup.argtypes = None, [C.c_uint64, C.c_int32, C.c_int32]
    lib.tpw_pw_serialise.restype, lib.tpw_pw_serialise.argtypes = C.c_int32, [C.c_uint64, C.c_int32, C.c_int32, C.c_uint32]
    lib.tpw_pw_layout.argtypes = [C.POINTER(C.c_int64)]
    out = (C.c_int64 * 16)()
    n = lib.tpw_pw_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, (list(out)[:n], HOST_LAYOUT)
    lib.bound = True


def run(s, case):
    try:
        block = s.alloc(SIZE + 2 * GUARD)
        s.write(block, case["raw"])
        obj = block + GUARD
        s.start(obj, case)
        result = s.call(obj, case)
        return {"answer": result, "log": s.log(), "image": s.read(block, SIZE + 2 * GUARD)}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators


def make_case(r, save=None, version=None, fail=-1, fail_value=None, unused=None):
    if save is None:
        save = r.choice([0, 0, 1, 1, 2, -1, INT_MIN, T.rint(r)])
    if version is None:
        version = r.choice([0, 0, 1, 1, -1, 2, INT_MIN, INT_MAX, T.rint(r)])
    if fail_value is None:
        fail_value = r.choice([1, -1, 2, 0x80000000])
    if unused is None:
        unused = r.choice([0, MASK, r.getrandbits(32)])
    raw = bytes(r.getrandbits(8) for _ in range(SIZE + 2 * GUARD))
    return {"save": save, "version": version, "unused": unused, "fail": fail, "fail_value": fail_value & MASK, "raw": raw}


def edges():
    import random
    r = random.Random(162)
    # every save and version, no failure and the CThing call failing
    for save in (0, 1, 2, -1, INT_MIN):
        for version in (-1, 0, 1, 2, INT_MIN, INT_MAX):
            for fail in (-1, 0):
                yield make_case(r, save, version, fail)
    # unused 0 and 0xffffffff
    for unused in (0, MASK):
        for save in (0, 1):
            yield make_case(r, save, 1, -1, None, unused)
    # every failing position outside the cell loop, then the cell loop's edges, in both branches
    for save in (0, 1):
        for k, fail in enumerate(OUTSIDE + CELLS):
            yield make_case(r, save, k % 2, fail)
        for fail in OUTSIDE[:6]:
            yield make_case(r, save, 1 - save, fail, 0x80000000)
    # a failure number past the last call changes nothing
    yield make_case(r, 0, 0, 1091)
    yield make_case(r, 1, 1, 1091)


def rnd(r):
    fail = r.choice([-1, -1] + OUTSIDE + CELLS + [6 + r.randrange(1024)])
    return make_case(r, None, None, fail)


TESTS = [FnTest("CParkAnalyser::Serialise", ppc, host, edge=edges, rnd=rnd)]
