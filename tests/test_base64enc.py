"""Unit v51015: base64enc, Email::Attachment::Attachment, Connection::Connection, Connection::SendCommandLine and CUIGrapher::DrawAxes,
against libtpw_unit_base64enc.dylib (tests/unit_base64enc.cpp), main app ("SimTheme Park"), src/game/online/Connection.cpp, src/game/online/EmailMessage.cpp, src/game/ui/UIGrapher.cpp:
  base64enc 0x10202114, Email::Attachment::Attachment 0x10202670, Connection::SendCommandLine 0x10201b14,
  Connection::Connection 0x10201a84, CUIGrapher::DrawAxes 0x101fda4c

A case is a world and a call, built by ONE piece of Python code in the emulator's memory (big-endian) and in the host library's memory
(little-endian). The callees outside the unit are fakes on both sides that record their arguments: the string constructor and assign
(offset in the object, length and a text hash), fopen/fseek/ftell/fread/fclose, new[] and delete[] (block ids), CSock::CSock,
CSock::SendData (this, text, count) and the two LbDraw lines (the colour as the two registers it travels in: four bytes and the index).

What is compared: the call log, the whole object (Attachment: the two words and the base64 block mData points to; Connection: all 0x424 bytes;
CUIGrapher: the 0x3a bytes) and the output buffer of base64enc (guard bytes included). No fused operations. Not tested: a negative file
size (the original then writes before the block), a size near 2^31 (the padding loop overflows).
"""
import ctypes as C
import random

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_base64enc"
MASK = 0xFFFFFFFF

FN_B64, FN_ATT, FN_SEND, FN_CONN, FN_AXES = 0x10202114, 0x10202670, 0x10201b14, 0x10201a84, 0x101fda4c
SIZES = {FN_B64: 264, FN_ATT: 496, FN_SEND: 108, FN_CONN: 104, FN_AXES: 164}

HOST_LAYOUT = [8, 0x20, 0x420, 4, 8, 0xC, 0x10, 0x14, 0x1C, 0x24, 0x30, 0x35]

STUBS = CL.Stubs()
STUBS.real.update(SIZES)

IDS = {}
CUR = {"obj": 0}


def s32(v):
    return T.s32(v)


def raw(x):
    return x.encode("latin1") if isinstance(x, str) else bytes(x)


def hsh(b):
    h = 0
    for i, c in enumerate(raw(b)):
        h = (h + (i + 1) * c) & MASK
    return h


# ------------------------------------------------------------------------------------------------------------ the PowerPC fakes


def st_strlen(e):
    e.set_r(3, len(raw(e.cstr(e.r(3)))))


def st_memset(e):
    dest, val, n = e.r(3), e.r(4) & 0xFF, e.r(5)
    e.write(dest, bytes([val]) * n)


def st_string_init(e):
    e.cl.rec("StringInit", (s32(e.r(3) - CUR["obj"]),))


def st_string_assign(e):
    e.cl.rec("StringAssign", (s32(e.r(3) - CUR["obj"]), s32(e.r(5)), hsh(e.cstr(e.r(4)))))


def st_fopen(e):
    L = e.cl
    L.rec("Fopen", (hsh(e.cstr(e.r(3))), hsh(e.cstr(e.r(4)))))
    e.set_r(3, L.ret("Fopen") & MASK)


def st_fseek(e):
    L = e.cl
    L.rec("Fseek", (s32(e.r(3)), s32(e.r(4)), s32(e.r(5))))
    e.set_r(3, L.ret("Fseek") & MASK)


def st_ftell(e):
    L = e.cl
    L.rec("Ftell", (s32(e.r(3)),))
    e.set_r(3, L.ret("Ftell") & MASK)


def st_new(e):
    L = e.cl
    size = e.r(3) & MASK
    L.rec("NewArray", (size,))
    if L.ret("NewArray"):
        e.set_r(3, 0)
        return
    n = max(size, 1)
    a = e.alloc(n)
    e.write(a, bytes(n))
    IDS[a] = len(IDS) + 1
    e.set_r(3, a)


def st_fread(e):
    buf, size, count, f = e.r(3), e.r(4) & MASK, e.r(5) & MASK, e.r(6)
    e.cl.rec("Fread", (size, count, s32(f)))
    e.write(buf, bytes((i * 37 + 11) & 0xFF for i in range(size * count)))
    e.set_r(3, count)


def st_fclose(e):
    e.cl.rec("Fclose", (s32(e.r(3)),))


def st_delete(e):
    e.cl.rec("DeleteArray", (IDS.get(e.r(3), 0),))


def st_line(name):
    def fn(e):
        e.cl.rec(name, (s32(e.r(3)), s32(e.r(4)), s32(e.r(5)), e.r(6) & MASK, (e.r(7) >> 24) & 0xFF))
    return fn


def glue(fn):
    """An import reached through glue: the glue saves the caller's TOC (r2) in the linkage slot 0x14(r1), the code reloads it after."""
    def wrapped(e):
        e.write(e.r(1) + 0x14, (e.r(2) & MASK).to_bytes(4, "big"))
        fn(e)
    return wrapped


STUBS.internal(0x10238650, glue(st_strlen))
STUBS.internal(0x10238560, glue(st_memset))
STUBS.internal(0x1007363C, st_string_init)
STUBS.internal(0x101B2440, st_string_assign)
STUBS.internal(0x102389B0, glue(st_fopen))
STUBS.internal(0x102388F0, glue(st_fseek))
STUBS.internal(0x10238908, glue(st_ftell))
STUBS.internal(0x10238668, glue(st_new))
STUBS.internal(0x10239508, glue(st_fread))
STUBS.internal(0x102388C0, glue(st_fclose))
STUBS.internal(0x10238638, glue(st_delete))
STUBS.internal(0x10201F50, CL.recorder("CSockCtor", "L"))
STUBS.internal(0x10202058, CL.recorder("SendData", "Lsi", ret="i"))
STUBS.internal(0x1023ADE0, glue(st_line("HLine")))
STUBS.internal(0x1023ADC8, glue(st_line("VLine")))


def setup(runner):
    pass


# ------------------------------------------------------------------------------------------------------------ the two sides


class Side:
    def words(self, a, ws):
        import struct
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        import struct
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def cstr_alloc(self, data):
        a = self.alloc(len(data) + 1)
        self.write(a, bytes(data) + b"\0")
        return a


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        IDS.clear()
        CUR["obj"] = 0
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def deref(self, off):
        return off

    def log(self):
        return self.L.log

    def b64(self, out, inp, n):
        T.call_ppc(self.e, FN_B64, [out, inp, n & MASK])

    def attach(self, obj, path):
        CUR["obj"] = obj
        T.call_ppc(self.e, FN_ATT, [obj, path])

    def conn(self, obj):
        T.call_ppc(self.e, FN_CONN, [obj])

    def send(self, obj, line):
        T.call_ppc(self.e, FN_SEND, [obj, line])

    def axes(self, obj):
        T.call_ppc(self.e, FN_AXES, [obj])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        lib.tpw_b64_reset()
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_b64_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def deref(self, off):
        return self.lib.tpw_b64_deref(off)

    def log(self):
        return CL.host_end(self.lib)

    def b64(self, out, inp, n):
        self.lib.tpw_b64_base64enc(out, inp, s32(n))

    def attach(self, obj, path):
        self.lib.tpw_b64_attach(obj, path)

    def conn(self, obj):
        self.lib.tpw_b64_conn(obj)

    def send(self, obj, line):
        self.lib.tpw_b64_send(obj, line)

    def axes(self, obj):
        self.lib.tpw_b64_axes(obj)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_b64_alloc.restype, lib.tpw_b64_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_b64_deref.restype, lib.tpw_b64_deref.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_b64_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_b64_base64enc.argtypes = [C.c_uint64, C.c_uint64, C.c_int32]
    lib.tpw_b64_attach.argtypes = [C.c_uint64, C.c_uint64]
    lib.tpw_b64_conn.argtypes = [C.c_uint64]
    lib.tpw_b64_send.argtypes = [C.c_uint64, C.c_uint64]
    lib.tpw_b64_axes.argtypes = [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_b64_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ------------------------------------------------------------------------------------------------------------ the worlds


def out_size(n):
    groups = (max(n, 0) + 2) // 3
    chars = 4 * groups
    return chars + 2 * (chars // 76 + 1) + 8


def op_b64(s, case):
    n = case["n"]
    osz = out_size(n)
    out = s.alloc(osz)
    s.write(out, b"\xAA" * osz)
    data = case["data"]
    inp = s.alloc(len(data) + 1)
    s.write(inp, data)
    s.b64(0 if case["outnull"] else out, 0 if case["innull"] else inp, n)
    return {"out": s.read(out, osz), "in": s.read(inp, len(data))}


def op_attach(s, case):
    obj = s.alloc(16)
    s.words(obj, case["init"])
    path = s.cstr_alloc(case["path"])
    s.attach(obj, path)
    w = s.rd_words(obj, 3)
    res = {"w0": w[0], "w1": w[1], "present": w[2] != 0, "log": s.log()}
    if w[2]:
        n = case["queues"]["Ftell"][0]
        pad = n + (-n) % 3
        chars = 4 * pad // 3
        chars += (chars // 76) * 2
        res["data"] = s.read(s.deref(w[2]), chars + 1)
    return res


def op_conn(s, case):
    obj = s.alloc(0x430)
    s.words(obj, case["init"])
    s.conn(obj)
    return {"words": s.rd_words(obj, 0x109), "log": s.log()}


def op_send(s, case):
    obj = s.alloc(0x430)
    s.words(obj, case["init"])
    line = s.cstr_alloc(case["line"])
    s.send(obj, line)
    return {"words": s.rd_words(obj, 0x109), "log": s.log(), "line": s.read(line, len(case["line"]) + 1)}


def op_axes(s, case):
    obj = s.alloc(0x40)
    s.words(obj, case["ints"])
    s.write(obj + 0x30, bytes(case["colours"]))
    s.axes(obj)
    return {"words": s.rd_words(obj, 12), "colours": s.read(obj + 0x30, 10), "log": s.log()}


OPS = {"b64": op_b64, "attach": op_attach, "conn": op_conn, "send": op_send, "axes": op_axes}


def run(s, case):
    try:
        return OPS[case["op"]](s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ------------------------------------------------------------------------------------------------------------ the cases


def rw(r, n):
    return [r.getrandbits(32) for _ in range(n)]


def text(r, top=40):
    return bytes(r.choice(b"abcdefXYZ019 /+=") for _ in range(r.randrange(0, top)))


# ---- base64enc
def b64_case(r, n=None, outnull=False, innull=False):
    n = r.choice([0, 1, 2, 3, 4, 5, 56, 57, 58, 100, 171, 300]) if n is None else n
    data = bytes(r.getrandbits(8) for _ in range(max(n, 0) + 2))
    return {"op": "b64", "queues": {}, "n": n, "data": data, "outnull": outnull, "innull": innull}


def b64_edges():
    r = random.Random(11)
    for n in (-1, 0, 1, 2, 3, 4, 5, 6, 56, 57, 58, 59, 113, 114, 115, 171, 172, 228, 229, 230):
        yield b64_case(r, n)
    yield b64_case(r, 10, outnull=True)
    yield b64_case(r, 10, innull=True)
    yield b64_case(r, 0, outnull=True, innull=True)


def b64_random(r):
    return b64_case(r, r.randrange(-2, 400) if r.random() < 0.7 else None, r.random() < 0.05, r.random() < 0.05)


# ---- Attachment
def att_case(r, n=None, fopen=None, flags=None):
    n = r.choice([0, 1, 2, 3, 56, 57, 58, 100, 114, 170]) if n is None else n
    fh = r.choice([0, 0x1234, 7]) if fopen is None else fopen
    flags = [r.choice([0, 0, 0, 1]), r.choice([0, 0, 0, 1])] if flags is None else flags
    q = {"Fopen": [fh], "Ftell": [n], "NewArray": flags, "Fseek": [r.choice([0, 1])]}
    return {"op": "attach", "queues": q, "init": rw(r, 4), "path": text(r)}


def att_edges():
    r = random.Random(12)
    yield att_case(r, 5, 0)
    for n in (0, 1, 2, 3, 4, 56, 57, 58, 113, 114, 115, 170, 171):
        yield att_case(r, n, 0x1234, [0, 0])
    yield att_case(r, 10, 0x1234, [1, 0])
    yield att_case(r, 10, 0x1234, [0, 1])
    yield att_case(r, 10, 0x1234, [1, 1])


def att_random(r):
    return att_case(r, r.randrange(0, 300) if r.random() < 0.8 else None)


# ---- Connection
def conn_case(r):
    return {"op": "conn", "queues": {}, "init": rw(r, 0x109)}


def conn_edges():
    r = random.Random(13)
    yield conn_case(r)
    c = conn_case(r)
    c["init"] = [0xFFFFFFFF] * 0x109
    yield c


def send_case(r, line=None, total=None):
    c = {"op": "send", "queues": {"SendData": [r.choice([0, 1])]}, "init": rw(r, 0x109),
         "line": text(r) if line is None else line}
    if total is not None:
        c["init"][6] = total
    return c


def send_edges():
    r = random.Random(14)
    for line, total in ((b"", 0), (b"a", 0), (b"USER x\r\n", 5), (b"abc", 0x7FFFFFFF), (b"abc", 0xFFFFFFFD), (b"hello world", 0xFFFFFFF0)):
        yield send_case(r, line, total)


def send_random(r):
    return send_case(r)


# ---- DrawAxes
def axes_case(r, f1c=None, f24=None):
    ints = [r.randrange(-60, 60) & MASK for _ in range(12)]
    ints[1] = r.randrange(-100, 100) & MASK
    if f1c is not None:
        ints[7] = f1c & MASK
    if f24 is not None:
        ints[9] = f24 & MASK
    return {"op": "axes", "queues": {}, "ints": ints, "colours": [r.getrandbits(8) for _ in range(10)]}


def axes_edges():
    r = random.Random(15)
    for f1c in (-7, -1, 0, 1, 5):
        for f24 in (-20, -8, -7, -1, 0, 1, 6, 30):
            yield axes_case(r, f1c, f24)
    c = axes_case(r, -2, 100)
    c["ints"][1] = 0x7FFFFFFF
    yield c


def axes_random(r):
    return axes_case(r)


TESTS = [
    FnTest("base64enc", ppc, host, edge=b64_edges, rnd=b64_random),
    FnTest("Email::Attachment::Attachment", ppc, host, edge=att_edges, rnd=att_random),
    FnTest("Connection::SendCommandLine", ppc, host, edge=send_edges, rnd=send_random),
    FnTest("Connection::Connection", ppc, host, edge=conn_edges, rnd=conn_case),
    FnTest("CUIGrapher::DrawAxes", ppc, host, edge=axes_edges, rnd=axes_random),
]
