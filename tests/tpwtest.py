"""Shared helpers for differential tests (run by difftest.py).

A Test here owns the whole case protocol (difftest.Runner.run_case delegates to
Test.run_case), which adds what the pilot's simple Test lacked:

- arguments placed exactly as the PowerOpen ABI does: ints/pointers take the
  next GPR slot (r3..r10, then 24+4*slot(r1)); a single-precision float goes in
  the next FPR and consumes one GPR slot, a double consumes two; 8- and 16-bit
  integers are extended into the register as a CodeWarrior caller would; a struct
  passed by value (SVal) occupies one slot per word (the host shim gets a pointer);
- globals: Glob(name, ppc_addr, host_sym, fmt, values) writes the same values
  into emulator memory and into the host library's global before the call and
  compares both afterwards (host_sym: "tpw_..." = an extern "C" accessor returning
  the address; ("tpw_fn", "key") = accessor taking a name; anything else = the
  data symbol itself);
- return types u32/i32/u16/s16/u8/s8/bool/f32/f64/u64/none (f32 compared
  bit-exactly; NaNs compare equal to NaNs unless exact_nan=True);
- import stubs that call back into PowerPC code (stub_cb, e.g. __construct_array
  calling a constructor), a persistent PowerPC heap for objects that live across
  cases (persist_alloc), and the usual C library / MathLib / Toolbox stubs
  (install_std_stubs);
- in-binary callees can be replaced on the PowerPC side by Python stubs
  (stub_internal), for functions whose callees are not translated yet.

Case description: args(case) returns a list of I/U/S16/U16/S8/U8/F/D/Buf/Ptr/SVal
items; globs(case) a list of Glob items; compare names the buffers and globals to
compare after the call (all by default).
"""
import ctypes
import math
import re
import struct

from unicorn import UC_HOOK_CODE
from unicorn import ppc_const as P

import ppcemu

# ----------------------------------------------------------------- integers


def s32(v):
    return ((v + 0x80000000) & 0xFFFFFFFF) - 0x80000000


def u32(v):
    return v & 0xFFFFFFFF


def s16(v):
    return ((v + 0x8000) & 0xFFFF) - 0x8000


def u16(v):
    return v & 0xFFFF


def s8(v):
    return ((v + 0x80) & 0xFF) - 0x80


def u8(v):
    return v & 0xFF


def f32_bits(x):
    return struct.unpack("<I", struct.pack("<f", x))[0]


def bits_f32(b):
    return struct.unpack("<f", struct.pack("<I", b & 0xFFFFFFFF))[0]


def f64_bits(x):
    return struct.unpack("<Q", struct.pack("<d", x))[0]


def f32(x):
    """Round a Python float to the nearest float32 (overflow -> inf)."""
    try:
        return struct.unpack("<f", struct.pack("<f", x))[0]
    except OverflowError:
        return math.copysign(math.inf, x)


INT_EDGES = [0, 1, -1, 2, -2, 3, 7, 8, 15, 16, 31, 32, 0x7F, 0x80, 0xFF, 0x100, 0x7FFF, 0x8000, 0xFFFF,
             0x10000, -0x10000, 0x12345678, -0x12345678, 0x7FFFFFFF, -0x80000000, 0x3FFFFFFF, -0x40000000]

FLOAT_EDGES = [0.0, -0.0, 1.0, -1.0, 0.5, -0.5, 2.0, 1e-6, -1e-6, 1e6, -1e6, 3.4028234663852886e38,
               -3.4028234663852886e38, 1.1754943508222875e-38, 1.401298464324817e-45, math.inf, -math.inf]


def rint(r):
    """Random 32-bit int: 40% full range, 40% small, 20% edge values."""
    k = r.random()
    if k < 0.4:
        return r.randint(-0x80000000, 0x7FFFFFFF)
    if k < 0.8:
        return r.randint(-0x400, 0x400)
    return r.choice(INT_EDGES)


def rfloat(r, nan=False):
    """Random single-precision float (as a Python float that is exactly a float32)."""
    k = r.random()
    if k < 0.3:
        while True:
            x = bits_f32(r.getrandbits(32))
            if nan or not math.isnan(x):
                return x
    if k < 0.8:
        return f32(r.uniform(-1000.0, 1000.0))
    return r.choice(FLOAT_EDGES)


def rfloat_mod(r, lo=-100.0, hi=100.0):
    """Random float32 in a moderate range (keeps intermediate results finite)."""
    return f32(r.uniform(lo, hi))


def rwords(r, n):
    return [r.getrandbits(32) for _ in range(n)]

# ----------------------------------------------------------------- argument kinds


class I:
    """32-bit integer (signed or unsigned, compared as 32 bits)."""
    gprs = 1

    def __init__(self, v):
        self.v = v

    def reg(self):
        return u32(self.v)

    def host(self):
        return ctypes.c_uint32(u32(self.v))


U = I


class S16(I):
    def reg(self):
        return u32(s16(self.v))

    def host(self):
        return ctypes.c_int16(s16(self.v))


class U16(I):
    def reg(self):
        return u16(self.v)

    def host(self):
        return ctypes.c_uint16(u16(self.v))


class S8(I):
    def reg(self):
        return u32(s8(self.v))

    def host(self):
        return ctypes.c_int8(s8(self.v))


class U8(I):
    def reg(self):
        return u8(self.v)

    def host(self):
        return ctypes.c_uint8(u8(self.v))


class F:
    """float argument: next FPR (as a double holding the float value), one GPR slot."""
    gprs = 1

    def __init__(self, v):
        self.v = bits_f32(f32_bits(v)) if not math.isnan(v) else v

    def host(self):
        return ctypes.c_float(self.v)


class D:
    """double argument: next FPR, two GPR slots."""
    gprs = 2

    def __init__(self, v):
        self.v = float(v)

    def host(self):
        return ctypes.c_double(self.v)


class Buf:
    """Pointer to a scratch buffer holding struct-packed values (fmt without byte order)."""
    gprs = 1

    def __init__(self, name, fmt, values, size=None):
        self.name, self.fmt, self.values = name, fmt, list(values)
        self.size = size if size is not None else struct.calcsize("<" + fmt)


class SVal:
    """Struct passed by value. PowerPC: its words take consecutive argument slots (GPRs,
    then the parameter area); host: the shim receives a pointer to a native copy."""

    def __init__(self, name, fmt, values):
        self.name, self.fmt, self.values = name, fmt, list(values)
        self.size = struct.calcsize("<" + fmt)
        self.gprs = (self.size + 3) // 4


class Ptr(I):
    """Raw pointer value (e.g. NULL), the same number on both sides."""


class PpcOnly(I):
    """Argument passed only on the PowerPC side (e.g. a `this` the host wrapper supplies)."""
    ppc_only = True


class Same:
    """The pointer of an earlier Buf again (aliasing arguments)."""
    gprs = 1

    def __init__(self, name):
        self.name = name


class Glob:
    """A global: ppc_addr (int, or a function label whose address is used), host_sym
    (see host_global_addr), fmt, values."""

    def __init__(self, name, ppc_addr, host_sym, fmt, values):
        self.name, self.ppc_addr, self.host_sym, self.fmt, self.values = name, ppc_addr, host_sym, fmt, list(values)
        self.size = struct.calcsize("<" + fmt)


def host_global_addr(lib, sym):
    """("tpw_fn", "key") -> tpw_fn("key"); "tpw_..." -> accessor(); else the data symbol."""
    if isinstance(sym, tuple):
        fn = getattr(lib, sym[0])
        fn.restype = ctypes.c_void_p
        fn.argtypes = [ctypes.c_char_p]
        a = fn(sym[1].encode())
        if not a:
            raise KeyError(f"{sym[0]}({sym[1]!r}) returned NULL")
        return a
    if sym.startswith("tpw_"):
        fn = getattr(lib, sym)
        fn.restype = ctypes.c_void_p
        fn.argtypes = []
        return fn()
    return ctypes.addressof(ctypes.c_char.in_dll(lib, sym))


def host_int(lib, sym):
    return ctypes.c_int32.in_dll(lib, sym)

# ----------------------------------------------------------------- PowerPC side

PERSIST_BASE, PERSIST_SIZE = 0x7C000000, 0x200000


def persist_alloc(emu, size, data=None, alignment=16):
    """Memory that survives reset_scratch within a module (objects, heap blocks).

    The runner creates a fresh emulator for the next module, so these addresses
    and fake-function hooks must stay local to the emulator that allocated them.
    """
    if not hasattr(emu, "persist_top"):
        emu.uc.mem_map(PERSIST_BASE, PERSIST_SIZE)
        emu.persist_top = PERSIST_BASE
    addr = (emu.persist_top + alignment - 1) & ~(alignment - 1)
    if addr + size > PERSIST_BASE + PERSIST_SIZE:
        raise MemoryError("persistent region exhausted")
    emu.persist_top = addr + size
    emu.uc.mem_write(addr, data if data is not None else b"\0" * size)
    return addr


def _place_args(emu, sp, args, bufaddr):
    uc = emu.uc
    slot, fi = 0, 1

    def word(v):
        nonlocal slot
        if slot < 8:
            emu.set_r(3 + slot, v)
        else:
            uc.mem_write(sp + 24 + 4 * slot, struct.pack(">I", v & 0xFFFFFFFF))
        slot += 1

    for a in args:
        if isinstance(a, (Buf, Same)):
            word(bufaddr[a.name])
        elif isinstance(a, SVal):
            data = struct.pack(">" + a.fmt, *a.values)
            data += b"\0" * (-len(data) % 4)
            for i in range(0, len(data), 4):
                word(struct.unpack(">I", data[i:i + 4])[0])
        elif isinstance(a, (F, D)):
            if fi <= 13:
                emu.set_f(fi, a.v)
                fi += 1
            else:
                data = struct.pack(">f", a.v) if isinstance(a, F) else struct.pack(">d", a.v)
                uc.mem_write(sp + 24 + 4 * slot, data)
            slot += a.gprs
        elif isinstance(a, int):
            word(u32(a))
        else:
            word(a.reg())


def _start(emu, pc, max_insns, timeout_us):
    uc = emu.uc
    try:
        uc.emu_start(pc, ppcemu.RETURN_ADDR, timeout=timeout_us, count=max_insns)
    except ppcemu.UcError as e:
        if emu.unstubbed:
            raise ppcemu.ImportCalled(emu.unstubbed["name"]) from None
        raise RuntimeError(f"emulation error {e} at pc={uc.reg_read(P.UC_PPC_REG_PC):#x} "
                           f"fault={emu.last_fault}") from None
    if emu.unstubbed:
        raise ppcemu.ImportCalled(emu.unstubbed["name"])


def _run(emu, pc, max_insns, timeout_us):
    """Run until the top-level return, servicing callback stubs (stub_cb) on the way."""
    uc = emu.uc
    _start(emu, pc, max_insns, timeout_us)
    while getattr(emu, "pending_cb", None) is not None:
        fn, emu.pending_cb = emu.pending_cb, None
        ctx = uc.context_save()
        sp0 = emu.r(1)
        resume = uc.reg_read(P.UC_PPC_REG_LR)
        depth = getattr(emu, "cb_depth", 0)

        def call(target, *args):
            """Nested call: target is a TVector address (code, TOC) or ("code", addr)."""
            if isinstance(target, tuple):
                code, toc = target[1], emu.toc
            else:
                code, toc = emu.u32(target), emu.u32(target + 4)
            sp = ((sp0 - 0x2000 * (depth + 1)) & ~0xF) - 0x400
            uc.mem_write(sp, b"\0" * 0x400)
            _place_args(emu, sp, list(args), {})
            emu.set_r(1, sp)
            emu.set_r(2, toc)
            uc.reg_write(P.UC_PPC_REG_LR, ppcemu.RETURN_ADDR)
            emu.cb_depth = depth + 1
            try:
                _run(emu, code, max_insns, timeout_us)
            finally:
                emu.cb_depth = depth
            return emu.r(3)

        result = fn(emu, call)
        uc.context_restore(ctx)
        if result is not None:
            emu.set_r(3, result)
        _start(emu, resume, max_insns, timeout_us)
    pc = uc.reg_read(P.UC_PPC_REG_PC)
    if pc != ppcemu.RETURN_ADDR:
        raise RuntimeError(f"did not return: pc={pc:#x} (instruction budget or timeout)")


def call_ppc(emu, entry, args, bufaddr=None, max_insns=50_000_000, timeout_us=60_000_000, r0=0):
    """Call `entry` with PowerOpen argument placement. Returns (r3, r4, f1)."""
    uc = emu.uc
    emu.unstubbed = None
    emu.last_fault = None
    emu.pending_cb = None
    emu.calls = []
    uc.reg_write(P.UC_PPC_REG_MSR, 0x2000)
    frame = 0x400
    sp = ppcemu.STACK_BASE + ppcemu.STACK_SIZE - 0x1000 - frame
    uc.mem_write(sp, b"\0" * frame)
    _place_args(emu, sp, args, bufaddr or {})
    emu.set_r(1, sp)
    emu.set_r(2, emu.toc)
    emu.set_r(0, r0)
    uc.reg_write(P.UC_PPC_REG_LR, ppcemu.RETURN_ADDR)
    _run(emu, entry, max_insns, timeout_us)
    return emu.r(3), emu.r(4), emu.f(1)


def stub_cb(emu, name, fn):
    """Import stub that calls PowerPC code: fn(emu, call) runs outside the Unicorn hook
    with the registers of the import call; call(tvector, *args) runs a nested PowerPC
    call and returns its r3; a non-None result of fn goes to r3."""
    def hook(e):
        e.pending_cb = fn
        e.uc.reg_write(P.UC_PPC_REG_PC, ppcemu.RETURN_ADDR)   # leave emu_start; _run resumes
    emu.stub(name, hook)


def stub_internal(emu, label, fn):
    """Replace an in-binary function (by label or address) with a Python stub on the
    PowerPC side: fn(emu) runs at the entry, then execution returns to LR."""
    entry = label if isinstance(label, int) else emu.func(label)
    if not hasattr(emu, "internal_stubs"):
        emu.internal_stubs = {}
        emu.internal_hooks = {}

    def hook(uc, address, size, user):
        f = emu.internal_stubs.get(address)
        if f is None:
            return
        emu.calls.append((label, [emu.r(i) for i in range(3, 7)]))
        f(emu)
        uc.reg_write(P.UC_PPC_REG_PC, uc.reg_read(P.UC_PPC_REG_LR))

    emu.internal_stubs[entry] = fn
    if entry not in emu.internal_hooks:
        emu.internal_hooks[entry] = emu.uc.hook_add(UC_HOOK_CODE, hook, begin=entry, end=entry)


def ppc_addr(emu, a):
    return emu.func(a) if isinstance(a, str) else a

# ----------------------------------------------------------------- standard stubs


def c_math(fn):
    """Python math function with C semantics: domain errors give NaN, overflow inf."""
    def wrapped(x):
        try:
            return fn(x)
        except ValueError:
            return math.nan
        except OverflowError:
            return math.inf
    return wrapped


def _sqrt(x):
    if math.isnan(x):
        return x
    if x < 0:
        return math.nan
    return math.sqrt(x)


MATH1 = {"sin": c_math(math.sin), "cos": c_math(math.cos), "tan": c_math(math.tan), "sqrt": _sqrt,
         "asin": c_math(math.asin), "acos": c_math(math.acos), "atan": c_math(math.atan),
         "exp": c_math(math.exp), "log": c_math(math.log), "fabs": abs, "floor": c_math(math.floor),
         "ceil": c_math(math.ceil)}


_FMT = re.compile(r"%([-+ 0#]*)(\d*)(?:\.(\d+))?(hh|h|ll|l|L)?([diuxXcs%])")


def c_format(emu, fmt, first_reg, va=None):
    """printf-style formatting for PowerPC stubs: %d %i %u %x %X %c %s %% with flags,
    width and precision (no floating point). Integer and pointer arguments are read from
    GPRs first_reg.. r10, then from the caller's parameter area; with va (a CodeWarrior
    va_list: the address of the next argument word) they are read from memory instead."""
    slot = [first_reg - 3]

    def arg():
        k = slot[0]
        slot[0] += 1
        if va is not None:
            return emu.u32(va + 4 * k)
        if k < 8:
            return emu.r(3 + k)
        return emu.u32(emu.r(1) + 24 + 4 * k)

    def one(m):
        flags, width, prec, _, conv = m.groups()
        if conv == "%":
            return "%"
        spec = "%" + flags + width + ("." + prec if prec else "")
        if conv == "s":
            return (spec + "s") % emu.cstr(arg())
        if conv == "c":
            return (spec + "c") % chr(arg() & 0xFF)
        v = arg()
        if conv in "di":
            return (spec + "d") % s32(v)
        return (spec + {"u": "d", "x": "x", "X": "X"}[conv]) % u32(v)
    return _FMT.sub(one, fmt)


def install_std_stubs(emu):
    """C library, MathLib, Toolbox and Sam's Utilities stubs used across the engine.
    Faithful where the semantics are fixed (memset, sqrt, byte swaps); recorded where a
    test may compare the call (operator new/delete, asserts, DebugStr); cycle counter
    values come from emu.cycles (set per case)."""
    def memset(e):
        d, c, n = e.r(3), e.r(4) & 0xFF, e.r(5)
        if n:
            e.write(d, bytes([c]) * n)

    def memcpy(e):
        d, s, n = e.r(3), e.r(4), e.r(5)
        if n:
            e.write(d, e.read(s, n))

    def strcpy(e):
        d, s = e.r(3), e.r(4)
        e.write(d, e.cstr(s, limit=1 << 20).encode("latin-1") + b"\0")

    def strlen(e):
        e.set_r(3, len(e.cstr(e.r(3), limit=1 << 20).encode("latin-1")))

    def strrchr(e):
        s, c = e.r(3), e.r(4) & 0xFF
        text = e.cstr(s, limit=1 << 20).encode("latin-1") + b"\0"
        i = text.rfind(bytes([c]))
        e.set_r(3, s + i if i >= 0 else 0)

    for name in ("memset",):
        emu.stub(name, memset)
    for name in ("memcpy", "memmove"):
        emu.stub(name, memcpy)
    emu.stub("strcpy", strcpy)
    emu.stub("strlen", strlen)
    emu.stub("strrchr", strrchr)

    def snprintf(e):
        buf, size, fmt = e.r(3), e.r(4), e.cstr(e.r(5))
        text = c_format(e, fmt, 6).encode("latin-1")
        if size:
            e.write(buf, text[:size - 1] + b"\0")
        e.set_r(3, len(text))

    def sprintf(e):
        buf, fmt = e.r(3), e.cstr(e.r(4))
        text = c_format(e, fmt, 5).encode("latin-1")
        e.write(buf, text + b"\0")
        e.set_r(3, len(text))

    emu.stub("snprintf", snprintf)
    emu.stub("sprintf", sprintf)
    for name, fn in MATH1.items():
        emu.stub(name, (lambda f: lambda e: e.set_f1(f(e.f(1))))(fn))

    emu.news, emu.deletes, emu.asserts, emu.debugstrs = [], [], [], []
    # alloc_log: every operator new / new[] / delete / delete[] in call order, as
    # (kind, address, size); heap_in_scratch=True puts new blocks in the per-case scratch
    # area (reset_scratch frees them) instead of the persistent region.
    emu.alloc_log, emu.heap_in_scratch = [], False

    def heap(e, size):
        return e.alloc(max(size, 4)) if e.heap_in_scratch else persist_alloc(e, max(size, 4))

    def op_new(e):
        size = e.r(3)
        a = heap(e, size)
        e.news.append((size, a))
        e.alloc_log.append(("new", a, size))
        e.set_r(3, a)

    def op_delete(e):
        e.deletes.append(e.r(3))
        e.alloc_log.append(("delete", e.r(3), 0))

    emu.stub("__nw__FUl", op_new)
    emu.stub("__dl__FPv", op_delete)
    emu.news_arr, emu.deletes_arr = [], []

    def op_new_arr(e):
        size = e.r(3)
        a = heap(e, size)
        e.news_arr.append((size, a))
        e.alloc_log.append(("new[]", a, size))
        e.set_r(3, a)

    def op_delete_arr(e):
        e.deletes_arr.append(e.r(3))
        e.alloc_log.append(("delete[]", e.r(3), 0))

    emu.stub("__nwa__FUl", op_new_arr)
    emu.stub("__dla__FPv", op_delete_arr)
    emu.gestalt = (0, 0)

    def gestalt(e):
        err, value = e.gestalt
        if err == 0:
            e.write(e.r(4), struct.pack(">i", s32(value)))
        e.set_r(3, u32(err))

    emu.stub("Gestalt", gestalt)
    emu.stub("BlockMoveData", lambda e: e.write(e.r(4), e.read(e.r(3), e.r(5))) if e.r(5) else None)

    def destroy_arr(e, call):
        block, dtor, size, n = e.r(3), e.r(4), e.r(5), e.r(6)
        for i in range(n - 1, -1, -1):
            call(dtor, block + i * size, 0xFFFF)     # flag -1: destroy without freeing
        return None

    stub_cb(emu, "__destroy_arr", destroy_arr)
    emu.stub("__assertion_failed", lambda e: e.asserts.append((e.cstr(e.r(3)), e.cstr(e.r(4)), s32(e.r(5)))))
    emu.stub("DebugStr", lambda e: e.debugstrs.append(e.read(e.r(3) + 1, e.read(e.r(3), 1)[0])))
    emu.registered = []
    emu.stub("__register_global_object", lambda e: e.registered.append((e.r(3), e.r(4), e.r(5))))

    def swap32(e):
        a, n = e.r(3), s32(e.r(4))
        for i in range(max(n, 0)):
            w = e.read(a + 4 * i, 4)
            e.write(a + 4 * i, w[::-1])

    emu.stub("_USwapBlock32__13SamsUtilitiesFPUll", swap32)

    emu.cycles, emu.cycle_i = [], 0

    def cycles(e):
        v = e.cycles[min(e.cycle_i, len(e.cycles) - 1)] if e.cycles else 0
        e.cycle_i += 1
        e.set_r(3, (v >> 32) & 0xFFFFFFFF)
        e.set_r(4, v & 0xFFFFFFFF)

    emu.stub("GetAbsoluteCycles__Q213SamsUtilities6UTimerFv", cycles)

    def construct_array(e, call):
        block, ctor, dtor, size, n = e.r(3), e.r(4), e.r(5), e.r(6), e.r(7)
        for i in range(n):
            call(ctor, block + i * size, 1)
        return None

    stub_cb(emu, "__construct_array", construct_array)

# ----------------------------------------------------------------- the test


RET_HOST = {"u32": ctypes.c_uint32, "i32": ctypes.c_int32, "u16": ctypes.c_uint16, "s16": ctypes.c_int16,
            "u8": ctypes.c_uint8, "s8": ctypes.c_int8, "bool": ctypes.c_uint8, "f32": ctypes.c_float,
            "f64": ctypes.c_double, "u64": ctypes.c_uint64, "none": None}


def ret_value(kind, r3, r4, f1):
    if kind == "u32":
        return u32(r3)
    if kind == "i32":
        return s32(r3)
    if kind == "u16":
        return u16(r3)
    if kind == "s16":
        return s16(r3)
    if kind in ("u8", "bool"):
        return u8(r3)
    if kind == "s8":
        return s8(r3)
    if kind == "f32":
        return f32_bits(f32(f1)) if not math.isnan(f1) else "nan"
    if kind == "f64":
        return f64_bits(f1) if not math.isnan(f1) else "nan"
    if kind == "u64":
        return (u32(r3) << 32) | u32(r4)
    return None


def host_ret_value(kind, v):
    if kind in ("u32", "i32", "u16", "s16", "u8", "s8", "bool", "u64"):
        return ret_value(kind, v & 0xFFFFFFFF, 0, 0.0) if kind != "u64" else v & 0xFFFFFFFFFFFFFFFF
    if kind == "f32":
        return "nan" if math.isnan(v) else f32_bits(v)
    if kind == "f64":
        return "nan" if math.isnan(v) else f64_bits(v)
    return None


def _fmtvals(fmt, vals):
    out = []
    for v in vals:
        if isinstance(v, float):
            out.append(repr(v))
        elif isinstance(v, int):
            out.append(hex(v))
        else:
            out.append(repr(v))
    return out


class Test:
    def __init__(self, name, ppc_name, host_name, args, ret="none", edge=None, rnd=None, globs=None,
                 compare=None, check=None, ppc_setup=None, host_setup=None, exact_nan=False, r0=0,
                 ppc_ret=None):
        self.name, self.ppc_name, self.host_name = name, ppc_name, host_name
        self.args_fn, self.ret = args, ret
        self._edge, self._rnd = edge or (lambda: []), rnd
        self.globs_fn = globs or (lambda c: [])
        self.compare_names = compare
        self.check = check              # extra (p, h, case) -> list of mismatches
        self.ppc_setup, self.host_setup = ppc_setup, host_setup
        self.exact_nan = exact_nan
        self.r0 = r0
        self.ppc_ret = ppc_ret          # (r3, r4, f1, addr) -> value compared with the host return

    def edge_cases(self):
        return list(self._edge())

    def random_case(self, r):
        return self._rnd(r)

    def run_case(self, runner, case):
        emu, lib = runner.emu, runner.lib
        args = self.args_fn(case)
        globs = self.globs_fn(case)
        bufs = [a for a in args if isinstance(a, Buf)]
        svals = [a for a in args if isinstance(a, SVal)]
        # ---- PowerPC
        emu.reset_scratch()
        addr = {}
        for b in bufs:
            addr[b.name] = emu.alloc(max(b.size, 4), struct.pack(">" + b.fmt, *b.values) if b.values else None)
        for g in globs:
            emu.write(ppc_addr(emu, g.ppc_addr), struct.pack(">" + g.fmt, *g.values))
        if self.ppc_setup:
            self.ppc_setup(emu, case, addr)
        try:
            r3, r4, f1 = call_ppc(emu, emu.func(self.ppc_name), args, addr, r0=self.r0)
        except ppcemu.ImportCalled as ex:
            return [f"PPC side called unstubbed import {ex}"]
        pret = self.ppc_ret(r3, r4, f1, addr) if self.ppc_ret else ret_value(self.ret, r3, r4, f1)
        p = {"ret": pret, "calls": list(emu.calls), "addr": addr}
        for b in bufs:
            p[b.name] = struct.unpack(">" + b.fmt, emu.read(addr[b.name], b.size)) if b.fmt else b""
        for g in globs:
            p[g.name] = struct.unpack(">" + g.fmt, emu.read(ppc_addr(emu, g.ppc_addr), g.size))
        p["emu"] = emu
        # ---- host
        hbuf = {}
        for b in bufs + svals:
            data = struct.pack("<" + b.fmt, *b.values) if b.values else b"\0" * b.size
            hbuf[b.name] = ctypes.create_string_buffer(data, max(b.size, 4))
        gaddr = {}
        for g in globs:
            gaddr[g.name] = host_global_addr(lib, g.host_sym)
            ctypes.memmove(gaddr[g.name], struct.pack("<" + g.fmt, *g.values), g.size)
        if self.host_setup:
            self.host_setup(lib, case, hbuf)
        fn = lib[self.host_name]          # a fresh function object: restype stays local to this test
        fn.restype = RET_HOST[self.ret]
        cargs = []
        for a in args:
            if getattr(a, "ppc_only", False):
                continue
            if isinstance(a, (Buf, SVal, Same)):
                cargs.append(ctypes.cast(hbuf[a.name], ctypes.c_void_p))
            else:
                cargs.append(a.host())
        hres = fn(*cargs)
        h = {"ret": host_ret_value(self.ret, hres) if self.ret != "none" else None, "lib": lib}
        for b in bufs:
            h[b.name] = struct.unpack("<" + b.fmt, bytes(hbuf[b.name].raw[:b.size])) if b.fmt else b""
        for g in globs:
            h[g.name] = struct.unpack("<" + g.fmt, ctypes.string_at(gaddr[g.name], g.size))
        # ---- compare
        mism = []
        if self.ret != "none" and p["ret"] != h["ret"]:
            pv, hv = p["ret"], h["ret"]
            mism.append(f"return ppc={pv if isinstance(pv, str) else hex(pv)} host={hv if isinstance(hv, str) else hex(hv)}")
        names = self.compare_names if self.compare_names is not None else [b.name for b in bufs] + [g.name for g in globs]
        fmts = {b.name: b.fmt for b in bufs}
        fmts.update({g.name: g.fmt for g in globs})
        for n in names:
            if not self._same(p[n], h[n]):
                mism.append(f"{n}: ppc={_fmtvals(fmts[n], p[n])} host={_fmtvals(fmts[n], h[n])}")
        if self.check:
            mism += self.check(p, h, case)
        return mism

    def _same(self, a, b):
        if len(a) != len(b):
            return False
        for x, y in zip(a, b):
            if isinstance(x, float) and isinstance(y, float):
                if math.isnan(x) and math.isnan(y) and not self.exact_nan:
                    continue
                if struct.pack("<d", x) != struct.pack("<d", y):     # bit-exact, -0.0 != 0.0
                    return False
            elif x != y:
                return False
        return True

# ----------------------------------------------------------------- object-level tests


def fake_function(emu, fn):
    """A PowerPC-callable function implemented in Python: returns the address of a
    TVector {code, toc} whose code is a hooked `blr`; fn(emu) reads r3.. and sets r3."""
    code = persist_alloc(emu, 8, b"\x4e\x80\x00\x20" * 2, alignment=8)
    tvec = persist_alloc(emu, 8, struct.pack(">II", code, emu.toc), alignment=8)

    def hook(uc, address, size, user):
        fn(emu)

    emu.uc.hook_add(UC_HOOK_CODE, hook, begin=code, end=code)
    return tvec


FAKE_FILE_HANDLE = 0x7C0FF000


class FakeFile:
    """A byte stream for the engine's file callbacks (fopen/fread/fseek/fclose style),
    with the same semantics on both sides (tests/shim_landscape.cpp has the host copy):
    the position is a 32-bit unsigned value (seeks wrap), reads past the end give zeros,
    read returns `count`, seek and close return 0. The log records every call:
    ("open", name, mode), ("read", size, count, handle_ok), ("seek", offset, whence,
    handle_ok), ("close", handle_ok)."""

    def __init__(self, data=b""):
        self.data, self.pos, self.log, self.fail_open = bytes(data), 0, [], False

    def read(self, n):
        chunk = self.data[self.pos:self.pos + n] if self.pos < len(self.data) else b""
        self.pos = (self.pos + n) & 0xFFFFFFFF
        return chunk + b"\0" * (n - len(chunk))

    def seek(self, off, whence):
        if whence == 1:
            self.pos = (self.pos + off) & 0xFFFFFFFF
        elif whence == 0:
            self.pos = off & 0xFFFFFFFF


def install_ppc_file_callbacks(emu, addrs):
    """addrs: dict open/close/read/seek -> address of the engine's function-pointer global.
    The fake functions use emu.fake_file (a FakeFile; replace it per case); open returns
    FAKE_FILE_HANDLE (or NULL when fake_file.fail_open)."""
    def f_open(e):
        e.fake_file.log.append(("open", e.cstr(e.r(3)), e.cstr(e.r(4))))
        e.set_r(3, 0 if e.fake_file.fail_open else FAKE_FILE_HANDLE)

    def f_close(e):
        e.fake_file.log.append(("close", e.r(3) == FAKE_FILE_HANDLE))
        e.set_r(3, 0)

    def f_read(e):
        dst, size, count = e.r(3), e.r(4), e.r(5)
        n = (size * count) & 0xFFFFFFFF
        e.fake_file.log.append(("read", size, count, e.r(6) == FAKE_FILE_HANDLE))
        data = e.fake_file.read(n)
        if n:
            e.write(dst, data)
        e.set_r(3, count)

    def f_seek(e):
        e.fake_file.log.append(("seek", e.r(4), e.r(5), e.r(3) == FAKE_FILE_HANDLE))
        e.fake_file.seek(e.r(4), e.r(5))
        e.set_r(3, 0)

    for name, fn in (("open", f_open), ("close", f_close), ("read", f_read), ("seek", f_seek)):
        tv = fake_function(emu, fn)
        emu.write(addrs[name], struct.pack(">I", tv))
    emu.fake_file = FakeFile()


def same_values(a, b, path="", exact_nan=False):
    """Recursive comparison of dicts/lists/tuples/bytes/ints/floats; returns mismatches."""
    if isinstance(a, dict) and isinstance(b, dict):
        out = []
        for k in sorted(set(a) | set(b), key=str):
            if k not in a or k not in b:
                out.append(f"{path}{k}: missing on one side")
            else:
                out += same_values(a[k], b[k], f"{path}{k}.", exact_nan)
        return out
    if isinstance(a, (list, tuple)) and isinstance(b, (list, tuple)):
        if len(a) != len(b):
            return [f"{path[:-1]}: length {len(a)} != {len(b)}"]
        out = []
        for i, (x, y) in enumerate(zip(a, b)):
            out += same_values(x, y, f"{path}{i}.", exact_nan)
            if len(out) > 4:
                break
        return out
    if isinstance(a, float) and isinstance(b, float):
        if math.isnan(a) and math.isnan(b) and not exact_nan:
            return []
        return [] if struct.pack("<d", a) == struct.pack("<d", b) else [f"{path[:-1]}: ppc={a!r} host={b!r}"]
    if isinstance(a, (bytes, bytearray)) and isinstance(b, (bytes, bytearray)):
        if a == b:
            return []
        i = next(i for i in range(min(len(a), len(b)) + 1) if i >= min(len(a), len(b)) or a[i] != b[i])
        return [f"{path[:-1]}: bytes differ at {i} (len {len(a)}/{len(b)}): "
                f"ppc={a[i:i + 8].hex()} host={b[i:i + 8].hex()}"]
    return [] if a == b else [f"{path[:-1]}: ppc={a!r} host={b!r}"]


class FnTest:
    """Free-form differential test for object-heavy functions:
    ppc(emu, case) -> dict of observable results (it builds memory, calls, reads back);
    host(lib, case) -> dict of the same keys. Results compare with same_values."""

    def __init__(self, name, ppc, host, edge=None, rnd=None, exact_nan=False):
        self.name, self.ppc, self.host = name, ppc, host
        self._edge, self._rnd, self.exact_nan = edge or (lambda: []), rnd, exact_nan

    def edge_cases(self):
        return list(self._edge())

    def random_case(self, r):
        return self._rnd(r)

    def run_case(self, runner, case):
        emu, lib = runner.emu, runner.lib
        emu.reset_scratch()
        try:
            p = self.ppc(emu, case)
        except ppcemu.ImportCalled as ex:
            return [f"PPC side called unstubbed import {ex}"]
        h = self.host(lib, case)
        return same_values(p, h, "", self.exact_nan)


def host_asserts(lib, reset=False):
    """The host's msl_assertion_failed log (tests/shim_placeholders.cpp) as (condition,
    file, line) tuples, the same shape as emu.asserts on the PowerPC side."""
    n = ctypes.c_int32.in_dll(lib, "tpw_asserts_n").value
    cond = ((ctypes.c_char * 32) * 16).in_dll(lib, "tpw_assert_cond")
    file = ((ctypes.c_char * 64) * 16).in_dll(lib, "tpw_assert_file")
    line = (ctypes.c_int32 * 16).in_dll(lib, "tpw_assert_line")
    out = [(cond[i].value.decode("latin-1"), file[i].value.decode("latin-1"), line[i]) for i in range(n)]
    if reset:
        ctypes.c_int32.in_dll(lib, "tpw_asserts_n").value = 0
    return out
