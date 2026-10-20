"""The theme index and the original reference-returning string accessor."""
import ctypes as C
import struct
import calllog as CL
import tpwtest as T
from tpwtest import FnTest
SHIM_LIBRARY = "unit_Game"
def ppc(e, theme):
    game = e.alloc(0x40, b"\xa5" * 0x40)
    e.write(game + 0x14, struct.pack(">I", theme & 0xffffffff))
    st = CL.Stubs(); st.real.update([0x10149b70, 0x1014a058]); saved = st.activate(e)
    try:
        value = T.call_ppc(e, 0x10149b70, [game])[0]
        name = T.call_ppc(e, 0x1014a058, [game])[0]
        return T.s32(value), int(name == game)
    finally: CL.Stubs.deactivate(e, saved)
def host(lib, theme):
    fn = lib.tpw_game_getters; fn.argtypes = [C.c_int32, C.POINTER(C.c_int32)]; fn.restype = None
    out = (C.c_int32 * 2)(); fn(theme, out); return tuple(out)
TESTS = [FnTest("CGame theme getters", ppc, host, edge=lambda: iter([-2147483648, -1, 0, 1, 2, 3, 2147483647]), rnd=lambda r: r.randrange(-2147483648, 2147483648))]
