"""Will_HandleCameraMessages (the camera's share of the game window's messages) and CKeyTable::GetState."""
import ctypes as C
import struct
import calllog as CL
import tpwtest as T
from tpwtest import FnTest
SHIM_LIBRARY = "unit_CameraMessages"
MESSAGES, GET_STATE = 0x100331c4, 0x1014fff0
# The words of a message case: flags, mode, F68, F5BF4, pointer x and y, the two guard bytes and their floats.
WORDS = [(0x102b5a40, 4), (0x102b5aa4, 4), (0x1029cc68, 4), (0x102b5bf4, 4), (0x102b5a94, 4), (0x102b5a98, 4),
         (0x102b5a18, 1), (0x102b5a1c, 4), (0x102b5a10, 1), (0x102b5a14, 4)]
WIDTH, HEIGHT = 0x10395bbc, 0x10395bba
MASK = 0xffffffff
KNOWN = [0x1e, 0x20, 0x22, 0x10001, 0x10002, 0x10003, 0x10004, 0x10005, 0x1000d]
def fb(v): return struct.unpack(">I", struct.pack(">f", v))[0]
def nan(v): return 0x7fc00000 if v & 0x7f800000 == 0x7f800000 and v & 0x007fffff else v
def setup(runner): T.install_std_stubs(runner.emu)
def ppc(e, case):
    saved = {a: e.read(a, n) for a, n in WORDS + [(WIDTH, 2), (HEIGHT, 2)]}
    e.cl = log = CL.PpcLog({}); st = CL.Stubs(); st.real.update([MESSAGES, GET_STATE]); old = st.activate(e)
    try:
        if case[0] == 0:
            _, message, a, b, width, height, state = case
            for (address, n), v in zip(WORDS, state): e.write(address, struct.pack(">B" if n == 1 else ">I", v & (0xff if n == 1 else MASK)))
            e.write(WIDTH, struct.pack(">h", width)); e.write(HEIGHT, struct.pack(">h", height))
            T.call_ppc(e, MESSAGES, [message & MASK, a & MASK, b & MASK])
            out = [e.read(address, 1)[0] if n == 1 else e.u32(address) for address, n in WORDS]
        else:
            _, count, id_, entries = case
            table = e.alloc(0x18); block = e.alloc(max(1, len(entries)) * 0x14, b"\xa5" * (max(1, len(entries)) * 0x14))
            for i, (mid, state) in enumerate(entries):
                e.write(block + i * 0x14, struct.pack(">h", mid)); e.write(block + i * 0x14 + 6, struct.pack(">h", state))
            e.write(table, struct.pack(">Ih", block, count))
            r3, _, _ = T.call_ppc(e, GET_STATE, [table, id_ & MASK])
            out = [r3 & MASK]
        return [nan(v) for v in out], log.log
    finally:
        CL.Stubs.deactivate(e, old)
        for a, b in saved.items(): e.write(a, bytes(b))
def host(lib, case):
    CL.host_begin(lib, {})
    if case[0] == 0:
        _, message, a, b, width, height, state = case
        fn = lib.tpw_cmsg_case; fn.argtypes = [C.c_int32] * 5 + [C.POINTER(C.c_uint32)] * 2; fn.restype = None
        out = (C.c_uint32 * 10)(); fn(message, a, b, width, height, (C.c_uint32 * 10)(*[v & MASK for v in state]), out)
        out = list(out)
    else:
        _, count, id_, entries = case
        fn = lib.tpw_cmsg_state; fn.argtypes = [C.c_int32, C.c_int32, C.c_int32, C.POINTER(C.c_int32)]; fn.restype = C.c_int32
        flat = [v for entry in entries for v in entry] or [0]
        out = [fn(count, id_, len(entries), (C.c_int32 * len(flat))(*flat)) & MASK]
    return [nan(v) for v in out], CL.host_end(lib)
def message(m, a=0, b=0, width=640, height=480, flags=0, mode=0x80, f68=110., f5bf4=0., px=0., py=0., g1=0, scale=0., g2=0, f5a14=1.):
    return 0, m, a, b, width, height, [flags, mode, fb(f68), fb(f5bf4), fb(px), fb(py), g1, fb(scale), g2, fb(f5a14)]
def state(count, id_, entries): return 1, count, id_, entries
def edges():
    positions = [0, 0x04000300, 0x07ff05ff, 0x08000600, 0xffffffff, 0x0001ffff, 0x7fff8000, 0x80000001, 0x00010001]
    sizes = [(640, 480), (800, 600), (1024, 768), (2048, 1536), (1, 1), (-640, -480), (32767, 32767), (-32768, 7)]
    for m in (0x10001, 0x10003):
        for a in positions:
            for width, height in sizes: yield message(m, a, 0x12345678, width, height, flags=0x1ff, px=7., py=-3.)
    for m in (0x10001, 0x10002, 0x22):
        for flags in (0, 0x100, 0xfffffeff, MASK): yield message(m, 0x04000300, flags=flags, mode=0xff)
    for m in (0x10004, 0x10005):
        for a in (0, 1, 2, 3, -1, 0x10000, 4):
            for mode in (0, 7, 0x80, MASK): yield message(m, a, mode=mode)
    for a in (-1, 0, 1, -2147483648, 120, -120):
        for f68 in (110., 0., -5., 1e30, float("inf")): yield message(0x1000d, a, f68=f68)
    for mode in (0, 2, 0x80, 0x82, MASK):
        for px in (0., 320., 640., 1000., -50., 3.5, float("nan")):
            for width in (640, 1024, 0, -640): yield message(0x1e, mode=mode, px=px, width=width, f5bf4=0.25)
    for g1, g2 in ((0, 0), (1, 0), (0, 1), (1, 1), (0xff, 0x80)):
        yield message(0x1e, mode=2, px=500., g1=g1, scale=0.125, g2=g2, f5a14=3.)
        yield message(0x10003, 0x02000100, g1=g1, scale=0.125, g2=g2, f5a14=3.)
    for m in (0x20, 0x22, 0, 0x1f, 0x21, 0x10000, 0x10006, 0x1000c, 0x1000e, -1):
        for mode in (0, 0x78, 0x7f, 0x80, MASK): yield message(m, 0x04000300, mode=mode, flags=0x55)
    table = [(0, 1), (1, 0), (2, 5), (3, -1), (7, 9), (4, 2), (4, 3), (-2, 8), (6, 11), (5, 32767)]
    for count in (-1, 0, 1, 5, 10):
        for id_ in (-32768, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 32767): yield state(count, id_, table)
    yield state(3, 1, [(2, 4), (2, 5), (1, 6)])
    yield state(3, 2, [(1, 4), (2, 5), (2, 6)])
    yield state(4, 9, [(9, 1), (9, 2), (9, 3), (8, 4)])
    yield state(0, 0, [])
def rnd(r):
    if r.random() < 0.3:
        count = r.randrange(0, 12)
        entries = [(r.choice([r.randrange(-3, 14), r.randrange(-32768, 32768)]), r.randrange(-32768, 32768)) for _ in range(count)]
        return state(count, r.choice([r.randrange(-3, 14), r.randrange(-32768, 32768)]), entries)
    m = r.choice(KNOWN + [r.getrandbits(32)])
    a = r.choice([r.getrandbits(32), (r.randrange(2048) << 16) | r.randrange(1536), r.randrange(-3, 4)])
    a = a - (1 << 32) if a & 0x80000000 else a
    width, height = r.choice([(640, 480), (800, 600), (1024, 768), (r.randrange(-32768, 32768), r.randrange(-32768, 32768))])
    return message(m, a, r.getrandbits(31), width, height, flags=r.getrandbits(32), mode=r.getrandbits(32), f68=r.uniform(-500, 500),
                   f5bf4=r.uniform(-4, 4), px=r.uniform(-100, 1200), py=r.uniform(-100, 900), g1=r.choice([0, 0, 1, r.randrange(256)]),
                   scale=r.uniform(-1, 1), g2=r.choice([0, 0, 1, r.randrange(256)]), f5a14=r.uniform(-9, 9))
TESTS = [FnTest("Will_HandleCameraMessages and CKeyTable::GetState", ppc, host, edge=edges, rnd=rnd)]
