"""Proof for candidate 14: which colours CUI2DMap::DrawCoasterTracks draws with once the map's static initialiser has run, against
libtpw_unit_CUI2DMap_CoasterColour.dylib (tests/unit_CUI2DMap_CoasterColour.cpp), main app ("SimTheme Park"), src/game/ui/ui_2dmap.cpp:
  __sinit_ui_2dmap_cpp 0x102319d4, then CUI2DMap::DrawCoasterTracks 0x1022f160 with the real UI2DMReturnScaleColour 0x1022bb44

The Mac has one table at 0x103970f0: the initialiser fills it and the draw reads it. The port has two names for it, so this test runs the three real functions
in sequence on both sides and compares the colour each line is drawn in (the event AAColour of test_CUI2DMap_UpdateSpritePosition's draw cases).
"""
import test_CUI2DMap_UpdateSpritePosition as B
import test_sinit_ui_2dmap_cpp as S
import calllog as CL
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CUI2DMap_CoasterColour"

del B.STUBS.internals[B.SCALE_COLOUR]
B.STUBS.real.add(B.SCALE_COLOUR)


def setup(runner):
    B.setup(runner)


def ppc(e, case):
    s = S.Ppc(e, {"queues": {}})
    s.call()
    s.done()
    return B.ppc(e, case)


def host(lib, case):
    if not getattr(lib, "bound2", False):
        lib.bound = False
        B.bind(lib)
        lib.bound = False
        S.bind(lib)
        lib.bound2 = True
    s = S.Host(lib, {"queues": {}})
    s.call()
    CL.host_end(lib)
    return B.host(lib, case)


TESTS = [FnTest("__sinit_ui_2dmap_cpp, then DrawCoasterTracks", ppc, host, edge=B.draw_edges, rnd=B.draw_random)]
