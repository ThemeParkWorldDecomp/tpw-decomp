"""Unit v5L31, the status panels' tables of virtual slots (src/ui_StatusPanel_tables.cpp), against libtpw_unit_UIStatusPanel_tables.dylib.
The Mac's tables are the vtables the panel constructors store at +0x20:
  UIStatusPanel 0x102a8644, RideStatusPanel 0x102ad5fc, ShopStatusPanel 0x102adbd8, ShowStatusPanel 0x102ae980,
  StaffStatusPanel 0x102af264, StaffRoomStatusPanel 0x102aed88, UpgradeStatusPanel 0x102af4dc, FeatureStatusPanel 0x102a9ba0,
  BogStatusPanel 0x102a9384, VisitorStatusPanel 0x102a4b5c

No PowerPC code runs: for every slot (+0x08 to +0x44, and +0x48 for FeatureStatusPanel) the PowerPC side reads the vtable entry and the
code address of the transition vector it names; the host calls the same slot through its table (UIStatusPanel_CallSlot) with recording
fakes for every member and answers the Mac address the fake was given (from analysis/function-inventory.csv). Also compared: the member
got the panel, it got the argument when it takes one (Show at +0x30, ChangeName at +0x38) and nothing otherwise, GetName's answer
(+0x3c), and a second call of the table's maker answering the same table.
"""
import ctypes as C
import struct

from tpwtest import FnTest

SHIM_LIBRARY = "unit_UIStatusPanel_tables"

TABLES = [0x102a8644, 0x102ad5fc, 0x102adbd8, 0x102ae980, 0x102af264, 0x102aed88, 0x102af4dc, 0x102a9ba0, 0x102a9384, 0x102a4b5c]
FEATURE = 7
TAKES_ARG = (0x30, 0x38)
GET_NAME = 0x3c


def offsets(which):
    return range(0x08, 0x4c if which == FEATURE else 0x48, 4)


def setup(runner):
    pass


def ppc(e, case):
    out = []
    for off in offsets(case["which"]):
        tvector = struct.unpack(">I", e.read(TABLES[case["which"]] + off, 4))[0]
        code = struct.unpack(">I", e.read(tvector, 4))[0]
        arg = case["arg"] if off in TAKES_ARG else 0
        out.append((off, code, 1, arg, code if off == GET_NAME else 0, 1))
    return {"slots": out}


def host(lib, case):
    f = lib.tpw_tbl_call
    f.restype, f.argtypes = None, [C.c_uint32, C.c_uint32, C.c_uint32, C.POINTER(C.c_uint32)]
    out = []
    for off in offsets(case["which"]):
        r = (C.c_uint32 * 5)()
        f(case["which"], off, case["arg"], r)
        out.append((off, r[0], r[1], r[2], r[3], r[4]))
    return {"slots": out}


def edges():
    for which in range(len(TABLES)):
        yield {"which": which, "arg": 0x1000}
        yield {"which": which, "arg": 0xFFFFFFF0}


def rnd(r):
    return {"which": r.randrange(len(TABLES)), "arg": r.getrandbits(32) & ~3 or 4}


TESTS = [FnTest("UIStatusPanel tables", ppc, host, edge=edges, rnd=rnd)]
