"""Unit v51014: TpBody::Load 0x1002e6ec, TpBody::Save 0x1002e57c, the Sound Sprockets command constructors (SpeakerType 0x1002d7cc,
Volume 0x1002cdcc, ListenerScale 0x1002b788, CPULoad 0x1002b700, SoundType 0x1002b158) and UReferenceCounted::RemoveReference 0x1002af28,
against libtpw_unit_TpBody_Load.dylib (tests/unit_TpBody_Load.cpp), src/sound/CSprocketsModule.cpp, src/sound/SprocketsChannel.cpp, src/sound/TpBody.cpp.

A case is a world and a call, built on both sides: a body (list pointer at +0x28), a fake file whose Read/Write/Seek (the glue
0x1002f154 / 0x1002f07c on the Mac, told apart by the vtable slot in r12) record their arguments, a fake list (Load/Save record),
TbSoundPosition::Get (answers three words from the case) and UCommand's constructor (records). Compared: the answer, the event
log, the fields the constructors store (vtable word and values at the Mac offsets), the reference count after RemoveReference.

No fused operations. Not tested: nothing excluded.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "sound symbols shared"
SHIM_LIBRARY = "unit_TpBody_Load"
MASK = 0xFFFFFFFF

F_LOAD, F_SAVE = 0x1002E6EC, 0x1002E57C
F_SPEAKER, F_VOLUME, F_SCALE, F_CPU, F_STYPE, F_REMOVE = 0x1002D7CC, 0x1002CDCC, 0x1002B788, 0x1002B700, 0x1002B158, 0x1002AF28
SIZES = {F_LOAD: 328, F_SAVE: 264, F_SPEAKER: 88, F_VOLUME: 76, F_SCALE: 104, F_CPU: 72, F_STYPE: 72, F_REMOVE: 120}
OWN = set(SIZES)

GLUE_CALL, GLUE_WRITE, GET, LIST_LOAD, LIST_SAVE, UCMD = 0x1002F154, 0x1002F07C, 0x10015F3C, 0x1001DA8C, 0x1001D624, 0x1002A7E8
HOST_LAYOUT = [128]

TAG = {8: 0xA008, 0xC: 0xA00C, 0x10: 0xA010, 0x18: 0xA018}
VTABLE = {"speaker": 0x1003DCE0, "volume": 0x1003DD7C, "scale": 0x1003DE58, "cpu": 0x1003DE80, "stype": 0x1003DFB4}
# fields stored by each constructor: (offset, kind) with I a word, Q a double's bits
FIELDS = {"speaker": [(4, "I"), (8, "I"), (0xC, "I")], "volume": [(4, "I"), (8, "Q"), (0x10, "Q")],
          "scale": [(4, "I"), (8, "Q"), (0x10, "Q"), (0x18, "Q")], "cpu": [(4, "I"), (8, "Q")], "stype": [(4, "I"), (8, "I")]}


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def save_r2(e):
    e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2) & MASK))


def glue_call(e):
    L = e.cl
    tag = e.r(12) & MASK
    r3 = e.r(3)
    if tag == 0xA010:
        L.rec("Read", (L.lab(r3), e.r(5) & MASK))
        e.write(e.r(4), struct.pack("<I", L.ret("rd") & MASK))
        e.set_r(3, 4)
    elif tag == 0xA018:
        L.rec("Seek", (L.lab(r3), e.r(4) & MASK, e.r(5) & MASK))
        e.set_r(3, 0)
    elif tag == 0xA00C:
        L.rec("Dispose", (L.lab(r3), e.u32(r3 + 4)))
    elif tag == 0xA008:
        L.rec("Delete", (L.lab(r3), e.r(4) & MASK))
        if (e.r(4) & MASK) > 0:       # the deleting destructor frees the object through operator delete
            L.rec("delete", (L.lab(r3),))
    else:
        raise AssertionError("glue tag %x" % tag)
    save_r2(e)


def glue_write(e):
    L = e.cl
    b = e.read(e.r(5), 4)
    L.rec("Write", (L.lab(e.r(4)), e.r(6) & MASK, b[0], b[1], b[2], b[3]))
    e.set_r(3, 4)
    save_r2(e)


def stub_get(e):
    L = e.cl
    L.rec("Get", (L.lab(e.r(3)),))
    for reg, key in ((4, "getx"), (5, "gety"), (6, "getz")):
        e.write(e.r(reg), struct.pack(">I", L.ret(key) & MASK))


def stub_list_load(e):
    L = e.cl
    L.rec("ListLoad", (L.lab(e.r(4)), L.lab(e.r(5)), 1 if (e.r(6) & 0xFF) else 0))


def stub_list_save(e):
    L = e.cl
    L.rec("ListSave", (L.lab(e.r(4)), L.lab(e.r(5))))


def stub_ucmd(e):
    L = e.cl
    L.rec("UCommand", (L.lab(e.r(3)),))


STUBS.internal(GLUE_CALL, glue_call)
STUBS.internal(GLUE_WRITE, glue_write)
STUBS.internal(GET, stub_get)
STUBS.internal(LIST_LOAD, stub_list_load)
STUBS.internal(LIST_SAVE, stub_list_save)
STUBS.internal(UCMD, stub_ucmd)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Ppc:
    fmt = ">I"
    E = ">"

    def __init__(self, e, case):
        self.e = e
        vt = e.alloc(0x40)
        for off, tag in TAG.items():
            e.write(vt + off, struct.pack(">I", tag))
        self.file = e.alloc(8)
        e.write(self.file, struct.pack(">I", vt))
        self.list = e.alloc(16)
        self.body = e.alloc(0x40)
        e.write(self.body + 0x28, struct.pack(">I", self.list if case.get("has_list") else 0))
        self.pos = self.body + 0x34
        self.cmd = e.alloc(0x80)
        self.ref = e.alloc(16)
        e.write(self.ref, struct.pack(">I", vt))
        e.write(self.ref + 4, struct.pack(">I", case.get("count", 0) & MASK))
        self.L = e.cl = CL.PpcLog({self.file: 1, self.list: 2, self.body: 3, self.pos: 4, self.cmd: 5, self.ref: 6}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def read(self, a, n):
        return self.e.read(a, n)

    def log(self):
        return self.L.log

    def save(self):
        buf = self.e.alloc(4)
        T.call_ppc(self.e, F_SAVE, [buf, self.body, self.file])
        return self.e.s32(buf)

    def load(self, flag):
        buf = self.e.alloc(4)
        T.call_ppc(self.e, F_LOAD, [buf, self.body, self.file, flag])
        return self.e.s32(buf)

    def ctor(self, kind, args):
        if kind == "speaker":
            T.call_ppc(self.e, F_SPEAKER, [self.cmd, args[0] & MASK, args[1] & MASK])
        elif kind == "stype":
            T.call_ppc(self.e, F_STYPE, [self.cmd, args[0] & MASK])
        else:
            fn = {"volume": F_VOLUME, "scale": F_SCALE, "cpu": F_CPU}[kind]
            T.call_ppc(self.e, fn, [self.cmd] + [T.D(a) for a in args])

    def remove(self):
        r3, _, _ = T.call_ppc(self.e, F_REMOVE, [self.ref])
        return r3 & MASK

    def refcount(self):
        return self.e.u32(self.ref + 4)


class Host:
    fmt = "<I"
    E = "<"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.file = lib.tpw_tb_addr(b"file")
        self.list = lib.tpw_tb_addr(b"list")
        self.body = lib.tpw_tb_body(self.list if case.get("has_list") else 0)
        self.pos = lib.tpw_tb_bodypos(self.body)
        self.cmd = lib.tpw_tb_alloc(0x80)
        self.ref = lib.tpw_tb_newref(case.get("count", 0) & MASK) if case["op"] == "remove" else 0
        fixed = {1: self.file, 2: self.list, 3: self.body, 4: self.pos, 5: self.cmd}
        if self.ref:
            fixed[6] = self.ref
        CL.host_begin(lib, fixed, case["queues"])

    def done(self):
        pass

    def read(self, a, n):
        return C.string_at(a, n)

    def log(self):
        return CL.host_end(self.lib)

    def save(self):
        return self.lib.tpw_tb_save(self.body, self.file)

    def load(self, flag):
        return self.lib.tpw_tb_load(self.body, self.file, flag)

    def ctor(self, kind, args):
        l = self.lib
        if kind == "speaker":
            l.tpw_tb_speaker(self.cmd, s32(args[0]), s32(args[1]))
        elif kind == "stype":
            l.tpw_tb_stype(self.cmd, s32(args[0]))
        elif kind == "volume":
            l.tpw_tb_volume(self.cmd, args[0])
        elif kind == "scale":
            l.tpw_tb_scale(self.cmd, *args)
        else:
            l.tpw_tb_cpu(self.cmd, args[0])

    def remove(self):
        return self.lib.tpw_tb_remove(self.ref) & MASK

    def refcount(self):
        return self.lib.tpw_tb_refcount(self.ref) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    for n, res, args in (
            ("addr", C.c_uint64, [C.c_char_p]), ("alloc", C.c_uint64, [C.c_uint32]), ("body", C.c_uint64, [C.c_uint64]),
            ("bodypos", C.c_uint64, [C.c_uint64]), ("save", C.c_int32, [C.c_uint64, C.c_uint64]),
            ("load", C.c_int32, [C.c_uint64, C.c_uint64, C.c_uint32]),
            ("speaker", None, [C.c_uint64, C.c_int32, C.c_int32]), ("volume", None, [C.c_uint64, C.c_double]),
            ("scale", None, [C.c_uint64, C.c_double, C.c_double, C.c_double]), ("cpu", None, [C.c_uint64, C.c_double]),
            ("stype", None, [C.c_uint64, C.c_int32]), ("newref", C.c_uint64, [C.c_uint32]),
            ("remove", C.c_uint32, [C.c_uint64]), ("refcount", C.c_uint32, [C.c_uint64])):
        f = getattr(lib, "tpw_tb_" + n)
        f.restype, f.argtypes = res, args
    lib.tpw_tb_layout.argtypes = [C.POINTER(C.c_int64)]
    out = (C.c_int64 * 32)()
    n = lib.tpw_tb_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        op = case["op"]
        if op == "save":
            res = {"answer": s.save()}
        elif op == "load":
            res = {"answer": s.load(case["flag"])}
        elif op == "remove":
            res = {"answer": s.remove()}
        else:
            s.ctor(op, case["args"])
            raw = s.read(s.cmd, 0x80)
            fields = []
            used = set()
            for off, kind in FIELDS[op]:
                n = 4 if kind == "I" else 8
                fields.append(struct.unpack(s.E + kind, raw[off:off + n])[0])
                used.update(range(off, off + n))
            res = {"fields": fields, "tail": bytes(b for i, b in enumerate(raw) if i not in used)}
        res["log"] = s.log()
        if op == "remove" and case["count"] != 1:
            res["count"] = s.refcount()
        return res
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


SPECIAL = [0, 0x80000000, 0x7FC00000, 0x7F800000, 0xFF800000, 1, 0x3F800000, 0xBF800000, 0x7F7FFFFF, 0x00800000]
DSPECIAL = [0.0, -0.0, 1.0, -1.0, 0.5, 1e300, -1e300, 5e-324, float("inf"), float("-inf"), 255.0, 100.0]


def rf(r):
    k = r.random()
    if k < 0.1:
        return r.choice(SPECIAL)
    return struct.unpack(">I", struct.pack(">f", r.uniform(-300, 300) * 10 ** r.uniform(-6, 6)))[0]


def rd(r):
    if r.random() < 0.15:
        return r.choice(DSPECIAL)
    return r.uniform(-1000, 1000) * 10 ** r.uniform(-10, 10)


def rw(r):
    return r.choice([0, 1, 4, 5, 0xFFFFFFFF, 0x80000000, r.randrange(0, 1 << 32), r.randrange(0, 1000)])


def base(r, op, has_list=None):
    return {"op": op, "has_list": r.choice([0, 1]) if has_list is None else has_list,
            "queues": {"rd": [rw(r), rw(r), rw(r), rw(r)], "getx": [rf(r)], "gety": [rf(r)], "getz": [rf(r)]}}


def save_case(r, has_list=None):
    return base(r, "save", has_list)


def load_case(r, has_list=None, flag=None):
    c = base(r, "load", has_list)
    c["flag"] = r.choice([0, 1, 2, 255, r.randrange(256)]) if flag is None else flag
    return c


def ctor_case(kind):
    n = {"speaker": 2, "stype": 1, "volume": 1, "scale": 3, "cpu": 1}[kind]

    def make(r, args=None):
        c = base(r, kind)
        if args is not None:
            c["args"] = args
        elif kind in ("speaker", "stype"):
            c["args"] = [r.choice([0, 1, -1, 2, 0x7FFFFFFF, -0x80000000, r.randrange(-1000, 1000)]) for _ in range(n)]
        else:
            c["args"] = [rd(r) for _ in range(n)]
        return c
    return make


def ctor_edges(kind):
    def gen():
        r = random.Random(hash(kind) & 0xFFFF if False else len(kind) * 7)
        mk = ctor_case(kind)
        if kind in ("speaker", "stype"):
            n = 2 if kind == "speaker" else 1
            for v in (0, 1, -1, 0x7FFFFFFF, -0x80000000):
                yield mk(r, [v] * n)
            if n == 2:
                yield mk(r, [3, -5])
        else:
            n = {"volume": 1, "scale": 3, "cpu": 1}[kind]
            for v in DSPECIAL:
                yield mk(r, [v] * n)
            if n == 3:
                yield mk(r, [1.0, 2.0, 3.0])
    return gen


def ctor_rnd(kind):
    mk = ctor_case(kind)
    return lambda r: mk(r)


def save_edges():
    r = random.Random(11)
    for hl in (0, 1):
        for _ in range(2):
            yield save_case(r, hl)


def load_edges():
    r = random.Random(12)
    for hl in (0, 1):
        for flag in (0, 1, 2, 255):
            yield load_case(r, hl, flag)
    for rdv in ([0, 0, 0, 0], [1, 2, 3, 4], [0, 0, 0, 1], [0, 0, 0, 0x04000000], [0, 0, 0, 0xFFFFFFFF], [5, 6, 7, 0x80000000]):
        c = load_case(r, 0, 0)
        c["queues"]["rd"] = rdv
        yield c


def remove_case(r, count=None):
    c = base(r, "remove")
    c["count"] = r.choice([0, 1, 1, 2, 3, 0xFFFFFFFF, r.randrange(0, 1 << 32), r.randrange(1, 50)]) if count is None else count
    return c


def remove_edges():
    r = random.Random(13)
    for cnt in (0, 1, 2, 3, 0x80000000, 0x80000001, 0xFFFFFFFF):
        yield remove_case(r, cnt)


TESTS = [
    FnTest("TpBody::Load", ppc, host, edge=load_edges, rnd=lambda r: load_case(r)),
    FnTest("TpBody::Save", ppc, host, edge=save_edges, rnd=lambda r: save_case(r)),
    FnTest("SamsUtilities::UCommandSpeakerType::UCommandSpeakerType", ppc, host, edge=ctor_edges("speaker"), rnd=ctor_rnd("speaker")),
    FnTest("SamsUtilities::UCommandVolume::UCommandVolume", ppc, host, edge=ctor_edges("volume"), rnd=ctor_rnd("volume")),
    FnTest("SamsUtilities::UCommandListenerScale::UCommandListenerScale", ppc, host, edge=ctor_edges("scale"), rnd=ctor_rnd("scale")),
    FnTest("SamsUtilities::UCommandCPULoad::UCommandCPULoad", ppc, host, edge=ctor_edges("cpu"), rnd=ctor_rnd("cpu")),
    FnTest("SamsUtilities::UCommandSoundType::UCommandSoundType", ppc, host, edge=ctor_edges("stype"), rnd=ctor_rnd("stype")),
    FnTest("SamsUtilities::UReferenceCounted::RemoveReference", ppc, host, edge=remove_edges, rnd=lambda r: remove_case(r)),
]
