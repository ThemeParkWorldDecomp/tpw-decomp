"""Shared protocol for the balance batch, main app.

CBalanceTable::Init/UnInit are deliberately stubbed, not translated: both sides
record this, dynamic type, incoming row pointer and call order, then XOR every
value slot and the row pointer. The imported string constructor records its text
and mutates capacity/grow fields, sets length and points at the supplied text.
The PowerPC-only registration fake preserves next and writes object/destructor.
Its calls, destructor TVectors, records and order are checked separately. These
are test assumptions; this suite does not verify any of those callees' implementations.

Native vptrs and pointer slots differ in size. Accessors
compare logical values, and the original's exact vptr stores are checked separately.
"""
import ctypes
import struct
import tpwtest as T
from tpwtest import FnTest

VP, U32, U64, I32, I64 = ctypes.c_void_p, ctypes.c_uint32, ctypes.c_uint64, ctypes.c_int32, ctypes.c_int64
MAX_DESCRIPTION_ROW = ((1 << 31) - 1) // 60
UNITS = [
    dict(file="HoardingsBalance", cls="CHoardingsBalance", count=130, table=0x102948a8, rows=5,
         desc=0x1002112c, start=0x1002117c, dtor=0x100211c4, vt=0x102949f0),
    dict(file="MainBalance", cls="CMainBalance", count=504, table=0x10294a00, rows=284,
         desc=0x100212dc, start=0x10021328, dtor=0x1002136c, vt=0x10298cac,
         sinit=0x10021258, glob=0x102b4568, node=0x102b4d50, tv=0x10265fb8),
    dict(file="OnlineBalance", cls="COnlineBalance", count=14, table=0x10298cbc, rows=16,
         desc=0x10021480, start=0x100214cc, dtor=0x10021510, vt=0x10299098,
         sinit=0x100213fc, glob=0x102b4d5c, node=0x102b4d9c, tv=0x10265fd0),
    dict(file="OptionsBalance", cls="COptionsBalance", count=28, table=0x102990a8, rows=32,
         desc=0x10021628, start=0x10021678, dtor=0x100216c0, vt=0x10299844,
         sinit=0x100215a0, glob=0x102b4da8, node=0x102b4e20, tv=0x10265fe8),
    dict(file="SoundBalance", cls="CSoundBalance", count=30, table=0x1029b744, rows=28,
         desc=0x100217d4, start=0x10021820, dtor=0x10021864, vt=0x1029bdf0,
         sinit=0x10021750, glob=0x102b4e2c, node=0x102b4eac, tv=0x10266000),
    dict(file="ResolutionBalance", cls="CResolutionBalance", count=2, table=0x102b2a10, rows=4,
         desc=0x10237824, start=0x10237874, dtor=0x102378bc, vt=0x102b2b1c,
         sinit=0x1023779c, glob=0x103bc640, node=0x103bc650, tv=0x10268a30),
]
OPTIONS = 0x103bc664
STRINGS = [0x103bc6a8 + 20 * i for i in range(16)] + [0x103bc688, 0x103bc668]
RECORDS = [0x103bc89c - 12 * i for i in range(16)] + [0x103bc69c, 0x103bc67c]
STRING_DTOR = 0x103bcd38
STRING_VT = 0x73545247       # fake, used only to verify every imported ctor ran


def F(lib, name, result, *args):
    fn = lib[name]
    fn.restype, fn.argtypes = result, list(args)
    return fn


class Event(ctypes.Structure):
    _fields_ = [("kind", I32), ("type", I32), ("alloc_before", I32), ("object", VP),
                ("rows", ctypes.c_size_t), ("text", ctypes.c_char * 64)]


def unit_case(u, r):
    return dict(rows=r.getrandbits(32), values=T.rwords(r, u["count"]), vtable=r.getrandbits(32),
                mask=r.getrandbits(32),
                index=r.choice([r.randint(-MAX_DESCRIPTION_ROW, MAX_DESCRIPTION_ROW),
                                r.randrange(u.get("rows", 1))]),
                flag=r.choice([T.rint(r), -1, 0, 1, 0x7fff, 0x8000, 0x10000, 0x1ffff]),
                record=T.rwords(r, 3))


def unit_edges(u, mode):
    import random
    r = random.Random(81)
    # Include every typed row and terminator, nearby negative/past-end rows, and
    # the largest signed byte offsets whose row * 60 cannot wrap on PowerPC.
    items = (sorted(set(range(u["rows"])) | set(range(-1000, 1001)) |
                    {-MAX_DESCRIPTION_ROW, MAX_DESCRIPTION_ROW}) if mode == "desc" else
             [-0x80000000, -0x8000, -1, 0, 1, 2, 0x7fff, 0x10000, 0x18000, 0x1ffff]
             if mode == "dtor" else [0, 1, 0xffffffff])
    out = []
    for i in items:
        c = unit_case(u, r)
        c["index"], c["flag"] = i, i
        out.append(c)
    return out


def ppc_begin(e, c, labels):
    e.balance_trace, e.balance_registered, e.alloc_log, e.deletes = [], [], [], []
    e.balance_mask, e.balance_labels = c["mask"], labels


def label(e, ptr):
    return e.balance_labels.get(ptr, "?%x" % ptr)


def ppc_fake_balance(e, kind):
    obj = e.r(3)
    typ = next((i for i, u in enumerate(UNITS) if u["vt"] == e.u32(obj + 4)), -1)
    assert typ >= 0, "Init/UnInit did not receive the proper derived vtable"
    rows = e.u32(obj)
    e.balance_trace.append((kind, label(e, obj), typ, rows, len(e.alloc_log)))
    e.write(obj, struct.pack(">I", rows ^ e.balance_mask))
    for i in range(UNITS[typ]["count"]):
        addr = obj + 8 + 4 * i
        e.write(addr, struct.pack(">I", e.u32(addr) ^ T.u32(e.balance_mask + i)))


def ppc_string(e):
    obj, text = e.r(3), e.r(4)
    s = e.cstr(text)
    e.balance_trace.append((4, label(e, obj), s, len(e.alloc_log)))
    e.write(obj, struct.pack(">IIIII", STRING_VT, e.u32(obj + 4) ^ e.balance_mask,
                             text, len(s), e.u32(obj + 16) ^ T.u32(e.balance_mask + len(s))))
    e.set_r(3, obj)  # real constructor return: this


def ppc_register(e):
    obj, dtor, rec = e.r(3), e.r(4), e.r(5)
    e.balance_registered.append((obj, dtor, rec))
    if dtor == STRING_DTOR:
        dt = "string"
    else:
        u = next((u for u in UNITS if u.get("tv") == dtor), None)
        dt = u["file"] if u is not None else "unknown destructor"
    e.balance_trace.append((3, label(e, obj), dt, label(e, rec), len(e.alloc_log)))
    e.write(rec + 4, struct.pack(">II", dtor, obj))


def setup(runner):
    e, lib = runner.emu, runner.lib
    T.install_std_stubs(e)
    T.stub_internal(e, "Init__13CBalanceTableFv", lambda e: ppc_fake_balance(e, 1))
    T.stub_internal(e, "UnInit__13CBalanceTableFv", lambda e: ppc_fake_balance(e, 2))
    e.stub("__ct__26TbDynamicStringTemplate<c>FPCc", ppc_string)
    e.stub("__register_global_object", ppc_register)


def sinit_check(e, expected, records, trace):
    """PowerPC-only runtime checks, following test_tb_shadow's sinit_check."""
    mism = []
    if e.balance_registered != expected:
        mism.append("__register_global_object calls: %r != %r" % (e.balance_registered, expected))
    if e.balance_trace != trace:
        mism.append("PowerPC construction/registration order: %r != %r" % (e.balance_trace, trace))
    for (obj, dtor, rec), words in zip(expected, records):
        if [e.u32(rec + 4 * i) for i in range(3)] != [words[0], dtor, obj]:
            mism.append("PowerPC registration record at %#x" % rec)
        if dtor == STRING_DTOR:
            name = "__dt__26TbDynamicStringTemplate<c>Fv"
            if (e.imports.get(dtor, {}).get("name") != name or
                    e.trap_to_import.get(e.u32(dtor), {}).get("name") != name):
                mism.append("registered TVector is not the imported string destructor")
        else:
            u = next(u for u in UNITS if u.get("tv") == dtor)
            if e.u32(dtor) != u["dtor"] or e.u32(dtor + 4) != e.toc:
                mism.append("registered TVector is not %s::~%s" % (u["cls"], u["cls"]))
    return mism


class PowerPCSinitTest(FnTest):
    """Compare common construction results, then check PowerPC registration."""
    def __init__(self, *args, ppc_check, **kwargs):
        super().__init__(*args, **kwargs)
        self.ppc_check = ppc_check

    def run_case(self, runner, case):
        mism = super().run_case(runner, case)
        return mism + self.ppc_check(runner.emu, case)


def host_begin(lib, c):
    I32.in_dll(lib, "tpw_balance_log_on").value = 1
    I32.in_dll(lib, "tpw_balance_log_n").value = 0
    I32.in_dll(lib, "tpw_alloc_log_on").value = 1
    I32.in_dll(lib, "tpw_alloc_n").value = 0
    U32.in_dll(lib, "tpw_balance_mask").value = c["mask"]


def host_end(lib, labels):
    n = I32.in_dll(lib, "tpw_balance_log_n").value
    trace = []
    for ev in (Event * 64).in_dll(lib, "tpw_balance_log")[:n]:
        obj = labels.get(ev.object, "?%x" % (ev.object or 0))
        if ev.kind in (1, 2):
            trace.append((ev.kind, obj, ev.type, ev.rows, ev.alloc_before))
        elif ev.kind == 4:
            trace.append((4, obj, ev.text.decode("ascii"), ev.alloc_before))
        else:
            trace.append((ev.kind, obj))
    n = I32.in_dll(lib, "tpw_alloc_n").value
    kinds = (I32 * 1024).in_dll(lib, "tpw_alloc_kind")
    ptrs = (U64 * 1024).in_dll(lib, "tpw_alloc_ptr")
    sizes = (U64 * 1024).in_dll(lib, "tpw_alloc_size")
    allocs = [({1: "new", 2: "new[]", 3: "delete", 4: "delete[]"}[kinds[i]],
               labels.get(ptrs[i], "unknown allocation"), sizes[i]) for i in range(n)]
    I32.in_dll(lib, "tpw_balance_log_on").value = 0
    I32.in_dll(lib, "tpw_alloc_log_on").value = 0
    return dict(trace=trace, allocs=allocs)


def ppc_object(e, u, c, obj=None):
    raw = struct.pack(">II%dI" % u["count"], c["rows"], c["vtable"], *c["values"])
    if obj is None:
        return e.alloc(len(raw), raw)
    e.write(obj, raw)
    return obj


def ppc_fields(e, obj, u):
    return dict(rows=e.u32(obj), values=list(struct.unpack(">%dI" % u["count"], e.read(obj + 8, 4 * u["count"]))))


def host_fields(lib, obj, u):
    rows, values = U64(), (U64 * u["count"])()
    F(lib, "tpw_" + u["file"] + "_read", None, VP, VP, VP)(obj, ctypes.byref(rows), values)
    return dict(rows=rows.value, values=list(values))


def ppc_table(e, u):
    return [struct.unpack(">i32siiIIiI", e.read(u["table"] + i * 60, 60)) for i in range(u["rows"])]


def host_table(lib, u):
    addr = F(lib, "tpw_" + u["file"] + "_table", VP)()
    return [struct.unpack("<i32siiIIiI", ctypes.string_at(addr + i * 60, 60)) for i in range(u["rows"])]


def ppc_member(u, mode):
    def run(e, c):
        obj = ppc_object(e, u, c)
        ppc_begin(e, c, {obj: "object"})
        args = [obj, T.I(c["index"])] if mode == "desc" else [obj, T.I(c["flag"])] if mode == "dtor" else [obj]
        ret = T.call_ppc(e, u[{"desc": "desc", "values": "start", "dtor": "dtor"}[mode]], args)[0]
        out = ppc_fields(e, obj, u)
        out.update(trace=e.balance_trace, allocs=[(k, label(e, a), n) for k, a, n in e.alloc_log],
                   abi_ok=e.u32(obj + 4) == (u["vt"] if mode == "dtor" else c["vtable"]))
        if mode == "desc":
            out.update(ret=T.s32(ret - u["table"]), table=ppc_table(e, u))
        elif mode == "values":
            out["ret"] = T.s32(ret - obj)
        else:
            out["ret_this"] = ret == obj
        return out
    return run


def host_member(u, mode):
    def run(lib, c):
        pre = "tpw_" + u["file"]
        vals = (U32 * u["count"])(*c["values"])
        obj = F(lib, pre + "_create", VP, U32, VP)(c["rows"], vals)
        host_begin(lib, c)
        if mode == "dtor":
            rows, values = U64(), (U64 * u["count"])()
            ret = F(lib, pre + "_destroy", VP, VP, ctypes.c_int16, VP, VP)(
                obj, T.s16(c["flag"]), ctypes.byref(rows), values)
            out = dict(rows=rows.value, values=list(values), ret_this=ret == obj)
        else:
            ret = (F(lib, pre + "_description", I64, VP, I32)(obj, c["index"]) if mode == "desc" else
                   F(lib, pre + "_values", I32, VP)(obj))
            out = host_fields(lib, obj, u)
            out["ret"] = ret
            if mode == "desc":
                out["table"] = host_table(lib, u)
        out.update(host_end(lib, {obj: "object"}), abi_ok=True)
        if mode != "dtor" or T.s16(c["flag"]) <= 0:
            F(lib, pre + "_release", None, VP)(obj)
        return out
    return run


def ppc_sinit(u):
    def run(e, c):
        obj, node = u["glob"], u["node"]
        ppc_object(e, u, c, obj)
        e.write(node, struct.pack(">3I", *c["record"]))
        ppc_begin(e, c, {obj: "global", node: "record"})
        T.call_ppc(e, u["sinit"], [])
        out = ppc_fields(e, obj, u)
        out.update(trace=[ev for ev in e.balance_trace if ev[0] != 3],
                   allocs=[], abi_ok=e.u32(obj + 4) == u["vt"])
        return out
    return run


def host_sinit(u):
    def run(lib, c):
        pre = "tpw_" + u["file"]
        obj = F(lib, pre + "_global", VP)()
        F(lib, pre + "_seed", None, VP, U32, VP)(obj, c["rows"], (U32 * u["count"])(*c["values"]))
        host_begin(lib, c)
        F(lib, "tpw_sinit_" + u["file"], None)()
        out = host_fields(lib, obj, u)
        out.update(host_end(lib, {obj: "global"}), abi_ok=True)
        return out
    return run


def tests_for(file):
    u = next(u for u in UNITS if u["file"] == file)
    tests = []
    for mode, name in (("desc", "GetBalanceDescription"), ("values", "GetBalanceValuesStart"), ("dtor", "~" + u["cls"])):
        tests.append(FnTest(u["cls"] + "::" + name, ppc_member(u, mode), host_member(u, mode),
                            edge=lambda mode=mode: unit_edges(u, mode), rnd=lambda r: unit_case(u, r)))
    if "sinit" in u:
        tests.append(PowerPCSinitTest("__sinit_" + file + "_cpp", ppc_sinit(u), host_sinit(u),
                            ppc_check=lambda e, c: sinit_check(
                                e, [(u["glob"], u["tv"], u["node"])], [c["record"]],
                                [(1, "global", UNITS.index(u), c["rows"], 0),
                                 (3, "global", file, "record", 0)]),
                            edge=lambda: unit_edges(u, "sinit"), rnd=lambda r: unit_case(u, r)))
    return tests
