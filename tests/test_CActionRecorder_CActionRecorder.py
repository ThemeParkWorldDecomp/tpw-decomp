"""Unit v5075, CActionRecorder's constructor, destructor, Serialise and RecordAction, against
libtpw_unit_CActionRecorder_CActionRecorder.dylib (tests/unit_CActionRecorder_CActionRecorder.cpp), main app, src/game/ActionRecorder.cpp:
  CActionRecorder::CActionRecorder 0x10143658, ~CActionRecorder 0x10143880, Serialise 0x101439dc, RecordAction 0x1014430c

A case is a recorder object and a call. The object is built twice by ONE piece of Python code: in emulator memory (big-endian, the Mac's
offsets) and in the host library's memory (little-endian; the host's three files are 16-byte slots and the table 0x34..0x3d3 is the member
mF34, so the shared code maps a Mac offset to the host's). The callees outside the unit are fakes on both sides that record the same
events: CTRACE (the text as a hash of its bytes), the CFile constructor and destructor, the virtual Close / Read / Write / Seek / GetSize
(on the Mac through a vtable written at 0x102a4d0c whose entries point at stubbed code; on the host through a recording CMemFile derived
class copied into the slots), SAVEVAR / LOADVAR of int and unsigned int, LbFile_Read / LbFile_Write, ResetActionRecording,
ActivateActionRecording and SaveRecordingToDisk. Read and LbFile_Read fill their buffer with a pattern of the length; Write and
LbFile_Write record a hash of the bytes. operator new[] / delete[] are the real ones on both sides.

The world of a case: the recorder's words (mState, mMode, mF08, mRecording and the three others, and the 232 table words, random), the
files holding the recording vtable (except for the constructor), the queues of answers. What is compared: the answer, every word of the
object, whether each file slot holds a vtable and an access object, and the event log.

No fused operations. Not tested: the destructor with a positive delete flag (the original calls operator delete on the object, the host
destructor has no such parameter) and a NULL this (the host has none).

With the port fixes (tpw_ar_portfix answers 1; port_fix tag parkload) a load compares the recording size with the bytes the file has
left (the size less the position, the queues PortSize and PortPos, which the original never reads) and with the count the read gave
(the queue Got): a size larger than what is left answers 1 right after the size is read, and a short read answers 1 right after
LbFile_Read. The original's side is cut there (its answer 1, its log up to that event; nothing before it touches the object), and in
the first case it runs with a recording size of 0 (only the port's cases ask for more than the file holds, up to 0xffffffff).
With the port fixes (port_fix tag loopcode) a load also puts back the actions the Mac's recorder wrote as 0 in the recording it read
(PortRepairOldPylonCodes, read with the table words): the original's Write of the recording is compared as the hash of the bytes
repair() below gives (the model of that function; the host's table words past the 232 are 0). ser_edges adds loads of recordings
built to hold such 0s.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CActionRecorder_CActionRecorder"
MASK = 0xFFFFFFFF

FN_CTOR, FN_DTOR, FN_SERIALISE, FN_RECORD = 0x10143658, 0x10143880, 0x101439dc, 0x1014430c
SIZES = {FN_CTOR: 348, FN_DTOR: 304, FN_SERIALISE: 664, FN_RECORD: 264}
OWN = set(SIZES)

VTABLE = 0x102A4D0C
SLOTS = [0xC, 0x10, 0x14, 0x18, 0x1C]          # Close, Read, Write, Seek, GetSize
AR_TEXT = b"AR - %d (%d, %d)\n"

# host offsets, checked against tpw_ar_layout: mState mMode mF08 mActionFile mTimingFile mCameraFile mRecording mReplayType mF2C mTurnsToNext mF34 sizeof
HOST_LAYOUT = [0, 4, 8, 16, 32, 48, 64, 68, 72, 76, 80, 1104]
HEAD = [0x0, 0x4, 0x8, 0x24, 0x28, 0x2C, 0x30]
MAC_SIZE = 0x3D4
TAIL_WORDS = 232
VT_BASE = None


def s32(v):
    return T.s32(v)


def hsh(data):
    h = 0
    for b in bytes(data):
        h = (h * 31 + b) & MASK
    return h


def pattern(n):
    return bytes(((i * 7) + n) & 0xFF for i in range(n))


OLD = -1


def repair(data, kinds):
    """With the port fixes (loopcode): the recording as PortRepairOldPylonCodes leaves it, read with the operand kinds given (a line for
    line model of it: the rules are in its comment)."""
    d = bytearray(data)
    n, ent, i = len(d), [], 0
    fixed = {0: 2, 2: 4, 3: 3, 4: 6, 5: 10, 6: 4, 7: 6, 8: 0, 11: 128}
    while i < n:
        b, at = d[i], i
        k = kinds[b] & MASK
        i += 1
        if k == 1:
            if i + 2 <= n and b == 0 and (d[i] | d[i + 1] << 8) > 0xFF:
                ent.append([at, OLD, -1, True])
                continue
            need = 2
        elif k == 9:
            need = 1 if i + 1 > n else 1 + 2 * d[i]
        elif k == 10:
            need = 3 if i + 3 > n else 3 + 2 * d[i + 2]
        else:
            need = fixed.get(k, 0)
        if i + need > n:
            break
        ent.append([at, b, (d[i] | d[i + 1] << 8) if (b == 0 and need == 2) else -1, False])
        i += need
    m = len(ent)
    code = [e[1] for e in ent]
    mode = [e[2] for e in ent]
    press, end = [False] * m, [False] * m

    def c(j):
        return code[j] if 0 <= j < m else -2

    def click(j):
        return c(j) in (0x14, 0x2D)

    def after_click(j):
        return click(j - 1) or (c(j - 1) == 0x44 and click(j - 2))

    def is_press(j):
        return press[j] or (c(j) in (0x37, 0x38) and c(j - 1) in (0x1E, 0x3F))

    def is_end(j):
        return end[j] or (c(j) == 0x37 and after_click(j))

    def is_type(j):
        return c(j) in (0x37, 0x38, 0x41)

    def stops(k):
        return is_type(k) or (c(k) == 0 and mode[k] != 0x15)

    def resumes(j):
        if c(j) != 0x41:
            return False
        undone = 1
        for k in range(j - 1, -1, -1):
            if click(k):
                undone -= 1
                if undone == 0:
                    return False
            elif c(k) == 0x41:
                undone += 1
            elif c(k) == 0x3F:
                return False
            elif undone == 1 and is_end(k):
                return True
        return False

    def ends_loop(j):
        clicks, stop, first, seen = 0, -1, -1, []
        for k in range(j - 1, -1, -1):
            if click(k):
                if first >= 0:
                    break
                clicks += 1
                if clicks > 2:
                    return False
            elif is_press(k):
                seen.append(k)
                if clicks == 2 and first < 0:
                    first = k
            elif stops(k):
                stop = k
                break
        resumed = stop >= 0 and resumes(stop)
        if clicks == 1 and resumed:
            first = seen[-1] if seen else -1
        elif not (clicks == 2 and not resumed and first >= 0):
            return False
        for k in seen:
            if press[k]:
                code[k] = 0x38
            if k == first:
                break
        return True

    def undid_loop_start(j):
        placed = False
        for k in range(j - 1, -1, -1):
            if click(k):
                if placed:
                    return
                placed = True
            elif is_press(k):
                if press[k]:
                    code[k] = 0x38
                if placed:
                    return
            elif stops(k):
                return

    j = 0
    while j < m:
        if code[j] == OLD:
            p = c(j - 1)
            if p in (0x1E, 0x3F):
                code[j], press[j] = 0x37, True
            elif c(j + 1) == OLD:
                code[j] = 0x41
                j += 1
                code[j] = 0x37
                undid_loop_start(j - 1)
            elif p == 0x41:
                code[j] = 0x37
            elif after_click(j) and ends_loop(j):
                code[j], end[j] = 0x37, True
            else:
                code[j] = 0x41
        j += 1
    for e, cd in zip(ent, code):
        if e[3]:
            d[e[0]] = cd
    return bytes(d)


def cbytes(e, addr):
    s = e.cstr(addr)
    return s if isinstance(s, (bytes, bytearray)) else s.encode("latin1")


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_ctrace(e):
    fmt = cbytes(e, e.r(4))
    args = (e.r(3) & MASK, hsh(fmt))
    if fmt == AR_TEXT:
        args += (e.r(5) & MASK, e.r(6) & MASK, e.r(7) & MASK)
    e.cl.rec("CTRACE", args)


def stub_dtor_file(e):
    e.cl.rec("CFile::~CFile", (e.cl.lab(e.r(3)), e.r(4) & MASK))


def mk_var(name, save, key):
    def stub(e):
        L = e.cl
        ptr = e.r(4)
        cur = struct.unpack(">I", e.read(ptr, 4))[0]
        L.rec(name, (L.lab(e.r(3)), cur, hsh(cbytes(e, e.r(5))), e.r(6) & MASK))
        if not save:
            e.write(ptr, struct.pack(">I", L.ret(key + "_v") & MASK))
        e.set_r(3, L.ret(key) & MASK)
    return stub


def stub_lb_read(e):
    L = e.cl
    n = e.r(6)
    L.rec("LbFile_Read", (L.lab(e.r(4)), n & MASK))
    e.write(e.r(5), pattern(n))
    e.write(e.r(3), struct.pack(">I", L.ret("LbFile_Read") & MASK))


def stub_lb_write(e):
    L = e.cl
    n = e.r(6)
    L.rec("LbFile_Write", (L.lab(e.r(4)), n & MASK, hsh(e.read(e.r(5), n))))
    e.write(e.r(3), struct.pack(">I", L.ret("LbFile_Write") & MASK))


def stub_reset(e):
    e.cl.rec("ResetActionRecording", (e.cl.lab(e.r(3)),))
    e.write(e.r(3), struct.pack(">I", 0x5A5A))


def stub_activate(e):
    e.cl.rec("ActivateActionRecording", (e.cl.lab(e.r(3)), e.r(4) & MASK))
    e.write(e.r(3) + 8, struct.pack(">I", 0x1234567))


def stub_save(e):
    e.cl.rec("SaveRecordingToDisk", (e.cl.lab(e.r(3)),))


def v_close(e):
    e.cl.rec("Close", (e.cl.lab(e.r(3)),))


def v_read(e):
    L = e.cl
    n = e.r(5)
    L.rec("Read", (L.lab(e.r(3)), n & MASK))
    e.write(e.r(4), pattern(n))
    e.set_r(3, L.ret("Read") & MASK)


def v_write(e):
    L = e.cl
    n = e.r(5)
    L.rec("Write", (L.lab(e.r(3)), n & MASK, hsh(e.read(e.r(4), n))))
    e.set_r(3, L.ret("Write") & MASK)


def v_seek(e):
    L = e.cl
    L.rec("Seek", (L.lab(e.r(3)), e.r(4) & MASK, e.r(5) & MASK))
    e.set_r(3, L.ret("Seek") & MASK)


def v_size(e):
    L = e.cl
    L.rec("GetSize", (L.lab(e.r(3)),))
    e.set_r(3, L.ret("GetSize") & MASK)


VFUNCS = [v_close, v_read, v_write, v_seek, v_size]

def glue(fn):
    """A glue import saves the caller's TOC at 0x14(r1) (the caller reloads r2 from there); an address stub skips the glue, so do it here."""
    def wrapped(e):
        e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2) & MASK))
        fn(e)
    return wrapped


STUBS.internal(0x10238F38, glue(CL.recorder("CFile::CFile", "N")))
STUBS.internal(0x10239868, glue(stub_dtor_file))
STUBS.internal(0x1006FCBC, stub_ctrace)
STUBS.internal(0x1000F4B0, mk_var("SAVEVAR<int>", True, "SAVEVAR_i"))
STUBS.internal(0x100F45BC, mk_var("SAVEVAR<uint>", True, "SAVEVAR_u"))
STUBS.internal(0x1000EF18, mk_var("LOADVAR<int>", False, "LOADVAR_i"))
STUBS.internal(0x100F44A4, mk_var("LOADVAR<uint>", False, "LOADVAR_u"))
STUBS.internal(0x10238848, glue(stub_lb_read))
STUBS.internal(0x10238878, glue(stub_lb_write))
STUBS.internal(0x10143E08, stub_reset)
STUBS.internal(0x10143CF0, stub_activate)
STUBS.internal(0x10143CAC, stub_save)


PORT_FIX = False


def setup(runner):
    global VT_BASE, PORT_FIX
    runner.lib.tpw_ar_portfix.restype, runner.lib.tpw_ar_portfix.argtypes = C.c_int32, []
    PORT_FIX = runner.lib.tpw_ar_portfix() == 1
    e = runner.emu
    T.install_std_stubs(e)
    VT_BASE = T.persist_alloc(e, 0x400)
    for k, fn in enumerate(VFUNCS):
        code = VT_BASE + 0x200 + 16 * k
        e.write(VT_BASE + 8 * k, struct.pack(">II", code, 0))
        STUBS.internal(code, fn)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]


class Ppc(Side):
    fmt = ">I"
    host = False

    def __init__(self, e, case):
        self.e = e
        self.obj = e.alloc(0x400)
        self.fileh = e.alloc(16)
        fixed = {self.obj: 1, self.fileh: 5}
        if case["op"] != "ctor":
            fixed.update({self.obj + 0xC: 2, self.obj + 0x14: 3, self.obj + 0x1C: 4})
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        for k, off in enumerate(SLOTS):
            e.write(VTABLE + off, struct.pack(">I", VT_BASE + 8 * k))
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def moff(self, m):
        return m

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def dress(self):
        for off in (0xC, 0x14, 0x1C):
            self.words(self.obj + off, [VTABLE, 0])

    def file_state(self, off):
        vt, impl = self.rd_words(self.obj + off, 2)
        return (vt != 0, impl != 0)

    def call(self, fn, args):
        r3, _, _ = T.call_ppc(self.e, fn, [a & MASK for a in args])
        return r3 & MASK


class Host(Side):
    fmt = "<I"
    host = True

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.obj = lib.tpw_ar_alloc(HOST_LAYOUT[-1])
        self.fileh = lib.tpw_ar_alloc(16)
        fixed = {1: self.obj, 5: self.fileh}
        if case["op"] != "ctor":
            fixed.update({2: self.obj + 16, 3: self.obj + 32, 4: self.obj + 48})
        CL.host_begin(lib, fixed, case["queues"])

    def done(self):
        pass

    def moff(self, m):
        if m < 0xC:
            return m
        if m < 0x24:
            return {0xC: 16, 0x14: 32, 0x1C: 48}[m]
        if m < 0x34:
            return 64 + (m - 0x24)
        return 80 + (m - 0x34)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def dress(self):
        self.lib.tpw_ar_prep(self.obj)

    def file_state(self, off):
        vt, impl = struct.unpack("<QQ", self.read(self.obj + self.moff(off), 16))
        return (vt != 0, impl != 0)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_ar_alloc.restype, lib.tpw_ar_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_ar_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ar_prep.argtypes = [C.c_uint64]
    lib.tpw_ar_ctor.argtypes = [C.c_uint64]
    lib.tpw_ar_dtor.argtypes = [C.c_uint64]
    lib.tpw_ar_serialise.restype = C.c_int32
    lib.tpw_ar_serialise.argtypes = [C.c_uint64, C.c_uint64, C.c_int32, C.c_int32, C.c_uint32]
    lib.tpw_ar_record.argtypes = [C.c_uint64, C.c_int32, C.c_uint16, C.c_uint16]
    out = (C.c_int64 * 32)()
    n = lib.tpw_ar_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    for m, w in case["head"].items():
        s.words(s.obj + s.moff(m), [w])
    s.words(s.obj + s.moff(0x34), case["tail"])
    if case["op"] != "ctor":
        s.dress()


def execute(s, case):
    op = case["op"]
    if op == "ctor":
        if s.host:
            s.lib.tpw_ar_ctor(s.obj)
        else:
            s.call(FN_CTOR, [s.obj])
        return 0
    if op == "dtor":
        if s.host:
            s.lib.tpw_ar_dtor(s.obj)
        else:
            s.call(FN_DTOR, [s.obj, case["flag"]])
        return 0
    if op == "ser":
        saving, version, unused = case["args"]
        if s.host:
            return s.lib.tpw_ar_serialise(s.obj, s.fileh, s32(saving), s32(version), unused & MASK) & MASK
        return s.call(FN_SERIALISE, [s.obj, s.fileh, saving, version, unused])
    action, a, b = case["args"]
    if s.host:
        s.lib.tpw_ar_record(s.obj, s32(action), a & 0xFFFF, b & 0xFFFF)
    else:
        s.call(FN_RECORD, [s.obj, action, a & 0xFFFF, b & 0xFFFF])
    return 0


def observe(s, case, answer):
    out = {"answer": answer, "log": s.log()}
    out["head"] = [s.rd_word(s.obj + s.moff(m)) for m in HEAD]
    out["tail"] = s.rd_words(s.obj + s.moff(0x34), TAIL_WORDS)
    out["files"] = [s.file_state(off) for off in (0xC, 0x14, 0x1C)]
    return out


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, execute(s, case))
    finally:
        s.done()


def port_stop(case):
    """With the port fixes: the event after which the port's load gives up (answering 1), else None."""
    if case["op"] != "ser":
        return None
    saving, version, _ = case["args"]
    q = case["queues"]
    if saving != 0 or not 0 <= s32(version) < 2 or q["LOADVAR_i"][0] != 0 or q["LOADVAR_u"][0] != 0:
        return None
    size = q["LOADVAR_u_v"][0] & MASK
    left = s32(q["PortSize"][0] - q["PortPos"][0])
    if left < 0 or size > left:
        return "LOADVAR<uint>"
    if s32(q["LbFile_Read"][0]) < 0:
        return None
    if q["Got"][0] & MASK != size:
        return "LbFile_Read"
    return None


def ppc(e, case):
    stop = port_stop(case) if PORT_FIX else None
    if stop == "LOADVAR<uint>":
        # nothing the original does after the size is read is kept, so it runs with an empty recording (not a 4 GB one)
        case = dict(case, queues=dict(case["queues"], LOADVAR_u_v=[0]))
    out = run(Ppc(e, case), case)
    if stop is not None:
        at = max(i for i, ev in enumerate(out["log"]) if ev[0] == stop)
        out = dict(out, answer=1, log=out["log"][:at + 1])
    elif PORT_FIX and case["op"] == "ser" and case["args"][0] == 0 and 0 <= s32(case["args"][1]) < 2:
        # a load: the recording it keeps is the one read (pattern) with the old pylon codes put back (loopcode)
        kinds = list(case["tail"]) + [0] * (256 - TAIL_WORDS)
        log = []
        for name, ints, texts in out["log"]:
            if name == "Write":
                ints = (ints[0], ints[1], hsh(repair(pattern(ints[1]), kinds)))
            log.append((name, ints, texts))
        out = dict(out, log=log)
    return out


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators


def r32(r):
    return r.choice([0, 1, 0xFFFFFFFF, 0x80000000, r.getrandbits(32), r.getrandbits(32)])


def queues(r, **over):
    q = {
        "Read": [r.choice([0, 5, -1])],
        "Write": [r.choice([0, 5, -1])],
        "Seek": [r.choice([0, 0, -1])],
        "GetSize": [r.choice([0, 1, 2, 5, 64, 100, 200])],
        "SAVEVAR_i": [r.choice([0, 0, 0, 1])],
        "SAVEVAR_u": [r.choice([0, 0, 0, 1])],
        "LOADVAR_i": [r.choice([0, 0, 0, 1])],
        "LOADVAR_i_v": [r32(r)],
        "LOADVAR_u": [r.choice([0, 0, 0, 1])],
        "LOADVAR_u_v": [r.choice([0, 1, 2, 7, 64, 150, 255, r.randrange(0, 300)])],
        "LbFile_Read": [r.choice([0, 0, 5, -1, -2])],
        "LbFile_Write": [r.choice([0, 0, 5, -1, -2])],
    }
    q.update(over)
    # the port's view of the file (read only with the port fixes): mostly room for the recording and all of it read
    size = q["LOADVAR_u_v"][0] & MASK
    if "PortSize" not in q:
        q["PortPos"] = [r.choice([0, 4, 0x200, r.randrange(0, 0x10000)])]
        q["PortSize"] = [(q["PortPos"][0] + r.choice([size, size, size + 1, size + 100, max(size - 1, 0), 0, -1])) & MASK]
    if "Got" not in q:
        q["Got"] = [r.choice([size, size, size, size, max(size - 1, 0), 0, size + 1]) & MASK]
    return q


def base(r, op, **over):
    head = {0x0: r.choice([0, 1, 2, r32(r)]), 0x4: r.choice([0, 0, 1, 2, 3]), 0x8: r32(r), 0x24: r.choice([0, 0, 1, r32(r)]),
            0x28: r32(r), 0x2C: r32(r), 0x30: r32(r)}
    return {"op": op, "queues": queues(r, **over), "head": head, "tail": [r32(r) for _ in range(TAIL_WORDS)]}


# ---- constructor
def ctor_case(r):
    return base(r, "ctor")


def ctor_edges():
    r = random.Random(11)
    for _ in range(4):
        yield ctor_case(r)
    c = ctor_case(r)
    c["tail"] = [0] * TAIL_WORDS
    c["head"] = {m: 0 for m in HEAD}
    yield c
    c = ctor_case(r)
    c["tail"] = [0xFFFFFFFF] * TAIL_WORDS
    yield c


# ---- destructor
def dtor_case(r, mode=None, flag=None):
    c = base(r, "dtor")
    if mode is not None:
        c["head"][0x4] = mode
    c["flag"] = r.choice([0, 0, -1, -5, 0x8000, 0xFFFF8000]) & MASK if flag is None else flag & MASK
    return c


def dtor_edges():
    r = random.Random(12)
    for mode in (0, 1, 2, 0x80000000, 0xFFFFFFFF):
        for flag in (0, -1, 0x8000):
            yield dtor_case(r, mode, flag)


# ---- Serialise
def ser_case(r, args=None, **over):
    c = base(r, "ser", **over)
    c["args"] = args if args is not None else [r.choice([0, 1, 1, -1, 7, r32(r)]), r.choice([-2, -1, 0, 0, 1, 1, 2, 3, 100]),
                                                r32(r)]
    return c


def ser_edges():
    r = random.Random(13)
    variants = [
        dict(SAVEVAR_i=[0], SAVEVAR_u=[0], LOADVAR_i=[0], LOADVAR_u=[0], LbFile_Read=[0], LbFile_Write=[0]),
        dict(SAVEVAR_i=[1], LOADVAR_i=[1]),
        dict(SAVEVAR_i=[0], SAVEVAR_u=[1], LOADVAR_i=[0], LOADVAR_u=[1]),
        dict(SAVEVAR_i=[0], SAVEVAR_u=[0], LOADVAR_i=[0], LOADVAR_u=[0], LbFile_Read=[-1], LbFile_Write=[-1]),
        dict(SAVEVAR_i=[0], SAVEVAR_u=[0], LOADVAR_i=[0], LOADVAR_u=[0], GetSize=[0], LOADVAR_u_v=[0], LbFile_Read=[0], LbFile_Write=[0]),
        dict(SAVEVAR_i=[0], SAVEVAR_u=[0], LOADVAR_i=[0], LOADVAR_u=[0], GetSize=[130], LOADVAR_u_v=[130], LbFile_Read=[0], LbFile_Write=[0]),
    ]
    for saving in (1, 0, -1, 7):
        for version in (-2, -1, 0, 1, 2, 3):
            for v in variants:
                yield ser_case(r, [saving, version, r32(r)], **v)
    # a load's recording size against the bytes the file has left, and a short read (what the port fixes change)
    ok = dict(LOADVAR_i=[0], LOADVAR_u=[0], LbFile_Read=[1])
    for size in (0, 1, 130, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF) if PORT_FIX else (0, 1, 130):
        for pos, end in ((0, 0), (0, 130), (10, 140), (10, 139), (100, 50), (0, 0x7FFFFFFF), (0x7FFFFFFF, 0x80000000)):
            if size > 0x100000 and end - pos >= size:
                continue   # the original would really allocate and copy it
            for got in {size, max(size - 1, 0), 0}:
                yield ser_case(r, [0, 1, 0], LOADVAR_u_v=[size], PortPos=[pos & MASK], PortSize=[end & MASK], Got=[got & MASK], **ok)
    # a load of a recording that holds 0s the Mac's recorder wrote for its actions without operands (what loopcode changes): each
    # byte an action without operands but the 0 (a halfword, so the 0 before the pattern's next two bytes is such a 0)
    for size in (1, 2, 3, 37, 130, 256, 600, 2000):
        for kind0 in (1, 0, 8):
            c = ser_case(r, [0, r.choice([0, 1]), 0], LOADVAR_u_v=[size], PortPos=[0], PortSize=[size], Got=[size], **ok)
            c["tail"] = [kind0] + [r.choice([8, 8, 8, 12, 0xFFFFFFFF]) for _ in range(TAIL_WORDS - 1)]
            yield c


# ---- RecordAction
def rec_case(r, args=None, state=None, recording=None):
    c = base(r, "rec")
    if state is not None:
        c["head"][0x0] = state
    else:
        c["head"][0x0] = r.choice([0, 1, 1, 1, 2])
    c["head"][0x24] = r.choice([0, 0, 0, 1]) if recording is None else recording
    c["args"] = args if args is not None else [r.choice([0, 1, 0xE7, 0xFF, 0x100, 0x1FF, 0xFFFFFFFF, 0x80000000, r.randrange(0, 256), r32(r)]),
                                              r.choice([0, 1, 0xFF, 0x100, 0x8000, 0xFFFF, r.randrange(0, 65536)]),
                                              r.choice([0, 1, 0xFF, 0x100, 0x8000, 0xFFFF, r.randrange(0, 65536)])]
    return c


def rec_edges():
    r = random.Random(14)
    for state in (0, 1, 2):
        for recording in (0, 1, 0x100):
            yield rec_case(r, [0x43, 0x1234, 0xABCD], state, recording)
    yield rec_case(r, [0, 0, 0], 1, 0)
    yield rec_case(r, [0xFFFFFFFF, 0xFFFF, 0xFFFF], 1, 0)
    yield rec_case(r, [0x123, 0x00FF, 0xFF00], 1, 0)


TESTS = [
    FnTest("CActionRecorder::CActionRecorder", ppc, host, edge=ctor_edges, rnd=ctor_case),
    FnTest("CActionRecorder::~CActionRecorder", ppc, host, edge=dtor_edges, rnd=dtor_case),
    FnTest("CActionRecorder::Serialise", ppc, host, edge=ser_edges, rnd=ser_case),
    FnTest("CActionRecorder::RecordAction", ppc, host, edge=rec_edges, rnd=rec_case),
]
