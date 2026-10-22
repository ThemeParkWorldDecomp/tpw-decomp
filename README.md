# tpw-decomp

A decompilation of Theme Park World (Sim Theme Park) as released for the Mac in 1999 (the 2.04b3 patch, PowerPC), into C++.

Only the game's own functions are here, each translated to behave exactly as the Mac build does, bugs included. The aim is a
complete, tested decompilation that is behaviour-equivalent to the original, not byte-matching: the C++ is not meant to
compile back to the same machine code, it is meant to compute the same results. Every function is checked against the
original by a differential test, which runs the original PowerPC code in an emulator beside the C++ build and compares the
results, memory writes and calls case by case.

![functions translated](https://img.shields.io/badge/functions_translated-100.00%25-2d9f4f) ![bytes translated](https://img.shields.io/badge/bytes_translated-100.00%25-2d9f4f)

Progress: 9184 functions translated and tested (100.00% of the five programs' functions), 2743.6 KB of code (100.00% of the bytes).

This is a reference: there is no launcher, no platform layer and no fix of the game's bugs here. The playable port, which
builds this repository as a submodule and lists every deliberate change it makes, is
[tpw-port](https://gitlab.com/tpw-decomp/tpw-port).

## Nothing from the game

This repository holds no part of the game: no program, no data files, no memory dumps, no decompiler output and no text
from the game. You need your own copy of the game, and the build and the tests read what they need from it:

- the build reads six data tables (the advisor's lines and messages, the balance descriptions, the save file's legal text,
  the handheld's window scripts) from any full Mac SimTheme Park program (the CD's own is fine) at configure time
  (devtools/gen_game_tables.py; it finds each table by its content and checks the SHA-1 of every file it writes);
- the tests run the original code out of memory dumps of the programs and read some of the game's files.

## The working folder

The tests look for the game's files beside this repository:

    WORK/
      tpw-decomp/                      this repository
      extracted/
        disc-hfs/Theme Park Data/      the CD's data folder and its "SimTheme Park" program, the full Mac program the
                                       build reads the tables from (devtools/hfsdump.py unpacks the CD's HFS partition)
        wads/                          the game's .wad archives unpacked, one NAME.wad.d folder each (devtools/wad.py)
      ghidra/
        dumps/<program>/               memory dumps of each program ("SimTheme Park", "engine symbols shared", ...)
        staging/<program>              each program's PEF container (data fork), for the import symbol classes

TPW_DUMPS=<folder> names another dumps folder. A dump folder holds blocks.json (the memory blocks: name, start, size,
initialized, execute and the file of its bytes), the blocks' .bin files, imports.json (each import slot's address, library
and name), toc.json ({"toc": the TOC address}) and functions.json (each function's name, entry, size and labels). They were
made with Ghidra from the game's own programs; the scripts that make them are not part of this repository.

## Building and testing

Needs CMake 3.20, a C++17 compiler, Python 3 and, for the tests, the Python package unicorn (zlib is built from
third_party/zlib).

    cmake -S tpw-decomp -B build
    cmake --build build -j8
    cd tpw-decomp && TPW_SHIM=../build/libtpw_testshim.dylib python3 -I tests/difftest.py            # every module
    cd tpw-decomp && TPW_SHIM=../build/libtpw_testshim.dylib python3 -I tests/difftest.py test_Clock  # one module

Each tests/test_*.py module names the functions it covers and their addresses; the libraries it runs against are built
from cmake/units/ (one per module, the unit's own sources and the recording fakes of everything it calls in
tests/unit_*.cpp). devtools/mutate.py is the mutation check: it applies small changes to a unit's source and expects the
unit's module to catch each one.

## Host adaptations

The translation runs on a 64-bit little-endian host, so a few things are done the host's way where that does not change
what the game computes:

- Memory the game keeps in 32-bit fields: the Mac's pointers are 32 bits and the game stores them in its structures and
  files. Such memory comes from an arena in the low 4 GB (platform/arena.cpp) and is held as 32-bit offsets (Ptr32).
- Host structure sizes and offsets: where the game's code steps through a structure by the Mac's size or offset, the host
  uses the host structure's size and its field names (the Mac's offset is named in the comment).
- Byte order: data the game reads in big-endian order (files, sprite records, script records) is swapped where the Mac
  read it directly.
- Integer arithmetic wraps at 32 bits as on the PowerPC (-fwrapv), and floating point is never contracted
  (-ffp-contract=off), so the host rounds as the Mac did.
- Destructors: the Mac's derived destructors carry their base's body inlined and do not call the base destructor; C++
  always runs the base destructor after the derived one. Where that would repeat the work (the lobby's globe and island
  lists, the online park and city lists), the base's body is skipped when the derived one has already cleared its list.
- Fixed buffers past the Mac's own overruns: where the Mac's code writes past the end of a local buffer into stack space it
  no longer uses (the action recorder's text, the balance table's report text, the outbox's mail reader, the RLE loader,
  the clipper's work entries, the map zoom's pairs, the cross-section optimiser's used table), the host's buffer is large
  enough to hold the run, since the host's stack is laid out differently. The results in range are the Mac's.
- Reads of memory the Mac never set (for example the byte before an empty path in the ride signs' font folder, or the
  exit error box's uninitialised buffer) take a fixed value on the host; every defined case is the Mac's.

## Layout

    src/          the translation, one file per original source file of the game (src/FILES.md lists them; each
                  function names its original address)
    include/      the game's classes and structures as the translation uses them
    platform/     the few host files the tests link (the arena, the C library's helpers, the pixel unpacker)
    tests/        the differential tests: difftest.py, ppcemu.py (the emulator harness), test_*.py, unit_*.cpp, shim_*.cpp
    cmake/units/  the source list of each test's library, with the definitions it builds for real (its keep list)
    devtools/     gen_game_tables.py, mutate.py, slicer.py (the per-test copies of the src files), hfsdump.py, wad.py

## See also

- [tpw-port](https://gitlab.com/tpw-decomp/tpw-port): the playable port (launcher, platform layer, the port's fixes in its FIXES.md).

No license has been chosen yet.
