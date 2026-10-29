#!/usr/bin/env python3
"""Count the game's data tables' records in built files: the proof that a build made with TPW_TABLES_AT_CONFIGURE off
(the default) holds none of them, the launcher reading them at run time (platform/game_tables.cpp). Run it by hand; it
prints counts only, never a byte of the tables.

usage: python3 -I devtools/check_no_tables.py --program FILE BUILT_FILE...

  --program FILE   a full Mac "SimTheme Park" application (its data fork) the tables are read from, as
                   devtools/gen_game_tables.py reads them (the CD's 2.0.1 or any other build it knows)
  BUILT_FILE       a library or program of the build (libtpw.a and tpw_launcher, say)

The tables are cut into their records: the seven legal texts, the 611 response rows, the 351 message records, the 775
balance description rows, the 14 ride templates and the 48 window scripts. Each record is a key in the program's own
byte order and as the host holds it (little-endian words, the message records and balance rows in their host
structures, the texts as UTF-16). Two kinds of record are left out, since finding them says nothing about these tables:
those with fewer than five distinct byte values, and those the program also holds outside the seven tables (balance
rows that src/game/balance's own tables share, for one: that data is translated in src/ already). Per table and file
the script prints how many times its records occur; the answer for a build without the tables is 0 everywhere, and the
exit status is 1 when any count is not. A file that does hold the tables (tpw_game_tables_tests, or a
TPW_TABLES_AT_CONFIGURE build's libtpw.a) shows the check sees them.
"""
import argparse
import importlib.util
import pathlib
import struct
import sys

HERE = pathlib.Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("gen_game_tables", HERE / "gen_game_tables.py")
g = importlib.util.module_from_spec(spec)
spec.loader.exec_module(g)

RIDE_TEMPLATE_BYTES = 0x34 * 4


def swap(data, size):
    """data with each size-byte word reversed (the program's big-endian words as the host's)."""
    fmt = {2: "H", 4: "I"}[size]
    n = len(data) // size
    return struct.pack("<%d%s" % (n, fmt), *struct.unpack(">%d%s" % (n, fmt), data[:n * size]))


def records(view):
    """{table: [(the record as the program holds it, [its forms])]}."""
    blocks = {name: view.read(addr, length) for name, addr, length, _, _ in g.PARTS}
    out = {}
    legal, texts, start = blocks["legal texts"], [], 0
    for i in range(0, len(legal), 2):
        if legal[i:i + 2] == b"\0\0":
            texts.append(legal[start:i])
            start = i + 2
    out["legal texts"] = [(t, [t, swap(t, 2)]) for t in texts]
    lines = blocks["advisor lines"]
    out["advisor lines"] = [(lines[i:i + 32], [lines[i:i + 32], swap(lines[i:i + 32], 4)])
                            for i in range(0, len(lines), 32)]
    m, rows = blocks["message table"], []
    for i in range(0, len(m), 0x30):
        r = m[i:i + 0x30]
        w = struct.unpack(">12I", r)
        host = struct.pack("<4I", *w[0:4]) + bytes(16) + struct.pack("<5I", *w[7:12]) + bytes(4)
        rows.append((r, [r, swap(r, 4), host]))
    out["message table"] = rows
    b = blocks["balance rows"]
    out["balance rows"] = [(b[i:i + 0x3C], [b[i:i + 0x3C], swap(b[i:i + 4], 4) + b[i + 4:i + 36] + swap(b[i + 36:i + 0x3C], 4)])
                           for i in range(0, len(b), 0x3C)]
    rt = blocks["ride templates"]
    out["ride templates"] = [(rt[i:i + RIDE_TEMPLATE_BYTES], [rt[i:i + RIDE_TEMPLATE_BYTES], swap(rt[i:i + RIDE_TEMPLATE_BYTES], 4)])
                             for i in range(0, len(rt), RIDE_TEMPLATE_BYTES)]
    scripts = [view.read(addr, 2 * words) for addr, words, _ in g.WINDOW_SCRIPTS]
    out["window scripts"] = [(s, [s, swap(s, 2)]) for s in scripts]
    return out


def held_elsewhere(program, view, record):
    """Whether the program holds record anywhere outside the seven tables' own blocks."""
    own = [(lo + delta, hi + delta) for lo, hi, delta in view.regions]
    for base, buf in ((g.CODE_BASE, bytes(program.code)), (program.data_base, bytes(program.data))):
        i = buf.find(record)
        while i >= 0:
            at = base + i
            if not any(lo <= at and at + len(record) <= hi for lo, hi in own):
                return True
            i = buf.find(record, i + 1)
    return False


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--program", required=True)
    ap.add_argument("files", nargs="+")
    a = ap.parse_args(argv)
    program = g.Program(a.program)
    view = g.BuildView(program, HERE.parent / "include" / "AdvisorScoring.h")
    keys = {}
    for name, recs in records(view).items():
        kept = [forms for be, forms in recs if len(set(be)) >= 5 and not held_elsewhere(program, view, be)]
        keys[name] = sorted({f for forms in kept for f in forms})   # a record the table repeats is searched once
        print("%-16s %d records, %d searched (%d distinct forms)" % (name, len(recs), len(kept), len(keys[name])))
    total = 0
    for path in a.files:
        data = pathlib.Path(path).read_bytes()
        for name, k in keys.items():
            n = sum(data.count(f) for f in k)
            total += n
            print("%-16s %s: %d" % (name, path, n))
    print("total %d" % total)
    return 1 if total else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
