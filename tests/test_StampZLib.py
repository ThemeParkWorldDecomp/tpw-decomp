"""w216: 28 bullfrog stamp/calendar and zlib wrapper functions.

The original zlib 1.1.3 runs through zlib_bridge, the host uses its installed zlib.
Calendar imports, allocations and C callbacks record every argument they read.
Guarded stamps, hidden results, sources, destinations and callback buffers compare.
Compression streams may differ by zlib version: validate headers, chunk boundaries,
late stores, round trips and untouched storage, and report byte identity separately.
No floating instructions. Invalid pointers, undersized workspaces and buffered pack
buffers below 28 bytes are excluded because the original writes outside them.
"""
import atexit
import ctypes as C
from pathlib import Path
import random
import struct
import zlib

import calllog as CL
import tpwtest as T
from tpwtest import FnTest
import zlib_bridge as ZB

PROGRAM = "bullfrog symbols shared"
SHIM_LIBRARY = "unit_StampZLib"
ROOT = Path(__file__).resolve().parents[2]
VP, U32, I32 = C.c_void_p, C.c_uint32, C.c_int32
M64 = (1 << 64) - 1
STAMP = [0x10026a24, 0x100268a4, 0x10026660, 0x1002640c, 0x1002637c, 0x10026020,
         0x100260a8, 0x100262b8, 0x10026254, 0x10026190, 0x1002631c, 0x10026130, 0x10025f68]
ZF = [0x10024d0c, 0x10025708, 0x100251b8, 0x10025ae8, 0x100253e4, 0x10025d14, 0x10025e24, 0x10025e98]
DT = [0x100255d8, 0x1002566c, 0x10025c00, 0x10025c8c]
SZ = [0x100250dc, 0x10025a28, 0x1002596c]
OWN = set(STAMP + ZF + DT + SZ)
STUBS = CL.Stubs()
STUBS.real.update(OWN | {0x10026740, 0x10024c5c, 0x1002508c, 0x10024bcc, 0x10024c2c})
FT = "FileTimeToSystemTime__10NS_MacDozeFPCQ210NS_MacDoze11tagFILETIMEPQ210NS_MacDoze11_SYSTEMTIME"
ST = "SystemTimeToFileTime__10NS_MacDozeFPCQ210NS_MacDoze11_SYSTEMTIMEPQ210NS_MacDoze11tagFILETIME"
EDGES = [0, 1, M64, 0xffffffff, 0x100000000, 0x7fffffffffffffff, 0x8000000000000000,
         0x80000000ffffffff, 0x7fffffff00000000, 0xffffffff00000000]
IDENTICAL = [0, 0]


def identity_report():
    if IDENTICAL[1]:
        print(f"Compression byte-identical streams: {IDENTICAL[0]}/{IDENTICAL[1]}")


atexit.register(identity_report)


def filetime(e):
    ft, st = e.r(3), e.r(4)
    e.cl.rec("FileTimeToSystemTime", [e.u32(ft), e.u32(ft + 4)])
    e.write(st, struct.pack(">8H", *[e.cl.ret("ftw", 0) & 0xffff for _ in range(8)]))
    e.set_r(3, T.u32(e.cl.ret("ftres", 0)))


def systime(e):
    st, ft = e.r(3), e.r(4)
    fields = struct.unpack(">8H", e.read(st, 16))
    e.cl.rec("SystemTimeToFileTime", [fields[i] for i in [0, 1, 3, 4, 5, 6, 7]])
    e.write(ft, struct.pack(">II", e.cl.ret("low", 0) & 0xffffffff, e.cl.ret("high", 0) & 0xffffffff))
    e.set_r(3, T.u32(e.cl.ret("stres", 0)))


def new_array(e):
    size = e.r(3)
    p = 0 if e.cl.ret("newnull", 0) else e.alloc(size + 32, b"\xd3" * 16 + b"\xa5" * size + b"\xd3" * 16) + 16
    if p:
        e.szl_owned.append((p, size))
    e.cl.rec("new[]", [size, e.cl.newlab(p) if p else 0])
    e.set_r(3, p)


def buffer_full(e):
    p, n, ctx = e.r(3), e.r(4), e.r(5)
    e.cl.rec("full", [e.cl.lab(p), n, e.cl.lab(ctx)])
    e.szl_chunks.append(e.read(p, n))
    e.set_r(3, T.u32(e.cl.ret("cbret", 0)))


def buffer_empty(e):
    p, n, ctx = e.r(3), e.r(4), e.r(5)
    e.cl.rec("empty", [e.cl.lab(p), n, e.cl.lab(ctx)])
    e.szl_empty_chunks.append(e.read(p, n))
    chunk = e.szl_input[e.szl_offset:e.szl_offset + n]
    e.write(p, chunk.ljust(n, b"\0"))
    e.szl_offset += len(chunk)
    e.set_r(3, T.u32(e.cl.ret("cbret", 0)))


STUBS.imp(FT, filetime)
STUBS.imp(ST, systime)
STUBS.imp("__nwa__FUl", new_array)
STUBS.imp("__dla__FPv", lambda e: e.cl.rec("delete", [e.cl.lab(e.r(3))]))
STUBS.imp("__dl__FPv", lambda e: e.cl.rec("delete", [e.cl.lab(e.r(3))]))
STUBS.imp("__register_global_object", lambda e: None)


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    ZB.install(e, ROOT / "ghidra/dumps")
    # Real Callbacks vtables call through two persistent C transition vectors.
    e.szl_full_code = T.persist_alloc(e, 4, bytes.fromhex("4e800020"))
    e.szl_empty_code = T.persist_alloc(e, 4, bytes.fromhex("4e800020"))
    e.szl_full = T.persist_alloc(e, 8, struct.pack(">II", e.szl_full_code, e.toc))
    e.szl_empty = T.persist_alloc(e, 8, struct.pack(">II", e.szl_empty_code, e.toc))
    STUBS.internal(e.szl_full_code, buffer_full)
    STUBS.internal(e.szl_empty_code, buffer_empty)


def bind(lib):
    if getattr(lib, "szl_bound", False):
        return
    signatures = {
        "stamp": (I32, [I32, VP, VP, VP, VP]),
        "zlib": (U32, [U32, VP, VP, U32, VP, U32, VP, VP]),
        "size": (U32, [U32]), "config": (None, [VP, U32]),
        "full_bytes": (VP, []), "full_sizes": (VP, []), "full_count": (U32, []),
        "empty_bytes": (VP, []), "empty_sizes": (VP, []), "empty_count": (U32, []),
        "input_offset": (U32, []), "construct": (None, [U32, VP]), "dtor": (VP, [U32, VP, C.c_int16]),
        "owned_guard": (I32, []),
        "layout": (I32, [VP]),
    }
    for name, (ret, args) in signatures.items():
        f = getattr(lib, "tpw_szl_" + name)
        f.restype, f.argtypes = ret, args
    layout = (C.c_int64 * 4)()
    lib.tpw_szl_layout(layout)
    assert list(layout) == [8, 8, 28, C.sizeof(VP) * 4]
    lib.szl_bound = True


class Side:
    """Shared guarded allocation and endian-aware field observations."""
    def __init__(self, e=None, lib=None):
        self.e, self.lib = e, lib
        self.endian = ">" if e else "<"
        self.keep, self.blocks = [], []
        self.fixed = {}
        if lib:
            bind(lib)

    def read(self, a, n):
        return self.e.read(a, n) if self.e else C.string_at(a, n)

    def write(self, a, b):
        if self.e:
            self.e.write(a, b)
        else:
            C.memmove(a, b, len(b))

    def alloc(self, n, data=None, label=None):
        raw = b"\xd3" * 16 + (data if data is not None else b"\xa5" * n) + b"\xd3" * 16
        if self.e:
            a = self.e.alloc(n + 32, raw) + 16
        else:
            b = C.create_string_buffer(raw, n + 32)
            self.keep.append(b)
            a = C.addressof(b) + 16
        self.blocks.append((a, n))
        if label:
            self.fixed[a if self.e else label] = label if self.e else a
        return a

    def words(self, a, words):
        self.write(a, struct.pack(self.endian + str(len(words)) + "I", *[w & 0xffffffff for w in words]))

    def ticks(self, a):
        return int.from_bytes(self.read(a, 8), "big" if self.e else "little")

    def guards(self):
        return all(self.read(a - 16, 16) == b"\xd3" * 16 and self.read(a + n, 16) == b"\xd3" * 16 for a, n in self.blocks)

    def begin(self, c):
        if self.e:
            self.e.cl = CL.PpcLog(self.fixed, c["q"])
            self.saved = STUBS.activate(self.e)
        else:
            CL.host_begin(self.lib, self.fixed, c["q"])

    def end(self):
        if self.e:
            CL.Stubs.deactivate(self.e, self.saved)
            return self.e.cl.log
        return CL.host_end(self.lib)


class Check(FnTest):
    """Compare logs and semantics, counting exact stream matches separately."""
    def run_case(self, runner, c):
        runner.emu.reset_scratch()
        p, h = self.ppc(runner.emu, c), self.host(runner.lib, c)
        errors = []
        if "stream" in p:
            pb, hb = p.pop("stream"), h.pop("stream")
            if pb is not None and hb is not None:
                IDENTICAL[0] += pb == hb
                IDENTICAL[1] += 1
        diff = CL.first_difference(p.pop("log", []), h.pop("log", []))
        if diff:
            errors.append(diff)
        errors.extend(T.same_values(p, h, "", False))
        for name, value in p.items():
            if name.endswith("_ok") and value is not True:
                errors.append(f"PPC invariant {name}: {value}")
        for name, value in h.items():
            if name.endswith("_ok") and value is not True:
                errors.append(f"host invariant {name}: {value}")
        return errors


def stamp_run(s, c, k):
    a = s.alloc(8, (c["a"] & M64).to_bytes(8, "big" if s.e else "little"))
    b = s.alloc(8)
    if k in (3, 4):
        s.words(b, [c["b"] & 0xffffffff, c["b"] >> 32])
    else:
        s.write(b, (c["b"] & M64).to_bytes(8, "big" if s.e else "little"))
    before_b = s.read(b, 8)
    out = s.alloc(8, b"\xa5" * 8)
    args = s.alloc(28)
    s.words(args, c["args"])
    s.begin(c)
    try:
        if s.e:
            e = s.e
            if k < 3:
                result = s.alloc(4)
                T.call_ppc(e, STAMP[k], [result, a] + [T.I(v) for v in (c["args"] if k == 2 else c["args"][:1])])
                ret = T.s32(e.u32(result))
            elif k == 12:
                T.call_ppc(e, STAMP[k], [out, a, b])
                ret = 0
            else:
                ret = T.call_ppc(e, STAMP[k], [a, b])[0]
                ret = int(ret == a) if k < 7 else ret & 255
        else:
            ret = s.lib.tpw_szl_stamp(k, a, b, out, args)
        return dict(ret=ret, a=s.ticks(a), b_ok=s.read(b, 8) == before_b, out=s.ticks(out), guards_ok=s.guards())
    finally:
        # Attach the complete ordered fake call log to the returned dictionary.
        c["last_log"] = s.end()


def stamp_side(k, is_ppc):
    def run(obj, c):
        out = stamp_run(Side(e=obj) if is_ppc else Side(lib=obj), c, k)
        out["log"] = c.pop("last_log")
        return out
    return run


def stamp_case(r):
    return dict(a=r.getrandbits(64), b=r.choice(EDGES + [r.getrandbits(64)]),
                args=[T.s32(r.getrandbits(32)) for _ in range(7)],
                q=dict(ftres=[r.choice([0, 1, -1])], ftw=[r.randrange(65536) for _ in range(8)],
                       stres=[r.choice([0, 1, -1])], low=[r.getrandbits(32)],
                       high=[r.choice([0, 0x7fffffff, 0x80000000, 0xffffffff, r.getrandbits(32)])]))


def stamp_edges():
    r = random.Random(216)
    for a in EDGES:
        for b in EDGES:
            c = stamp_case(r)
            c.update(a=a, b=b)
            yield c
    # Distinct calendar fields expose the original shifted time fields.
    for month in [0, 1, 2, 11, 12, 65535]:
        for n in [0, 1, -1, -13, 12, 0x7fffffff, -0x80000000, 0x40000000, -65536]:
            c = stamp_case(r)
            c["args"] = [n, 0x10002, -1, 0x12345, -65537, 60, 65536]
            c["q"].update(ftres=[1], ftw=[2024, month, 3, 29 if month == 2 else 31, 17, 43, 51, 999], stres=[1], high=[123])
            yield c


class ZCase(dict):
    def __repr__(self):
        return f"<length={len(self['data'])} size={self['size']} given={self['given']} mode={self['mode']} fail={self['q']['newnull']}>"


def z_case(r, n=None, size=None, mode=None):
    n = n if n is not None else r.choice([64, 128, 256, 512, 1024])
    mode = mode or r.choice(["good", "good", "noise", "big", "badtag", "truncated", "short"])
    data = r.randbytes(n) if mode == "noise" else (r.randbytes(r.choice([1, 3, 8])) * (n + 1))[:n]
    raw = zlib.compress(data)
    unpacked = max(0, n - 1) if mode == "short" else n
    header = struct.pack((">" if mode == "big" else "<") + "7I", 0 if mode == "badtag" else 0x5a4c4942,
                         unpacked, len(raw) + 28, 15, 9, 0, 0)
    if mode == "truncated":
        raw = raw[:max(1, len(raw) // 2)]
    return ZCase(data=data, input=header + raw, mode=mode, size=size if size is not None else r.choice([28, 29, 30, 64, 1024]),
                 given=r.choice([False, True]), q=dict(newnull=[r.randrange(8) == 0], cbret=[r.choice([0, 1, -1, 0x12345678])]))


def z_edges():
    r = random.Random(216)
    for n in [0, 28, 29, 64, 256, 2048]:
        for size in [28, 29, 30, 64, 1024, 4096]:
            for mode in ["good", "noise", "big", "badtag", "truncated", "short"]:
                c = z_case(r, n, size, mode)
                c["q"]["newnull"] = [False]
                yield c
    for given, fail in [(False, False), (False, True), (True, True)]:
        c = z_case(r, 512, 64, "good")
        c.update(given=given)
        c["q"]["newnull"] = [fail]
        yield c


def z_run(s, c, k):
    n, size = len(c["data"]), c["size"]
    source = s.alloc(n, c["data"], 1)
    dest_size = max(n, 28) + 64
    dest = s.alloc(dest_size, label=2)
    buf = s.alloc(size, label=3)
    work = s.alloc(0x62a39, label=4) if c["given"] else 0
    ctx = s.alloc(4, label=5)
    obj = s.alloc(4)
    cb = s.alloc(16) if s.e else 0
    if s.e:
        s.words(obj, [0x100730f4])
        s.words(cb, [0x10073160, s.e.szl_full, s.e.szl_empty, ctx])
        s.e.szl_chunks, s.e.szl_empty_chunks, s.e.szl_input, s.e.szl_offset, s.e.szl_owned = [], [], c["input"], 0, []
    else:
        input_buf = C.create_string_buffer(c["input"])
        s.keep.append(input_buf)
        s.lib.tpw_szl_config(C.addressof(input_buf), len(c["input"]))
    s.begin(c)
    try:
        if s.e:
            e = s.e
            args = [[obj, dest, source, n, work], [source, dest, n, work],
                    [obj, source, n, buf, size, work, cb], [source, buf, n, size, e.szl_full, ctx, work],
                    [obj, dest, buf, size, work, cb], [dest, buf, size, e.szl_empty, ctx, work],
                    [cb, buf, size], [cb, buf, size]][k]
            ret = T.call_ppc(e, ZF[k], args)[0] & 0xffffffff
            chunks, empty_chunks, used = e.szl_chunks, e.szl_empty_chunks, e.szl_offset
        else:
            ret = s.lib.tpw_szl_zlib(k, source, dest, n, buf, size, work, ctx)
            count = s.lib.tpw_szl_full_count()
            sizes = list(struct.unpack("<" + str(count) + "I", C.string_at(s.lib.tpw_szl_full_sizes(), count * 4)))
            flat = C.string_at(s.lib.tpw_szl_full_bytes(), sum(sizes))
            chunks, off = [], 0
            for length in sizes:
                chunks.append(flat[off:off + length])
                off += length
            empty_count = s.lib.tpw_szl_empty_count()
            empty_sizes = list(struct.unpack("<" + str(empty_count) + "I", C.string_at(s.lib.tpw_szl_empty_sizes(), empty_count * 4)))
            empty_flat = C.string_at(s.lib.tpw_szl_empty_bytes(), sum(empty_sizes))
            empty_chunks, off = [], 0
            for length in empty_sizes:
                empty_chunks.append(empty_flat[off:off + length])
                off += length
            used = s.lib.tpw_szl_input_offset()
        out = dict(source_ok=s.read(source, n) == c["data"], guards_ok=s.guards())
        out["owned_guard_ok"] = (all(s.read(p - 16, 16) == b"\xd3" * 16 and s.read(p + n, 16) == b"\xd3" * 16
                                     for p, n in s.e.szl_owned) if s.e else bool(s.lib.tpw_szl_owned_guard()))
        output, buffer = s.read(dest, dest_size), s.read(buf, size)
        if k < 4:
            pack_observe(out, c, k, ret, output, buffer, chunks)
        elif k < 6:
            out.update(ret=ret, output=output, used=used)
            out["empty_chunks"] = [((struct.unpack(s.endian + "7I", b[:28]), b[28:]) if i == 1 else b)
                                   for i, b in enumerate(empty_chunks)]
            # SwapHeader's resulting words are native on each processor.
            if used and used <= size:
                out["buffer"] = struct.unpack(s.endian + "7I", buffer[:28]), buffer[28:]
            else:
                out["buffer"] = buffer
        else:
            out.update(ret=ret, output=output, buffer=buffer, chunks=chunks, empty_chunks=empty_chunks, used=used)
        return out
    finally:
        c["last_log"] = s.end()


def pack_observe(out, c, k, ret, output, buffer, chunks):
    """Version-independent compression checks retain header bytes, order and untouched ranges."""
    n, size = len(c["data"]), c["size"]
    active = (c["given"] or not c["q"]["newnull"][0]) and (k >= 2 or n > 28)
    out.update(success=bool(ret), stream=None)
    if not active:
        out.update(output=output, buffer=buffer, chunks=chunks)
        return
    if k < 2:
        out["buffer"] = buffer
        if ret:
            expected = struct.pack("<7I", 0x5a4c4942, n, ret, 15, 9, 0, 0)
            out["header_ok"] = output[:28] == expected
            stream = output[28:ret]
            out["tail_ok"] = output[ret:] == b"\xa5" * (len(output) - ret)
        else:
            out["header_ok"] = output[:28] == b"\xa5" * 28
            out["tail_ok"] = output[n:] == b"\xa5" * (len(output) - n)
            # A partial failed stream is version-specific, but must begin with the same zlib header when room permits.
            out["partial_header_ok"] = n < 30 or output[28:30] == b"\x78\x9c"
            return
    else:
        out["output"] = output
        flat = b"".join(chunks)
        out["chunk_shape_ok"] = bool(chunks) and all(len(b) == size for b in chunks[:-1]) and len(chunks[-1]) <= size
        out["header_ok"] = flat[:28] == struct.pack(">7I", 0x5a4c4942, n, 0, 15, 9, 0, 0)
        if not ret:
            out["failure_ok"] = size == 28 and chunks == [flat] and len(flat) == 28
            return
        out["sum_ok"] = len(flat) == ret
        out["late_size_ok"] = buffer[8:12] == struct.pack(">I", ret)
        # The last buffer is retained except for the late packed-size store.
        expected = bytearray(b"\xa5" * size)
        for chunk in chunks:
            expected[:len(chunk)] = chunk
        expected[8:12] = struct.pack(">I", ret)
        out["retained_buffer_ok"] = buffer == expected
        stream = flat[28:]
    d = zlib.decompressobj()
    try:
        decoded = d.decompress(stream) + d.flush()
        out["roundtrip_ok"] = decoded == c["data"] and d.eof and not d.unused_data and not d.unconsumed_tail
    except zlib.error:
        out["roundtrip_ok"] = False
    out["stream_header_ok"] = stream[:2] == b"\x78\x9c"
    out["stream"] = stream


def z_side(k, is_ppc):
    def run(obj, c):
        out = z_run(Side(e=obj) if is_ppc else Side(lib=obj), c, k)
        log = c.pop("last_log")
        if k in (2, 3):
            out["callback_args_ok"] = all(event[1][0] == 3 and event[1][2] == 5 for event in log if event[0] == "full")
        # Chunk sizes can differ across zlib versions; pack_observe validates their complete structure.
        out["log"] = [event for event in log if not (k in (2, 3) and event[0] == "full")]
        return out
    return run


def d_case(r):
    return dict(flag=T.s32(r.getrandbits(32)), null=r.randrange(8) == 0, payload=r.randbytes(60), q={})


def d_edges():
    r = random.Random(216)
    for flag in [-0x80000000, -0x8000, -1, 0, 1, 2, 0x7fff, 0x10000, 0x18000, 0x1ffff]:
        for null in [False, True]:
            c = d_case(r)
            c.update(flag=flag, null=null)
            yield c


def d_run(s, c, k):
    obj = s.alloc(64, b"\xa5" * 4 + c["payload"], 1)
    if not s.e:
        s.lib.tpw_szl_construct(k, obj)
    before = s.read(obj, 64)
    target = 0 if c["null"] else obj
    s.begin(c)
    try:
        if s.e:
            ret = T.call_ppc(s.e, DT[k], [target, T.S16(c["flag"])])[0]
            after = s.read(obj, 64)
            abi = c["null"] or int.from_bytes(after[:4], "big") == (0x10071afc if k < 2 else 0x10073174)
            payload = after[4:] == before[4:]
        else:
            ret = s.lib.tpw_szl_dtor(k, target, T.s16(c["flag"])) or 0
            after = s.read(obj, 64)
            abi = True
            payload = after[(0 if k < 2 else C.sizeof(VP)):] == before[(0 if k < 2 else C.sizeof(VP)):]
        return dict(ret_this_ok=ret == target, abi_ok=abi, payload_ok=payload, guards_ok=s.guards())
    finally:
        c["last_log"] = s.end()


def d_side(k, is_ppc):
    def run(obj, c):
        out = d_run(Side(e=obj) if is_ppc else Side(lib=obj), c, k)
        out["log"] = c.pop("last_log")
        return out
    return run


def size_side(k, is_ppc):
    def run(obj, c):
        s = Side(e=obj) if is_ppc else Side(lib=obj)
        this = s.alloc(4)
        if s.e:
            s.words(this, [0x100730f4])
        s.begin(c)
        try:
            ret = T.call_ppc(obj, SZ[k], [this] if k == 0 else [])[0] if is_ppc else obj.tpw_szl_size(k)
            return dict(ret=ret)
        finally:
            s.end()
    return run


STAMP_NAMES = ["AddYears", "AddMonths", "SetTime", "operator= FILETIME", "operator= TbFileStamp", "operator+=", "operator-=",
               "operator!=", "operator>=", "operator<=", "operator==", "operator<", "operator- TbTimeDiff"]
Z_NAMES = ["CZLib::MemCompress", "LbZLib_MemCompress", "CZLib::BufferedCompress", "LbZLib_BufferedCompress",
           "CZLib::BufferedDecompress", "LbZLib_BufferedDecompress", "Callbacks::BufferFull", "Callbacks::BufferEmpty"]
D_NAMES = ["CZLib::~CZLib", "CBufferedCompressionBase::~CBufferedCompressionBase", "Callbacks::~Callbacks", "CBufferCallback::~CBufferCallback"]
TESTS = [Check("TbTimeStamp::" + name, stamp_side(k, True), stamp_side(k, False), edge=stamp_edges, rnd=stamp_case)
         for k, name in enumerate(STAMP_NAMES)]
TESTS += [Check(name, z_side(k, True), z_side(k, False), edge=z_edges, rnd=z_case) for k, name in enumerate(Z_NAMES)]
TESTS += [Check(name, d_side(k, True), d_side(k, False), edge=d_edges, rnd=d_case) for k, name in enumerate(D_NAMES)]
TESTS += [Check(name, size_side(k, True), size_side(k, False), edge=lambda: [{"q": {}}], rnd=lambda r: {"q": {}})
          for k, name in enumerate(["CZLib::DecompressWorkspaceSize", "LbZLib_DeCompressWorkspaceSize", "LbZLib_CompressWorkspaceSize"])]
