#!/usr/bin/env python3
"""Differential test runner: original PowerPC function (Unicorn, via
../../tools/ppcemu.py and the Ghidra dumps) versus our C++ build (ctypes on
build/libtpw_testshim.dylib), same inputs, compare results and memory.

Conventions
- Inputs are described per test as a list of "cases"; each case builds the
  PowerPC-side memory (big-endian) and the host-side ctypes objects (native
  little-endian) from the SAME Python values, so endianness is handled once,
  here, and never inside the game code under test.
- Integers compare exactly. Floats compare bit-exactly by default
  (struct.pack('>f')); a test may pass `ulp=N` to allow N units in the last
  place when the original uses an instruction sequence we cannot reproduce
  bit-for-bit (documented per test).
- Memory side effects: each case lists the buffers to compare after the call
  (as (name, format) pairs); both sides are unpacked with the same struct
  format (big-endian on the PPC side, native on the host side) and compared
  value by value.

- Program: a test module may set PROGRAM (a dump directory name such as
  "engine symbols shared"); the default is the main app, "SimTheme Park".
  Each module runs in a fresh process, with a fresh runner and emulator before
  setup(). Within a module, the runner keeps one emulator per program.
- A test object may define run_case(runner, case) -> list of mismatch strings
  to take over the whole protocol (globals, float arguments, internal stubs);
  tests/tpwtest.py provides such a Test class.
- TPW_SHIM=<path> overrides the shim library (for a private build directory).
- SHIM_LIBRARY: a test module may name a per-unit library (e.g. "unit_GameSys" for
  libtpw_unit_GameSys.dylib, next to the shim library) to run against instead.

usage: python3 -I tests/difftest.py [test module names...]   (from decomp/)
"""
import ctypes
import importlib
import json
import os
import pathlib
import random
import struct
import subprocess
import sys
import time

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parent.parent          # theme-park-world-mac
sys.path.insert(0, str(HERE))
from ppcemu import PPCEmu, ImportCalled   # noqa: E402

DUMPS = pathlib.Path(os.environ.get("TPW_DUMPS", ROOT / "ghidra/dumps"))
SHIM = pathlib.Path(os.environ.get("TPW_SHIM", HERE.parent / "build/libtpw_testshim.dylib"))
DEFAULT_PROGRAM = "SimTheme Park"


class Side:
    """Helpers shared by the two sides so tests describe memory once."""

    def __init__(self, big_endian):
        self.e = ">" if big_endian else "<"

    def pack(self, fmt, *vals):
        return struct.pack(self.e + fmt, *vals)

    def unpack(self, fmt, data):
        return struct.unpack(self.e + fmt, data)


class Runner:
    def __init__(self, program=DEFAULT_PROGRAM):
        self.emus = {}
        self.use(program)
        self.libs = {}
        self.lib = self.shim()
        self.ppc = Side(True)
        self.host = Side(False)
        self.results = []

    def use(self, program):
        """Switch to the emulator for `program` (created on first use)."""
        if program not in self.emus:
            self.emus[program] = PPCEmu(DUMPS / program)
        self.program = program
        self.emu = self.emus[program]
        return self.emu

    def shim(self, name=None):
        """The shim library, or the per-unit library `name` (loaded once each)."""
        path = SHIM if name is None else SHIM.parent / f"libtpw_{name}.dylib"
        if path not in self.libs:
            self.libs[path] = ctypes.CDLL(str(path))
        return self.libs[path]

    def run_case(self, test, case):
        """test: object with .name, .ppc_name (mangled), .host_name (extern C), and
        .build(side, case) -> (args, buffers), .call_host(lib, args, buffers) -> result,
        .compare(ppc_result, host_result, ppc_bufs, host_bufs) -> list of mismatch strings.
        A test with its own run_case(runner, case) handles the whole case itself.
        """
        if hasattr(test, "run_case"):
            return test.run_case(self, case)
        emu = self.emu
        emu.reset_scratch()
        # PPC side
        p_args, p_bufs = test.build(self.ppc, case)
        ppc_addrs = {}
        for name, data in p_bufs.items():
            ppc_addrs[name] = emu.alloc(len(data), data)
        args = [ppc_addrs[a] if isinstance(a, str) else a for a in p_args]
        try:
            if getattr(test, "returns_float", False):
                p_res = emu.call_f(emu.func(test.ppc_name), args)
            else:
                p_res = emu.call(emu.func(test.ppc_name), args)
        except ImportCalled as ex:
            return [f"PPC side called unstubbed import {ex}"]
        p_after = {name: emu.read(ppc_addrs[name], len(data)) for name, data in p_bufs.items()}
        # host side
        h_args, h_bufs = test.build(self.host, case)
        h_objs = {name: ctypes.create_string_buffer(data, len(data)) for name, data in h_bufs.items()}
        h_res = test.call_host(self.lib, [h_objs[a] if isinstance(a, str) else a for a in h_args], h_objs)
        h_after = {name: bytes(h_objs[name].raw) for name in h_bufs}
        return test.compare(p_res, h_res, p_after, h_after, self.ppc, self.host, case)

    def run_test(self, test, n_random=200, seed=1):
        rnd = random.Random(seed)
        cases = list(test.edge_cases()) + [test.random_case(rnd) for _ in range(n_random)]
        fails = []
        t0 = time.time()
        for i, case in enumerate(cases):
            mism = self.run_case(test, case)
            if mism:
                fails.append((i, case, mism))
        dt = time.time() - t0
        ok = len(cases) - len(fails)
        self.results.append((test.name, ok, len(cases), dt, fails))
        print(f"{test.name}: {ok}/{len(cases)} cases match ({dt:.1f}s)" + ("" if not fails else f"  FIRST FAIL: case {fails[0][0]} {fails[0][1]} -> {fails[0][2][:3]}"))
        return not fails


def run_module(name):
    """Keep emulator memory, fakes and stub registries local to one module."""
    mod = importlib.import_module(name)
    runner = Runner(getattr(mod, "PROGRAM", DEFAULT_PROGRAM))
    runner.lib = runner.shim(getattr(mod, "SHIM_LIBRARY", None))
    if hasattr(mod, "setup"):
        mod.setup(runner)
    allok = True
    for test in mod.TESTS:
        allok &= runner.run_test(test)
    return allok, runner.results


def module_totals(results):
    return [sum(r[1] for r in results), sum(r[2] for r in results),
            sum(1 for r in results if not r[4]), len(results)]


def run_module_process(name):
    """Isolate Python globals, native libraries/allocator state and Unicorn hooks.

    Function output goes directly to stdout. Only aggregate counts cross the
    private pipe, so the public output retains exactly one TOTAL line.
    """
    read_fd, write_fd = os.pipe()
    try:
        try:
            worker = subprocess.run(
                [sys.executable, "-I", "-u", str(HERE / "difftest.py"),
                 "--module-worker", name, str(write_fd)],
                pass_fds=(write_fd,), timeout=3600)
        finally:
            os.close(write_fd)
        with os.fdopen(read_fd) as pipe:
            read_fd = None
            data = pipe.read()
        if not data:
            sys.exit(worker.returncode if worker.returncode > 0 else 1)
        return worker.returncode == 0, json.loads(data)
    finally:
        if read_fd is not None:
            os.close(read_fd)


def main():
    names = sys.argv[1:]
    if names[:1] == ["--module-worker"]:
        ok, results = run_module(names[1])
        with os.fdopen(int(names[2]), "w") as pipe:
            json.dump(module_totals(results), pipe)
        sys.exit(0 if ok else 1)
    if not names:
        names = sorted(p.stem for p in HERE.glob("test_*.py"))
    totals = [0, 0, 0, 0]
    allok = True
    for n in names:
        ok, counts = run_module_process(n)
        allok &= ok
        totals = [a + b for a, b in zip(totals, counts)]
    print(f"TOTAL: {totals[0]}/{totals[1]} cases, {totals[2]}/{totals[3]} functions fully matching")
    sys.exit(0 if allok else 1)


if __name__ == "__main__":
    main()
