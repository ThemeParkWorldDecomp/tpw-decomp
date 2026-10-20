"""Unit v5727, CParser::CParser and CParser::SetupWhitespaceSet, against libtpw_unit_CParser_CParser.dylib (tests/unit_CParser_CParser.cpp),
main app ("SimTheme Park"), src/COnlineParser_ParseGoto_COnlineParser_COnlineParser_cmp_nocase_CParser_CParser.cpp:
  CParser::CParser 0x10211064, CParser::SetupWhitespaceSet 0x10211344

The callees (std::wstring ctor, the two std::set tree ctors, SetupSeparatorSet, AddWhitespace) are fakes on both sides recording which
field of the object they received (0 string, 1 separators, 2 whitespace) or the character. Compared: the event log and the vtable word
(0x102b1b44 on the Mac, the address of the vtable block + 8 on the host). No fused operations. Not tested: nothing.
"""
import ctypes as C

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CParser_CParser"
MASK = 0xFFFFFFFF

FN_CTOR, FN_WS = 0x10211064, 0x10211344
SIZES = {FN_CTOR: 136, FN_WS: 100}
OWN = set(SIZES)
VT = 0x102b1b44

STR_CTOR, TREE_CTOR, SEP, ADDWS = 0x101e0b84, 0x10212a88, 0x102112b0, 0x102117fc

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def field(e, a):
    return {0: 0, 4: 0, 8: 1, 0x14: 2}.get(a - e.cur_obj, -1)


def stub_str(e):
    e.cl.rec("WStringCtor", (field(e, e.r(3)),))


def stub_tree(e):
    e.cl.rec("TreeCtor", (field(e, e.r(3)),))


def stub_sep(e):
    e.cl.rec("SetupSeparatorSet", ())


def stub_add(e):
    e.cl.rec("AddWhitespace", (e.r(4) & 0xFFFF,))


STUBS.internal(STR_CTOR, stub_str)
STUBS.internal(TREE_CTOR, stub_tree)
STUBS.internal(SEP, stub_sep)
STUBS.internal(ADDWS, stub_add)


def setup(runner):
    T.install_std_stubs(runner.emu)


def ppc_run(e, case, fn):
    e.cl = CL.PpcLog({}, {})
    saved = STUBS.activate(e)
    try:
        obj = e.alloc(0x20)
        e.cur_obj = obj + 0  # field(): offsets from the object
        # field() maps (address - cur_obj); the string is at +4 on the Mac
        T.call_ppc(e, fn, [obj])
        return {"log": e.cl.log, "vt": e.u32(obj)}
    finally:
        CL.Stubs.deactivate(e, saved)


def host_run(lib, case, name):
    bind(lib)
    CL.host_begin(lib, {}, {})
    obj = lib.tpw_CParser_alloc(0x80)
    getattr(lib, name)(obj)
    return {"log": CL.host_end(lib), "vt": 1 if lib.tpw_CParser_vtable(obj) == lib.tpw_CParser_addr(b"vt") else 0}


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_CParser_addr.restype, lib.tpw_CParser_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_CParser_alloc.restype, lib.tpw_CParser_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_CParser_vtable.restype, lib.tpw_CParser_vtable.argtypes = C.c_uint64, [C.c_uint64]
    lib.tpw_CParser_ctor.argtypes = [C.c_uint64]
    lib.tpw_CParser_setup_ws.argtypes = [C.c_uint64]
    lib.bound = True


def ppc_ctor(e, case):
    r = ppc_run(e, case, FN_CTOR)
    r["vt"] = 1 if r["vt"] == VT else 0
    return r


def ppc_ws(e, case):
    r = ppc_run(e, case, FN_WS)
    r["vt"] = 0
    return r


def host_ctor(lib, case):
    return host_run(lib, case, "tpw_CParser_ctor")


def host_ws(lib, case):
    r = host_run(lib, case, "tpw_CParser_setup_ws")
    r["vt"] = 0
    return r


def one(r):
    yield {"n": 0}


TESTS = [
    FnTest("CParser::CParser", ppc_ctor, host_ctor, edge=lambda: iter([{"n": 0}]), rnd=lambda r: {"n": 0}),
    FnTest("CParser::SetupWhitespaceSet", ppc_ws, host_ws, edge=lambda: iter([{"n": 0}]), rnd=lambda r: {"n": 0}),
]
