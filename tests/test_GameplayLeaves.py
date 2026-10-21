"""Unit v5L07, leaves a park run reaches, against libtpw_unit_GameplayLeaves.dylib (tests/unit_GameplayLeaves.cpp), main app
("SimTheme Park"), src/game/Object.cpp, src/game/advisor/Advisor.cpp, src/game/park/SkirtDeform.cpp, src/game/ui/ui_staffPanel.cpp:
  CellIsRide 0x1006d718, LocatorByeByeKid 0x101bf98c, CObject::IsCoaster 0x10110cd8,
  CAdvisor::FinalDestroy 0x1000a0ec

A case is the staff panels' thing (gStaffPanelThing 0x10395a3c), written on both sides, and one call with its argument. The answer
and the call log are compared; RemoveSlideoutPanel (0x101a83d8) is a fake that records its argument. The thing pointers are compared,
never followed, so they are plain numbers on both sides. GetFirstPersonRideID 0x10032b64, a leaf the same runs reach, landed on main in
src/game/rides/BumperRide.cpp, src/game/TPCamera.cpp, src/game/advisor/Advisor.cpp, src/game/rides/TPCoaster.cpp, src/game/rides/CoasterTrain.cpp, src/game/rides/CoasterQueue.cpp before this unit.

IsCoaster's case is the object's type halfword (+0xe) and the track type word of the record (+0x9c on the Mac, slot WhichTrackType on
the host) that the fake CRideDatabase::GetRideInfo (0x10154868) answers for any type; the fake records the database and the type.
CAdvisor::FinalDestroy's fake CThing::FinalDestroy (0x1012e9f8) records the object. Not tested: a NULL record (the Mac reads address
0x9c).
"""
import ctypes as C
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_GameplayLeaves"
MASK = 0xFFFFFFFF
CELL_IS_RIDE, BYE_BYE = 0x1006D718, 0x101BF98C
REMOVE_SLIDEOUT = 0x101A83D8
IS_COASTER, ADVISOR_FINAL_DESTROY = 0x10110CD8, 0x1000A0EC
GET_RIDE_INFO, THING_FINAL_DESTROY = 0x10154868, 0x1012E9F8
RIDE_DATABASE = 0x1037F6C8
STAFF_THING = 0x10395A3C


def setup(runner):
    T.install_std_stubs(runner.emu)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_gl_set.restype, lib.tpw_gl_set.argtypes = None, [C.c_uint64]
    lib.tpw_gl_cell_is_ride.restype, lib.tpw_gl_cell_is_ride.argtypes = C.c_int32, [C.c_int32]
    lib.tpw_gl_bye_bye.restype, lib.tpw_gl_bye_bye.argtypes = None, [C.c_uint64]
    lib.tpw_gl_database.restype, lib.tpw_gl_database.argtypes = C.c_void_p, []
    lib.tpw_gl_advisor.restype, lib.tpw_gl_advisor.argtypes = C.c_void_p, []
    lib.tpw_gl_is_coaster.restype, lib.tpw_gl_is_coaster.argtypes = C.c_int32, [C.c_uint32, C.c_int32]
    lib.tpw_gl_advisor_final_destroy.restype, lib.tpw_gl_advisor_final_destroy.argtypes = None, []
    lib.bound = True


def ppc(e, case):
    advisor = e.alloc(0x100)
    e.cl = CL.PpcLog({RIDE_DATABASE: 1, advisor: 2})
    st = CL.Stubs()
    st.real = {CELL_IS_RIDE, BYE_BYE, IS_COASTER, ADVISOR_FINAL_DESTROY}
    st.internal(REMOVE_SLIDEOUT, CL.recorder("RemoveSlideoutPanel", "h"))
    record = e.alloc(0x100)
    e.write(record + 0x9C, struct.pack(">i", T.s32(case.get("track", 0))))
    st.internal(GET_RIDE_INFO, CL.recorder("GetRideInfo", "Lw", ret=lambda e_, log: record))
    st.internal(THING_FINAL_DESTROY, CL.recorder("CThing::FinalDestroy", "L"))
    saved = e.u32(STAFF_THING)
    e.write(STAFF_THING, struct.pack(">I", case["thing"] & MASK))
    old = st.activate(e)
    try:
        if case["fn"] == 0:
            answer = T.call_ppc(e, CELL_IS_RIDE, [T.u32(case["arg"])])[0] & MASK
        elif case["fn"] == 2:
            T.call_ppc(e, BYE_BYE, [T.u32(case["arg"])])
            answer = 0
        elif case["fn"] == 3:
            obj = e.alloc(0x100)
            e.write(obj + 0xE, struct.pack(">H", case["arg"] & 0xFFFF))
            answer = T.call_ppc(e, IS_COASTER, [obj])[0] & MASK
        else:
            T.call_ppc(e, ADVISOR_FINAL_DESTROY, [advisor])
            answer = 0
    finally:
        CL.Stubs.deactivate(e, old)
        e.write(STAFF_THING, struct.pack(">I", saved))
    return {"answer": answer, "log": e.cl.log}


def host(lib, case):
    bind(lib)
    CL.host_begin(lib, {1: lib.tpw_gl_database(), 2: lib.tpw_gl_advisor()})
    try:
        lib.tpw_gl_set(case["thing"] & MASK)
        if case["fn"] == 0:
            answer = lib.tpw_gl_cell_is_ride(T.s32(case["arg"])) & MASK
        elif case["fn"] == 2:
            lib.tpw_gl_bye_bye(case["arg"] & MASK)
            answer = 0
        elif case["fn"] == 3:
            answer = lib.tpw_gl_is_coaster(case["arg"] & 0xFFFF, T.s32(case.get("track", 0))) & MASK
        else:
            lib.tpw_gl_advisor_final_destroy()
            answer = 0
    finally:
        log = CL.host_end(lib)
    return {"answer": answer, "log": log}


THINGS = [0, 0x10, 0x2000_0000, 0x7FFF_FFF0, 0x8000_0000, 0xFFFF_FFF0]
TYPES = [0, 1, 0x7F, 0x80, 0xFF, 0x100, 0x7FFF, 0x8000, 0xFFFF]
TRACKS = [0, 1, 2, 3, 4, 5, -1, -3, 0x103, 0x80000003, 0x7FFFFFFF]


def edges():
    for mode in list(range(0, 0x22)) + [0x100, 0x7FFFFFF0, 0x7FFFFFFF, 0x80000000, 0xFFFFFFF0, 0xFFFFFFFF]:
        yield {"fn": 0, "arg": mode, "thing": 0}
    for panel in THINGS:
        for thing in THINGS:
            yield {"fn": 2, "arg": thing, "thing": panel}
    for kind in TYPES:
        for track in TRACKS:
            yield {"fn": 3, "arg": kind, "track": track, "thing": 0}
    yield {"fn": 4, "arg": 0, "thing": 0}


def rnd(r):
    thing = r.choice(THINGS + [r.getrandbits(32)])
    fn = r.choice([0, 2, 3, 4])
    arg = r.getrandbits(32) if fn == 0 else (r.choice([thing, r.choice(THINGS), r.getrandbits(32)]) if fn == 2 else 0)
    if fn == 3:
        arg = r.choice(TYPES + [r.getrandbits(16)])
    return {"fn": fn, "arg": arg, "track": r.choice(TRACKS + [r.getrandbits(32), r.randrange(8)]), "thing": thing}


TESTS = [
    FnTest("CellIsRide, LocatorByeByeKid, CObject::IsCoaster, CAdvisor::FinalDestroy", ppc, host, edge=edges, rnd=rnd),
]
