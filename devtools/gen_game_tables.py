#!/usr/bin/env python3
"""Write the six data tables the translation compiles in, from the player's own copy of the game.

usage: python3 -I devtools/gen_game_tables.py --program "<the 2.04b3 SimTheme Park application>" --out DIR

The tables are the original program's data, so they are not in this repository: CMake runs this script at
configure time (TPW_GAME_PROGRAM, see CMakeLists.txt) and compiles what it writes into DIR:

  advisor_lines.cpp           the advisor's response table, 611 rows of 0x20 bytes at 0x10278ce4
  AdvisorScoring_table.cpp    the advisor message table, 351 records of 0x30 bytes at 0x1027efa4 (and record -1)
  AdvisorBalance_table.cpp    the description rows of the advisor balance table, 0x3c bytes each at 0x1028318c
  AdvisorScorerSources.cpp    the advisor scorers, 351 pointers to member of 12 bytes at 0x1027df30 (named by include/AdvisorScoring.h)
  SaveLegalText.inc           the seven legal texts of the save file header, UTF-16 at 0x10249f40
  WindowScripts.inc           the window scripts of the park panels the handheld opens (WINDOW_SCRIPTS: address, words, array)

The program is the data fork of the "SimTheme Park" application of the 2.04b3 patch, a PowerPC PEF container.
The addresses are the decompile's: the code section at 0x10000000, the data section right after it on a 16-byte
boundary (0x1025fcc0), unpacked and relocated as the Code Fragment Manager does, imports from 0x103bc8b0 in
order. Only that build has these tables at these addresses, so the file's SHA-1 is checked first.
"""
import argparse
import re
import hashlib
import pathlib
import struct
import sys

PROGRAM_SHA1 = "ce184baca37824def7faa1fb033840a2ce273ef9"
CODE_BASE = 0x10000000
IMPORTS_BASE = 0x103BC8B0


# ---- the PEF container

def _u32(b, o):
    return struct.unpack_from(">I", b, o)[0]


def _arg(b, p):
    v = 0
    while True:
        c = b[p]
        p += 1
        v = (v << 7) | (c & 0x7F)
        if not c & 0x80:
            return v, p


def unpack_pidata(b, size):
    """A pattern-initialised data section, unpacked to `size` bytes."""
    out = bytearray()
    p = 0
    while p < len(b):
        op, count = b[p] >> 5, b[p] & 0x1F
        p += 1
        if count == 0:
            count, p = _arg(b, p)
        if op == 0:                     # zero
            out += bytes(count)
        elif op == 1:                   # block copy
            out += b[p:p + count]
            p += count
        elif op == 2:                   # repeated block
            rep, p = _arg(b, p)
            out += b[p:p + count] * (rep + 1)
            p += count
        elif op == 3:                   # repeated block interleaved with copied blocks
            custom, p = _arg(b, p)
            rep, p = _arg(b, p)
            common = b[p:p + count]
            p += count
            out += common
            for _ in range(rep):
                out += b[p:p + custom]
                p += custom
                out += common
        elif op == 4:                   # zeros interleaved with copied blocks
            custom, p = _arg(b, p)
            rep, p = _arg(b, p)
            out += bytes(count)
            for _ in range(rep):
                out += b[p:p + custom]
                p += custom
                out += bytes(count)
        else:
            raise ValueError("pidata opcode %d" % op)
    return bytes(out) + bytes(size - len(out))


class Program:
    """The code section as stored and the data section unpacked and relocated."""

    def __init__(self, path):
        f = pathlib.Path(path).read_bytes()
        if f[:12] != b"Joy!peffpwpc":
            raise ValueError("%s is not a PowerPC PEF container (give the data fork of the application)" % path)
        sections = []
        for i in range(struct.unpack_from(">H", f, 0x20)[0]):
            _, _, total, unpacked, length, offset, kind = struct.unpack_from(">iIIIIIB", f, 40 + 28 * i)
            sections.append(dict(total=total, unpacked=unpacked, kind=kind, raw=f[offset:offset + length]))
        self.code_index = next(i for i, s in enumerate(sections) if s["kind"] in (0, 3))
        self.data_index = next(i for i, s in enumerate(sections) if s["kind"] in (1, 2))
        code, data = sections[self.code_index], sections[self.data_index]
        self.code = code["raw"]
        self.data_base = (CODE_BASE + code["total"] + 15) & ~15
        self.bases = {self.code_index: CODE_BASE, self.data_index: self.data_base}
        if data["kind"] == 2:
            self.data = bytearray(unpack_pidata(data["raw"], data["unpacked"]))
        else:
            self.data = bytearray(data["raw"] + bytes(data["unpacked"] - len(data["raw"])))
        self._relocate(next(s for s in sections if s["kind"] == 4)["raw"])

    def _relocate(self, L):
        nlibs, nsyms, nrel, instr_off = struct.unpack_from(">IIII", L, 24)
        headers = 56 + 24 * nlibs + 4 * nsyms
        d, bases = self.data, self.bases
        for r in range(nrel):
            sect, _, n, first = struct.unpack_from(">HHII", L, headers + 12 * r)
            if sect != self.data_index:
                continue
            ins = struct.unpack_from(">%dH" % n, L, instr_off + first)
            st = dict(pos=0, imp=0, c=bases[self.code_index], d=bases[self.data_index])

            def add(v):
                struct.pack_into(">I", d, st["pos"], (_u32(d, st["pos"]) + v) & 0xFFFFFFFF)

            def word(v):
                add(v)
                st["pos"] += 4

            def repeat(k, blocks, times):
                for _ in range(times):
                    j = k - blocks
                    while j < k:
                        j += run(j)

            def run(k):
                h = ins[k]
                if h >> 14 == 0:                                    # by section D, after a skip
                    st["pos"] += 4 * ((h >> 6) & 0xFF)
                    for _ in range(h & 0x3F):
                        word(st["d"])
                    return 1
                if h >> 13 == 0b010:                                # runs
                    sub = (h >> 9) & 0xF
                    for _ in range((h & 0x1FF) + 1):
                        if sub == 0:
                            word(st["c"])
                        elif sub == 1:
                            word(st["d"])
                        elif sub in (2, 3):                         # transition vectors of 12 and 8 bytes
                            at = st["pos"]
                            word(st["c"])
                            word(st["d"])
                            st["pos"] = at + (12 if sub == 2 else 8)
                        elif sub == 4:                              # virtual table entries of 8 bytes
                            word(st["d"])
                            st["pos"] += 4
                        elif sub == 5:                              # a run of imports
                            word(IMPORTS_BASE + 4 * st["imp"])
                            st["imp"] += 1
                        else:
                            raise ValueError("relocation run %d" % sub)
                    return 1
                if h >> 13 == 0b011:                                # small index forms
                    sub, idx = (h >> 9) & 0xF, h & 0x1FF
                    if sub == 0:
                        word(IMPORTS_BASE + 4 * idx)
                        st["imp"] = idx + 1
                    elif sub == 1:
                        st["c"] = bases[idx]
                    elif sub == 2:
                        st["d"] = bases[idx]
                    elif sub == 3:
                        word(bases[idx])
                    else:
                        raise ValueError("relocation small index %d" % sub)
                    return 1
                if h >> 12 == 0b1000:                               # advance
                    st["pos"] += (h & 0xFFF) + 1
                    return 1
                if h >> 12 == 0b1001:                               # small repeat
                    repeat(k, ((h >> 8) & 0xF) + 1, (h & 0xFF) + 1)
                    return 1
                arg = ((h & 0x3FF) << 16) | ins[k + 1] if k + 1 < len(ins) else None
                if h >> 10 == 0b101000:                             # set position
                    st["pos"] = arg
                elif h >> 10 == 0b101001:                           # by import, large index
                    word(IMPORTS_BASE + 4 * arg)
                    st["imp"] = arg + 1
                elif h >> 10 == 0b101100:                           # large repeat
                    repeat(k, ((h >> 6) & 0xF) + 1, (((h & 0x3F) << 16) | ins[k + 1]) + 1)
                elif h >> 10 == 0b101101:                           # by or set section, large index
                    sub, idx = (h >> 6) & 0xF, ((h & 0x3F) << 16) | ins[k + 1]
                    if sub == 0:
                        word(bases[idx])
                    elif sub == 1:
                        st["c"] = bases[idx]
                    elif sub == 2:
                        st["d"] = bases[idx]
                    else:
                        raise ValueError("relocation large section %d" % sub)
                else:
                    raise ValueError("relocation opcode 0x%04x" % h)
                return 2

            k = 0
            while k < len(ins):
                k += run(k)

    def read(self, addr, size):
        if CODE_BASE <= addr and addr + size <= CODE_BASE + len(self.code):
            return self.code[addr - CODE_BASE:addr - CODE_BASE + size]
        o = addr - self.data_base
        if 0 <= o and o + size <= len(self.data):
            return bytes(self.data[o:o + size])
        raise ValueError("0x%08x is outside the program's sections" % addr)


# ---- the tables

GENERATED = "// Generated by devtools/gen_game_tables.py from the player's own 2.04b3 \"SimTheme Park\" at configure time; not in the repository."


def s32(v):
    return v - (1 << 32) if v & 0x80000000 else v


def advisor_lines(p):
    table, rows = 0x10278CE4, 611
    w = [struct.unpack(">8I", p.read(table + 0x20 * r, 0x20)) for r in range(rows)]
    assert w[610][0] == 9999 and all(r[0] != 9999 for r in w[:610]), "the response table's terminator row is not where expected"
    out = ["// advisor_lines.cpp: the advisor's response table (0x10278ce4, 611 rows of 0x20 bytes: 610 ids from 0 to 612 in no strict order, 396, 397 and 551 absent,",
           "// then the terminator row, all words 9999).",
           GENERATED,
           '#include "Advisor.h"',
           "",
           "// 0x10278ce4. The columns are AdvisorLine's members in order.",
           "AdvisorLine gAdvisorLines[611] = {"]
    for r in w:
        out.append("    {%d, %d, %d, %d, 0x%08x, %d, %d, %d}," % (s32(r[0]), s32(r[1]), s32(r[2]), s32(r[3]), r[4], s32(r[5]), s32(r[6]), s32(r[7])))
    out.append("};")
    return out


def advisor_scoring(p):
    base, count = 0x1027EFA4, 351
    rows = [struct.unpack(">12I", p.read(base + i * 0x30, 0x30)) for i in range(count)]
    assert all(w[4:7] == (0, 0, 0) for w in rows), "the scorer slot is not empty (a static initialiser fills it at run time)"
    b = [s32(x) for x in struct.unpack(">12I", p.read(base - 0x30, 0x30))]
    out = ["// The advisor message table, 351 records of 0x30 bytes at 0x1027efa4 .. 0x10283174 (TOC slot 0x1026076c), with the 0x30 bytes",
           "// before it (0x1027ef74) as record -1 and a zero record after it. The scorer slot (Mac +0x10, 12 bytes) is empty in the file: the",
           "// static initialiser of the scorers fills it with a pointer to member per record, so it is null here.",
           GENERATED,
           '#include "AdvisorScoring.h"',
           "",
           "// {mSkipped, mMessage, mCategory, mMode, mScorer, mMaxDuplicates, mFirstResponse, mNumResponses, mCycling, mWord2c}",
           "AdvisorMessageRecord gAdvisorMessageBlock[kAdvisorMessageCount + 2] = {",
           "    // record -1: the file holds a block of pointers to member here; the lookup of message -1 finds it (its number word is -1)",
           "    {%d, %d, 0x%x, %d, nullptr, %d, 0x%x, %d, %d, 0x%x}," % (b[0], b[1], b[2], b[3], b[7], b[8], b[9], b[10], b[11])]
    for w in rows:
        out.append("    {%d, %d, %d, %d, nullptr, %d, %d, %d, %d, 0x%x}," % (w[0], w[1], w[2], w[3], w[7], w[8], w[9], w[10], w[11]))
    out.append("    {0, 0, 0, 0, nullptr, 0, 0, 0, 0, 0},   // the spare record behind the table (see AdvisorScoring.h)")
    out.append("};")
    return out


def advisor_balance(p):
    a, desc = 0x1028318C, []
    while True:
        raw = p.read(a, 0x3C)
        t = struct.unpack(">i", raw[:4])[0]
        name = raw[4:36].split(b"\0")[0].decode("ascii")
        mi, ma, fi, fa, ac, fl = struct.unpack(">iiffiI", raw[36:60])
        assert fi == 0.0 and fa == 0.0, "a balance description row has float bounds"
        desc.append((t, name, mi, ma, ac, fl))
        a += 0x3C
        if t == 12:
            break
        assert len(desc) < 2000, "no type 12 row ends the balance description table"
    out = ["// The description rows of the advisor balance table (CAdvisorBalance), %d rows of 0x3c bytes at 0x1028318c, ending with the" % len(desc),
           "// type 12 row.",
           GENERATED,
           '#include "AdvisorScoring.h"',
           "",
           "const BalanceDescription gAdvisorBalanceDescriptions[%d] = {" % len(desc)]
    for t, name, mi, ma, ac, fl in desc:
        out.append('    {%d, "%s", %d, %d, 0.0f, 0.0f, %d, %du},' % (t, name, mi, ma, ac, fl))
    out.append("};")
    return out


def save_legal_text(p):
    langs = ["English", "Spanish", "Italian", "Swedish", "German", "French", "Japanese"]
    a, texts = 0x10249F40, []
    for lang in langs:
        start, units = a, []
        while True:
            u = struct.unpack(">H", p.read(a, 2))[0]
            a += 2
            if u == 0:
                break
            units.append(u)
        texts.append((lang, start, units))
    out = ["// The seven legal texts of the save file header (language index 0 English, 1 Spanish, 2 Italian, 3 Swedish, 4 German, 5 French,",
           "// 6 Japanese), as the Mac image holds them at 0x10249f40.. : UTF-16 code units, each text ends with a 0 here. The Japanese text keeps",
           "// one Shift-JIS code unit per element (all below 0x100). The lengths are %s units." % ", ".join(str(len(u)) for _, _, u in texts),
           GENERATED]
    for i, (lang, start, units) in enumerate(texts):
        out += ["", "// %d %s, %d units (image address 0x%x)" % (i, lang, len(units), start), "static const char16_t kSaveLegalText%d[] = {" % i]
        cells = ["0x%04x," % u for u in units + [0]]
        out += ["    " + " ".join(cells[j:j + 12]) for j in range(0, len(cells), 12)]
        out.append("};")
    return out


def advisor_scorers(p):
    """The scorer sources: 351 pointers to member, {this delta 0, virtual index -1, TVector} each, at 0x1027df30. The TVector's
    first word is the scorer's code address, which include/AdvisorScoring.h names (every CAdvisorScoring::_ADVISOR_* declaration
    carries its address), so the host table holds real pointers to member; a scorer nobody has translated is a launcher stand-in."""
    header = pathlib.Path(__file__).resolve().parent.parent / "include" / "AdvisorScoring.h"
    names = {}
    for line in header.read_text(encoding="utf-8").splitlines():
        m = re.match(r"\s*u?int32_t\s+(_ADVISOR_\w+)\(\);\s*//\s*(0x[0-9a-fA-F]{8})", line)
        if m:
            address = int(m.group(2), 16)
            assert names.get(address, m.group(1)) == m.group(1), "two scorers at 0x%08x in %s" % (address, header)
            names[address] = m.group(1)
    base, count = 0x1027DF30, 351
    out = ["// The advisor scorer sources, 351 pointers to member of 12 bytes at 0x1027df30 .. 0x1027f9a4 ({this delta 0, virtual index",
           "// -1, TVector} each), which the static initialiser of AdvisorScoring.cpp copies into the message table's scorer slots. Each",
           "// TVector's code address is named by its declaration in include/AdvisorScoring.h.",
           GENERATED,
           '#include "AdvisorScoring.h"',
           "",
           "AdvisorScorer gAdvisorScorerSources[kAdvisorMessageCount] = {"]
    missing = []
    for k in range(count):
        delta, vindex, tvector = struct.unpack(">3I", p.read(base + k * 12, 12))
        assert delta == 0 and vindex == 0xFFFFFFFF, "scorer %d is not a plain pointer to member" % k
        code = struct.unpack(">I", p.read(tvector, 4))[0]
        name = names.get(code)
        if name is None:
            missing.append("%d: 0x%08x" % (k, code))
            continue
        out.append("    &CAdvisorScoring::%s,%s// %d: 0x%08x" % (name, " " * max(1, 58 - len(name)), k, code))
    if missing:
        sys.exit("gen_game_tables.py: scorers with no declaration in %s (add them, with their addresses): %s" % (header, ", ".join(missing)))
    out.append("};")
    return out


# The park panels' window scripts the handheld's buttons build: (address, words, the array that holds them). The word count is the
# walk's (below), checked so a wrong address stops the configure instead of building a broken panel.
WINDOW_SCRIPTS = [
    (0x102A868C, 245, "gAllKidsScript"),
    (0x102A8884, 235, "gAllItemsScript"),
    (0x102A8A5A, 116, "gAllRidesScript"),
    (0x102A8C3C, 89, "gAllFeaturesScript"),
    (0x102A8D0E, 441, "gAllStaffScript"),
    (0x102A96A8, 198, "gEntryPriceScript"),
    (0x102A9BEC, 380, "gFinanceScript"),
    (0x102A9EE8, 538, "gHirePanelScript"),
    (0x102AA4A4, 458, "gLoansScript"),
    (0x102AB734, 572, "gParkStatusScript"),
    (0x102AC35A, 924, "gResearchPanelScript"),
    (0x102ACA9C, 541, "gRideListScript"),
    (0x102ADC20, 674, "gStaffCostsScript"),
    (0x102B2366, 674, "gMapPanelScript"),
    # Unit v5L42: the scripts the launcher had as zero stand-ins (src/ui_window_scripts.cpp) and those its files held as two-word
    # placeholders. Each address is the one its building function loads from the TOC; each walk ends where the next named data begins
    # (the year end panel's 28 bytes before it are RTTI pointers, not script).
    (0x102A772C, 21, "gCongratsScript"),
    (0x102A7756, 21, "gBankruptScript"),
    (0x102A8B42, 125, "gAllShowsScript"),
    (0x102AA330, 186, "gLoadSaveScript"),
    (0x102AADE4, 1090, "gOptionsPanelScript"),
    (0x102AC1E6, 184, "gAddressBookDescription"),
    (0x102AF524, 478, "gYearEndScript"),
    (0x102AFE24, 43, "gProgBarScript"),
    (0x102B0288, 58, "gCityInfoOverlayScript"),
    (0x102B05C4, 311, "gOutboxPanelData"),
    (0x102B0E58, 279, "gOnlineParkInfoScript"),
]


def window_script_words(p, address):
    """The words of the window script at address, as far as InterfaceWindow::Build reads them (src/InterfaceWindow_script.cpp
    describes the records): the walk follows the nesting and the window kinds that records 9 and 10 depend on, and ends with the
    record that ends the top level."""
    pos = [0]

    def word():
        v = struct.unpack(">h", p.read(address + 2 * pos[0], 2))[0]
        pos[0] += 1
        return v

    def words(n):
        for _ in range(n):
            word()

    def level(kind):
        while True:
            r = word()
            if r < 0 or r > 0x12 or r == 5:
                return
            child = None
            if r == 0:
                child = word()
                words(8)                                  # attributes, id, rectangle
            elif r in (1, 2, 17, 18, 13):
                words(2)
            elif r == 3:
                words(4)
            elif r == 4:
                shape = word()
                if shape == 4:
                    words(2 * word())
                else:
                    words({1: 4, 2: 3, 3: 4}.get(shape, 0))
            elif r in (6, 7, 8, 14, 15):
                words(4)
                child = 2
            elif r in (9, 10):
                if kind in (4, 7):                        # a list box or a list control; elsewhere the record has no words
                    words(4)
                    child = 3 if r == 9 else None
            elif r == 11:
                words(2 * word())
            elif r == 12:
                words(5)
                child = 2
            elif r == 16:
                words(5)
                child = 6
            if child is not None:
                level(child)
            assert pos[0] < 4096, "the window script at 0x%08x does not end" % address

    level(-1)
    return list(struct.unpack(">%dh" % pos[0], p.read(address, 2 * pos[0])))


def window_scripts(p):
    out = ["// The park panels' window scripts, as the Mac image holds them (signed 16-bit records, see src/InterfaceWindow_script.cpp): one",
           "// macro per script, named by its image address, that the array's definition in src expands.",
           GENERATED]
    for address, count, name in WINDOW_SCRIPTS:
        w = window_script_words(p, address)
        assert len(w) == count, "the window script %s at 0x%08x has %d words, not %d" % (name, address, len(w), count)
        out += ["", "// %s, %d words" % (name, count), "#define TPW_WINDOW_SCRIPT_%08X \\" % address]
        rows = [", ".join(str(v) for v in w[j:j + 16]) for j in range(0, len(w), 16)]
        out += ["    %s%s" % (row, ", \\" if k + 1 < len(rows) else "") for k, row in enumerate(rows)]
    return out


TABLES = {"advisor_lines.cpp": advisor_lines, "AdvisorScoring_table.cpp": advisor_scoring,
          "AdvisorBalance_table.cpp": advisor_balance, "AdvisorScorerSources.cpp": advisor_scorers,
          "SaveLegalText.inc": save_legal_text, "WindowScripts.inc": window_scripts}


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--program", required=True, help='the 2.04b3 "SimTheme Park" application (its data fork)')
    ap.add_argument("--out", required=True, help="the folder to write the six files into")
    a = ap.parse_args(argv)
    path = pathlib.Path(a.program)
    if not path.is_file():
        sys.exit("gen_game_tables.py: %s does not exist (set TPW_GAME_PROGRAM to the 2.04b3 \"SimTheme Park\" application)" % path)
    sha1 = hashlib.sha1(path.read_bytes()).hexdigest()
    if sha1 != PROGRAM_SHA1:
        sys.exit("gen_game_tables.py: %s has SHA-1 %s, not the 2.04b3 build's %s" % (path, sha1, PROGRAM_SHA1))
    p = Program(path)
    out = pathlib.Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    for name, make in TABLES.items():
        text = "\n".join(make(p)) + "\n"
        f = out / name
        if not f.exists() or f.read_text(encoding="utf-8") != text:
            f.write_text(text, encoding="utf-8")


if __name__ == "__main__":
    main(sys.argv[1:])
