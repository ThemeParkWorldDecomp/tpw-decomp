"""Differential tests for the three small bullfrog functions unit w108 translated for the save file reader and the most-recent-save
loader: LbFile_CompareFileStamps (0x10000520, src/LbFile_stamp.cpp), CZLib::DecompressedSize (0x1002512c) and
LbZLib_MemDecompressedSize (0x100258a0, both src/lb_zlib_size.cpp), against libtpw_unit_SaveLoadLib.dylib (tests/unit_SaveLoadLib.cpp).

NS_MacDoze::CompareFileTime (a platform function; the original's is and
platform/tests/platform_tests.cpp) is a fake on both sides that records the four words and answers a queue. The size functions run
the original's SwapHeader on the PowerPC side (which swaps the little endian words of the block in place) and the host's, which only
marks the header; the block is placed as a file has it (little endian bytes). A block whose mSwapped word already reads as the marker
is left out: the original would not swap it on a big endian machine and the port cannot see the difference (see include/bfl_ZLib.h).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "bullfrog symbols shared"
SHIM_LIBRARY = "unit_SaveLoadLib"

STAMPS, DECOMPRESSED_SIZE, MEM_DECOMPRESSED_SIZE = 0x10000520, 0x1002512C, 0x100258A0
MARKER = 0x53574150
TAG = 0x5A4C4942
LABEL_A, LABEL_B = 1, 2
VP, I32, U32 = C.c_void_p, C.c_int32, C.c_uint32

STUBS = None


def setup(runner):
    global STUBS
    T.install_std_stubs(runner.emu)
    S = CL.Stubs()

    def compare(e):
        L = e.cl
        a, b = e.r(3), e.r(4)
        L.rec("CompareFileTime", (e.u32(a), e.u32(a + 4), e.u32(b), e.u32(b + 4)))
        e.set_r(3, T.u32(L.ret("CompareFileTime")))

    S.imp("CompareFileTime__10NS_MacDozeFPCQ210NS_MacDoze11tagFILETIMEPCQ210NS_MacDoze11tagFILETIME", compare)
    S.real.update({STAMPS, DECOMPRESSED_SIZE, MEM_DECOMPRESSED_SIZE, 0x10024C5C})
    STUBS = S


def words():
    return [0, 1, 0xFFFFFFFF, 0x80000000, 0x7FFFFFFF, 0x12345678]


def rnd_word(r):
    return r.choice(words() + [r.getrandbits(32)])


def run(call_ppc, call_host, observe_ppc=None, observe_host=None):
    def ppc(e, c):
        saved = STUBS.activate(e)
        try:
            e.cl = CL.PpcLog({}, c.get("q", {}))
            res = call_ppc(e, c)
            out = dict(result=res, log=e.cl.log, extra=observe_ppc(e, c) if observe_ppc else None)
        finally:
            CL.Stubs.deactivate(e, saved)
        return out

    def host(lib, c):
        CL.host_begin(lib, {}, c.get("q", {}))
        res = call_host(lib, c)
        log = CL.host_end(lib)
        return dict(result=res, log=log, extra=observe_host(lib, c) if observe_host else None)
    return ppc, host


# ---- LbFile_CompareFileStamps
def g_stamps(r):
    return dict(a=None if r.random() < 0.2 else [rnd_word(r), rnd_word(r)], b=None if r.random() < 0.2 else [rnd_word(r), rnd_word(r)],
                q={"CompareFileTime": [r.choice([1, 0, -1, 7, -9, 0x7FFFFFFF, -0x80000000])]})


def stamps_ppc(e, c):
    args = []
    for k in ("a", "b"):
        if c[k] is None:
            args.append(0)
        else:
            obj = e.alloc(8, struct.pack(">II", *c[k]))
            args.append(obj)
    return T.s32(T.call_ppc(e, STAMPS, args)[0])


def stamps_host(lib, c):
    keep = []
    args = []
    for k in ("a", "b"):
        if c[k] is None:
            args.append(None)
        else:
            arr = (U32 * 2)(*c[k])
            keep.append(arr)
            args.append(C.addressof(arr))
    f = lib.tpw_slib_stamps
    f.restype, f.argtypes = I32, [VP, VP]
    return f(*args)


# ---- the block sizes
def g_block(r):
    tag = TAG if r.random() < 0.7 else rnd_word(r)
    swapped = 0 if r.random() < 0.6 else r.choice([r.getrandbits(32), 1, 0xFFFFFFFF])
    if swapped == MARKER:
        swapped = 0
    header = [tag, rnd_word(r), rnd_word(r), r.choice([15, 15, 9, 0, 16]), r.choice([9, 1, 0]), 0, swapped]
    return dict(block=struct.pack("<7I", *header) + bytes(r.getrandbits(8) for _ in range(r.choice([0, 4, 20]))))


def block_ppc(entry):
    def call(e, c):
        obj = e.alloc(len(c["block"]) + 8, c["block"])
        c["_addr"] = obj
        return T.u32(T.call_ppc(e, entry, [obj] if entry == MEM_DECOMPRESSED_SIZE else [0, obj])[0])
    return call


def block_host(name):
    def call(lib, c):
        buffer = C.create_string_buffer(c["block"], len(c["block"]) + 8)
        c["_buffer"] = buffer
        f = getattr(lib, name)
        f.restype, f.argtypes = U32, [VP]
        return f(C.addressof(buffer))
    return call


def marker_ppc(e, c):
    return e.u32(c["_addr"] + 0x18)


def marker_host(lib, c):
    return struct.unpack("<I", bytes(c["_buffer"].raw[0x18:0x1C]))[0]


def gen(make, n=60, seed=5):
    def edge():
        r = random.Random(seed)
        return [make(r) for _ in range(n)]
    return edge, make


TESTS = []


def add(name, make, ppc_host):
    ppc, host = ppc_host
    edge, rnd = gen(make)
    TESTS.append(FnTest(name, ppc, host, edge=edge, rnd=rnd))


add("LbFile_CompareFileStamps", g_stamps, run(stamps_ppc, stamps_host))
add("CZLib::DecompressedSize", g_block, run(block_ppc(DECOMPRESSED_SIZE), block_host("tpw_slib_decompressed_size"),
                                            marker_ppc, marker_host))
add("LbZLib_MemDecompressedSize", g_block, run(block_ppc(MEM_DECOMPRESSED_SIZE), block_host("tpw_slib_memdecompressed_size"),
                                               marker_ppc, marker_host))
