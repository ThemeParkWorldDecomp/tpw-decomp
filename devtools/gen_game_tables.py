#!/usr/bin/env python3
"""Write the seven data tables the translation compiles in, from the player's own copy of the game.

usage: python3 -I devtools/gen_game_tables.py --program "<a full Mac SimTheme Park / Theme Park World application>" --out DIR

The tables are the original program's data, so they are not in this repository: CMake runs this script at
configure time (TPW_GAME_PROGRAM, see CMakeLists.txt) and compiles what it writes into DIR:

  advisor_lines.cpp           the advisor's response table, 611 rows of 0x20 bytes at 0x10278ce4
  AdvisorScoring_table.cpp    the advisor message table, 351 records of 0x30 bytes at 0x1027efa4 (and record -1)
  AdvisorBalance_table.cpp    the description rows of the advisor balance table, 0x3c bytes each at 0x1028318c
  AdvisorScorerSources.cpp    the advisor scorers, 351 pointers to member of 12 bytes at 0x1027df30 (named by include/AdvisorScoring.h)
  SaveLegalText.inc           the seven legal texts of the save file header, UTF-16 at 0x10249f40
  WindowScripts.inc           the window scripts of the park and status panels (WINDOW_SCRIPTS: address, words, array)
  RideTemplates.inc           the 14 ride templates of BMP_CreateRide, 0xd0 bytes each at 0x1029be00 (gRideTemplates)

The program is the data fork of a "SimTheme Park" or "Theme Park World" application, a PowerPC PEF container. Any full Mac
build will do (the CD's own 2.0.1, the 2.04b3 patch's, and the 2.1, 2.1.1, 2.1.2 and 2.1.4 releases): the tables are
byte for byte the same in all of them, only at other addresses. The addresses in the code below are the decompile's (the
2.04b3 build's: the code section at 0x10000000, the data section right after it on a 16-byte boundary, unpacked and
relocated as the Code Fragment Manager does, imports from 0x103bc8b0 in order). Each table is found in the program by its
content: a table of the 2.04b3 build is a block of bytes whose SHA-1 is listed below (PARTS, WINDOW_HASHES), found by the SHA-1
of its first 32 bytes and then checked whole. The scorers' pointers differ by build, so they are followed instead: the
scorer table sits with the message table, its TVectors are 241 consecutive 8-byte records, and each scorer function is named
by its rank among the 241 (they lie in the same order in every build; include/AdvisorScoring.h declares them with their
2.04b3 addresses). The seven files written must then have the SHA-1s in KNOWN_OUTPUT_SHA1, or nothing is written and the
script stops: a program that is not one of those builds cannot pass quietly.
"""
import argparse
import re
import hashlib
import pathlib
import struct
import sys

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
           "BalanceDescription gAdvisorBalanceDescriptions[%d] = {" % len(desc)]
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


# The park panels' window scripts the handheld's buttons build, and those of the status panels and their name and upgrade panels
# (unit v5L31): (address, words, the array that holds them). The word count is the walk's (below), checked so a wrong address stops
# the configure instead of building a broken panel.
WINDOW_SCRIPTS = [
    (0x102A4758, 500, "gVisitorStatusScript"),
    (0x102A8530, 134, "gEditNamePanelScript"),
    (0x102A868C, 245, "gAllKidsScript"),
    (0x102A8884, 235, "gAllItemsScript"),
    (0x102A8A5A, 116, "gAllRidesScript"),
    (0x102A8C3C, 89, "gAllFeaturesScript"),
    (0x102A8D0E, 441, "gAllStaffScript"),
    (0x102A908C, 365, "gBogStatusScript"),
    (0x102A96A8, 198, "gEntryPriceScript"),
    (0x102A9834, 134, "gEditParkNameScript"),
    (0x102A9948, 286, "gFeatureStatusScript"),
    (0x102A9BEC, 380, "gFinanceScript"),
    (0x102A9EE8, 538, "gHirePanelScript"),
    (0x102AA4A4, 458, "gLoansScript"),
    (0x102AB734, 572, "gParkStatusScript"),
    (0x102AC35A, 924, "gResearchPanelScript"),
    (0x102ACA9C, 541, "gRideListScript"),
    (0x102ACEF0, 768, "gRideStatusScript"),
    (0x102AD4F0, 117, "gUpgradePanelScript"),
    (0x102AD644, 700, "gShopStatusDescription"),
    (0x102ADC20, 674, "gStaffCostsScript"),
    (0x102AE3E8, 701, "gShowStatusScript"),
    (0x102AE9C8, 460, "gStaffRoomScript"),
    (0x102AEDD0, 462, "gStaffStatusScript"),
    (0x102AF16C, 109, "gStaffEditNameScript"),
    (0x102AF2AC, 266, "gUpgradeStatusScript"),
    (0x102B2366, 674, "gMapPanelScript"),
    # Unit v5L42: the scripts the launcher had as zero stand-ins (src/game/ui/ui_allStaffPanel.cpp, src/game/ui/ui_parkStatus.cpp) and those its files held as two-word
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
    """The words of the window script at address, as far as InterfaceWindow::Build reads them (src/game/ui/InterfaceWindow.cpp
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
    out = ["// The park panels' window scripts, as the Mac image holds them (signed 16-bit records, see src/game/ui/InterfaceWindow.cpp): one",
           "// macro per script, named by its image address, that the array's definition in src expands.",
           GENERATED]
    for address, count, name in WINDOW_SCRIPTS:
        w = window_script_words(p, address)
        assert len(w) == count, "the window script %s at 0x%08x has %d words, not %d" % (name, address, len(w), count)
        out += ["", "// %s, %d words" % (name, count), "#define TPW_WINDOW_SCRIPT_%08X \\" % address]
        rows = [", ".join(str(v) for v in w[j:j + 16]) for j in range(0, len(w), 16)]
        out += ["    %s%s" % (row, ", \\" if k + 1 < len(rows) else "") for k, row in enumerate(rows)]
    return out


def ride_templates(p):
    """The ride templates BMP_CreateRide copies into a new ride: 14 records of 0xd0 bytes (52 words) at 0x1029be00, the kinds -1 to -14.
    They are plain integers (no word is a program address), so the host table holds the Mac's values."""
    words = struct.unpack(">%dI" % (14 * 0x34), p.read(0x1029BE00, 14 * 0xD0))
    out = ["// The 14 ride templates of BMP_CreateRide (kind -1 to -14), 52 words each, as the Mac image holds them at 0x1029be00..0x1029c960.",
           "// The macro is the initialiser of gRideTemplates in src/game/GameVariables.cpp.",
           GENERATED, "", "#define TPW_RIDE_TEMPLATES \\"]
    rows = []
    for t in range(14):
        for j in range(0, 0x34, 8):
            rows.append(", ".join("0x%08x" % w for w in words[t * 0x34 + j:t * 0x34 + min(j + 8, 0x34)]))
    out += ["    %s%s" % (row, ", \\" if k + 1 < len(rows) else "") for k, row in enumerate(rows)]
    return out


TABLES = {"advisor_lines.cpp": advisor_lines, "AdvisorScoring_table.cpp": advisor_scoring,
          "AdvisorBalance_table.cpp": advisor_balance, "AdvisorScorerSources.cpp": advisor_scorers,
          "SaveLegalText.inc": save_legal_text, "WindowScripts.inc": window_scripts, "RideTemplates.inc": ride_templates}


# ---- finding the tables in any build

# The SHA-1 of each of the seven files this script writes; the same for every full Mac build (2.0.1, 2.04b3, 2.1, 2.1.1, 2.1.2,
# 2.1.4). A change of include/AdvisorScoring.h that renames a scorer changes AdvisorScorerSources.cpp: update its entry then.
KNOWN_OUTPUT_SHA1 = {
    "advisor_lines.cpp": "49b974cb42f7d1eff627a24b630e0799b7723339",
    "AdvisorBalance_table.cpp": "975c982b9a89432b2d9a23167fca2535c3195f83",
    "AdvisorScorerSources.cpp": "da5b2c3df9bac53f4040a90c1baab0b3c377a3ba",
    "AdvisorScoring_table.cpp": "1cbc72c3e2d4d8bd2303e1fb415e4dd55f7ba3f2",
    "SaveLegalText.inc": "c29bf03a884224a571e4b9f7b1ba862e58ee1441",
    "WindowScripts.inc": "3c06b6192bc76bac990e70910da4f26ce4fb69e4",
    "RideTemplates.inc": "dffca3874e4ccb2414a33d104a703e489ade663e",
}

ANCHOR = 32                     # bytes hashed to find a block; the whole block's SHA-1 then confirms it
SCORERS, SCORER_COUNT = 0x1027DF30, 351
MESSAGES = 0x1027EFA4
TVECTORS_FIRST, TVECTORS_END = 0x10265800, 0x10265F8C     # the scorers' TVectors in the 2.04b3 build (241 of 8 bytes)

# (name, 2.04b3 address, length, SHA-1 of the first ANCHOR bytes, SHA-1 of all of it) of the blocks without pointers
PARTS = [
    ("legal texts", 0x10249F40, 6518, "1861c0241c6ece2fa2ce93280baef0f027492229", "668f087e18b1a207cb4af5b09030f4e440449b41"),
    ("advisor lines", 0x10278CE4, 19552, "30b116a942e5bdd1264390ffefa7135b5451bc6f", "2c6160aade733840aaf39984bee905a36ebc8a96"),
    ("message table", 0x1027EFA4, 16848, "37c1f7292dc7ffa6e939d8057152e2697aa70538", "376addbbbfe47d7d1b4a727779c3648a1140d998"),
    ("balance rows", 0x1028318C, 46500, "de8a847bff8c343d69b853a215e6ee775ef2ef96", "6b7b5c4e0e479129d1dd44bdb45f9ea486ecd8c9"),
    ("ride templates", 0x1029BE00, 2912, "015033c1bf6951e5e0fab1bc402e6a43a68ddaff", "c70b59faf6ccfcb5d9bbd1d4000846471300a5a6"),
]

# window script address: (SHA-1 of the first ANCHOR bytes, SHA-1 of the whole script, its place among the identical copies by
# address, the number of identical copies in the 2.04b3 build). The length is WINDOW_SCRIPTS'.
WINDOW_HASHES = {
    0x102A4758: ("69051fed3bedfa47d0ab7dbc48fb01a1f6f7e5cf", "151ee73970154e19e3c49fe11f43cca7a76826ae", 0, 1),
    0x102A8530: ("483299a14aaa6d36056363cbba2941c1f5d3c463", "df855f92ac20f4670c3c04f2f45e5039d54f7ddf", 0, 2),
    0x102A868C: ("deb614b0b6e070fa0cddbd501524e368054a82b4", "fdb30562b2c1fb419a2c80dc94eda8f1586cf789", 0, 1),
    0x102A8884: ("9bf6071a9cb11e36605c5fd71b3270b3edc6f691", "dceb22a07c1b0081e3a94a0811005ac50b302d5f", 0, 1),
    0x102A8A5A: ("f919cf9d3564eab5af385464a394529784165295", "057beb472c16ecbc57a56b47cf5382f8b0c073a1", 0, 1),
    0x102A8C3C: ("f919cf9d3564eab5af385464a394529784165295", "b1d9fac38d56f125828f3b73e3a6ee320676888f", 0, 1),
    0x102A8D0E: ("4278167f4f959cf5e8748cca562d87064614a6ac", "21ef6f71cd52f81dfb93193b033544269feb9046", 0, 1),
    0x102A908C: ("ddb95898cb254742996b4e8b26a8d60054ff0823", "78dc57135a2800d279964fd42d6e0ece8f17e871", 0, 1),
    0x102A96A8: ("6534a23c1aaf703816baebc7db9e0072153a4ef0", "71ed6b1bd7e30566ac07cb92db64a78080eee9bf", 0, 1),
    0x102A9834: ("483299a14aaa6d36056363cbba2941c1f5d3c463", "df855f92ac20f4670c3c04f2f45e5039d54f7ddf", 1, 2),
    0x102A9948: ("7ea6a67c59e94483d7ea33c2be739d41686b1600", "b87d1a90bf6aa7f9def347bf236d2b44d62af97b", 0, 1),
    0x102A9BEC: ("81a0aa9a9d7ca52414e392d3bd51ade5c9b64e15", "ef1078d00b2c1d0f72fae76a292bf5e67e437ceb", 0, 1),
    0x102A9EE8: ("ad32c1fb40876faca314cc207723e3d9357f2277", "3dd77aa63c1b51dc43d87f95ed1cc7ffd60756f2", 0, 1),
    0x102AA4A4: ("e1df70867301ed61736648f3d9d12e9178935f0a", "935b9135b9d4cd26ea40b29826e5a52b74b59ac4", 0, 1),
    0x102AB734: ("72f25798d9eb46848b7eac289f4084390e8f7bd7", "86cdd6aa8b4a9cbd67f2a3ebe0fc9d31f3b51f55", 0, 1),
    0x102AC35A: ("768f544e451894636c3c2a8fdd7c98a561b81917", "57b841ca2416d0ef75f25bcf965051d9fb95c588", 0, 1),
    0x102ACA9C: ("d2279c61ca1928eeffdc2743aec4930c4c53bcfa", "fb930ddb2c0663399dc7c26af331e922af5f8cef", 0, 1),
    0x102ACEF0: ("ee0a244daa4a2b90aef146f3ffb10d8818758464", "fd1ccff032a286a3a1a009a203978f2b1e88acf8", 0, 1),
    0x102AD4F0: ("82ec1b79af93aebc9ac527a1546afdcd31c30f8c", "d617f154f3b93566bf148af9f35308ad6de58ad7", 0, 1),
    0x102AD644: ("53e0a956aa8d8916b35fafb4e7cc6fa561b8d762", "8010a6e4b014044ed646e2a5986dc7015d1c5faf", 0, 1),
    0x102ADC20: ("07cbec8d5ce12a7f5011e3ef3a5f327cb16a33a7", "43610c6de00a9cb517d3c8b686269fc61dd5b6e1", 0, 1),
    0x102AE3E8: ("ac3dd320af908b54a9ef1e7eb003798cae967dfc", "8c39e6229e71d9b0a1b5a3b53fad3af56187b01d", 0, 1),
    0x102AE9C8: ("0e166b138ef196370a89af63603900b484de9716", "9a54678ffd31f632a4309f97384041ba132617ec", 0, 1),
    0x102AEDD0: ("3b390b069e930f420fcc93d40e046f5c55372db9", "63b023993e44e8922217568e2ca19dc30e50b4ae", 0, 1),
    0x102AF16C: ("8fb0011dd9a048d74840e9026b5e2a5939303304", "4dbd386df49ed5157c5cb4b3f8259945274972e2", 0, 1),
    0x102AF2AC: ("edef7cd78ed5c7e6fd504422f3566794ad7bf728", "e637f9b8b97cf820ee0dd9454c57f05ac427490d", 0, 1),
    0x102B2366: ("164b2204a48e944a7c099e9e424db8372cebd37c", "86d6ca9ec7cbc350882235e3224bd77cba6f162a", 0, 1),
    0x102A772C: ("5e3cb750cd9b3ecbb86935721925bc2285e474b6", "c6f2ba6540f2664e66838f5102f48c4780530b13", 0, 1),
    0x102A7756: ("c06076e3574b4ed1f9e03a630f13d3e423a75b46", "c9e6730fde886e0d9e209ffd3372f65c1884920c", 0, 1),
    0x102A8B42: ("17ca7a6c014b503e0482e4e6c6585b8c1b84a7e0", "d815b6d6fcdbe35381a9e56fa0b68543b0b4f152", 0, 1),
    0x102AA330: ("da2679968f8fe8461702629f723dc909ccca40da", "c38887c499b4c8407661bf13eec1a50688cb68ee", 0, 1),
    0x102AADE4: ("ded62b31441252282e2938702780ea6c3b244869", "81f38de09dc394945e3f18fb57f1eb786c47891f", 0, 1),
    0x102AC1E6: ("e8dcf800b891c06a40bbe86d04fd08f091f323d2", "c8e6358b1b56bdb9199f27ffb19c793af9ffa059", 0, 1),
    0x102AF524: ("3808cf796c44b5cc14460ebcfea9757fe127b41b", "76a5c86db2f19babf5a991ff13f25ba8893ddec3", 0, 1),
    0x102AFE24: ("52deb8e049f5cfbad246c1ec65768e80e8c6ee12", "3a7842f1f36d2a377e5d5f11e9f7987daf427b13", 0, 1),
    0x102B0288: ("ece7a8e9e0e672aaa7c21e285a461079e713ef63", "79752d8168099563bdd8df4dc42e2ff70f2d3fef", 0, 1),
    0x102B05C4: ("0f45abc16873a6c95dc075183ea587fc119d88a7", "1d07465b5f17acede29b656f706a680e0c29a488", 0, 1),
    0x102B0E58: ("622bef38bd3a77b7c382136369804c705d0eb1c8", "9e1dbba2c65303a335631e255332282ed51a53c9", 0, 1),
}


def _sha1(b):
    return hashlib.sha1(b).hexdigest()


def _sections(p):
    return ((CODE_BASE, bytes(p.code)), (p.data_base, bytes(p.data)))


def find_blocks(p, wanted):
    """{key: [addresses in p, ascending]} of the blocks in WANTED, a list of (key, length, anchor SHA-1, whole SHA-1). The
    program's two sections are scanned once, on even addresses (the window scripts are halfword aligned)."""
    by_anchor = {}
    for want in wanted:
        by_anchor.setdefault(want[2], []).append(want)
    found = {want[0]: [] for want in wanted}
    sha1 = hashlib.sha1
    for base, buf in _sections(p):
        for i in range(0, len(buf) - ANCHOR + 1, 2):
            group = by_anchor.get(sha1(buf[i:i + ANCHOR]).digest().hex())
            if group:
                for key, length, _, whole in group:
                    if i + length <= len(buf) and sha1(buf[i:i + length]).hexdigest() == whole:
                        found[key].append(base + i)
    return {k: sorted(v) for k, v in found.items()}


class BuildView:
    """The program of another build, answering reads at the 2.04b3 build's addresses: each block the tables read is found by
    its content and shifted by its own delta; the scorer table and its TVectors are followed (see the docstring)."""

    def __init__(self, p, header):
        self.p = p
        self.regions = []                 # (low, high, delta in the 2.04b3 address space)
        self.patch = {}                   # 2.04b3 word address -> the word the tables must see there
        wanted = [(("part", i), length, anchor, whole) for i, (_, _, length, anchor, whole) in enumerate(PARTS)]
        for addr, (anchor, whole, _, _) in WINDOW_HASHES.items():
            wanted.append((("script", addr), 2 * next(n for a, n, _ in WINDOW_SCRIPTS if a == addr), anchor, whole))
        hits = find_blocks(p, wanted)
        for i, (name, addr, length, _, _) in enumerate(PARTS):
            h = hits[("part", i)]
            if len(h) != 1:
                raise ValueError("the %s (%d bytes) are found %d times in the program, not once" % (name, length, len(h)))
            self.regions.append((addr, addr + length, h[0] - addr))
        for (kind, addr), h in hits.items():
            if kind != "script":
                continue
            _, _, ordinal, copies = WINDOW_HASHES[addr]
            if len(h) == 1 and copies == 1:
                at = h[0]
            elif len(h) == copies and copies > 1:
                at = h[ordinal]            # identical copies: the one in the same place among them
            else:
                raise ValueError("the window script at 0x%08x is found %d times in the program, not %d" % (addr, len(h), copies))
            self.regions.append((addr, addr + 2 * next(n for a, n, _ in WINDOW_SCRIPTS if a == addr), at - addr))
        self.regions.sort(key=lambda r: r[1] - r[0])        # the smallest block first, so a script wins over a wider block
        message_delta = next(d for lo, hi, d in self.regions if lo == MESSAGES)
        self.regions.append((SCORERS, MESSAGES, message_delta))          # record -1 is its last 0x30 bytes
        # the scorer table: 351 pointers to member {0, -1, TVector}; the TVectors are consecutive in the 241-scorer region
        entries = []
        for k in range(SCORER_COUNT):
            delta, vindex, tv = struct.unpack(">3I", self._raw(SCORERS + 12 * k, 12))
            if delta != 0 or vindex != 0xFFFFFFFF:
                raise ValueError("scorer %d is not a plain pointer to member" % k)
            entries.append(tv)
        distinct = sorted(set(entries))
        names = sorted(set(self._declared_scorers(header)))
        if len(distinct) != len(names) or any(b - a != 8 for a, b in zip(distinct, distinct[1:])):
            raise ValueError("the scorer table names %d TVectors, not %d consecutive ones" % (len(distinct), len(names)))
        tv_delta = distinct[0] - TVECTORS_FIRST
        if distinct[-1] - tv_delta + 8 > TVECTORS_END + 4:
            raise ValueError("the scorers' TVectors end at an unexpected address")
        self.regions.append((TVECTORS_FIRST, TVECTORS_END, tv_delta))
        codes = sorted({struct.unpack(">I", p.read(tv, 4))[0] for tv in entries})
        if len(codes) != len(names):
            raise ValueError("%d scorer functions in the program, %d declared" % (len(codes), len(names)))
        rank = dict(zip(codes, names))
        for k, tv in enumerate(entries):
            self.patch[SCORERS + 12 * k + 8] = tv - tv_delta
            self.patch[tv - tv_delta] = rank[struct.unpack(">I", p.read(tv, 4))[0]]

    @staticmethod
    def _declared_scorers(header):
        for line in header.read_text(encoding="utf-8").splitlines():
            m = re.match(r"\s*u?int32_t\s+(_ADVISOR_\w+)\(\);\s*//\s*(0x[0-9a-fA-F]{8})", line)
            if m:
                yield int(m.group(2), 16)

    def _raw(self, a, n):
        for lo, hi, d in self.regions:
            if lo <= a and a + n <= hi:
                return self.p.read(a + d, n)
        raise ValueError("0x%08x+%d is outside the blocks found in the program" % (a, n))

    def read(self, a, n):
        b = bytearray(self._raw(a, n))
        for w in range(a - a % 4, a + n, 4):
            if w in self.patch:
                v = struct.pack(">I", self.patch[w])
                for i in range(4):
                    if a <= w + i < a + n:
                        b[w + i - a] = v[i]
        return bytes(b)


def generate(program_path, header=None):
    """{file name: text} of the seven tables from the program at program_path; ValueError when it is not a build whose tables
    are the known ones."""
    header = header or pathlib.Path(__file__).resolve().parent.parent / "include" / "AdvisorScoring.h"
    view = BuildView(Program(program_path), header)
    texts = {}
    for name, make in TABLES.items():
        texts[name] = "\n".join(make(view)) + "\n"
        got = _sha1(texts[name].encode("utf-8"))
        if got != KNOWN_OUTPUT_SHA1[name]:
            raise ValueError("%s has SHA-1 %s, not the known %s" % (name, got, KNOWN_OUTPUT_SHA1[name]))
    return texts


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--program", required=True, help='a full Mac "SimTheme Park" or "Theme Park World" application (its data fork)')
    ap.add_argument("--out", required=True, help="the folder to write the seven files into")
    a = ap.parse_args(argv)
    path = pathlib.Path(a.program)
    if not path.is_file():
        sys.exit("gen_game_tables.py: %s does not exist (set TPW_GAME_PROGRAM to the data fork of a full Mac SimTheme Park or "
                 "Theme Park World application: the CD's own program, import_originals.py writes it)" % path)
    try:
        texts = generate(path)
    except ValueError as e:
        sys.exit("gen_game_tables.py: %s is not a full Mac build whose tables are known: %s" % (path, e))
    out = pathlib.Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    for name, text in texts.items():
        f = out / name
        if not f.exists() or f.read_bytes() != text.encode("utf-8"):
            with open(f, "w", encoding="utf-8", newline="\n") as fh:
                fh.write(text)


if __name__ == "__main__":
    main(sys.argv[1:])
