#!/usr/bin/env python3
"""Unicorn harness for running original PowerPC functions out of the Ghidra
memory dumps (tools/ghidra_dump.py) for differential testing.

Layout (same addresses as the Ghidra project, so decompiler output and
emulator agree): code at 0x10000000, data (initialized + BSS) after it, the
IMPORTS block, then harness regions:
    STACK   0x7f000000 .. +1 MB      r1 starts near the top
    TRAPS   0x7e000000              one `blr` per import, hooked
    TVEC    0x7e100000              fake TVectors {trap, toc} for imports
    SCRATCH 0x7d000000 .. +4 MB      test inputs / outputs (alloc())
    RETURN  0x7fff0000              LR for the top-level call; emulation stops there

Imports: Ghidra's relocation model makes each TOC entry for an import point
at the 4-byte IMPORTS slot, and the cross-TOC glue treats that slot as the
callee's TVector (code pointer at +0, TOC at +4).  So every function-import
slot is filled with a unique trap address (a `blr` that is hooked); calling
through the glue lands on the trap, a code hook runs the Python stub
registered with stub(name, fn) (fn(emu) reads arguments from registers or
memory and writes the result), and the `blr` returns to the caller, which
reloads its own r2 from 20(r1) as the real code does.  Unstubbed imports
raise ImportCalled.  Data imports (PEF symbol class 1) get 1 KB of zeroed
scratch storage and every TOC entry that pointed at their slot is re-pointed
at that storage; emu.data_import(name) returns its address.

Floats: the PowerPC side stores IEEE754 big-endian; f1..f13 hold doubles
(single-precision arguments are passed as doubles per the PowerOpen ABI).
Results are compared bit-exactly (struct.pack) unless a test chooses a
tolerance, because the original code is compiled for exact IEEE single/
double arithmetic and our C++ build must match it.

usage (library):
    emu = PPCEmu(ROOT / "ghidra/dumps/SimTheme Park")
    emu.stub("tan", lambda e: e.set_f1(math.tan(e.f(1))))
    r3 = emu.call(emu.func("xaxis_circle_intersect__FR8Vector2dl"), args=[ptr, 100])
"""
import json
import math
import pathlib
import struct

from unicorn import (Uc, UC_ARCH_PPC, UC_MODE_32, UC_MODE_BIG_ENDIAN, UC_HOOK_CODE,
                     UC_HOOK_MEM_UNMAPPED, UC_HOOK_MEM_INVALID, UcError, UC_PROT_ALL)
from unicorn import ppc_const as P

STACK_BASE, STACK_SIZE = 0x7F000000, 0x100000
TRAPS = 0x7E000000
DATA_IMPORT_SIZE = 0x400
SCRATCH_BASE, SCRATCH_SIZE = 0x7D000000, 0x400000
RETURN_ADDR = 0x7FFF0000

GPR = [getattr(P, f"UC_PPC_REG_{i}") for i in range(32)]
FPR = [getattr(P, f"UC_PPC_REG_FPR{i}") for i in range(32)]


class ImportCalled(Exception):
    pass


def align(x, a=0x1000):
    return (x + a - 1) & ~(a - 1)


class PPCEmu:
    def __init__(self, dump_dir, verbose=False):
        self.dir = pathlib.Path(dump_dir)
        self.verbose = verbose
        self.blocks = json.loads((self.dir / "blocks.json").read_text())
        self.imports = {int(k, 16): v for k, v in json.loads((self.dir / "imports.json").read_text()).items()}
        self.toc = json.loads((self.dir / "toc.json").read_text())["toc"]
        self.functions = json.loads((self.dir / "functions.json").read_text())
        self.import_classes = self._pef_import_classes()
        self.data_imports = {}
        self.by_label = {}
        for f in self.functions:
            for l in f["labels"]:
                self.by_label[l] = f
            self.by_label[f["name"]] = f
        self.stubs = {}
        self.trap_to_import = {}
        self.calls = []            # log of (import name, r3..r6) for stubbed calls
        self.uc = Uc(UC_ARCH_PPC, UC_MODE_32 | UC_MODE_BIG_ENDIAN)
        self._map_image()
        self._map_harness()
        self.scratch_top = SCRATCH_BASE

    def _pef_import_classes(self):
        """name -> PEF import symbol class (0 code, 1 data, 2 TVector, 3 TOC, 4 glue),
        read from the staged PEF next to the dumps (ROOT/ghidra/staging/<program>)."""
        pef = self.dir.parent.parent / "staging" / self.dir.name
        classes = {}
        if not pef.exists():
            return classes
        b = pef.read_bytes()
        nsec = struct.unpack_from(">H", b, 32)[0]
        L = None
        for i in range(nsec):
            if b[40 + i * 28 + 24] == 4:
                L = struct.unpack_from(">I", b, 40 + i * 28 + 20)[0]
        if L is None:
            return classes
        (_, _, _, _, _, _, nlib, nimp, _, _, str_off, _, _, _) = struct.unpack_from(">iIiIiIIIIIIIII", b, L)
        symtab = L + 56 + nlib * 24
        for j in range(nimp):
            v = struct.unpack_from(">I", b, symtab + 4 * j)[0]
            off = L + str_off + (v & 0xFFFFFF)
            e = b.find(b"\0", off)
            classes[b[off:e].decode("latin-1")] = (v >> 24) & 0xF
        return classes

    # ------------------------------------------------------------------ memory
    def _map_image(self):
        uc = self.uc
        for b in self.blocks:
            start = b["start"]
            size = align(b["size"] + (start & 0xFFF)) if (start & 0xFFF) else align(b["size"])
            base = start & ~0xFFF
            # blocks may share a page: map lazily by page
            for page in range(base, base + size, 0x1000):
                try:
                    uc.mem_map(page, 0x1000, UC_PROT_ALL)
                except UcError:
                    pass
            if b["initialized"]:
                uc.mem_write(start, (self.dir / b["file"]).read_bytes())
        self.imports_block = next(b for b in self.blocks if b["name"] == "IMPORTS")

    def _map_harness(self):
        uc = self.uc
        uc.mem_map(STACK_BASE, STACK_SIZE)
        uc.mem_map(SCRATCH_BASE, SCRATCH_SIZE)
        uc.mem_map(RETURN_ADDR, 0x1000)
        n = max(1, len(self.imports))
        uc.mem_map(TRAPS, align(4 * n))
        data_blocks = [b for b in self.blocks if b["initialized"] and not b["execute"] and b["name"] != "IMPORTS"]
        for i, (slot, info) in enumerate(sorted(self.imports.items())):
            trap = TRAPS + 4 * i
            uc.mem_write(trap, b"\x4e\x80\x00\x20")                  # blr
            self.trap_to_import[trap] = info
            if self.import_classes.get(info["name"]) == 1:            # data import
                buf = SCRATCH_BASE + SCRATCH_SIZE - DATA_IMPORT_SIZE * (len(self.data_imports) + 1)
                self.data_imports[info["name"]] = buf
                # re-point every relocated TOC entry (data section word == slot) at the buffer
                needle = struct.pack(">I", slot)
                for b in data_blocks:
                    blob = bytes(uc.mem_read(b["start"], b["size"]))
                    pos = blob.find(needle)
                    while pos != -1:
                        if pos % 4 == 0:
                            uc.mem_write(b["start"] + pos, struct.pack(">I", buf))
                        pos = blob.find(needle, pos + 1)
                uc.mem_write(slot, struct.pack(">I", buf))
            else:
                uc.mem_write(slot, struct.pack(">I", trap))               # slot acts as the TVector
        uc.mem_write(RETURN_ADDR, b"\x4e\x80\x00\x20")
        uc.hook_add(UC_HOOK_CODE, self._on_trap, begin=TRAPS, end=TRAPS + 4 * n)
        uc.hook_add(UC_HOOK_MEM_UNMAPPED, self._on_bad_mem)
        uc.hook_add(UC_HOOK_MEM_INVALID, self._on_bad_mem)

    def _on_bad_mem(self, uc, access, address, size, value, user):
        self.last_fault = (access, address, size, value)
        return False

    def _on_trap(self, uc, address, size, user):
        info = self.trap_to_import.get(address)
        if info is None:
            return
        name = info["name"]
        self.calls.append((name, [self.r(i) for i in range(3, 7)]))
        fn = self.stubs.get(name)
        if fn is None:
            self.unstubbed = info
            uc.emu_stop()
            return
        fn(self)

    def data_import(self, name):
        return self.data_imports[name]

    def alloc(self, size, data=None, alignment=16):
        addr = (self.scratch_top + alignment - 1) & ~(alignment - 1)
        self.scratch_top = addr + size
        if self.scratch_top > SCRATCH_BASE + SCRATCH_SIZE - DATA_IMPORT_SIZE * (len(self.data_imports) + 1):
            raise MemoryError("scratch exhausted")
        if data is not None:
            self.uc.mem_write(addr, data)
        else:
            self.uc.mem_write(addr, b"\0" * size)
        return addr

    def reset_scratch(self):
        self.scratch_top = SCRATCH_BASE

    def read(self, addr, size):
        return bytes(self.uc.mem_read(addr, size))

    def write(self, addr, data):
        self.uc.mem_write(addr, data)

    def u32(self, addr):
        return struct.unpack(">I", self.read(addr, 4))[0]

    def s32(self, addr):
        return struct.unpack(">i", self.read(addr, 4))[0]

    def f32(self, addr):
        return struct.unpack(">f", self.read(addr, 4))[0]

    def cstr(self, addr, limit=4096):
        out = b""
        while len(out) < limit:
            c = self.read(addr + len(out), 1)
            if c == b"\0":
                break
            out += c
        return out.decode("latin-1")

    # ------------------------------------------------------------------ registers
    def r(self, i):
        return self.uc.reg_read(GPR[i]) & 0xFFFFFFFF

    def set_r(self, i, v):
        self.uc.reg_write(GPR[i], v & 0xFFFFFFFF)

    def f(self, i):
        """FPR i as a Python float (Unicorn returns the raw 64-bit pattern)."""
        v = self.uc.reg_read(FPR[i])
        if isinstance(v, float):
            return v
        return struct.unpack(">d", struct.pack(">Q", v & 0xFFFFFFFFFFFFFFFF))[0]

    def set_f(self, i, x):
        self.uc.reg_write(FPR[i], struct.unpack(">Q", struct.pack(">d", float(x)))[0])

    def set_f1(self, x):
        self.set_f(1, x)

    # ------------------------------------------------------------------ calling
    def func(self, name):
        f = self.by_label.get(name)
        if f is None:
            raise KeyError(name)
        return f["entry"]

    def stub(self, name, fn):
        self.stubs[name] = fn

    def call(self, entry, args=(), fargs=(), max_insns=5_000_000, timeout_us=10_000_000):
        """Call a function. args -> r3.. (ints/addresses), fargs -> f1.. (floats, each
        also consumes a GPR slot per the PowerOpen ABI: pass them positionally via
        `args` entries of type float instead if mixed ordering matters)."""
        uc = self.uc
        self.unstubbed = None
        self.last_fault = None
        self.calls = []
        uc.reg_write(P.UC_PPC_REG_MSR, 0x2000)          # MSR[FP]
        sp = STACK_BASE + STACK_SIZE - 0x1000
        # caller frame: linkage area + 8 home words + outgoing args (plenty)
        frame = 0x200
        sp -= frame
        uc.mem_write(sp, b"\0" * frame)
        gi = 3
        fi = 1
        stack_args = []
        for a in args:
            if isinstance(a, float):
                if fi <= 13:
                    self.set_f(fi, a)
                    fi += 1
                gi += 2 if gi <= 10 else 0   # double: two GPR slots (shadow)
                if gi > 11:
                    stack_args.append(("d", a))
            else:
                if gi <= 10:
                    self.set_r(gi, a)
                else:
                    stack_args.append(("i", a))
                gi += 1
        for a in fargs:
            self.set_f(fi, a)
            fi += 1
        # overflow args go at 56(r1) upwards
        off = 56
        for kind, a in stack_args:
            if kind == "i":
                uc.mem_write(sp + off, struct.pack(">I", a & 0xFFFFFFFF))
                off += 4
            else:
                uc.mem_write(sp + off, struct.pack(">d", a))
                off += 8
        self.set_r(1, sp)
        self.set_r(2, self.toc)
        self.set_r(0, 0)
        uc.reg_write(P.UC_PPC_REG_LR, RETURN_ADDR)
        try:
            uc.emu_start(entry, RETURN_ADDR, timeout=timeout_us, count=max_insns)
        except UcError as e:
            if self.unstubbed:
                raise ImportCalled(self.unstubbed["name"]) from None
            raise RuntimeError(f"emulation error {e} at pc={uc.reg_read(P.UC_PPC_REG_PC):#x} "
                               f"fault={self.last_fault}") from None
        if self.unstubbed:
            raise ImportCalled(self.unstubbed["name"])
        pc = uc.reg_read(P.UC_PPC_REG_PC)
        if pc != RETURN_ADDR:
            raise RuntimeError(f"did not return: pc={pc:#x} (instruction budget or timeout)")
        return self.r(3)

    def call_f(self, entry, args=(), fargs=()):
        self.call(entry, args, fargs)
        return self.f(1)


if __name__ == "__main__":
    import sys
    emu = PPCEmu(sys.argv[1], verbose=True)
    print(f"loaded {len(emu.functions)} functions, {len(emu.imports)} imports, toc={emu.toc:#x}")
