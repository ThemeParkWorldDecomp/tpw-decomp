"""TPGlobals::TPGlobals (main app 0x102328bc): the 159 default strings. The original's
constructor runs with the bullfrog string constructors recorded (where each string is
built, 8- or 16-bit, and its text); the host's object is walked in memory order."""
import ctypes as C
import struct
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_TPGlobals"
CONSTRUCTOR = 0x102328bc


def setup(runner):
    T.install_std_stubs(runner.emu)


def ppc(e, c):
    built = []

    def narrow(e):
        built.append((e.r(3), "c", e.cstr(e.r(4))))

    def wide(e):
        chars, a = [], e.r(4)
        while True:
            value = struct.unpack(">H", e.read(a, 2))[0]
            if value == 0:
                break
            chars.append(chr(value & 0xff))
            a += 2
        built.append((e.r(3), "w", "".join(chars)))

    saved = dict(e.stubs)
    e.stub("__ct__26TbDynamicStringTemplate<c>FPCc", narrow)
    e.stub("__ct__26TbDynamicStringTemplate<w>FPCw", wide)
    try:
        this = e.alloc(0x4c90 + 16, b"\0" * (0x4c90 + 16))
        T.call_ppc(e, CONSTRUCTOR, [this])
    finally:
        e.stubs.clear()
        e.stubs.update(saved)
    # In memory order, with the Mac offsets: 20-byte strings from 0x4024 to 0x4c7c.
    built.sort()
    offsets = [a - this for a, _, _ in built]
    return dict(count=len(built), offsets_ok=offsets == list(range(0x4024, 0x4c7c + 1, 0x14)),
                strings=[kind + " " + text for _, kind, text in built])


def host(lib, c):
    f = lib["tpw_tpglobals_strings"]
    f.restype, f.argtypes = C.c_int32, [C.c_char_p, C.c_uint32]
    out = C.create_string_buffer(16384)
    count = f(out, len(out))
    return dict(count=count, offsets_ok=True, strings=out.value.decode("latin-1").splitlines())


# The constructor takes no input: every case is the same one.
TESTS = [FnTest("TPGlobals::TPGlobals", ppc, host, edge=lambda: [dict()], rnd=lambda r: dict())]
