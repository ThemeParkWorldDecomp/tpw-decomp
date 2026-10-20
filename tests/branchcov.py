#!/usr/bin/env python3
"""Branch coverage of the ORIGINAL PowerPC code under a test module: runs the module's
tests (all, or those whose name contains one of the given substrings) with a code hook
on [START, END) and lists every conditional branch (bc) that was reached but went only
one way, the blocks never executed inside the functions that were entered, and the
functions in the range that were never entered. Use it to find what
the random and edge cases miss (switch arms, error paths, rare flags).

Some one-way branches cannot be reached from C++ and are expected: a destructor's
`this == NULL` check (C++ never calls one on NULL), the inline member destructors that
run after a clear() already freed everything, NULL checks after operator new (the
runtime's new throws instead).

usage (from decomp/tests, TPW_SHIM as for difftest.py):
  ../../venv/bin/python -I branchcov.py <module> 0xSTART 0xEND [substrings...] [-n N]
"""
import importlib
import pathlib
import struct
import sys

from capstone import Cs, CS_ARCH_PPC, CS_MODE_32, CS_MODE_BIG_ENDIAN
from unicorn import UC_HOOK_CODE

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))   # -I leaves it out
import difftest  # noqa: E402


def main():
    args = sys.argv[1:]
    n_random = 200
    if "-n" in args:
        i = args.index("-n")
        n_random = int(args[i + 1])
        del args[i:i + 2]
    mod = importlib.import_module(args[0])
    start, end = int(args[1], 16), int(args[2], 16)
    subs = args[3:]
    runner = difftest.Runner(getattr(mod, "PROGRAM", difftest.DEFAULT_PROGRAM))
    runner.lib = runner.shim(getattr(mod, "SHIM_LIBRARY", None))
    if hasattr(mod, "setup"):
        mod.setup(runner)
    emu = runner.emu
    edges = set()
    prev = [None]

    def hook(uc, address, size, user):
        if prev[0] is not None:
            edges.add((prev[0], address))
        prev[0] = address

    emu.uc.hook_add(UC_HOOK_CODE, hook, begin=start, end=end - 1)
    for t in mod.TESTS:
        if subs and not any(s in t.name for s in subs):
            continue
        runner.run_test(t, n_random=n_random)
        prev[0] = None
    md = Cs(CS_ARCH_PPC, CS_MODE_32 | CS_MODE_BIG_ENDIAN)
    code = emu.read(start, end - start)
    seen = {a for e in edges for a in e}
    names = sorted((f["entry"], f["name"]) for f in emu.functions if start <= f["entry"] < end)

    def fname(a):
        best = "?"
        for e, n in names:
            if e <= a:
                best = n
        return best

    oneway = 0
    for off in range(0, len(code), 4):
        a = start + off
        w = struct.unpack(">I", code[off:off + 4])[0]
        if w >> 26 != 16 or a not in seen:          # bc, reached
            continue
        bd = w & 0xFFFC
        if bd & 0x8000:
            bd -= 0x10000
        tgt = bd & 0xFFFFFFFF if w & 2 else (a + bd) & 0xFFFFFFFF
        taken, fall = (a, tgt) in edges, (a, a + 4) in edges
        if not (taken and fall):
            ins = next(md.disasm(code[off:off + 4], a), None)
            txt = f"{ins.mnemonic} {ins.op_str}" if ins else f"{w:#x}"
            print(f"{a:#x} {fname(a)[:40]:40s} {txt:28s} {'only taken' if taken else 'only fall-through'}")
            oneway += 1
    # Code never executed inside the functions that were entered (a branch in such a
    # block is not listed above, since it was never reached).
    unreached = 0
    for f in sorted(emu.functions, key=lambda f: f["entry"]):
        e0 = f["entry"]
        if not (start <= e0 < end) or e0 not in seen:
            continue
        run = None
        for a in range(e0, e0 + f["size"] + 4, 4):
            hit = a in seen or a >= e0 + f["size"]
            if not hit and run is None:
                run = a
            elif hit and run is not None:
                print(f"{run:#x}..{a - 4:#x} {f['name'][:40]:40s} never executed ({(a - run) // 4} instructions)")
                unreached += 1
                run = None
    print(f"one-way branches: {oneway}; unexecuted blocks: {unreached}; "
          f"functions never entered: {[n for e, n in names if e not in seen]}")


if __name__ == "__main__":
    main()
