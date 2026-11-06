#!/usr/bin/env python3
"""Check the files devtools/gen_game_tables.py wrote into the repository (tables/ and three .inc files of include/, from
the Mac 2.0.1 CD's program).

usage: python3 -I devtools/check_game_tables.py [--root DIR] [--program FILE | --program-if-present FILE]

  --root DIR                 the repository (default: the one this script is in)
  --program FILE             also run devtools/gen_game_tables.py on FILE, a full Mac "SimTheme Park" or "Theme Park
                             World" application (its data fork), and compare its files with the committed ones
                             byte for byte; a FILE that does not exist is an error
  --program-if-present FILE  the same when FILE exists; without it only the committed files are checked (ctest's
                             game_tables test passes the Mac CD's program from ../extracted when it is there)

Each committed file must have the SHA-1 that KNOWN_OUTPUT_SHA1 in gen_game_tables.py lists for it. The script prints
one line per file and SHA-1s only, never a byte of the files; the exit status is 1 when anything differs.
"""
import argparse
import hashlib
import importlib.util
import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("gen_game_tables", HERE / "gen_game_tables.py")
g = importlib.util.module_from_spec(spec)
spec.loader.exec_module(g)


def committed_path(root, name):
    """Where the repository holds a file: the source files in tables/, the included ones in include/."""
    return root / ("tables" if name.endswith(".cpp") else "include") / name


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--root", default=str(HERE.parent))
    which = ap.add_mutually_exclusive_group()
    which.add_argument("--program")
    which.add_argument("--program-if-present")
    a = ap.parse_args(argv)
    root = pathlib.Path(a.root).resolve()
    failed = False

    for name in g.TABLES:
        path = committed_path(root, name)
        if not path.is_file():
            print("%-26s missing (%s)" % (name, path.relative_to(root)))
            failed = True
            continue
        got = hashlib.sha1(path.read_bytes()).hexdigest()
        ok = got == g.KNOWN_OUTPUT_SHA1[name]
        print("%-26s %s %s" % (name, got, "ok" if ok else "differs from the known " + g.KNOWN_OUTPUT_SHA1[name]))
        failed |= not ok

    program = a.program or a.program_if_present
    if program and not pathlib.Path(program).is_file():
        if a.program:
            print("check_game_tables.py: %s does not exist" % program)
            return 1
        print("no program at %s: the tables were not generated again" % program)
        program = None
    if program:
        try:
            texts = g.generate(pathlib.Path(program), root / "include" / "AdvisorScoring.h")
        except ValueError as e:
            print("check_game_tables.py: %s is not a full Mac build whose tables are known: %s" % (program, e))
            return 1
        for name, text in texts.items():
            path = committed_path(root, name)
            same = path.is_file() and path.read_bytes() == text.encode("utf-8")
            print("%-26s generated again from the program: %s" % (name, "the same" if same else "DIFFERENT"))
            failed |= not same

    print("game tables: %s" % ("FAILED" if failed else "ok"))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
