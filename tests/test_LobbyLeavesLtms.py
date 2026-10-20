"""Unit w40, the ltms half: CStaticUnicodeBuffer (src/CStaticUnicodeBuffer.cpp), against libtpw_unit_LobbyLeaves.dylib
(tests/unit_LobbyLeaves.cpp), in the ltms library ("ltms symbols shared"):
  AppendMB(const unsigned char*, unsigned long) 0x100004f0, AppendMB(unsigned short) 0x100006bc, AppendUnicode(const wchar_t*, unsigned long)
  0x10000780, AppendUnicode(wchar_t) 0x10000884, Close 0x10000904, ~CStaticUnicodeBuffer 0x100009d8.

The buffer is the original's object in the emulator's memory (its real vtable at TOC + 0xae4, so the virtual AppendUnicode that AppendMB calls
and the Close that the destructor calls are the original's code) and the host's own over a guarded array of 16-bit cells. The converter is a
TbIMBToUnicode: on the PowerPC side an object whose first lead byte, count and table the case chooses, with a fake Release at vtable slot 8;
on the host the real TbIMBToUnicode (src/TbIMBToUnicode.cpp) with a fake Release. TbIMBInfo::GetIndex runs for real on both sides (the original's
is the ltms image's own). A case is a SCRIPT of operations: the six functions and three direct writes of the object's cursor, overflow flag and
converter pointer (to keep a long script alive past the end of the buffer). After every operation the cursor (in cells), the end, whether a
converter is held, the overflow flag and every cell of the array, guards included, are compared; at the end the call log (the converter's
Release) and the heap traffic (the destructor with a positive flag deletes the object). The object's own words are dead after the destructor
(the compiler may drop the writes), so that operation compares the cells and the log only.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "ltms symbols shared"
SHIM_LIBRARY = "unit_LobbyLeaves"
MASK = 0xFFFFFFFF
APPEND_MB, APPEND_MB_VALUE, APPEND_UNICODE, APPEND_CHAR, CLOSE, DESTRUCTOR = 0x100004F0, 0x100006BC, 0x10000780, 0x10000884, 0x10000904, 0x100009D8
VTABLE_OFFSET = 0xAE4        # CStaticUnicodeBuffer's vtable is TOC + 0xae4 (0x10010f74), the base class's TOC + 0xb04
GUARD, MAX_CELLS = 8, 40
RECORD = 4 + GUARD + MAX_CELLS + GUARD
FILL = 0x9999
NEG = 0xFFFFFFFF

# operation codes (the host's, tests/unit_LobbyLeaves.cpp tpw_ll_buf_run)
MB, MB_VALUE, UNI, UNI_CHAR, CLOSE_OP, DESTROY, SET_CURSOR, SET_OVERFLOW, SET_CONVERTER = range(9)


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    # the converter's vtable: only Release (slot 8) is ever called, and it records
    e.ll_conv_vt = T.persist_alloc(e, 16)
    e.write(e.ll_conv_vt + 8, struct.pack(">I", T.fake_function(e, lambda em: em.cl.rec("Release", [em.cl.lab(em.r(3))]))))


def heap_ppc(e):
    kinds = [k for k, _, _ in e.alloc_log]
    return [kinds.count("new"), kinds.count("new[]"), kinds.count("delete"), kinds.count("delete[]")]


def heap_host(lib):
    return list((C.c_int32 * 4).in_dll(lib, "tpw_cl_heap"))


def heap_clear_host(lib):
    for i in range(4):
        (C.c_int32 * 4).in_dll(lib, "tpw_cl_heap")[i] = 0


# ---------------------------------------------------------------- multibyte text for a converter

def is_lead(first, byte):
    """Whether TbIMBInfo::GetIndex takes a second byte after `byte` (an index of the byte minus one, 16 bits, at least the first lead)."""
    return first <= ((byte - 1) & 0xFFFF)


def split_chars(first, data):
    """The sizes of the characters of `data` as GetIndex reads them (a lead byte takes the next one with it)."""
    sizes, i = [], 0
    while i < len(data):
        n = 2 if is_lead(first, data[i]) else 1
        sizes.append(n)
        i += n
    return sizes


def make_chars(r, first, count):
    """A list of multibyte characters (each a bytes of 1 or 2 bytes, the first never zero) that a converter with the lead byte `first` reads
    whole: some near the first lead byte (small indexes, so that table entries are hit), some anywhere."""
    chars = []
    for _ in range(count):
        low = min(255, first + 1)
        k = r.random()
        byte = r.randint(1, 255) if k < 0.3 else min(255, max(1, low + r.randint(-2, 6)))
        if is_lead(first, byte):
            chars.append(bytes((byte, r.choice([0, r.randint(0, 255), r.randint(0, 255)]))))
        else:
            chars.append(bytes((byte,)))
    return chars


# ---------------------------------------------------------------- scripts

class Script:
    """One case: the buffer (cells, where the cursor starts, the overflow flag, the converter) and the operations."""

    def __init__(self, cells=12, cursor=0, overflow=0, conv=None):
        self.cells, self.cursor, self.overflow, self.conv = cells, cursor, overflow, conv
        self.ops, self.bytes, self.wide = [], bytearray(), []

    def add_bytes(self, data):
        off = len(self.bytes)
        self.bytes += bytes(data) + bytes(16)    # zeros after each text, for a count that runs past its end
        return off

    def add_wide(self, codes):
        off = len(self.wide)
        self.wide += list(codes) + [0] * 16
        return off

    def mb(self, data, size=NEG):
        """AppendMB over `data` (plus a terminating zero): `size` bytes or, by default, up to the terminator."""
        self.ops.append((MB, self.add_bytes(bytes(data) + b"\0"), size & MASK, 0, 0))
        return self

    def mb_value(self, value):
        self.ops.append((MB_VALUE, value & 0xFFFF, 0, 0, 0))
        return self

    def uni(self, codes, count=NEG):
        self.ops.append((UNI, self.add_wide(list(codes) + [0]), count & MASK, 0, 0))
        return self

    def char(self, value):
        self.ops.append((UNI_CHAR, value & 0xFFFF, 0, 0, 0))
        return self

    def close(self):
        self.ops.append((CLOSE_OP, 0, 0, 0, 0))
        return self

    def destroy(self, flag):
        self.ops.append((DESTROY, flag & MASK, 0, 0, 0))
        return self

    def set_cursor(self, cell):
        self.ops.append((SET_CURSOR, cell & MASK, 0, 0, 0))
        return self

    def set_overflow(self, value):
        self.ops.append((SET_OVERFLOW, value, 0, 0, 0))
        return self

    def set_converter(self, value):
        self.ops.append((SET_CONVERTER, value, 0, 0, 0))
        return self


def converter(first=0x80, count=300, base=0x1000, table=None):
    return {"first": first, "count": count, "table": table if table is not None else [(base + i) & 0xFFFF for i in range(count)]}


# ---------------------------------------------------------------- the two sides

def run_ppc(e, c):
    e.alloc_log.clear()
    array = e.alloc(2 * (GUARD + c.cells + GUARD), struct.pack(">H", FILL) * (GUARD + c.cells + GUARD))
    base = array + 2 * GUARD
    conv = c.conv if c.conv is not None else converter(first=0, count=0)    # a script with no converter can still attach this one
    # one entry past the table that is not zero: a translation that reads the table at an index equal to the count shows it
    table = e.alloc(2 * (conv["count"] + 1), b"".join(struct.pack(">H", v) for v in conv["table"] + [0xBEEF]))
    conv_ptr = e.alloc(0x10, struct.pack(">IIBBHI", e.ll_conv_vt, 1, conv["first"], 0, conv["count"], table))
    buffer = e.alloc(0x14, struct.pack(">IIIIBBBB", e.toc + VTABLE_OFFSET, 0, base + 2 * c.cells, 0, 0xA5, 0xA5, 0xA5, 0xA5))
    e.write(buffer + 4, struct.pack(">I", 0 if c.cursor < 0 else base + 2 * c.cursor))
    e.write(buffer + 0xC, struct.pack(">I", conv_ptr if c.conv is not None and c.has_converter else 0))
    e.write(buffer + 0x10, bytes([c.overflow]))
    pool = e.alloc(max(1, len(c.bytes)), bytes(c.bytes) or b"\0")
    wide = e.alloc(2 * max(1, len(c.wide)), b"".join(struct.pack(">H", v) for v in c.wide) or b"\0\0")
    e.cl = CL.PpcLog({conv_ptr: 4})
    states = []
    for code, p1, p2, _, _ in c.ops:
        gone = False
        if code == MB:
            T.call_ppc(e, APPEND_MB, [buffer, pool + p1, p2])
        elif code == MB_VALUE:
            T.call_ppc(e, APPEND_MB_VALUE, [buffer, p1])
        elif code == UNI:
            T.call_ppc(e, APPEND_UNICODE, [buffer, wide + 2 * p1, p2])
        elif code == UNI_CHAR:
            T.call_ppc(e, APPEND_CHAR, [buffer, p1])
        elif code == CLOSE_OP:
            T.call_ppc(e, CLOSE, [buffer])
        elif code == SET_CURSOR:
            e.write(buffer + 4, struct.pack(">I", 0 if p1 == NEG else base + 2 * p1))
        elif code == SET_OVERFLOW:
            e.write(buffer + 0x10, bytes([p1]))
        elif code == SET_CONVERTER:
            e.write(buffer + 0xC, struct.pack(">I", conv_ptr if p1 else 0))
        else:
            T.call_ppc(e, DESTRUCTOR, [buffer, p1])
            gone = True
        cells = struct.unpack(">%dH" % (GUARD + c.cells + GUARD), e.read(array, 2 * (GUARD + c.cells + GUARD)))
        if gone:
            states.append([0, 0, 0, 0] + list(cells))
            break
        cursor, end, held = e.u32(buffer + 4), e.u32(buffer + 8), e.u32(buffer + 0xC)
        states.append([NEG if cursor == 0 else (cursor - base) // 2, (end - base) // 2, 1 if held else 0, e.read(buffer + 0x10, 1)[0]] + list(cells))
    return {"states": states, "log": e.cl.log, "heap": heap_ppc(e)}


def run_host(lib, c):
    n = len(c.ops)
    ops = (C.c_uint32 * (5 * max(1, n)))(*[w for op in c.ops for w in op])
    conv = c.conv if c.conv is not None else converter(first=0, count=0)    # as on the PowerPC side
    table = (C.c_uint16 * (conv["count"] + 2))(*(conv["table"] + [0xBEEF, 0xBEEF]))
    pool = (C.c_uint8 * max(1, len(c.bytes)))(*c.bytes)
    wide = (C.c_uint16 * max(1, len(c.wide)))(*c.wide)
    out = (C.c_uint32 * (RECORD * max(1, n)))()
    lib.tpw_ll_buf_run.argtypes = [C.c_int32] * 6 + [C.c_void_p, C.c_void_p, C.c_int32, C.c_void_p, C.c_void_p, C.c_void_p]
    lib.tpw_ll_buf_prepare()
    CL.host_begin(lib, {})
    heap_clear_host(lib)
    lib.tpw_ll_buf_run(c.cells, c.cursor, c.overflow, 1 if c.conv is not None and c.has_converter else 0,
                       conv["first"], conv["count"], table, ops, n, pool, wide, out)
    log = CL.host_end(lib)
    states = []
    for i in range(n):
        record = list(out[i * RECORD:(i + 1) * RECORD])
        states.append(record[:4] + record[4:4 + GUARD + c.cells + GUARD])
        if c.ops[i][0] == DESTROY:
            break
    return {"states": states, "log": log, "heap": heap_host(lib)}


def ppc(e, c):
    return run_ppc(e, c)


def host(lib, c):
    return run_host(lib, c)


# Whether the converter is attached at the start: part of the script (a script with a converter spec attaches it unless this is cleared).
Script.has_converter = True


# ---------------------------------------------------------------- the edge scripts, by the function they are about

CONV = converter()
HELLO = b"Hello"


def edges_mb():
    # up to the terminator: a converter, characters inside the count, then past it (count 10: 'A' is index 64)
    yield Script(12, 0, 0, CONV).mb(HELLO)
    yield Script(12, 0, 0, converter(count=10)).mb(HELLO).mb(b"\x01\x02\x0a\x0b")
    yield Script(12, 0, 0, CONV).mb(b"")
    # with a size: none, part, all
    for size in (0, 1, 3, 5):
        yield Script(12, 0, 0, CONV).mb(HELLO, size)
    yield Script(12, 0, 0, CONV).mb(HELLO, 4).mb(HELLO, 1)
    # no converter: nothing happens (and with a converter that is detached, and one attached later)
    s = Script(12, 0, 0, CONV)
    s.has_converter = False
    yield s.mb(HELLO).mb(HELLO, 3)
    yield Script(12, 0, 0, CONV).set_converter(0).mb(HELLO).set_converter(1).mb(HELLO)
    yield Script(12, 0, 0, None).mb(HELLO).mb(HELLO, 2)
    # lead bytes: the first lead byte 0x80 (index 0x80 and up), two-byte characters, a sized scan ending after a lead byte's second byte
    yield Script(14, 0, 0, CONV).mb([0x81, 0x40, 0x41, 0x90, 0x00, 0xFF, 0xFF, 0x42])
    yield Script(14, 0, 0, CONV).mb([0x81, 0x40, 0x41, 0x90, 0x00, 0x42], 2).mb([0x81, 0x40, 0x41, 0x90, 0x00, 0x42], 3).mb([0x81, 0x40, 0x41], 3)
    for first in (0, 1, 0x7F, 0x80, 0xFE, 0xFF):
        yield Script(20, 0, 0, converter(first=first, count=500)).mb([0x41, 0x01, 0x7F, 0x80]).mb([0x41, 0x01])
    # the table's own values (zero, 0xffff, a lone surrogate); an index just inside and outside the count
    yield Script(10, 0, 0, converter(count=4, table=[0, 0xFFFF, 0xD800, 0x20AC])).mb([1, 2, 3, 4, 5])
    for count in (0, 1, 2, 64, 65, 66):
        yield Script(10, 0, 0, converter(count=count)).mb(b"\x01\x02\x03\x40\x41\x42")
    # into a small buffer: exactly full, over, no room, a flag already raised, a cursor of NULL left out
    yield Script(0, 0, 0, CONV).mb(HELLO)
    yield Script(1, 0, 0, CONV).mb(HELLO)
    yield Script(5, 0, 0, CONV).mb(HELLO).mb(HELLO, 2)
    yield Script(6, 0, 0, CONV).mb(HELLO).mb(HELLO, 2)
    yield Script(8, 4, 1, CONV).mb(HELLO)


def edges_mb_value():
    # high byte first (a high byte that is not a lead byte stands alone, the low byte is then ignored)
    for value in (0x0000, 0x0001, 0x0041, 0x4142, 0x7F7F, 0x8081, 0x8140, 0x81FF, 0xFE01, 0xFF00, 0xFFFF):
        yield Script(6, 0, 0, CONV).mb_value(value)
        yield Script(6, 0, 0, converter(first=0x40, count=2000)).mb_value(value)
    for first in (0, 1, 0x7F, 0x80, 0xFE, 0xFF):
        yield Script(20, 0, 0, converter(first=first, count=500)).mb_value(0x0041).mb_value(0x8140).mb_value(0xFFFF).mb_value(0x0001).mb_value(0)
    # no converter, a converter attached later, a full buffer
    s = Script(6, 0, 0, CONV)
    s.has_converter = False
    yield s.mb_value(0x41).mb_value(0x4142)
    yield Script(6, 0, 0, CONV).set_converter(0).mb_value(0x41).set_converter(1).mb_value(0x41)
    yield Script(2, 0, 0, CONV).mb_value(0x41).mb_value(0x42).mb_value(0x43).mb_value(0x44)
    yield Script(0, 0, 0, CONV).mb_value(0x41)
    yield Script(6, 3, 1, CONV).mb_value(0x41)
    for count in (0, 1, 2, 64, 65, 66):
        yield Script(10, 0, 0, converter(count=count)).mb_value(0x0041).mb_value(0x0042).mb_value(0x0043)


def edges_uni():
    # with a length, up to the terminator; into a buffer with room, exactly full and over
    yield Script(8, 0, 0, None).uni([65, 66, 67]).uni([68, 69], 2).uni([])
    yield Script(8, 0, 0, None).uni([65, 66, 67, 68, 69, 70, 71, 72]).uni([74], 1).uni([75])
    yield Script(8, 0, 0, None).uni([65, 66, 67, 68, 69, 70, 71, 72, 73, 74], 8).uni([1], 1)
    yield Script(8, 0, 0, None).uni([65, 66, 67, 68, 69, 70, 71, 72, 73, 74], 10)
    yield Script(8, 0, 0, None).uni([65, 66, 67], 0).uni([65, 66, 67], 1).uni([1, 2], 5)
    yield Script(8, 5, 0, None).uni([65, 66, 67, 68, 69], NEG).uni([1])
    yield Script(8, 8, 0, None).uni([1, 2, 3]).uni([1, 2, 3], 3)
    yield Script(8, 3, 1, None).uni([1, 2, 3]).uni([1, 2, 3], 3)
    yield Script(0, 0, 0, None).uni([1]).uni([1], 1)
    yield Script(0, 0, 0, None).uni([])
    yield Script(8, 0, 0, None).uni([0xFFFF, 0, 0x8000], 3).uni([0x1234, 0x5678, 0], NEG)
    yield Script(8, 0, 0, None).uni([1, 2, 3]).set_overflow(1).uni([4]).set_overflow(0).uni([5], 1)
    yield Script(8, 0, 0, None).uni([1, 2, 3]).set_cursor(7).uni([4, 5, 6], 3).uni([7, 8], NEG)
    # count -1 versus the biggest counts that are not (past the end of the terminated text the pool is zeros)
    yield Script(12, 0, 0, None).uni([1, 2, 3], 0x7FFFFFFF)
    yield Script(12, 0, 0, None).uni([1, 2, 3], 0xFFFFFFFE)


def edges_char():
    yield Script(8, 0, 0, None).char(70).char(0).char(0xFFFF).char(0x8000)
    yield Script(3, 0, 0, None).char(1).char(2).char(3).char(4).char(5)
    yield Script(0, 0, 0, None).char(1)
    yield Script(8, 8, 0, None).char(1).char(2)
    yield Script(8, 3, 1, None).char(1)
    yield Script(8, 7, 0, None).char(1).char(2).set_overflow(0).char(3).set_cursor(2).char(4)


def edges_close():
    # nothing written, partly, exactly full (that raises the flag), overflowed, a cursor of NULL; with and without a converter
    yield Script(8, 0, 0, None).close()
    yield Script(8, 0, 0, CONV).close().close()
    yield Script(8, 3, 0, CONV).uni([1, 2]).close()
    yield Script(8, 8, 0, None).close()
    yield Script(8, 8, 0, CONV).close()
    yield Script(8, 4, 1, None).close()
    yield Script(8, 4, 1, CONV).close()
    yield Script(8, -1, 0, CONV).close()
    yield Script(8, -1, 0, None).close()
    yield Script(0, 0, 0, None).close()
    yield Script(0, 0, 1, CONV).close()
    yield Script(0, 0, 0, CONV).close()
    yield Script(8, 8, 1, CONV).close()
    yield Script(8, 0, 1, CONV).close()
    yield Script(40, 39, 0, CONV).uni([1, 2, 3]).close()
    yield Script(8, 0, 0, CONV).uni([1, 2, 3]).close().set_cursor(0).set_converter(1).mb(b"AB").close()


def edges_destroy():
    # every kind of flag; a converter or none, a cursor or none, full or not
    for flag in (1, 0, -1, 2, 0x7FFF, 0x8000, 0xFFFF, 0x10000, 0x10001, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF):
        yield Script(8, 2, 0, CONV).uni([1, 2]).destroy(flag)
        yield Script(8, 8, 0, None).destroy(flag)
        yield Script(8, -1, 0, CONV).destroy(flag)
        yield Script(0, 0, 1, CONV).destroy(flag)
        yield Script(8, 4, 1, CONV).destroy(flag)


# ---------------------------------------------------------------- random scripts

# A random script of `length` operations, most of them of the kind in `focus` (an op code, or None for all kinds equally).
def make_random(r, focus=None, length=None):
    cells = r.choice([0, 1, 2, 3, 5, 8, 12, 16, 24, 40])
    first = r.choice([0, 1, 0x40, 0x7F, 0x80, 0x81, 0xA0, 0xFE, 0xFF, r.randint(0, 255)])
    count = r.choice([0, 1, 3, 10, 100, 255, 256, 300, 1000])
    conv = None
    if r.random() < 0.85:
        conv = converter(first, count, table=[r.choice([r.randint(1, 0xFFFF), r.randint(0x20, 0x7E), 0]) for _ in range(count)])
    first = conv["first"] if conv is not None else 0    # the converter a script without one can attach has first lead byte 0
    s = Script(cells, r.choice([0, 0, -1, r.randint(0, cells)]), r.choice([0, 0, 0, 1]), conv)
    s.has_converter = r.random() < 0.8
    closed = s.cursor < 0
    for _ in range(length or r.choice([5, 10, 40, 100, 200, 200])):
        k = r.random()
        if k < 0.04:    # the cursor put back to a cell, or to NULL
            closed = r.random() < 0.15
            s.set_cursor(NEG if closed else r.randint(0, cells))
        elif k < 0.07:
            s.set_overflow(r.randint(0, 1))
        elif k < 0.10:
            s.set_converter(r.randint(0, 1))
        elif k < 0.16 and focus in (None, CLOSE_OP):
            s.close()
            closed = True
        elif closed:
            s.close()
        else:
            kind = focus if focus is not None and r.random() < 0.8 else r.choice([MB, MB_VALUE, UNI, UNI_CHAR])
            if kind in (CLOSE_OP, DESTROY):
                kind = r.choice([MB, MB_VALUE, UNI, UNI_CHAR])
            if kind == MB:
                chars = make_chars(r, first, r.randint(0, 6))
                data = b"".join(chars)
                if r.random() < 0.5:
                    s.mb(data)
                else:
                    cut = r.randint(0, len(chars))
                    s.mb(data, sum(len(ch) for ch in chars[:cut]))
            elif kind == MB_VALUE:
                s.mb_value(r.choice([r.getrandbits(16), r.randint(0, 0x100), (first << 8) | r.randint(0, 255), r.randint(0x8000, 0x8200)]))
            elif kind == UNI:
                codes = [r.choice([r.randint(0x20, 0x7E), r.randint(1, 0xFFFF)]) for _ in range(r.randint(0, 6))]
                if r.random() < 0.5:
                    s.uni(codes)
                else:
                    s.uni(codes + [r.randint(1, 99) for _ in range(3)], r.randint(0, len(codes) + 2))
            else:
                s.char(r.getrandbits(16))
    if focus == CLOSE_OP and r.random() < 0.5:
        s.close()
    if focus == DESTROY or (focus is None and r.random() < 0.3):
        s.destroy(r.choice([1, 0, -1, 2, 0x10000, r.getrandbits(32)]))
    return s


def random_for(focus, length=None):
    return lambda r: make_random(r, focus, length)


# The random scripts of 200 operations for each function, and the mixed ones.
TESTS = [
    FnTest("CStaticUnicodeBuffer::AppendMB(bytes)", ppc, host, edge=edges_mb, rnd=random_for(MB)),
    FnTest("CStaticUnicodeBuffer::AppendMB(halfword)", ppc, host, edge=edges_mb_value, rnd=random_for(MB_VALUE)),
    FnTest("CStaticUnicodeBuffer::AppendUnicode(text)", ppc, host, edge=edges_uni, rnd=random_for(UNI)),
    FnTest("CStaticUnicodeBuffer::AppendUnicode(character)", ppc, host, edge=edges_char, rnd=random_for(UNI_CHAR)),
    FnTest("CStaticUnicodeBuffer::Close", ppc, host, edge=edges_close, rnd=random_for(CLOSE_OP)),
    FnTest("CStaticUnicodeBuffer::~CStaticUnicodeBuffer", ppc, host, edge=edges_destroy, rnd=random_for(DESTROY, 4)),
    FnTest("CStaticUnicodeBuffer scripts of 200 operations", ppc, host, edge=lambda: [], rnd=random_for(None, 200)),
]
