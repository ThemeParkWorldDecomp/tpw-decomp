"""Differential test for the advisor's speech: ADV_Call, ADV_Slap, ADV_Kill, ADV_GetTag and ADV_Stat (src/Advisor.cpp) and
the response table (src/advisor_lines.cpp), against libtpw_unit_Advisor.dylib (tests/unit_Advisor.cpp).

The world, the sides and the fakes are those of tests/test_Advisor.py (imported, not copied): a script of operations over
up to two advisor meshes, the state dumped after every operation, the call logs compared at the end. This module adds
the operations "ADV_Call" (id), "ADV_Slap", "ADV_Kill" (mode), "ADV_GetTag" (id) and "ADV_Stat", and the set-up items
"theme" (the text get_theme_path answers), "file" (the bytes of the lip-sync file), "row" (patch a row of the response
table), "inuse" (the static buffer's use flag), "push" (append a value to a fake's answer queue), and the reads "table"
(every word of the table) and "tags" (ADV_GetTag over a range of ids).

The fakes the speech adds (both sides record the same events): FindFilePath (answers "Found:<name>" or NULL from the queue
"FindFilePath"), TPFile_Open (the file token, fixed label 6, or NULL: "TPFile_Open"), get_theme_path, LbFile_GetSize (the
queue "LbFile_GetSize"), LbFile_Read (copies the case's file into the buffer), LbFile_Close, _USwapBlock32 (records pointer
and count and does nothing: the original swaps `count` words and is given the byte count, the host's does nothing on a
little-endian host), malloc (the lip-sync block, labelled from 200), the four sound handles' wrappers
(SOUND_GetCurrentSampleLengthInMilliseconds answers the queue of that name). The case's file is little-endian bytes; the
PowerPC side's LbFile_Read puts the words in memory as the Mac's swap would leave them, so that both sides read the same
numbers, partial last word included.

Not reproduced: a lip-sync file without a -1 word among its whole words when a tick follows (the mouth would read past the
block), a `which` of 4 or more (the slots stop at 3), a canned sequence number outside 0 to 13.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

import test_Advisor as TA
from test_Advisor import MASK

PROGRAM = TA.PROGRAM
SHIM_LIBRARY = TA.SHIM_LIBRARY

TABLE, ROWS = 0x10278CE4, 611
FILE_GLOBAL = 0x102B3548
SPEAK_ADDR = dict(ADV_Call=0x1000887C, ADV_Slap=0x10008DD4, ADV_Kill=0x10008F00, ADV_GetTag=0x10009050, ADV_Stat=0x100090DC)
SPEAK_OPS = dict(ADV_Call=26, ADV_Slap=27, ADV_Kill=28, ADV_GetTag=29, ADV_Stat=30)
SPEAK_REAL = set(SPEAK_ADDR.values())
FIND_FILE_PATH, TPFILE_OPEN, GET_THEME_PATH = 0x100597F8, 0x10003FD4, 0x1015139C
SOUND_LENGTH, SOUND_STOP, SOUND_FADE, SOUND_VOLUME = 0x100DE758, 0x100DE058, 0x100DE0FC, 0x100DDC5C
LENGTH_KEY = "SOUND_GetCurrentSampleLengthInMilliseconds"
TOKEN_LABEL = 6
BIG_BLOCK = 1 << 20

ROWS_DATA = []          # the Mac's table, read in setup()


def s32(v):
    v &= MASK
    return v - (1 << 32) if v & 0x80000000 else v


# =============================================================================================== the sides


class SpeakMixin:
    """The speech's operations and dump, over either side."""

    def apply(self, item):
        op = item[0]
        if op == "theme":
            self.set_theme(item[1])
        elif op == "file":
            self.set_file(item[1])
        elif op == "row":
            self.set_row(item[1], item[2:])
        elif op == "inuse":
            self.w32(self.in_use_address, item[1])
        elif op == "push":
            self.push(item[1], item[2])
        elif op == "table":
            return self.table_words()
        elif op == "tags":
            return [self.speak_call("ADV_GetTag", [i]) for i in range(item[1], item[2])]
        else:
            return super().apply(item)
        return None

    def run(self, op, a):
        if op in SPEAK_OPS:
            return self.speak_call(op, a)
        return super().run(op, a)

    def set_sync(self, mode, values):
        super().set_sync(mode, values)
        if mode == 2:
            self.sizes[self.ptr(self.state_address + self.off("state.mSyncData"))] = max(len(values), 1) * 4

    def block_size(self, address):
        size = self.foreign_block_size(address)       # the host's live malloc blocks (its heap reuses addresses)
        return size if size >= 0 else self.sizes.get(address, -1)

    def dump(self):
        out = super().dump()
        out.append(self.lab(self.ptr(self.file_global)))
        out += list(self.unpack("250I", self.rd(self.buffer_address, 1000)))
        block = self.ptr(self.state_address + self.off("state.mSyncData"))
        size = self.block_size(block) if block else -1
        if size >= 0:
            words = size // 4
            out += [size] + (list(self.unpack("%dI" % words, self.rd(block, words * 4))) if words else [])
        else:
            out.append(-1)
        return out


class SpeakPpc(SpeakMixin, TA.PpcSide):
    SAVED = TA.PpcSide.SAVED + [(FILE_GLOBAL, 4)]

    def __init__(self, e, case):
        self.table = e.read(TABLE, ROWS * 32)
        self.sizes = {}
        self.file_bytes = b""
        self.theme_ptr = 0
        self.found = 0
        super().__init__(e, case)
        self.file_global = FILE_GLOBAL
        self.table_address = TABLE
        self.token = e.alloc(16, b"\0" * 16)
        self.L.labels[self.token] = TOKEN_LABEL
        self.found = e.alloc(700, b"\0" * 700)
        self.theme_ptr = e.alloc(1, b"\0")

    def close(self):
        super().close()
        self.e.write(TABLE, self.table)

    def foreign_block_size(self, address):
        return -1

    def make_stubs(self):
        S = super().make_stubs()
        S.real.update(SPEAK_REAL)
        R, e, L = CL.recorder, self.e, self.e.cl

        def text(address):
            return e.cstr(address, limit=1 << 20) if address else None

        def find_file_path(e):
            name = text(e.r(3))
            L.rec("FindFilePath", [e.r(4), L.lab(e.r(5)), len(name)], [name[:159]])
            if L.ret("FindFilePath", 1) == 0:
                e.set_r(3, 0)
                return
            e.write(self.found, ("Found:" + name).encode("latin-1") + b"\0")
            e.set_r(3, self.found)
        S.internal(FIND_FILE_PATH, find_file_path)

        def tpfile_open(e):
            name, mode = text(e.r(3)), text(e.r(4))
            L.rec("TPFile_Open", [-1 if name is None else len(name)],
                  ["(null)" if name is None else name[:159], "(null)" if mode is None else mode[:159]])
            e.set_r(3, self.token if L.ret("TPFile_Open", 1) else 0)
        S.internal(TPFILE_OPEN, tpfile_open)

        def get_theme_path(e):
            L.rec("get_theme_path")
            e.set_r(3, self.theme_ptr)
        S.internal(GET_THEME_PATH, get_theme_path)
        S.internal(SOUND_LENGTH, R(LENGTH_KEY, "i", ret="i"))
        S.internal(SOUND_STOP, R("SOUND_StopSoundEffect", "i"))
        S.internal(SOUND_FADE, R("SOUND_FadeSoundEffect", "i"))
        S.internal(SOUND_VOLUME, R("SOUND_SetVolumeOnSound", "ii"))

        def get_size(e):
            L.rec("LbFile_GetSize", [L.lab(e.r(3))])
            e.set_r(3, T.u32(L.ret("LbFile_GetSize", 0)))
        S.imp("LbFile_GetSize__FPv", get_size)

        def read(e):
            result, file, buffer, length, transferred = e.r(3), e.r(4), e.r(5), e.r(6), e.r(7)
            L.rec("LbFile_Read", [L.lab(file), L.lab(buffer), length, L.lab(transferred)])
            count = min(length, len(self.file_bytes))
            if buffer and count:
                # the words the Mac's swap leaves: the file's little-endian words, numerically (a partial last word
                # keeps the buffer's other bytes, as the host's memcpy does)
                for start in range(0, count, 4):
                    taken = min(4, count - start)
                    new = int.from_bytes(self.file_bytes[start:start + taken], "little")
                    mask = (1 << (8 * taken)) - 1
                    old = e.u32(buffer + start)
                    e.write(buffer + start, struct.pack(">I", (old & ~mask & MASK) | new))
            e.write(result, b"\0\0\0\0")
        S.imp("LbFile_Read__FPvPvUlPUl", read)

        def close(e):
            L.rec("LbFile_Close", [L.lab(e.r(4))])
            e.write(e.r(3), b"\0\0\0\0")
        S.imp("LbFile_Close__FPv", close)
        S.imp("_USwapBlock32__13SamsUtilitiesFPUll", R("_USwapBlock32", "Li"))
        S.imp("strcat", lambda e: e.write(e.r(3), (e.cstr(e.r(3)) + e.cstr(e.r(4))).encode("latin-1") + b"\0"))

        def malloc(e):
            size = e.r(3)
            L.rec("malloc", [size])
            if size > BIG_BLOCK:
                e.set_r(3, 0)
                return
            block = e.alloc(size + 16, b"\xa5" * (size + 16))      # 16 spare bytes: a read past the block sees 0xa5
            L.newlab(block)
            self.sizes[block] = size
            e.set_r(3, block)
        S.imp("malloc", malloc)

        def free(e):
            block = e.r(3)
            if block and L.lab(block) > 0:
                L.rec("free", [L.lab(block)])
                L.forget(block)
                self.sizes.pop(block, None)
        S.imp("free", free)
        return S

    def set_theme(self, text):
        self.theme_ptr = self.e.alloc(len(text) + 1, text.encode("latin-1") + b"\0")

    def set_file(self, data):
        self.file_bytes = bytes(data)

    def set_row(self, index, words):
        self.e.write(TABLE + 32 * index, struct.pack(">8I", *[w & MASK for w in words]))

    def table_words(self):
        return self.unpack("%dI" % (ROWS * 8), self.rd(TABLE, ROWS * 32))

    def speak_call(self, op, a):
        result = T.call_ppc(self.e, SPEAK_ADDR[op], [x & MASK for x in a])[0] & MASK
        return result if op in ("ADV_Call", "ADV_GetTag", "ADV_Stat") else 0


class SpeakHost(SpeakMixin, TA.HostSide):
    def __init__(self, lib, case):
        self.sizes = {}
        for fn in ("tpw_ad_lines", "tpw_ad_file_global", "tpw_ad_file_token"):
            getattr(lib, fn).restype = C.c_void_p
        lib.tpw_ad_set_theme.argtypes = [C.c_char_p]
        lib.tpw_ad_set_file.argtypes = [C.c_char_p, C.c_int32]
        lib.tpw_ad_set_row.argtypes = [C.c_int32, C.POINTER(C.c_int32)]
        lib.tpw_ad_block_size.argtypes = [C.c_void_p]
        lib.tpw_ad_block_size.restype = C.c_int64
        super().__init__(lib, case)
        self.file_global = lib.tpw_ad_file_global()
        self.table_address = lib.tpw_ad_lines()
        lib.tpw_cl_label(TOKEN_LABEL, lib.tpw_ad_file_token())
        lib.tpw_cl_lab.argtypes = [C.c_void_p]
        lib.tpw_cl_lab.restype = C.c_int32

    def lab(self, value):
        """The host's labels live in the call log (blocks the fakes labelled included)."""
        return 0 if value == 0 else self.lib.tpw_cl_lab(value)

    def foreign_block_size(self, address):
        return self.lib.tpw_ad_block_size(address)

    def set_theme(self, text):
        self.lib.tpw_ad_set_theme(text.encode("latin-1"))

    def set_file(self, data):
        self.lib.tpw_ad_set_file(bytes(data), len(data))

    def set_row(self, index, words):
        self.lib.tpw_ad_set_row(index, (C.c_int32 * 8)(*[s32(w) for w in words]))

    def table_words(self):
        return self.unpack("%dI" % (ROWS * 8), self.rd(self.table_address, ROWS * 32))

    def speak_call(self, op, a):
        result = self.call(SPEAK_OPS[op], [x if x >= 0 else x & MASK for x in a]) & MASK
        return result if op in ("ADV_Call", "ADV_GetTag", "ADV_Stat") else 0


def speak_ppc(e, case):
    side = SpeakPpc(e, case)
    try:
        return TA.run_case(side, case)
    finally:
        side.close()


def speak_host(lib, case):
    side = SpeakHost(lib, case)
    try:
        return TA.run_case(side, case)
    finally:
        side.close()


def setup(runner):
    TA.setup(runner)
    ROWS_DATA[:] = [list(struct.unpack(">8I", runner.emu.read(TABLE + 32 * r, 32))) for r in range(ROWS)]


# =============================================================================================== building scripts

LENGTHS = [-500, -201, -200, -199, 0, 1, 500, 1666, 30000, 0x7FFFFF37, 0x7FFFFF38, 0x7FFFFFFF, -0x80000000, -0x7FFFFFFF]
CLOCKS = [0, 1, 200, 1000, 123456, 0x7FFFF000, 0xFFFFF000, 0xFFFFFCE0, 0xFFFFFCDF, 0xFFFFFCE1, 0x7FFFFFFF, 0x80000000]
SIZES = [0, 4, 996, 999, 1000, 1001, 4000]
LIP_WORDS = [0, 5, 999, 1000, 1001, 1999, 2000, 2999, 3000, 12345, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFE, 0xFFFFFC18, 0xFFFFFFFF]
THEMES = ["", "T", "Data:levels:" + "ab" * 50, "x" * 190]
IDS_EDGE = [0, 1, 7, 300, 609, 610, 611, 612, 613, 396, 397, 551, 9999, 0x270E, 0x270F, 0x2710, -1, -9999, 0x7FFFFFFF,
            0x80000000, 0xFFFFFFFF, 0x10000, 0x100001]


def ids_where(test):
    return [row[0] for row in ROWS_DATA[:610] if test(row)]


def first_with(test, dflt=1):
    found = ids_where(test)
    return found[0] if found else dflt


def pattern_words(rnd, count, first=None):
    """`count` words of lip-sync times that end with -1 words (three, or all of them when there are fewer)."""
    tail = min(3, count)
    words = [rnd.choice(LIP_WORDS[:-1]) for _ in range(count - tail)] + [0xFFFFFFFF] * tail
    if first is not None and count:
        words[0] = first
    return words


def file_bytes(words, size, tail=b""):
    data = b"".join(struct.pack("<I", w & MASK) for w in words) + tail
    return data[:size] if size >= 0 else data


def file_for(rnd, size, first=None):
    """A lip-sync file of `size` bytes: whole words that end in -1 and (when the size is not a multiple of 4) a tail."""
    count = max(size, 0) // 4
    words = pattern_words(rnd, count, first)
    tail = bytes(rnd.getrandbits(8) for _ in range(max(size, 0) % 4))
    return file_bytes(words, size, tail)


def make_case(script, variants=(), q=None, seed=1):
    return dict(seed=seed, variants=list(variants), q=q or {}, script=script)


def prelude(rnd, meshes=True, on=1, banks=None, lengths=None):
    """Two installed advisors and ADV_Init, or (meshes False) no meshes and the animation lengths set by hand."""
    banks = banks or (rnd.randrange(1, 90), rnd.randrange(1, 90), rnd.randrange(1, 90))
    if meshes:
        variants = [TA.pick_spec(rnd), TA.pick_spec(rnd)]
        script = [("install", 0, 0), ("install", 1, 1)]
    else:
        variants, script = [], []
    script += [("on", on), ("banks",) + tuple(banks)]
    if meshes:
        script.append(("ADV_Init",))
    else:
        lengths = lengths or {14: 777, 13: 0, 1: 400, 2: 600, 3: 900}
        script += [("state", "mAnimLengths", i, v & MASK) for i, v in sorted(lengths.items())]
    return variants, script


def speak_queues(rnd, lengths=None, found=None, opened=None, sizes=None):
    q = TA.queues(rnd)
    q[LENGTH_KEY] = lengths if lengths is not None else [rnd.choice(LENGTHS[3:]) for _ in range(rnd.randint(1, 4))]
    q["FindFilePath"] = found if found is not None else [rnd.choice([1, 1, 1, 0]) for _ in range(rnd.randint(1, 4))]
    q["TPFile_Open"] = opened if opened is not None else [rnd.choice([1, 1, 1, 0])]
    q["LbFile_GetSize"] = sizes if sizes is not None else [rnd.choice(SIZES)]
    q["Rand"] = rnd.sample(range(32768), 30) if rnd.random() < 0.4 else []
    return q


# ---- the worlds of one line
def line_world(rnd, size=None, found=True, opened=True, first=None, tickable=True):
    """The items that make the next ADV_Call find (or not) its lip-sync file: the theme, the file, the answers."""
    if size is None:
        size = rnd.choice(SIZES + [12, 100, 400])
        if tickable and size < 12:
            size = rnd.choice([12, 100, 996])
    items = [("theme", rnd.choice(THEMES)), ("file", file_for(rnd, size, first))]
    items += [("push", "FindFilePath", 1 if found else 0)]
    if found:
        items.append(("push", "FindFilePath", 1 if rnd.random() < 0.9 else 0))
    items += [("push", "TPFile_Open", 1 if opened else 0), ("push", "LbFile_GetSize", size)]
    return items


def old_sync(rnd, mode):
    """Lip-sync data the previous line left: none (0), in the static buffer (1) or in a block of its own (2)."""
    if mode == 0:
        return [("sync", 0)]
    values = pattern_words(rnd, rnd.randint(4, 30))
    return [("sync", mode) + tuple(values), ("state", "mSyncPos", 0, rnd.randrange(0, 3)),
            ("state", "mSyncSize", 0, 4 * len(values))]


def clock_items(rnd, now, paused):
    return [("clock", now & MASK, paused)]


def poke(name, value, index=0):
    return ("state", name, index, value & MASK)


# =============================================================================================== edge cases


def edge_guards():
    """ADV_Call with the advisor on and off, speaking and not, for a few ids."""
    for on in (0, 1):
        for speaking in (0, 1):
            for ident in (1, 5):
                rnd = random.Random(10 * on + 3 * speaking + ident)
                variants, script = prelude(rnd, meshes=False, on=on)
                script += [poke("mSpeaking", speaking), ("ADV_Call", ident), ("ADV_Stat",)]
                yield make_case(script, variants, speak_queues(rnd, lengths=[0]))


def edge_ids():
    """Every kind of id: the first and last rows, the gaps, the terminator, negatives, huge ones."""
    for ident in IDS_EDGE:
        rnd = random.Random(ident & 0xFFFF)
        variants, script = prelude(rnd, meshes=False)
        script += line_world(rnd, size=16, tickable=False)
        script += [("ADV_GetTag", ident), ("ADV_Call", ident), ("ADV_Stat",), ("ADV_GetTag", ident)]
        yield make_case(script, variants, speak_queues(rnd, lengths=[0], found=[1, 1], opened=[1], sizes=[16]))


def edge_table():
    """The whole table, word by word, and every id from -2 to 624 through ADV_GetTag; a patched row ends the search early."""
    rnd = random.Random(1)
    variants, script = prelude(rnd, meshes=False)
    script += [("table",), ("tags", -2, 625)]
    yield make_case(script, variants)
    script = list(script[:-2])
    script += [("row", 300, 9999, 1, 2, 3, 4, 5, 6, 7), ("tags", 296, 304), ("ADV_Call", 305), ("ADV_Call", 299),
               ("row", 5, 1, 2, 3, 4, 5, 6, 7, 8), ("tags", 0, 8), ("ADV_Call", 1), ("table",)]
    yield make_case(script, variants, speak_queues(rnd, lengths=[0]))


def edge_themes():
    """The theme path text (empty, short, long) for each row of the second sound set, and the answers of the path hook."""
    rows = ids_where(lambda r: (r[4] >> 16) != 0) + ids_where(lambda r: (r[4] >> 16) == 0)[:2]
    for ident in rows:
        for theme in THEMES:
            for found, opened in ((True, True), (True, False), (False, True)):
                rnd = random.Random(ident * 7 + len(theme))
                variants, script = prelude(rnd, meshes=False)
                script += [("theme", theme), ("file", file_for(rnd, 40)), ("push", "FindFilePath", 1 if found else 0)]
                if found:
                    script.append(("push", "FindFilePath", 1))
                script += [("push", "TPFile_Open", 1 if opened else 0), ("push", "LbFile_GetSize", 40)]
                script += [("ADV_Call", ident), ("ADV_Stat",)]
                yield make_case(script, variants, {LENGTH_KEY: [300]})


def edge_patched_rows():
    """Rows the table does not have: sound sets from the sign bit down, the advisor slots, file numbers, sequences, hats."""
    mixes = [0x00000000, 0x00000001, 0x00010000, 0x00010001, 0x00020002, 0x00030003, 0x7FFF0001, 0x80000001, 0x80010000,
             0xFFFF0000, 0xFFFF0003, 0x8000FFFF & 0x80000003, 0x12340002]
    lips = [0, 5, 636, 999, 1000, 123456, 0x7FFFFFFF, -1, -0x80000000, -12]
    for k, mix in enumerate(mixes):
        for lip in (lips[k % len(lips)], lips[(k + 3) % len(lips)]):
            rnd = random.Random(k * 31 + lip % 1000)
            variants, script = prelude(rnd, meshes=True)
            seq = rnd.choice([-1, -1, 16, 21, 25, 29])
            script += [("theme", rnd.choice(THEMES[:3])), ("file", file_for(rnd, 40)),
                       ("row", 1, 1, 3 + k, lip, seq, mix, rnd.choice(TA.HATS), rnd.choice([0, 12, 14, 0xFFFFFFFF]), 77),
                       ("ADV_Call", 1), ("ADV_GetTag", 1)]
            for _ in range(rnd.randint(1, 3)):
                script += [("clock", rnd.randrange(1, 100000), 0), ("ADV_Tick",), ("ADV_Tick2",)] if mix & 0xFFFF <= 1 else []
            yield make_case(script, variants, speak_queues(rnd, lengths=[rnd.choice([0, 300, 1666])], found=[1], opened=[1],
                                                           sizes=[40]))


def edge_sizes():
    """File sizes around the static buffer's 1,000 bytes, the buffer free or in use, old data none, static or malloc'd."""
    for size in SIZES:
        for in_use in (0, 1):
            for old in (0, 1, 2):
                rnd = random.Random(size * 11 + in_use * 5 + old)
                variants, script = prelude(rnd, meshes=False)
                script += old_sync(rnd, old)
                if old != 1:
                    script.append(("inuse", in_use))
                script += line_world(rnd, size=size, tickable=False)
                script += [("ADV_Call", 2), ("ADV_Stat",)]
                yield make_case(script, variants, {LENGTH_KEY: [300], "TPFile_Open": [1], "FindFilePath": [1]})
    # a negative size: the static buffer, a read of 4 GB bounded by the file
    for old in (0, 2):
        rnd = random.Random(900 + old)
        variants, script = prelude(rnd, meshes=False)
        script += old_sync(rnd, old)
        script += [("theme", ""), ("file", file_for(rnd, 120)), ("push", "LbFile_GetSize", -1), ("ADV_Call", 2)]
        yield make_case(script, variants, {LENGTH_KEY: [300]})


def edge_lip_words():
    """The first lip-sync time (divided by 1000, truncating) first in the file and later in stale data."""
    for word in LIP_WORDS:
        rnd = random.Random(word & 0xFFFF)
        variants, script = prelude(rnd, meshes=False)
        script += line_world(rnd, size=16, first=word, tickable=False)
        script += [("file", struct.pack("<4I", word & MASK, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF)), ("ADV_Call", 2)]
        yield make_case(script, variants, {LENGTH_KEY: [300], "FindFilePath": [1], "TPFile_Open": [1], "LbFile_GetSize": [16]})
        # no file: the previous data stays, read from its position
        variants, script = prelude(rnd, meshes=False)
        script += [("sync", 2, 1, 2, word, 4, 0xFFFFFFFF), ("state", "mSyncPos", 0, 2), ("state", "mSyncSize", 0, 20),
                   ("theme", ""), ("push", "FindFilePath", 0), ("ADV_Call", 2), ("ADV_Stat",)]
        yield make_case(script, variants, {LENGTH_KEY: [300]})
        # data that starts with -1
        variants, script = prelude(rnd, meshes=False)
        script += [("sync", 1, 0xFFFFFFFF, word, 0xFFFFFFFF), ("state", "mSyncPos", 0, 0), ("push", "FindFilePath", 0),
                   ("ADV_Call", 2)]
        yield make_case(script, variants, {LENGTH_KEY: [300]})


def edge_sample_lengths():
    """Every sample length (the -200 early answer among them) for a random row and for each kind of canned sequence."""
    kinds = [-1, 16, 17, 21, 22, 23, 25, 26, 27, 28, 29]
    for length in LENGTHS:
        for kind in kinds:
            ident = first_with(lambda r: s32(r[3]) == kind)
            rnd = random.Random(length * 3 + kind)
            variants, script = prelude(rnd, meshes=True)
            script += line_world(rnd, size=16, tickable=False)
            script += [("ADV_Call", ident), ("ADV_Stat",)]
            yield make_case(script, variants, {LENGTH_KEY: [length], "FindFilePath": [1], "TPFile_Open": [1],
                                               "LbFile_GetSize": [16], "Rand": rnd.sample(range(32768), 20)})


def edge_sound_time():
    """mBaseTime + 1000 == 0 (mSoundTime stays 0 and the sound is not stopped), one before and after it; canned rows that
    keep or clear mSoundTime; a key time before the call."""
    seqs = [-1, 16, 21, 28, 29]
    for now in CLOCKS:
        for kind in seqs:
            for key in (0, 5):
                ident = first_with(lambda r: s32(r[3]) == kind)
                rnd = random.Random(now ^ (kind * 977) ^ key)
                variants, script = prelude(rnd, meshes=True)
                script += [("clock", now, rnd.choice([0, 0, 1])), poke("mKeyTime", key)]
                script += line_world(rnd, size=16, tickable=False)
                script += [("ADV_Call", ident), ("ADV_Stat",), ("clock", (now + 1500) & MASK, 0), ("ADV_Tick",), ("ADV_Tick2",)]
                yield make_case(script, variants, {LENGTH_KEY: [rnd.choice([0, 1666])], "FindFilePath": [1],
                                                   "TPFile_Open": [1], "LbFile_GetSize": [16]})


def edge_key_times():
    for key in (0, 5, -5, 0x7FFFFFFF, 0x80000000, 1):
        for kind in (-1, 28, 29, 21):
            ident = first_with(lambda r: s32(r[3]) == kind)
            rnd = random.Random(key ^ kind)
            variants, script = prelude(rnd, meshes=True)
            script += [poke("mKeyTime", key), ("clock", rnd.choice(CLOCKS), 0)]
            script += line_world(rnd, size=16, tickable=False)
            script += [("ADV_Call", ident), ("clock", 5000, 0), ("ADV_Tick",)]
            yield make_case(script, variants, {LENGTH_KEY: [1000], "FindFilePath": [1], "TPFile_Open": [1], "LbFile_GetSize": [16]})


def edge_clock():
    """The clock paused and not, with the pause already seen or not, at the call and at ADV_Stat."""
    for paused in (0, 1):
        for seen in (0, 1):
            for later in (0, 1):
                rnd = random.Random(paused * 4 + seen * 2 + later)
                variants, script = prelude(rnd, meshes=False)
                script += [poke("mClockPaused", seen), poke("mPauseStart", 4000), poke("mPausedTotal", 700),
                           ("clock", 9000, paused)]
                script += line_world(rnd, size=16, tickable=False)
                script += [("ADV_Call", 3), ("ADV_Stat",), ("clock", 12000, later), ("ADV_Stat",), ("clock", 15000, 0),
                           ("ADV_Stat",)]
                yield make_case(script, variants, {LENGTH_KEY: [300], "FindFilePath": [1], "TPFile_Open": [1],
                                                   "LbFile_GetSize": [16]})


def edge_slap():
    """ADV_Slap: speaking or not, a sound handle or not, each rand outcome, the advisor on and off, paused sound."""
    for speaking in (0, 1):
        for handle in (0, 77):
            for pick in (0, 1, 2, 3, 4, 5, 32767, 32766):
                for on, paused in ((1, 0), (1, 1), (0, 1), (0, 0)):
                    rnd = random.Random(pick * 3 + handle + speaking)
                    variants, script = prelude(rnd, meshes=False, on=on)
                    script += [poke("mSpeaking", speaking), poke("mSoundHandle", handle), poke("mPausedSound", paused),
                               poke("mWhich", rnd.randrange(2)), ("ADV_Slap",), ("ADV_Slap",)]
                    yield make_case(script, variants, {"Rand": [pick, (pick + 1) % 3]})


def edge_kill():
    """ADV_Kill over everything it tests: speaking, the mode, the paused sound, the advisor on, the handle, the sync data."""
    for speaking in (0, 1):
        for mode in (0, 1, -1):
            for paused in (0, 1, 7):
                for on in (0, 1):
                    for handle in (0, 77):
                        for sync in (0, 1, 2):
                            rnd = random.Random(mode * 5 + paused * 3 + on + handle + sync)
                            meshes = (speaking + sync) % 2 == 1
                            variants, script = prelude(rnd, meshes=meshes, on=on)
                            script += old_sync(rnd, sync)
                            script += [poke("mSpeaking", speaking), poke("mSoundHandle", handle),
                                       poke("mPausedSound", paused), poke("mWhich", rnd.randrange(2)),
                                       ("ADV_Kill", mode), ("ADV_Kill", 1 - mode if mode in (0, 1) else 0)]
                            yield make_case(script, variants, TA.queues(rnd))


def edge_stat():
    for speaking in (0, 1):
        for paused in (0, 1):
            for seen in (0, 1):
                for now in (0, 1000, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF):
                    rnd = random.Random(now ^ speaking)
                    variants, script = prelude(rnd, meshes=False)
                    script += [poke("mSpeaking", speaking), poke("mEndTime", rnd.choice([0, 5000, 0x7FFFFFFF, 0x80000000])),
                               poke("mClockPaused", seen), poke("mPauseStart", 321), poke("mPausedTotal", 123),
                               ("clock", now, paused), ("ADV_Stat",), ("clock", (now + 777) & MASK, 1 - paused), ("ADV_Stat",)]
                    yield make_case(script, variants)


def edge_which_words():
    """The word of a patched row that holds the model and the sound set: the model is the LOW 16 BITS (not only 0 to 3), the
    sound set the signed high half. Triage 2026-10-13b, for the survivor `& 3` in place of `& 0xffff` (ADV_Call 0x1000887c)."""
    lows = [0, 1, 2, 3, 4]
    highs = [0, 1, 0x7FFF, 0x8000, 0xFFFF]
    for k, low in enumerate(lows):
        for high in highs:
            rnd = random.Random(k * 17 + high)
            variants, script = prelude(rnd, meshes=False)
            script += [("theme", "Data:T"), ("file", file_for(rnd, 16)),
                       ("row", 1, 1, 3 + k, 636, -1, ((high << 16) | low) & MASK, 0, 0, 77),
                       ("ADV_Call", 1), ("ADV_Stat",)]
            yield make_case(script, variants, {LENGTH_KEY: [300], "FindFilePath": [1, 1], "TPFile_Open": [1],
                                               "LbFile_GetSize": [16]})


def edge_kill_modes():
    """ADV_Kill's mode is tested as the whole word (zero: stop, any other: fade): 2, 256, 0x10000, the sign bit."""
    for mode in (2, 255, 256, 0x10000, 0x7FFFFFFF, 0x80000000, 0xFFFFFF00):
        for handle in (0, 77):
            rnd = random.Random(mode & 0xFFFF)
            variants, script = prelude(rnd, meshes=False)
            script += [poke("mSpeaking", 1), poke("mSoundHandle", handle), ("ADV_Kill", mode & MASK)]
            yield make_case(script, variants, TA.queues(rnd))


def edge_sets():
    for gen in (edge_guards, edge_ids, edge_table, edge_themes, edge_patched_rows, edge_sizes, edge_lip_words,
                edge_sample_lengths, edge_sound_time, edge_key_times, edge_clock, edge_slap, edge_kill, edge_stat,
                edge_which_words, edge_kill_modes):
        yield from gen()


# =============================================================================================== random scripts


def random_line_ops(rnd, with_ticks, which_limit=2):
    """One ADV_Call with its world, and (maybe) what the game does next."""
    ident = rnd.choice([rnd.choice(ids_where(lambda r: True)), rnd.randrange(0, 620), rnd.choice(IDS_EDGE)])
    ops = []
    ops += line_world(rnd, found=rnd.random() < 0.85, opened=rnd.random() < 0.9)
    if rnd.random() < 0.15:
        ops += old_sync(rnd, rnd.choice([1, 2]))
    if rnd.random() < 0.1:
        ops.append(("inuse", rnd.randrange(2)))
    if rnd.random() < 0.3:
        ops.append(poke("mKeyTime", rnd.choice([0, 7, 9000])))
    ops.append(("push", LENGTH_KEY, rnd.choice(LENGTHS[3:] + [rnd.randrange(0, 20000)])))
    ops.append(("ADV_Call", ident))
    return ops


def random_followup(rnd, now, paused, ticks):
    ops = []
    for _ in range(rnd.randint(0, 6)):
        r = rnd.random()
        if r < 0.45 and ticks:
            now = (now + rnd.choice([0, 16, 100, 250, 900, 2000, 20000])) & MASK
            ops += [("clock", now, paused), ("ADV_Tick",)]
            if rnd.random() < 0.6:
                ops.append(("TickAdvisor", rnd.choice([0, 0, 1])))
            if rnd.random() < 0.6:
                ops.append(("ADV_Tick2",))
        elif r < 0.55:
            ops.append(("ADV_Stat",))
        elif r < 0.62:
            ops.append(("ADV_GetTag", rnd.choice([1, 5, rnd.randrange(0, 620), -1])))
        elif r < 0.72:
            ops.append(("ADV_Slap",))
        elif r < 0.84:
            ops.append(("ADV_Kill", rnd.choice([0, 1])))
        elif r < 0.9:
            paused = 1 - paused
            ops.append(("clock", now, paused))
        elif r < 0.95:
            ops.append((rnd.choice(["ADV_Pause", "ADV_UnPause"]),))
        else:
            ops.append(("ADV_Close",))
    return ops, now, paused


def calls_case(rnd, ticks=False, lines=None):
    variants, script = prelude(rnd, meshes=True, on=rnd.choice([1, 1, 1, 1, 0]))
    now, paused = rnd.choice(CLOCKS), 0
    script.append(("clock", now, paused))
    for _ in range(lines or rnd.randint(1, 4)):
        script += random_line_ops(rnd, ticks)
        ops, now, paused = random_followup(rnd, now, paused, ticks)
        script += ops
        if rnd.random() < 0.5:
            script.append(poke("mSpeaking", 0))      # the line ended (ADV_Tick would clear it) before the next
    return make_case(script, variants, speak_queues(rnd), seed=rnd.getrandbits(32))


def ticks_case(rnd):
    return calls_case(rnd, ticks=True, lines=rnd.randint(1, 5))


def files_case(rnd):
    """The lip-sync file's world: sizes around 1,000, the static buffer's flag, old data, found or not, opened or not."""
    variants, script = prelude(rnd, meshes=rnd.random() < 0.4)
    for _ in range(rnd.randint(1, 4)):
        if rnd.random() < 0.5:
            script += old_sync(rnd, rnd.choice([0, 1, 2]))
        script.append(("inuse", rnd.randrange(2))) if rnd.random() < 0.4 else None
        script += line_world(rnd, size=rnd.choice(SIZES + [rnd.randrange(0, 1500)]), found=rnd.random() < 0.8,
                             opened=rnd.random() < 0.85, tickable=False)
        script.append(("push", LENGTH_KEY, rnd.choice([0, 1, 300, 1666])))
        script.append(("ADV_Call", rnd.choice([1, 2, 5, 50, 300])))
        script.append(poke("mSpeaking", rnd.choice([0, 0, 1])))
        if rnd.random() < 0.4:
            script.append(("ADV_Kill", rnd.randrange(2)))
    return make_case(script, variants, speak_queues(rnd, lengths=[300]), seed=rnd.getrandbits(32))


def stop_case(rnd):
    """ADV_Slap, ADV_Kill and ADV_Stat over made-up states."""
    variants, script = prelude(rnd, meshes=rnd.random() < 0.5, on=rnd.choice([1, 1, 0]))
    now, paused = rnd.choice(CLOCKS), 0
    for _ in range(rnd.randint(2, 12)):
        r = rnd.random()
        if r < 0.5:
            script += [poke("mSpeaking", rnd.randrange(2)), poke("mSoundHandle", rnd.choice([0, 1, 77])),
                       poke("mPausedSound", rnd.choice([0, 1, 7])), poke("mWhich", rnd.randrange(2)),
                       poke("mEndTime", rnd.choice([0, now, now + 500, now - 500, rnd.getrandbits(32)]))]
            script += old_sync(rnd, rnd.choice([0, 0, 1, 2]))
        elif r < 0.65:
            script.append(("on", rnd.randrange(2)))
        elif r < 0.75:
            now = (now + rnd.choice([0, 1, 33, 5000])) & MASK
            paused = rnd.randrange(2)
            script.append(("clock", now, paused))
        script.append(rnd.choice([("ADV_Slap",), ("ADV_Kill", rnd.randrange(3) - 1), ("ADV_Stat",), ("ADV_Stat",)]))
    return make_case(script, variants, speak_queues(rnd), seed=rnd.getrandbits(32))


def patched_case(rnd):
    """A line from a patched row: the sound set and slot, the file number, the sequence, the hat and the hand."""
    variants, script = prelude(rnd, meshes=True)
    now = rnd.choice(CLOCKS)
    script.append(("clock", now, 0))
    for _ in range(rnd.randint(1, 3)):
        index = rnd.randrange(0, 610)
        ident = ROWS_DATA[index][0]
        w4 = rnd.choice([rnd.randrange(0, 2) | rnd.choice([0, 1, 0xFFFF, 0x7FFF, 0x8000, 0x8001]) << 16,
                         rnd.randrange(0, 2) | rnd.choice([0, 1]) << 16])
        script.append(("row", index, ident, rnd.randrange(1, 650), rnd.choice([0, 5, 636, 999, -1, 123456, 0x7FFFFFFF, -0x80000000]),
                       rnd.choice([-1, -1, 16, 17, 21, 22, 23, 25, 26, 27, 28, 29]), w4 & MASK, rnd.choice(TA.HATS),
                       rnd.choice([0, 0, 12, 14, 0xFFFFFFFF]), rnd.getrandbits(32)))
        script += line_world(rnd, found=rnd.random() < 0.85, opened=rnd.random() < 0.9)
        script += [("push", LENGTH_KEY, rnd.choice([0, 300, 1666, 30000])), ("ADV_Call", ident), ("ADV_GetTag", ident)]
        ops, now, _ = random_followup(rnd, now, 0, True)
        script += ops
        script.append(poke("mSpeaking", 0))
    return make_case(script, variants, speak_queues(rnd), seed=rnd.getrandbits(32))


def tag_case(rnd):
    """ADV_GetTag and the table: random ids, the table read, a patched id that ends the search."""
    variants, script = prelude(rnd, meshes=False)
    for _ in range(rnd.randint(2, 8)):
        r = rnd.random()
        if r < 0.7:
            script.append(("ADV_GetTag", rnd.choice([rnd.randrange(-3, 625), rnd.getrandbits(32), rnd.choice(IDS_EDGE)])))
        elif r < 0.8:
            lo = rnd.randrange(0, 600)
            script.append(("tags", lo, lo + rnd.randrange(1, 25)))
        elif r < 0.9:
            index = rnd.randrange(0, 610)
            script.append(("row", index, rnd.choice([9999, ROWS_DATA[index][0], rnd.randrange(0, 700)])) + tuple(rnd.getrandbits(32) for _ in range(7)))
        else:
            script.append(("table",))
    return make_case(script, variants)


def long_case(rnd):
    """A long script that mixes everything over the made-up and the real advisor meshes."""
    variants, script = prelude(rnd, meshes=True, on=rnd.choice([1, 1, 1, 0]))
    now, paused = rnd.choice(CLOCKS), 0
    script.append(("clock", now, paused))
    for _ in range(rnd.randint(8, 20)):
        r = rnd.random()
        if r < 0.3:
            script += random_line_ops(rnd, True)
        elif r < 0.5:
            script.append(poke("mSpeaking", rnd.choice([0, 1])))
            script.append(poke(rnd.choice(["mSoundHandle", "mPausedSound", "mKeyTime", "mSoundTime"]),
                               rnd.choice([0, 1, 77, now + 100])))
        elif r < 0.6:
            script.append(("on", rnd.randrange(2)))
        elif r < 0.65:
            script += [("banks", rnd.randrange(1, 90), rnd.randrange(1, 90), rnd.randrange(1, 90))]
        ops, now, paused = random_followup(rnd, now, paused, True)
        script += ops
    return make_case(script, variants, speak_queues(rnd), seed=rnd.getrandbits(32))


def clock_case(rnd):
    """The advisor clock around the pause: ADV_Call's base time, ADV_Stat and the ticks while it is paused and resumed."""
    variants, script = prelude(rnd, meshes=True)
    now, paused = rnd.choice(CLOCKS), 0
    for _ in range(rnd.randint(2, 6)):
        script += [("clock", now, paused)]
        if rnd.random() < 0.6:
            script += random_line_ops(rnd, True)
        script += [("ADV_Stat",), ("ADV_Tick",)]
        now = (now + rnd.choice([0, 1, 200, 999, 1000, 5000, 70000])) & MASK
        paused = rnd.randrange(2)
        if rnd.random() < 0.3:
            script.append(poke("mClockPaused", rnd.randrange(2)))
    return make_case(script, variants, speak_queues(rnd), seed=rnd.getrandbits(32))


def sets_case(rnd):
    """Lines of the second sound set (the theme path) and the first, with the theme texts and the banks."""
    variants, script = prelude(rnd, meshes=rnd.random() < 0.5)
    both = ids_where(lambda r: (r[4] >> 16) != 0)
    for _ in range(rnd.randint(1, 4)):
        theme = rnd.choice(THEMES + ["levels:" + "".join(rnd.choice("abc:") for _ in range(rnd.randrange(0, 200)))])
        script += [("theme", theme), ("file", file_for(rnd, rnd.choice([16, 100]))),
                   ("push", "FindFilePath", 1), ("push", "FindFilePath", 1), ("push", "TPFile_Open", 1),
                   ("push", "LbFile_GetSize", 16)]
        script += [("push", LENGTH_KEY, rnd.choice([0, 300, 1666])), ("ADV_Call", rnd.choice(both + [1, 2, 3])),
                   ("ADV_Stat",), ("ADV_Kill", rnd.randrange(2)), ("banks", rnd.randrange(1, 90), rnd.randrange(1, 90), rnd.randrange(1, 90))]
    return make_case(script, variants, speak_queues(rnd), seed=rnd.getrandbits(32))


TESTS = [
    FnTest("Advisor speech edges", speak_ppc, speak_host, edge=lambda: edge_sets(), rnd=lambda r: stop_case(r)),
    FnTest("Advisor speech calls", speak_ppc, speak_host, edge=lambda: (c for c in []), rnd=calls_case),
    FnTest("Advisor speech ticks", speak_ppc, speak_host, edge=lambda: (c for c in []), rnd=ticks_case),
    FnTest("Advisor speech files", speak_ppc, speak_host, edge=lambda: (c for c in []), rnd=files_case),
    FnTest("Advisor speech stops", speak_ppc, speak_host, edge=lambda: (c for c in []), rnd=stop_case),
    FnTest("Advisor speech patched rows", speak_ppc, speak_host, edge=lambda: (c for c in []), rnd=patched_case),
    FnTest("Advisor speech tags", speak_ppc, speak_host, edge=lambda: (c for c in []), rnd=tag_case),
    FnTest("Advisor speech mixed", speak_ppc, speak_host, edge=lambda: (c for c in []), rnd=long_case),
    FnTest("Advisor speech clock", speak_ppc, speak_host, edge=lambda: (c for c in []), rnd=clock_case),
    FnTest("Advisor speech sound sets", speak_ppc, speak_host, edge=lambda: (c for c in []), rnd=sets_case),
]
