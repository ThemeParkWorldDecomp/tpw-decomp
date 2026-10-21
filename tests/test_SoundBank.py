"""Unit w72, the sound library's sample banks, against libtpw_unit_SoundBank.dylib (tests/unit_SoundBank.cpp), in the program
"sound symbols shared" (the Mac's sound library, Code at 0x10000000, TOC base r2 = 0x1003abf0):
  src/sound/Bank.cpp, src/sound/ListSentence.cpp, src/sound/TbBank.cpp, src/sound/TbMMFileBank.cpp, src/sound/TbSoundBankInfo.cpp, src/sound/TbSoundManager.cpp, src/sound/TbSoundPipeChannel.cpp, src/sound/TbSoundSampleInfo.cpp   TbBank 0x100088a8 (ctor), 0x10008c28 (dtor), GetNumberOfSamples 0x10008ac0, GetSampleLength 0x100089bc,
                        GetSampleName 0x10008af8, GetSamplePosition 0x10008b2c, GetHandle 0x10008b64, IsStreaming 0x10008b94,
                        Initialise 0x10008bc8; CRefManager 0x10008940 and 0x10000ec0; TbFileBank 0x10008cc0 0x10008e0c 0x10008e94
                        0x100090cc 0x10009108 0x10009144; TbMMFileBank 0x100091f4 0x100095d0 0x100096c8 0x10009780 0x100097bc
                        0x100097f4 0x10009834 0x100098e8 0x100140a0; TbSoundBankInfo 0x100099d8 0x10009a1c 0x10009a58 0x100277c4
                        0x10014150 0x10014188 0x100143b0; CBankFactory 0x10027820 0x10013fb4 0x100141c0; Bank::CBank 0x100143e8
                        0x10014464 0x10014508 0x10014884; CFileOperation::GetBankPath 0x1001e720; TbSoundSampleInfo::SetBank
                        0x1000df50; UMemoryWatcher 0x10014354; the swap helpers 0x10008db4 0x10008fd0 0x1000942c 0x10009020
                        0x1000947c 0x10009078 0x100094d4 0x10009528 0x10009588; the runtime's double to unsigned long 0x1002e988

The banks run as the original, one script of operations per case (create a bank, register it, retrieve samples, ask for lengths
and positions, deregister, destroy it with each flag), with the state of every bank slot dumped after each operation: the
words, the offsets, every header (the six fields, the data block's label and bytes), the cache of the file bank, the file
image's bytes. Every callee outside the unit is a fake that records the same event on both sides: the file handle (copy
constructor, destructor, Seek, Read, Close, MapFile over the case's bytes), the allocator (new, new[], delete: blocks are
zeroed and labelled; the queue "Alloc" says which allocation fails), the owners' AddRef and Release, the platform's
_USwapBlock32, SamsUtilities::UString. The vtables are the original's: the PowerPC side calls the bank's virtuals through the
slot numbers of include/TbSoundBank.h. A header array and the file bank's cache are allocated in the Mac's sizes there
(0x14 per header, 4 per entry) and in the host's here; the comparison scales the host's size (see normalise).
"""
import ctypes as C
import pathlib
import random
import struct

import calllog as CL
import ppcemu
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "sound symbols shared"
SHIM_LIBRARY = "unit_SoundBank"
MASK = 0xFFFFFFFF
ROOT = pathlib.Path(__file__).resolve().parent.parent.parent

# vtables and classes
BANK_VT, FILE_VT, MM_VT, FACTORY_VT, CBANK_VT = 0x1003BE84, 0x1003BEFC, 0x1003BF74, 0x1003C830, 0x1003C85C
# virtual slots of a bank
(S_DTOR, S_REG, S_NUMBER, S_RETR, S_LENGTH, S_NAME, S_POS, S_HANDLE, S_ISTYPE, S_STREAM, S_INIT, S_DEREG) = (
    0x10, 0x14, 0x18, 0x1C, 0x20, 0x24, 0x28, 0x2C, 0x30, 0x34, 0x38, 0x3C)
# functions
TBBANK_CTOR, CREF_CTOR, CREF_DTOR, MM_CTOR = 0x100088A8, 0x10008940, 0x10000EC0, 0x100140A0
FACTORY_CTOR, GENERATE, CBANK_CTOR, CBANK_FILE, CBANK_RETRIEVE = 0x10027820, 0x100141C0, 0x100143E8, 0x10014508, 0x10014884
GET_BANK_PATH, SET_BANK, WATCHER, CVT = 0x1001E720, 0x1000DF50, 0x10014354, 0x1002E988
INFO_CTORS = {0: 0x100099D8, 1: 0x10009A1C, 2: 0x10009A58, 3: 0x100277C4}
INFO_GET_FILE, INFO_GET_SIZE, INFO_GET_PATH = 0x10014150, 0x10014188, 0x100143B0
# imports (by name, as the glue stubs resolve them)
IMP_NEW, IMP_NEWA, IMP_DEL, IMP_DELA = "__nw__FUl", "__nwa__FUl", "__dl__FPv", "__dla__FPv"
IMP_REF_CTOR, IMP_REF_DTOR, IMP_ADDREF, IMP_RELEASE = "__ct__12TbRefReleaseFv", "__dt__12TbRefReleaseFv", "AddRef__12TbRefReleaseFv", "Release__12TbRefReleaseFv"
IMP_FILE_COPY = "__ct__Q33bfl10FileSystem5CFileFRCQ33bfl10FileSystem5CFile"
IMP_FILE_DTOR = "__dt__Q33bfl10FileSystem5CFileFv"
IMP_SWAP_BLOCK = "_USwapBlock32__13SamsUtilitiesFPUll"
IMP_USTRING, IMP_USTRING_DTOR = "__ct__Q213SamsUtilities7UStringFPCc", "__dt__Q213SamsUtilities7UStringFv"

# the operations (the order of enum Op in the unit file)
(OP_NEW_MM, OP_NEW_FILE, OP_NEW_FAKE, OP_REGISTER, OP_RETRIEVE, OP_LENGTH, OP_POSITION, OP_NAME, OP_HANDLE, OP_ISTYPE,
 OP_STREAMING, OP_NUMBER, OP_INITIALISE, OP_DEREGISTER, OP_DESTROY, OP_POKE, OP_NEW_FACTORY, OP_CONSTRUCT, OP_GENERATE,
 OP_CBANK_NEW, OP_CBANK_FILE, OP_CBANK_RETRIEVE, OP_CBANK_DESTROY, OP_REF_NEW, OP_REF_DEL) = range(25)
SLOTS = 4
KIND_NONE, KIND_MM, KIND_FILE, KIND_FAKE = 0, 1, 2, 3
SENTINEL = 0xDEADBEEF
LABEL_COMPONENT, LABEL_OWNER, LABEL_FILE_A, LABEL_INFO, LABEL_FILE_B, LABEL_IMAGE = 1, 2, 3, 4, 5, 6
FIXED_OWNERS = (LABEL_COMPONENT, LABEL_OWNER)
DUMP_LIMIT = 12
HOST_HEADER = None      # sizeof(TbSampleHeader) on the host, set in setup


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------- the PowerPC side

class World:
    """What one case's fakes work on: the file image and position, the banks, the objects."""


def call(emu, entry, args):
    """A top-level call with the stack below the frame cleared (the host's local buffers start as zeros)."""
    sp = ppcemu.STACK_BASE + ppcemu.STACK_SIZE - 0x1000 - 0x400
    emu.write(sp - 0x4000, b"\0" * 0x4000)
    return T.call_ppc(emu, entry, args)[0]


def vcall(emu, obj, slot, args=(), hidden=False):
    """A virtual call through the object's vtable (the slot holds a function descriptor); hidden: the TbError result."""
    vt = emu.u32(obj)
    code = emu.u32(emu.u32(vt + slot))
    if hidden:
        result = emu.alloc(16, b"\xa5" * 16)
        call(emu, code, [result, obj] + list(args))
        return s32(emu.u32(result))
    return call(emu, code, [obj] + list(args))


def ws(e, a, words):
    e.write(a, struct.pack(">" + "I" * len(words), *[w & MASK for w in words]))


def alloc_block(e, size, array):
    """The allocator pair of the case: the queue "Alloc" and a limit of 64 MB, blocks zeroed and labelled."""
    L = e.cl
    succeed = L.ret("Alloc", 1) != 0 and size < (64 << 20)
    name = "new[]" if array else "new"
    if not succeed:
        L.rec(name, (size, 0))
        return 0
    block = e.alloc(max(size, 16), b"\0" * max(size, 16))
    L.rec(name, (size, L.newlab(block)))
    return block


def stub_new(array):
    def stub(e):
        e.set_r(3, alloc_block(e, e.r(3), array))
    return stub


def stub_delete(e):
    L, ptr = e.cl, e.r(3)
    if ptr != 0 and L.lab(ptr) > 0:
        L.rec("delete", (L.lab(ptr),))
        L.forget(ptr)


def cb(fn):
    """An import stub that runs PowerPC code through fn(emu, call) (T.stub_cb for a CL.Stubs table)."""
    def hook(e):
        e.pending_cb = fn
        e.uc.reg_write(ppcemu.P.UC_PPC_REG_PC, ppcemu.RETURN_ADDR)
    return hook


def stub_addref(e):
    L, obj = e.cl, e.r(3)
    if L.lab(obj) in FIXED_OWNERS:
        L.rec("Owner.AddRef", (L.lab(obj),))
        e.set_r(3, 0)
        return
    count = e.u32(obj + 4) + 1
    ws(e, obj + 4, [count])
    e.set_r(3, count)


def release(e, callee):
    L, obj = e.cl, e.r(3)
    if L.lab(obj) in FIXED_OWNERS:
        L.rec("Owner.Release", (L.lab(obj),))
        return 0
    count = (s32(e.u32(obj + 4)) - 1) & MASK
    ws(e, obj + 4, [count])
    if count == 0:
        vt = e.u32(obj)
        callee(e.u32(vt + S_DTOR), obj, 1)
        return 0
    return count


def stub_ref_ctor(e):
    ws(e, e.r(3) + 4, [1])


def stub_file_copy(e):
    stub_file_copy_with(e, e.r(3), e.r(4))


def stub_file_dtor(e):
    L, obj = e.cl, e.r(3)
    L.rec("CFile::~CFile", (L.lab(obj),))
    L.forget(obj)


def stub_swap_block(e):
    L, words, count = e.cl, e.r(3), s32(e.r(4))
    L.rec("_USwapBlock32", (L.lab(words), count))
    for i in range(max(count, 0)):
        e.write(words + 4 * i, e.read(words + 4 * i, 4)[::-1])


def stub_ustring(e):
    L, obj, text = e.cl, e.r(3), e.r(4)
    L.rec("UString::UString", (L.newlab(obj),), (e.cstr(text)[:159] if text else "(null)",))


def stub_ustring_dtor(e):
    L, obj = e.cl, e.r(3)
    L.rec("UString::~UString", (L.lab(obj),))
    L.forget(obj)


def build_stubs():
    S = CL.Stubs()
    S.imp(IMP_NEW, stub_new(False))
    S.imp(IMP_NEWA, stub_new(True))
    S.imp(IMP_DEL, stub_delete)
    S.imp(IMP_DELA, stub_delete)
    S.imp(IMP_REF_CTOR, stub_ref_ctor)
    S.imp(IMP_REF_DTOR, lambda e: None)
    S.imp(IMP_ADDREF, stub_addref)
    S.imp(IMP_RELEASE, cb(release))
    S.imp(IMP_FILE_COPY, stub_file_copy)
    S.imp(IMP_FILE_DTOR, stub_file_dtor)
    S.imp(IMP_SWAP_BLOCK, stub_swap_block)
    S.imp(IMP_USTRING, stub_ustring)
    S.imp(IMP_USTRING_DTOR, stub_ustring_dtor)
    return S


STUBS = None
VT = {}          # the persistent fake vtables


# the fake file handle's slots: 0xc Close, 0x10 Read, 0x18 Seek, 0x24 MapFile

def f_close(e):
    L = e.cl
    L.rec("CFile::Close", (L.lab(e.r(3)),))


def f_read(e):
    L, w = e.cl, e.w
    dest, n = e.r(4), e.r(5)
    L.rec("CFile::Read", (n, L.lab(dest)))
    mode = L.ret("Read", -2)
    if mode == -1:
        e.set_r(3, MASK)
        return
    wanted = n
    if mode >= 0 and mode < wanted:
        wanted = mode
    left = w.length - w.position
    count = 0 if left <= 0 else min(wanted, left)
    if dest != 0 and count != 0:
        e.write(dest, e.read(w.image + w.position, count))
    w.position += count
    e.set_r(3, count)


def f_seek(e):
    L, w = e.cl, e.w
    offset, whence = e.r(4), e.r(5)
    L.rec("CFile::Seek", (L.lab(e.r(3)), offset, whence))
    if L.ret("Seek", 0) == -1:
        e.set_r(3, MASK)
        return
    if whence == 0:
        target = offset
    elif whence == 1:
        target = w.position + s32(offset)
    elif whence == 2:
        target = w.length + s32(offset)
    else:
        e.set_r(3, MASK)
        return
    if target < 0:
        e.set_r(3, MASK)
        return
    w.position = target
    e.set_r(3, target)


def f_map(e):
    L, w = e.cl, e.w
    L.rec("CFile::MapFile", (L.lab(e.r(3)),))
    if L.ret("Map", 1) == 0 or w.length == 0:
        e.set_r(3, 0)
        return
    e.set_r(3, w.image)


# the fake bank's slots (a vtable of the TbBank copy with the pure slots filled)

def fb_register(e):
    L = e.cl
    L.rec("Fake.RegisterBank", (L.lab(e.r(4)),))
    ws(e, e.r(3), [0])


def fb_retrieve(e):
    L = e.cl
    bank, out, index = e.r(4), e.r(5), e.r(6)
    L.rec("Fake.RetrieveSample", (L.lab(bank), index))
    error = L.ret("Retrieve", 0)
    if error >= 0:
        ws(e, out, [e.w.fake_headers[bank]])
    ws(e, e.r(3), [error])


def fb_istype(e):
    e.set_r(3, 0)


def fb_deregister(e):
    L = e.cl
    L.rec("Fake.Deregister", (L.lab(e.r(3)),))


# the fake factory's ConstructBank (slot 0xc)

def ff_construct(e):
    L = e.cl
    this, out, info, comp = e.r(4), e.r(5), e.r(6), e.r(7)
    L.rec("Factory.ConstructBank", (L.lab(this), L.lab(info), L.lab(comp)))
    slot = L.ret("CBSlot", -1)
    ws(e, out, [e.w.banks[slot] if 0 <= slot < SLOTS else 0])
    ws(e, e.r(3), [L.ret("CBErr", 0)])


# the fake string's operator+= (slot 0x30)

def fs_append(e):
    this, text = e.r(3), e.r(4)
    old = CL.tds_text(e, this)
    new = old + e.cstr(text)
    pointer = CL.new_text(e, new)
    ws(e, this + 8, [pointer, len(new)])
    e.set_r(3, this)


def setup(runner):
    global STUBS, HOST_HEADER
    e = runner.emu
    T.install_std_stubs(e)
    STUBS = build_stubs()
    lib = runner.lib
    lib.tpw_sb_header_size.restype = C.c_int64
    HOST_HEADER = lib.tpw_sb_header_size()

    def vtable_copy(address, size, patches):
        base = T.persist_alloc(e, size, e.read(address, size))
        for slot, fn in patches.items():
            ws(e, base + slot, [T.fake_function(e, fn)])
        return base
    VT["file"] = T.persist_alloc(e, 0x40)
    for slot, fn in ((0xC, f_close), (0x10, f_read), (0x18, f_seek), (0x24, f_map)):
        ws(e, VT["file"] + slot, [T.fake_function(e, fn)])
    VT["bank"] = vtable_copy(BANK_VT, 0x40, {S_REG: fb_register, S_RETR: fb_retrieve, S_ISTYPE: fb_istype, S_DEREG: fb_deregister})
    VT["factory"] = vtable_copy(FACTORY_VT, 0x10, {0xC: ff_construct})
    VT["string"] = T.persist_alloc(e, 0x40)
    ws(e, VT["string"] + 0x30, [T.fake_function(e, fs_append)])


# ---------------------------------------------------------------- the files

def sample_header(size, rate=0x5622, bits=0x10, flags=0x24, f8=0, fc=0, name=b"sample.mp2", last=0, first=0x28):
    return struct.pack("<II", first, size & MASK) + name.ljust(16, b"\0")[:16] + struct.pack("<HBBIII", rate, bits, flags, f8 & MASK, fc & MASK, last)


def bank_file(samples, gap=0, count=None, seed=0):
    """A bank file: the count, the offsets, then each sample's 0x28-byte header and its data. A sample is a dict with
    size, data (a length or bytes) and the header's other fields; gap: bytes between the offsets and the first header."""
    r = random.Random(seed)
    n = len(samples)
    position = 4 + 4 * n + gap
    offsets, body = [], b""
    for s in samples:
        offsets.append(position + len(body))
        data = s.get("data", min(max(s["size"], 0), 0x2000) if s["size"] < 0x80000000 else 0)
        if isinstance(data, int):
            data = bytes(r.getrandbits(8) for _ in range(data))
        body += sample_header(s["size"], s.get("rate", 0x5622), s.get("bits", 0x10), s.get("flags", 0x24), s.get("f8", 0),
                              s.get("fc", 0), last=s.get("last", 0)) + data
    return struct.pack("<I", n if count is None else count) + struct.pack("<%dI" % n, *offsets) + b"\0" * gap + body


def random_samples(r, n, big=True):
    sizes = [0, 1, 2, 7, 100, 0x1FFF, 0x2000, 0x2001, 0x2002, 0x3000, 0x4000, MASK, 0x80000000, 0x7FFFFFFF]
    if not big:
        sizes = [0, 1, 7, 100, 0x1FFF, 0x2000, 0x2001, 0x3000]
    out = []
    for _ in range(n):
        size = r.choice(sizes) if r.random() < 0.7 else r.randrange(0, 0x3000)
        out.append(dict(size=size, rate=r.choice([0x5622, 0x2B11, 0xAC44, 0, 1, 0xFFFF, r.getrandbits(16)]),
                        bits=r.choice([0x10, 8, 0x11, 0, 0xFF, r.getrandbits(8)]), flags=r.choice([0x24, 0, 1, 0x25, 0xFF, r.getrandbits(8)]),
                        f8=r.getrandbits(32), fc=r.getrandbits(32), last=r.getrandbits(32)))
    return out


def real_file(*parts):
    path = ROOT.joinpath("extracted", "disc-hfs", "Theme Park Data", "data", *parts)
    try:
        return path.read_bytes()
    except OSError:
        return None


# ---------------------------------------------------------------- the two sides

def fixed_objects(e):
    """The case's fixed objects in scratch: two file objects, an owner, the component (owner at +4)."""
    w = e.w
    w.file_a = e.alloc(8, struct.pack(">II", VT["file"], 0))
    w.file_b = e.alloc(8, struct.pack(">II", VT["file"], 0))
    w.owner = e.alloc(16, struct.pack(">IIII", BANK_VT, 1, 0, 0))
    w.component = e.alloc(16, struct.pack(">IIII", 0, BANK_VT, 1, 0))


def owner_address(w, which):
    return {0: 0, 1: w.owner, 2: w.component + 4}[which]


def file_address(w, which):
    return w.file_a if which == 0 else w.file_b


def ppc_normalise(e, w, slot, word):
    """The header pointer a RetrieveSample answered, in the form of the host's Normalise."""
    if word == SENTINEL:
        return -2
    if word == 0:
        return -3
    kind = w.kinds[slot]
    if kind == KIND_MM:
        return s32(word - e.u32(w.banks[slot] + 0x1C)) // 0x14
    if kind == KIND_FILE:
        return s32(word - e.u32(w.banks[slot] + 8))
    return 0 if word == w.fake_headers[w.banks[slot]] else -4


def new_bank(e, w, slot, size=0x20):
    block = alloc_block(e, size, False)
    w.banks[slot] = block
    return block


def alive(e, address):
    """A block still has its label (a delete takes it away)."""
    return address != 0 and e.cl.lab(address) > 0


def ppc_op(e, op, a, b, c, d):
    w, L = e.w, e.cl
    aux = [0] * 8
    slot = a
    if op in (OP_NEW_MM, OP_NEW_FILE, OP_NEW_FAKE):
        block = new_bank(e, w, slot)
        w.kinds[slot] = {OP_NEW_MM: KIND_MM, OP_NEW_FILE: KIND_FILE, OP_NEW_FAKE: KIND_FAKE}[op]
        if block == 0:
            return 0, aux
        owner = owner_address(w, b)
        if op == OP_NEW_MM:
            call(e, MM_CTOR, [block, owner, file_address(w, c)])
        else:
            call(e, TBBANK_CTOR, [block, owner])
            if op == OP_NEW_FILE:
                ws(e, block, [FILE_VT])
                stub_file_copy_with(e, block + 0x14, file_address(w, c))
            else:
                ws(e, block, [VT["bank"]])
                w.fake_headers[block] = e.alloc(0x14, b"\0" * 0x14)
        return 1, aux
    bank = w.banks[slot] if 0 <= slot < SLOTS and alive(e, w.banks[slot]) else 0
    if (OP_REGISTER <= op <= OP_DEREGISTER or op == OP_POKE) and bank == 0:
        return -99, aux              # the case could not make the bank (its allocation failed)
    if op in (OP_CBANK_FILE, OP_CBANK_RETRIEVE) and not alive(e, w.cbanks[a]):
        return -99, aux
    if op in (OP_CONSTRUCT, OP_GENERATE) and not w.factories[a]:
        return -99, aux
    if op == OP_REGISTER:
        return vcall(e, bank, S_REG, [e.alloc(8, b"path\0\0\0\0"), b & MASK], hidden=True), aux
    if op == OP_RETRIEVE:
        out = e.alloc(8, struct.pack(">II", SENTINEL, 0))
        error = vcall(e, bank, S_RETR, [out, b & MASK], hidden=True)
        aux[0] = ppc_normalise(e, w, slot, e.u32(out))
        return error, aux
    if op == OP_LENGTH:
        return vcall(e, bank, S_LENGTH, [b & MASK]) & MASK, aux
    if op == OP_POSITION:
        return vcall(e, bank, S_POS, [b & MASK]) & MASK, aux
    if op == OP_NAME:
        return int(vcall(e, bank, S_NAME, [b & MASK]) != 0), aux
    if op == OP_HANDLE:
        handle = vcall(e, bank, S_HANDLE)
        if handle == 0:
            return 0, aux
        return (1 if w.kinds[slot] == KIND_MM and handle == bank + 0x14 else 2), aux
    if op == OP_ISTYPE:
        return vcall(e, bank, S_ISTYPE, [b & MASK]) & MASK, aux
    if op == OP_STREAMING:
        return vcall(e, bank, S_STREAM) & MASK, aux
    if op == OP_NUMBER:
        return s32(vcall(e, bank, S_NUMBER)), aux
    if op == OP_INITIALISE:
        out = e.alloc(8, b"\0" * 8)
        error = vcall(e, bank, S_INIT, [out], hidden=True)
        word = e.u32(out)
        aux[0] = 1 if word == bank else 0 if word == 0 else 2
        return error, aux
    if op == OP_DEREGISTER:
        vcall(e, bank, S_DEREG)
        return 0, aux
    if op == OP_DESTROY:
        if bank:
            vcall(e, bank, S_DTOR, [b & MASK])
        w.banks[slot] = 0
        w.kinds[slot] = KIND_NONE
        return 0, aux
    if op == OP_POKE:
        fake = w.fake_headers[bank]
        if b == 0:
            ws(e, bank + 0xC, [c])
        elif b == 1:
            ws(e, fake, [c])
        elif b == 2:
            e.write(fake + 4, struct.pack(">H", c & 0xFFFF))
        elif b == 3:
            e.write(fake + 6, bytes([c & 0xFF]))
        elif b == 4:
            e.write(fake + 7, bytes([c & 0xFF]))
        elif b == 5:
            ws(e, fake + 8, [c])
        else:
            ws(e, fake + 0xC, [c])
        return 0, aux
    if op == OP_NEW_FACTORY:
        block = alloc_block(e, 4, False)
        w.factories[a] = block
        if block == 0:
            return 0, aux
        call(e, FACTORY_CTOR, [block])
        if a == 1:
            ws(e, block, [VT["factory"]])
        return 1, aux
    if op in (OP_CONSTRUCT, OP_GENERATE):
        info = e.alloc(16, b"\0" * 16)
        L.labels[info] = LABEL_INFO
        factory = w.factories[a]
        comp = w.component if c == 1 else 0
        if op == OP_CONSTRUCT:
            path = e.alloc(8, b"bank\0\0\0\0")
            ws(e, info, [7, 1234, path, file_address(w, d)])
            out = e.alloc(8, struct.pack(">II", SENTINEL, 0))
            error = vcall(e, factory, 0xC, [out, info, comp], hidden=True)
            made = e.u32(out)
            aux[0] = -2 if made == SENTINEL else -3 if made == 0 else 1
            if made not in (0, SENTINEL):
                w.banks[b] = made
                w.kinds[b] = KIND_MM
            return error, aux
        path = e.alloc(16, b"path.sdt\0\0\0\0\0\0\0\0")
        ws(e, info, [7, 77, path, file_address(w, d)])
        made = call(e, GENERATE, [factory, info, comp])
        w.cbanks[b] = made
        aux[0] = L.lab(made)
        return int(made != 0), aux
    if op == OP_CBANK_NEW:
        block = alloc_block(e, 0xC, False)
        w.cbanks[a] = block
        if block == 0:
            return 0, aux
        call(e, CBANK_CTOR, [block, w.banks[b] if b >= 0 else 0])
        return 1, aux
    if op == OP_CBANK_FILE:
        file = call(e, CBANK_FILE, [w.cbanks[a]])
        if file == 0:
            return 0, aux
        for s in range(SLOTS):
            if w.kinds[s] == KIND_MM and file == w.banks[s] + 0x14:
                return 1, aux
        return 2, aux
    if op == OP_CBANK_RETRIEVE:
        out = e.alloc(8, struct.pack(">II", SENTINEL, 0))
        result = s32(call(e, CBANK_RETRIEVE, [w.cbanks[a], out, b & MASK]))
        word = e.u32(out)
        if c >= 0 and w.banks[c]:
            aux[0] = ppc_normalise(e, w, c, word)
        else:
            aux[0] = -2 if word == SENTINEL else -3 if word == 0 else -4
        return result, aux
    if op == OP_CBANK_DESTROY:
        if alive(e, w.cbanks[a]):
            vcall(e, w.cbanks[a], S_DTOR, [b & MASK])
        w.cbanks[a] = 0
        return 0, aux
    if op == OP_REF_NEW:
        block = alloc_block(e, 4, False)
        w.ref = block
        if block == 0:
            return 0, aux
        if a == 0:
            return 1, aux          # the Mac's constructor would call through NULL: the object is made by hand
        call(e, CREF_CTOR, [block, owner_address(w, a)])
        return 1, aux
    if op == OP_REF_DEL:
        call(e, CREF_DTOR, [w.ref, a & MASK])
        w.ref = 0
        return 0, aux
    raise ValueError(op)


def stub_file_copy_with(e, dest, src):
    """What the copy constructor's import does (the file bank's constructor is not in the binary: the case builds it)."""
    L = e.cl
    e.write(dest, e.read(src, 8))
    L.rec("CFile::CFile", (L.newlab(dest), L.lab(src)))


def ppc_bank_state(e, slot):
    w, L = e.w, e.cl
    bank = w.banks[slot]
    if not alive(e, bank):
        return dict(exists=0)
    kind = w.kinds[slot]
    data, headers = e.u32(bank + 8), e.u32(bank + 0x1C)
    count = e.u32(bank + 0xC)
    state = dict(exists=1, kind=kind, rc=e.u32(bank + 4), count=count, data=L.lab(data), headers=L.lab(headers) if kind != KIND_FAKE else 0)
    n = min(count, DUMP_LIMIT)
    if kind == KIND_MM and data and count <= 64:
        state["offsets"] = [e.u32(data + 4 * i) for i in range(n)]
    if kind == KIND_MM and headers and count <= 64:
        rows = []
        for i in range(n):
            h = headers + 0x14 * i
            size = s32(e.u32(h))
            row = [size, struct.unpack(">H", e.read(h + 4, 2))[0], e.read(h + 6, 1)[0], e.read(h + 7, 1)[0], s32(e.u32(h + 8)),
                   e.u32(h + 0xC), L.lab(e.u32(h + 0x10))]
            block = e.u32(h + 0x10)
            rows.append(row + [e.read(block, min(max(size, 0), 0x2000)) if block and size > 0 else b""])
        state["headers_rows"] = rows
    if kind == KIND_FILE and headers and count <= 64:
        state["cache"] = [(-1 if e.u32(headers + 4 * i) == 0 else e.u32(headers + 4 * i) - data) for i in range(count)]
    return state


def norm_image(image, modified, endian):
    """The file image with the converted headers (the first 0x10 bytes at each offset the file bank has resolved) decoded in
    the side's byte order and cleared, so that the two sides' images compare."""
    buf = bytearray(image)
    fields = []
    for offset in sorted(modified):
        if offset + 16 <= len(buf):
            fields.append((offset, struct.unpack(endian + "i", buf[offset:offset + 4])[0], struct.unpack(endian + "H", buf[offset + 4:offset + 6])[0],
                           buf[offset + 6], buf[offset + 7], struct.unpack(endian + "i", buf[offset + 8:offset + 12])[0],
                           struct.unpack(endian + "I", buf[offset + 12:offset + 16])[0]))
            buf[offset:offset + 16] = bytes(16)
    return bytes(buf), fields


def ppc_cbank_state(e, slot):
    w = e.w
    cbank = w.cbanks[slot]
    if not alive(e, cbank):
        return dict(exists=0)
    held = e.u32(cbank + 8)
    return dict(exists=1, rc=e.u32(cbank + 4), slot=next((s for s in range(SLOTS) if alive(e, w.banks[s]) and w.banks[s] == held), -1))


def ppc_case(e, case):
    e.heap_in_scratch = True
    data = case["bytes"]
    w = e.w = World()
    w.length, w.position = len(data), 0
    w.image = e.alloc(len(data), data) if data else 0
    w.banks, w.kinds, w.cbanks, w.factories, w.fake_headers, w.ref = [0] * SLOTS, [KIND_NONE] * SLOTS, [0, 0], [0, 0], {}, 0
    fixed_objects(e)
    fixed = {w.component: LABEL_COMPONENT, w.component + 4: LABEL_COMPONENT, w.owner: LABEL_OWNER, w.file_a: LABEL_FILE_A,
             w.file_b: LABEL_FILE_B}
    if w.image:
        fixed[w.image] = LABEL_IMAGE
    e.cl = L = CL.PpcLog(fixed, case.get("q"))
    saved = STUBS.activate(e)
    steps = []
    modified = set()
    try:
        for op in case["ops"]:
            before = len(L.log)
            try:
                result, aux = ppc_op(e, *op)
            except RuntimeError as ex:
                steps.append(dict(crash=str(ex)[:120], op=op))
                break
            state = [ppc_bank_state(e, s) for s in range(SLOTS)]
            cbanks = [ppc_cbank_state(e, s) for s in range(2)]
            for st in state:
                modified.update(o for o in st.get("cache", []) if o >= 0)
            image, fields = norm_image(e.read(w.image, w.length) if w.image else b"", modified, ">")
            steps.append(dict(result=result, aux=aux, banks=state, cbanks=cbanks, image=image, fields=fields, log=normalise(L.log[before:], False)))
        return dict(steps=steps)
    finally:
        CL.Stubs.deactivate(e, saved)


def normalise(events, host):
    """The host's header array and cache are allocated in the host's sizes: scale them to the Mac's by the event that
    precedes them (the swap of the offsets for the header array, the mapping of the file for the cache)."""
    out = []
    for i, event in enumerate(events):
        name, ints, texts = event
        if host and name == "new[]" and i > 0:
            if events[i - 1][0] == "_USwapBlock32" and HOST_HEADER and ints[0] % HOST_HEADER == 0:
                ints = (ints[0] // HOST_HEADER * 0x14, ints[1])
            elif events[i - 1][0] == "CFile::MapFile" and ints[0] % 8 == 0:
                ints = (ints[0] // 2, ints[1])
        out.append((name, tuple(ints), texts))
    return out


def host_case(lib, case):
    lib.tpw_sb_begin.argtypes = [C.c_char_p, C.c_int32]
    lib.tpw_sb_op.restype = C.c_int64
    lib.tpw_sb_op.argtypes = [C.c_int32] + [C.c_int64] * 4
    lib.tpw_sb_aux.restype = C.c_int64
    lib.tpw_sb_state.argtypes = [C.c_int32, C.POINTER(C.c_int64)]
    lib.tpw_sb_offset.restype = C.c_int64
    lib.tpw_sb_cache.restype = C.c_int64
    lib.tpw_sb_header.argtypes = [C.c_int32, C.c_int32, C.POINTER(C.c_int64)]
    lib.tpw_sb_hdata.argtypes = [C.c_int32, C.c_int32, C.c_char_p, C.c_int32]
    lib.tpw_sb_image.argtypes = [C.c_char_p, C.c_int32]
    lib.tpw_sb_cbank.argtypes = [C.c_int32, C.POINTER(C.c_int64)]
    CL.host_begin(lib, {}, case.get("q"))
    C.c_int32.in_dll(lib, "tpw_cl_on").value = 0
    data = case["bytes"]
    lib.tpw_sb_begin(data, len(data))
    lib.tpw_sb_labels()
    C.c_int32.in_dll(lib, "tpw_cl_on").value = 1
    steps = []
    modified = set()
    for op in case["ops"]:
        before = C.c_int32.in_dll(lib, "tpw_cl_n").value
        result = lib.tpw_sb_op(*op)
        aux = [lib.tpw_sb_aux(i) for i in range(8)]
        after = C.c_int32.in_dll(lib, "tpw_cl_n").value
        events = host_events(lib, before, after)
        banks = []
        for slot in range(SLOTS):
            out = (C.c_int64 * 6)()
            lib.tpw_sb_state(slot, out)
            if not out[0]:
                banks.append(dict(exists=0))
                continue
            kind, count = out[1], out[3]
            state = dict(exists=1, kind=kind, rc=out[2] & MASK, count=count & MASK, data=out[4], headers=out[5])
            n = min(count & MASK, DUMP_LIMIT)
            if kind == KIND_MM and out[4] and (count & MASK) <= 64:
                state["offsets"] = [lib.tpw_sb_offset(slot, i) for i in range(n)]
            if kind == KIND_MM and out[5] and (count & MASK) <= 64:
                rows = []
                for i in range(n):
                    h = (C.c_int64 * 7)()
                    lib.tpw_sb_header(slot, i, h)
                    size = h[0]
                    block = C.create_string_buffer(0x2001)
                    length = min(max(size, 0), 0x2000) if h[6] and size > 0 else 0
                    if length:
                        lib.tpw_sb_hdata(slot, i, block, length)
                    rows.append([size, h[1], h[2], h[3], h[4], h[5] & MASK, h[6], bytes(block.raw[:length])])
                state["headers_rows"] = rows
            if kind == KIND_FILE and out[5] and (count & MASK) <= 64:
                state["cache"] = [lib.tpw_sb_cache(slot, i) for i in range(count & MASK)]
            banks.append(state)
        cbanks = []
        for slot in range(2):
            out = (C.c_int64 * 3)()
            lib.tpw_sb_cbank(slot, out)
            cbanks.append(dict(exists=0) if not out[0] else dict(exists=1, rc=out[1] & MASK, slot=out[2]))
        image = C.create_string_buffer(len(data) + 1)
        lib.tpw_sb_image(image, len(data))
        for st in banks:
            modified.update(o for o in st.get("cache", []) if o >= 0)
        image, fields = norm_image(bytes(image.raw[:len(data)]), modified, "<")
        steps.append(dict(result=result, aux=aux, banks=banks, cbanks=cbanks, image=image, fields=fields, log=events))
    C.c_int32.in_dll(lib, "tpw_cl_on").value = 0
    return dict(steps=steps)


def host_events(lib, before, after):
    log = (CL.TpwCallEvent * 4096).in_dll(lib, "tpw_cl_log")
    out = []
    for i in range(before, min(after, 4096)):
        ev = log[i]
        out.append((ev.name.decode("latin-1"), tuple(ev.ints[k] for k in range(ev.nints)),
                    tuple(ev.texts[k].value.decode("latin-1") for k in range(ev.ntexts))))
    if after > 4096:
        out.append(("OVERFLOW", (after,), ()))
    return normalise(out, True)


# ---------------------------------------------------------------- scripts

def op(kind, a=0, b=0, c=0, d=0):
    return (kind, a, b, c, d)


def case(data, ops, q=None):
    return dict(bytes=data, ops=ops, q=q or {})


def full_script(kind, count_hint, indexes, owner=1):
    """Create, register, then ask everything of slot 0. (Index 0 only retrieves: the length and the position read before the
    array. A position is only asked within the count.)"""
    new = OP_NEW_MM if kind == KIND_MM else OP_NEW_FILE
    ops = [op(new, 0, owner, 0), op(OP_REGISTER, 0, 5)]
    for i in indexes:
        ops.append(op(OP_RETRIEVE, 0, i))
        if i >= 1:
            ops.append(op(OP_LENGTH, 0, i))
        if kind == KIND_MM and 1 <= i <= count_hint:
            ops.append(op(OP_POSITION, 0, i))
        elif kind == KIND_FILE:
            ops.append(op(OP_NAME, 0, i))
    ops += [op(OP_NAME, 0, 1), op(OP_HANDLE, 0), op(OP_ISTYPE, 0, 1), op(OP_ISTYPE, 0, 2), op(OP_ISTYPE, 0, 0), op(OP_STREAMING, 0),
            op(OP_NUMBER, 0), op(OP_INITIALISE, 0)]
    return ops


def bank_edges():
    r = random.Random(7)
    samples = random_samples(r, 4, big=False)
    plain = bank_file([dict(size=100), dict(size=0x2000), dict(size=0x2001), dict(size=5, rate=0, bits=0x10, flags=1)])
    for kind in (KIND_MM, KIND_FILE):
        new = OP_NEW_MM if kind == KIND_MM else OP_NEW_FILE
        yield case(plain, full_script(kind, 4, [0 if kind == KIND_MM else 1, 1, 2, 3, 4, 5]) + [op(OP_DEREGISTER, 0), op(OP_DESTROY, 0, 1)])
        yield case(plain, full_script(kind, 4, [4, 1, 1, 2]) + [op(OP_DESTROY, 0, 0)])
        yield case(plain, full_script(kind, 4, [3]) + [op(OP_DEREGISTER, 0), op(OP_DEREGISTER, 0), op(OP_DESTROY, 0, -1)])
        yield case(bank_file(samples), full_script(kind, 4, [1, 2, 3, 4]) + [op(OP_DESTROY, 0, 2)])
        for flag in (1, 0, -1, 2, 0x10000, 0x8000, 0x7FFF, 0xFFFF):
            yield case(plain, [op(new, 0, 1, 0), op(OP_REGISTER, 0), op(OP_DESTROY, 0, flag)])
        yield case(plain, [op(new, 0, 2, 1), op(OP_REGISTER, 0), op(OP_RETRIEVE, 0, 2), op(OP_DESTROY, 0, 1)])
        yield case(plain, [op(new, 0, 1, 0), op(OP_DESTROY, 0, 1)])
        yield case(plain, [op(new, 0, 1, 0), op(OP_DEREGISTER, 0), op(OP_DESTROY, 0, 0)])
        yield case(plain, [op(new, 0, 1, 0), op(OP_RETRIEVE, 0, 1), op(OP_NUMBER, 0), op(OP_DESTROY, 0, 1)])
        # two banks over the same file (a second file bank would convert the first one's headers again: the two sides differ then)
        if kind == KIND_MM:
            yield case(plain, [op(new, 0, 1, 0), op(new, 1, 2, 1), op(OP_REGISTER, 0), op(OP_REGISTER, 1), op(OP_RETRIEVE, 0, 1),
                               op(OP_RETRIEVE, 1, 1), op(OP_DESTROY, 1, 1), op(OP_DESTROY, 0, 1)])
        else:
            yield case(plain, [op(new, 0, 1, 0), op(new, 1, 2, 1), op(OP_REGISTER, 0), op(OP_REGISTER, 1), op(OP_DESTROY, 1, 1),
                               op(OP_DESTROY, 0, 1)])
    real = real_file("levels", "fantasy", "Sound", "LobbySfxHD.sdt")
    if real is None:
        print("test_SoundBank: the real bank (extracted/.../fantasy/Sound/LobbySfxHD.sdt) is not there: its cases are skipped")
    else:
        n = struct.unpack("<I", real[:4])[0]
        for kind in (KIND_MM, KIND_FILE):
            yield case(real, full_script(kind, n, [1, 2, n // 2, n - 1, n, n + 1]) + [op(OP_DESTROY, 0, 1)])
            yield case(real, full_script(kind, n, list(range(1, min(n, 12) + 1))) + [op(OP_DEREGISTER, 0), op(OP_DESTROY, 0, 1)])
    # failures of the file and of the allocator
    for kind in (KIND_MM, KIND_FILE):
        new = OP_NEW_MM if kind == KIND_MM else OP_NEW_FILE
        script = [op(new, 0, 1, 0), op(OP_REGISTER, 0), op(OP_RETRIEVE, 0, 1), op(OP_RETRIEVE, 0, 2), op(OP_LENGTH, 0, 1), op(OP_DESTROY, 0, 1)]
        yield case(b"", [op(new, 0, 1, 0), op(OP_REGISTER, 0), op(OP_RETRIEVE, 0, 1), op(OP_NUMBER, 0), op(OP_DESTROY, 0, 1)])
        yield case(b"\1\0\0\0", [op(new, 0, 1, 0), op(OP_REGISTER, 0), op(OP_NUMBER, 0), op(OP_DESTROY, 0, 1)])
        yield case(plain, [op(new, 0, 1, 0), op(OP_REGISTER, 0), op(OP_RETRIEVE, 0, 1), op(OP_DESTROY, 0, 1)], q={"Map": [0]})
        if kind == KIND_MM:
            yield case(b"\0\0\0\0", [op(new, 0, 1, 0), op(OP_REGISTER, 0), op(OP_NUMBER, 0), op(OP_DESTROY, 0, 1)])
            for q in ({"Seek": [-1]}, {"Read": [0]}, {"Read": [-1]}, {"Read": [4, 0]}, {"Read": [4, -2, 3]}, {"Seek": [0, -1]},
                      {"Seek": [0, 0, -1]}, {"Read": [4, -2, -2, 0]}, {"Read": [4, -2, -2, 20]}, {"Read": [4, -2, -2, -2, 5]}):
                yield case(plain, script, q=q)
            for tail in ([0], [1, 0], [1, 1, 0], [1, 1, 1, 0], [1, 1, 1, 1, 0], [1, 0, 1, 1, 1], [1, 1, 0, 1, 1], [0, 1, 1, 1]):
                yield case(plain, script, q={"Alloc": [1, 1] + tail})
            yield case(plain, script, q={"Alloc": [1, 1, 1, 1, 1, 1, 1, 0]})
        else:
            yield case(plain, script)
        yield case(plain, [op(new, 0, 1, 0)], q={"Alloc": [0]})
        yield case(plain, [op(new, 0, 1, 0), op(new, 1, 1, 1)], q={"Alloc": [1, 0]})


def random_script(r, kind, claimed, safe, single):
    """claimed: the count the file's first word says; safe: no failure was injected, so a registration succeeded; single: the bank
    is registered once (a file bank always: a second cache would convert the headers again; with failing allocations too: a
    second registration could lose the offsets block, which the original then reads through NULL)."""
    single = single or kind == KIND_FILE
    new = OP_NEW_MM if kind == KIND_MM else OP_NEW_FILE
    ops = [op(new, 0, r.choice([1, 2]), r.randrange(2))]
    registered = ever = False         # a file bank is registered once: a second cache would convert the headers again
    dead = False                      # a file bank whose cache is gone must not be asked for samples
    for _ in range(r.randrange(2, 14)):
        choice = r.random()
        index = r.choice([1, 1, 2, 3, claimed, claimed + 1, claimed + 5, r.randrange(1, claimed + 3), MASK, 0x80000000])
        zero = kind == KIND_MM and r.random() < 0.15
        if kind == KIND_FILE:
            index = max(index, 1)
        if choice < 0.2 and not registered and not (single and ever):
            ops.append(op(OP_REGISTER, 0, r.randrange(100)))
            registered = ever = True
            dead = False
        elif choice < 0.5 and not dead:
            ops.append(op(OP_RETRIEVE, 0, 0 if zero else index))
        elif choice < 0.6 and kind == KIND_MM and safe and registered and claimed > 0:
            ops.append(op(OP_POSITION, 0, r.randrange(1, claimed + 1)))
        elif choice < 0.75 and not dead:
            ops.append(op(OP_LENGTH, 0, max(index, 1)))
        elif choice < 0.8:
            ops.append(op(r.choice([OP_NAME, OP_HANDLE, OP_STREAMING, OP_NUMBER, OP_INITIALISE]), 0, index))
        elif choice < 0.85:
            ops.append(op(OP_ISTYPE, 0, r.choice([0, 1, 2, 3, MASK])))
        elif choice < 0.9 and registered:
            ops.append(op(OP_DEREGISTER, 0))
            registered = False
            dead = kind == KIND_FILE
        elif choice < 0.95 and not single:
            ops.append(op(OP_REGISTER, 0, 1))
            registered = True
            dead = False
        else:
            ops.append(op(OP_NUMBER, 0))
    ops.append(op(OP_DESTROY, 0, r.choice([1, 1, 0, -1, 2])))
    return ops


def bank_random(kind):
    def make(r):
        count = r.choice([0, 1, 2, 3, 5, 8])
        samples = random_samples(r, count)
        claimed = count
        lie = None
        if r.random() < 0.1:
            lie = r.randrange(0, 12) if kind == KIND_MM else r.randrange(0, count + 1)
            claimed = lie
        data = bank_file(samples, gap=r.choice([0, 0, 3, 16]), count=lie, seed=r.getrandbits(16))
        q = {}
        if kind == KIND_MM and r.random() < 0.15 and data:
            data = data[:r.randrange(0, len(data))]
        safe = True
        if r.random() < 0.3:
            # the object and the first block must be made; the rest fail as they come
            q["Alloc"] = [1, 1] + [r.choice([1, 1, 1, 0]) for _ in range(r.randrange(1, 12))]
            safe = False
        if kind == KIND_MM and r.random() < 0.15:
            # (the first read, of the count, is whole or nothing: a part of it lands in the host's byte order)
            q["Read"] = [r.choice([-2, -2, 0, 4, -1])] + [r.choice([-2, -2, -2, 0, 1, 3, 4, 100, -1]) for _ in range(r.randrange(0, 10))]
            safe = False
        if kind == KIND_MM and r.random() < 0.1:
            q["Seek"] = [r.choice([0, 0, 0, -1]) for _ in range(r.randrange(1, 10))]
            safe = False
        if r.random() < 0.08:
            q["Map"] = [0]
            safe = False
        if data != bank_file(samples, gap=0, count=None, seed=0):
            safe = safe and kind == KIND_MM and len(data) >= 4
        return case(data, random_script(r, kind, claimed, safe, "Alloc" in q or "Read" in q or "Seek" in q), q)
    return make


# ---------------------------------------------------------------- the fake bank: GetSampleLength and the TbBank base

def length_cases():
    sizes = [0, 1, 2, 3, 100, 1000, 22050, 44100, 0x10000, 0x7FFFFFFF, MASK, 0x80000000, 0xFFFFFFF0, 0x01000001, 0x01000003, 123456789]
    rates = [0x5622, 0x2B11, 0xAC44, 1, 0, 0xFFFF, 8000, 11025, 3]
    for size in sizes:
        for flags, bits in ((0, 8), (1, 8), (0, 0x10), (1, 0x10), (0xFE, 0x10), (0xFF, 0x11), (3, 0x10), (0x24, 0x10)):
            rate = random.Random(size ^ flags ^ bits).choice(rates)
            yield case(b"", length_script(size, rate, bits, flags))
    for rate in rates:
        yield case(b"", length_script(44100, rate, 0x10, 1))
        yield case(b"", length_script(0, rate, 8, 0))
        yield case(b"", length_script(MASK, rate, 8, 0))
    for errors in ([-1], [1], [0x7FFFFFFF], [0x80000000 - 2 ** 32], [-1, 0], [0, -1]):
        yield case(b"", length_script(1000, 22050, 0x10, 0)[:2] + [op(OP_LENGTH, 0, 1), op(OP_LENGTH, 0, 2)], q={"Retrieve": errors})


def length_script(size, rate, bits, flags):
    return [op(OP_NEW_FAKE, 0, 1), op(OP_POKE, 0, 1, size), op(OP_POKE, 0, 2, rate), op(OP_POKE, 0, 3, bits), op(OP_POKE, 0, 4, flags),
            op(OP_LENGTH, 0, 1), op(OP_LENGTH, 0, 0), op(OP_LENGTH, 0, MASK), op(OP_DESTROY, 0, 0)]


def length_random(r):
    size = r.choice([0, 1, r.getrandbits(32), r.getrandbits(20), r.getrandbits(31), r.randrange(0, 5000000)])
    rate = r.choice([0x5622, 0xAC44, 0, 1, r.getrandbits(16), r.randrange(1, 50000)])
    q = {}
    if r.random() < 0.1:
        q["Retrieve"] = [r.choice([0, -1, 5, -7])]
    script = length_script(size, rate, r.choice([0x10, 8, r.getrandbits(8)]), r.choice([0, 1, r.getrandbits(8)]))
    return case(b"", script, q)


def base_edges():
    for owner in (1, 2):
        yield case(b"", [op(OP_NEW_FAKE, 0, owner), op(OP_NUMBER, 0), op(OP_POKE, 0, 0, 5), op(OP_NUMBER, 0), op(OP_POKE, 0, 0, MASK),
                         op(OP_NUMBER, 0), op(OP_NAME, 0, 3), op(OP_POSITION, 0, 3), op(OP_HANDLE, 0), op(OP_STREAMING, 0), op(OP_INITIALISE, 0),
                         op(OP_REGISTER, 0, 4), op(OP_ISTYPE, 0, 0), op(OP_DEREGISTER, 0), op(OP_DESTROY, 0, 1)])
        for flag in (1, 0, -1, 2, 0x10000, 0x8000):
            yield case(b"", [op(OP_NEW_FAKE, 0, owner), op(OP_DESTROY, 0, flag)])
    yield case(b"", [op(OP_NEW_FAKE, 0, 1), op(OP_NEW_FAKE, 1, 1), op(OP_NEW_FAKE, 2, 2), op(OP_DESTROY, 1, 1), op(OP_DESTROY, 0, 1),
                     op(OP_DESTROY, 2, 1)])
    yield case(b"", [op(OP_NEW_FAKE, 0, 1)], q={"Alloc": [0]})
    # the reference manager alone
    for owner in (0, 1, 2):
        for flag in (1, 0, -1, 0x10000, 0x8000, 2):
            yield case(b"", [op(OP_REF_NEW, owner), op(OP_REF_DEL, flag)])
    yield case(b"", [op(OP_REF_NEW, 1)], q={"Alloc": [0]})


def base_random(r):
    ops = [op(OP_NEW_FAKE, 0, r.choice([1, 2]))]
    for _ in range(r.randrange(1, 8)):
        kind = r.choice([OP_NUMBER, OP_NAME, OP_POSITION, OP_HANDLE, OP_STREAMING, OP_INITIALISE, OP_ISTYPE, OP_REGISTER, OP_DEREGISTER, OP_POKE])
        if kind == OP_POKE:
            ops.append(op(OP_POKE, 0, 0, r.choice([0, 3, MASK, 0x80000000])))
        else:
            ops.append(op(kind, 0, r.choice([0, 1, 5, MASK])))
    ops.append(op(OP_DESTROY, 0, r.choice([1, 0, -1])))
    return case(b"", ops)


# ---------------------------------------------------------------- the factory and the bank handle

def factory_edges():
    plain = bank_file([dict(size=100), dict(size=0x2000), dict(size=0x2001)])
    short = b""
    real = real_file("levels", "fantasy", "Sound", "LobbySfxHD.sdt")
    for factory in (0, 1):
        make = op(OP_NEW_FACTORY, factory)
        # (a missing component is not tried: the bank's reference manager would call through NULL, as the original does)
        yield case(plain, [make, op(OP_CONSTRUCT, factory, 0, 1, 0), op(OP_DESTROY, 0, 1)])
        yield case(plain, [make, op(OP_GENERATE, factory, 0, 1, 0), op(OP_CBANK_RETRIEVE, 0, 1, 0), op(OP_CBANK_RETRIEVE, 0, 3, 0),
                           op(OP_CBANK_RETRIEVE, 0, 4, 0), op(OP_CBANK_FILE, 0), op(OP_CBANK_DESTROY, 0, 1)],
                   q={"CBErr": [0], "CBSlot": [-1]})
        yield case(plain, [make, op(OP_GENERATE, factory, 0, 1, 1)], q={"CBErr": [0], "CBSlot": [-1]})
    # the factory made a bank that the case built first
    for err in (0, -1, 5, -9):
        for slot in (0, 1, 2, -1):
            yield case(plain, [op(OP_NEW_MM, 0, 1, 0), op(OP_NEW_FILE, 1, 1, 1), op(OP_NEW_FAKE, 2, 2), op(OP_NEW_FACTORY, 1),
                               op(OP_GENERATE, 1, 0, 1, 0), op(OP_CBANK_RETRIEVE, 0, 1, slot), op(OP_CBANK_DESTROY, 0, 1)],
                       q={"CBErr": [err], "CBSlot": [slot]})
    # the real factory over files that register and files that do not
    # (the allocations in a row: the factory, the bank, the offsets, the headers, the three samples' data, the CBank; the offsets
    # must be there: the original reads into them without a test)
    for data, q in ((plain, {}), (short, {}), (b"", {}), (plain, {"Seek": [-1]}), (plain, {"Read": [0]}), (plain, {"Read": [-1]}),
                    (plain, {"Alloc": [0]}), (plain, {"Alloc": [1, 0]}), (plain, {"Alloc": [1, 1, 1, 0]}), (plain, {"Alloc": [1, 1, 1, 1, 0]}),
                    (plain, {"Alloc": [1, 1, 1, 1, 1, 0]}), (plain, {"Alloc": [1, 1, 1, 1, 1, 1, 0]}), (plain, {"Alloc": [1, 1, 1, 1, 1, 1, 1, 0]})):
        yield case(data, [op(OP_NEW_FACTORY, 0), op(OP_GENERATE, 0, 0, 1, 0), op(OP_CBANK_RETRIEVE, 0, 1, -1), op(OP_CBANK_DESTROY, 0, 1)], q=q)
        yield case(data, [op(OP_NEW_FACTORY, 0), op(OP_CONSTRUCT, 0, 0, 1, 1), op(OP_REGISTER, 0), op(OP_DESTROY, 0, 1)], q=q)
    if real is not None:
        yield case(real, [op(OP_NEW_FACTORY, 0), op(OP_GENERATE, 0, 0, 1, 0), op(OP_CBANK_RETRIEVE, 0, 1, -1), op(OP_CBANK_RETRIEVE, 0, 5, -1),
                          op(OP_CBANK_FILE, 0), op(OP_CBANK_DESTROY, 0, 1)])
    # the bank handle
    for kind in (OP_NEW_MM, OP_NEW_FILE, OP_NEW_FAKE):
        zero = [] if kind == OP_NEW_FILE else [op(OP_CBANK_RETRIEVE, 0, 0, 0)]      # (a file bank reads before its cache at index 0)
        yield case(plain, [op(kind, 0, 1, 0), op(OP_REGISTER, 0), op(OP_CBANK_NEW, 0, 0), op(OP_CBANK_FILE, 0), op(OP_CBANK_RETRIEVE, 0, 1, 0)]
                   + zero + [op(OP_CBANK_RETRIEVE, 0, 9, 0), op(OP_CBANK_DESTROY, 0, 1)])
        yield case(plain, [op(kind, 0, 1, 0), op(OP_CBANK_NEW, 0, 0), op(OP_CBANK_DESTROY, 0, 0)])
        yield case(plain, [op(kind, 0, 1, 0), op(OP_REGISTER, 0), op(OP_CBANK_NEW, 0, 0), op(OP_CBANK_DESTROY, 0, 2), op(OP_NUMBER, 0)])
    yield case(plain, [op(OP_NEW_MM, 0, 1, 0), op(OP_CBANK_NEW, 0, -1), op(OP_CBANK_FILE, 0), op(OP_CBANK_RETRIEVE, 0, 1, -1)])
    yield case(plain, [op(OP_NEW_MM, 0, 1, 0), op(OP_REGISTER, 0), op(OP_CBANK_NEW, 0, 0), op(OP_CBANK_DESTROY, 0, 0x10000), op(OP_CBANK_NEW, 1, 0)],
               q={"Alloc": [1, 1, 1, 1, 1, 1, 1, 0]})
    yield case(plain, [op(OP_NEW_MM, 0, 1, 0), op(OP_CBANK_NEW, 0, 0)], q={"Alloc": [1, 0]})


def factory_random(r):
    count = r.choice([0, 1, 2, 4])
    data = bank_file(random_samples(r, count, big=False), seed=r.getrandbits(16))
    if r.random() < 0.2 and data:
        data = data[:r.choice([0, r.randrange(4, len(data) + 1)])]    # (a part of the count word would land in the host's order)
    q = {}
    real = r.random() < 0.7
    if r.random() < 0.4:
        # the factory, the bank and the offsets (a fake factory: three banks and factory first) are always there
        q["Alloc"] = [1] * (3 if real else 4) + [r.choice([1, 1, 1, 0]) for _ in range(r.randrange(1, 12))]
    if r.random() < 0.2:
        q["Read"] = [r.choice([-2, 0, 4, -1])] + [r.choice([-2, -2, 0, 4, 3, -1]) for _ in range(r.randrange(0, 6))]
    if r.random() < 0.15:
        q["Seek"] = [r.choice([0, -1]) for _ in range(r.randrange(1, 6))]
    ops = [op(OP_NEW_FACTORY, 0 if real else 1)]
    if not real:
        ops = [op(OP_NEW_MM, 0, 1, 0), op(OP_NEW_FILE, 1, 2, 1)] + ops
        q["CBErr"] = [r.choice([0, 0, -1, 4])]
        q["CBSlot"] = [r.choice([-1, 0, 1])]
    ops.append(op(OP_GENERATE, 0 if real else 1, 0, 1, r.randrange(2)))
    ops.append(op(OP_CBANK_RETRIEVE, 0, r.choice([0, 1, 2, count, count + 1]) if real else max(1, r.choice([1, 2, count, count + 1])), q["CBSlot"][0] if not real else -1))
    ops.append(op(OP_CBANK_DESTROY, 0, r.choice([1, 0])))
    return case(data, ops, q)


# ---------------------------------------------------------------- the direct functions

def swap_edges():
    return [dict(v=v) for v in (0, 1, 0xFF, 0x100, 0x12345678, 0x80000000, 0x7FFFFFFF, MASK, 0xDEADBEEF, 0x00FF00FF, 0xFF00FF00, 0x01020304)]


def swap_random(r):
    return dict(v=r.getrandbits(32))


def bswap(v, bits):
    return int.from_bytes((v & ((1 << bits) - 1)).to_bytes(bits // 8, "big"), "little")


# The file's bytes hold the little-endian value V. The emulator reads them as a big-endian word (bswap(V)) and the helper swaps
# it back; the host reads them as V and its helper does nothing: both answer V.
def swap_ppc(address, bits):
    def run(e, c):
        return dict(r=call(e, address, [bswap(c["v"], bits)]) & ((1 << bits) - 1))
    return run


def swap_host(which, bits):
    def run(lib, c):
        v = c["v"] & ((1 << bits) - 1)
        if bits == 16:
            lib.tpw_sb_swap16.restype = C.c_uint32
            lib.tpw_sb_swap16.argtypes = [C.c_uint32]
            return dict(r=lib.tpw_sb_swap16(v) & 0xFFFF)
        lib.tpw_sb_swap32.restype = C.c_uint32
        lib.tpw_sb_swap32.argtypes = [C.c_int32, C.c_uint32]
        return dict(r=lib.tpw_sb_swap32(which, v))
    return run


def inplace_ppc(e, c):
    word = e.alloc(8, struct.pack("<II", c["v"], 0x55555555))
    call(e, 0x10009588, [word])
    return dict(r=e.u32(word), next=struct.unpack("<I", e.read(word + 4, 4))[0])


def inplace_host(lib, c):
    words = (C.c_uint32 * 2)(c["v"], 0x55555555)
    lib.tpw_sb_swap_inplace.argtypes = [C.POINTER(C.c_uint32)]
    lib.tpw_sb_swap_inplace(words)
    return dict(r=words[0], next=words[1])


def block_case(r):
    n = r.choice([0, 1, 2, 5, 17])
    return dict(words=[r.getrandbits(32) for _ in range(n)])


def block_edges():
    return [dict(words=[]), dict(words=[1]), dict(words=[0x12345678, MASK, 0]), dict(words=[0x80000000] * 4)]


def block_ppc(e, c):
    L = e.cl = CL.PpcLog({})
    saved = STUBS.activate(e)
    try:
        words = c["words"]
        block = e.alloc(4 * len(words) + 8, struct.pack("<%dI" % len(words), *words) + b"\xaa" * 8)
        L.labels[block] = 1
        call(e, 0x10009528, [block, len(words)])
        return dict(words=list(struct.unpack(">%dI" % len(words), e.read(block, 4 * len(words)))), tail=e.read(block + 4 * len(words), 8), log=L.log)
    finally:
        CL.Stubs.deactivate(e, saved)


def block_host(lib, c):
    words = c["words"]
    array = (C.c_uint32 * (len(words) + 2))(*words, 0xAAAAAAAA, 0xAAAAAAAA)
    CL.host_begin(lib, {1: C.addressof(array)})
    lib.tpw_sb_swap_block.argtypes = [C.c_void_p, C.c_int32]
    lib.tpw_sb_swap_block(C.addressof(array), len(words))
    log = CL.host_end(lib)
    tail = struct.pack("<II", array[len(words)], array[len(words) + 1])
    return dict(words=list(array[:len(words)]), tail=tail, log=log)


def cvt_values():
    values = [0.0, -0.0, 1e-300, 0.5, 0.999999, 1.0, 1.5, 100.7, 2147483647.0, 2147483647.5, 2147483647.99, 2147483648.0, 2147483648.5,
              2147483649.0, 3000000000.7, 4294967294.5, 4294967295.0, 4294967295.5, 4294967295.9999, 4294967296.0, 4294967297.0, 1e10, 1e300,
              float("inf"), float("-inf"), float("nan"), -1.0, -0.5, -1e-300, -2147483648.0, -4294967296.0, 22.5, 1000.0, 65535.5, 16777216.0]
    return [dict(v=v) for v in values]


def cvt_random(r):
    return dict(v=r.choice([r.uniform(-10, 5e9), r.uniform(0, 4294967296.0), r.uniform(2e9, 2.3e9), float(r.getrandbits(32)), r.getrandbits(32) + 0.5]))


def cvt_ppc(e, c):
    return dict(r=T.call_ppc(e, CVT, [T.D(c["v"])])[0])


def cvt_host(lib, c):
    lib.tpw_sb_cvt.restype = C.c_uint32
    lib.tpw_sb_cvt.argtypes = [C.c_double]
    return dict(r=lib.tpw_sb_cvt(c["v"]))


INFO_STRINGS = [None, "alpha", "", "a much longer bank path name.sdt"]


def info_cases():
    out = []
    for kind in range(4):
        for path in (1, 2, 3):
            for type_, size in ((0, 0), (7, 123), (MASK, MASK), (0x80000000, 0x7FFFFFFF)):
                for file in (0, 3, 5):
                    out.append(dict(kind=kind, type=type_, path=path, size=size, file=file))
    out.append(dict(kind=1, type=3, path=0, size=0, file=0))
    out.append(dict(kind=3, type=3, path=0, size=0, file=5))
    return out


def info_random(r):
    return dict(kind=r.randrange(4), type=r.getrandbits(32), path=r.randrange(4), size=r.getrandbits(32), file=r.choice([0, 3, 5]))


def info_ppc(e, c):
    e.heap_in_scratch = True
    file_a = e.alloc(8, b"\0" * 8)
    file_b = e.alloc(8, b"\0" * 8)
    pre = e.alloc(8, b"pre\0\0\0\0\0")
    path = e.alloc(64, (INFO_STRINGS[c["path"]] or "").encode() + b"\0" * 40) if c["path"] else 0
    default = e.read(0x1002F382, 1)      # the shared empty string
    info = e.alloc(16, struct.pack(">IIII", 0xAAAAAAAA, 0xBBBBBBBB, pre, file_b))
    file = {0: 0, 3: file_a, 5: file_b}[c["file"]]
    kind = c["kind"]
    args = [info] + ([] if kind == 0 else [c["type"], path] + ([c["size"]] if kind == 2 else [file] if kind == 3 else []))
    call(e, INFO_CTORS[kind], args)
    f, s, p = call(e, INFO_GET_FILE, [info]), call(e, INFO_GET_SIZE, [info]), call(e, INFO_GET_PATH, [info])
    word_path, word_file = e.u32(info + 8), e.u32(info + 0xC)
    text = e.cstr(word_path) if word_path else "(null)"
    return dict(has_path=int(word_path != 0), type=e.u32(info), size=e.u32(info + 4), file=0 if word_file == 0 else 3 if word_file == file_a else 5 if word_file == file_b else -1,
                text=text[:31], getters=int(f == word_file and s == e.u32(info + 4) and p == word_path))


def info_host(lib, c):
    lib.tpw_sb_info.argtypes = [C.c_int32, C.c_uint32, C.c_char_p, C.c_uint32, C.c_int32, C.POINTER(C.c_int64), C.c_char_p]
    out = (C.c_int64 * 5)()
    text = C.create_string_buffer(32)
    path = INFO_STRINGS[c["path"]]
    lib.tpw_sb_info(c["kind"], c["type"], path.encode() if path is not None and c["path"] else None, c["size"], c["file"], out, text)
    return dict(has_path=out[0], type=out[1] & MASK, size=out[2] & MASK, file=out[3], text=text.value.decode("latin-1"), getters=out[4])


def setbank_cases():
    return [dict(flags=f, bank=b) for f in (0, 8, 1, 0xFFFFFFF7, MASK, 0x80000000, 7) for b in (0, 1)]


def setbank_random(r):
    return dict(flags=r.getrandbits(32), bank=r.randrange(2))


def setbank_ppc(e, c):
    info = e.alloc(0x68, b"\0" * 0x68)
    ws(e, info + 4, [0x1234])
    ws(e, info + 0x14, [c["flags"]])
    call(e, SET_BANK, [info, 0x5678 if c["bank"] else 0])
    word = e.u32(info + 4)
    return dict(flags=e.u32(info + 0x14), bank=0 if word == 0 else 1 if word == 0x5678 else 2)


def setbank_host(lib, c):
    lib.tpw_sb_setbank.argtypes = [C.c_uint32, C.c_int32, C.POINTER(C.c_int64)]
    out = (C.c_int64 * 2)()
    lib.tpw_sb_setbank(c["flags"], c["bank"], out)
    return dict(flags=out[0] & MASK, bank=out[1])


BANKPATH_TEXTS = ["", "a", "bank", "global:Sfx", "x" * 100, "Path:Name"]


def bankpath_cases():
    return [dict(text=t) for t in BANKPATH_TEXTS]


def bankpath_random(r):
    return dict(text="".join(r.choice("abcXYZ:_0 ") for _ in range(r.randrange(0, 40))))


def bankpath_ppc(e, c):
    e.heap_in_scratch = True
    path = e.alloc(0x14, b"\0" * 0x14)
    name = e.alloc(0x14, b"\0" * 0x14)
    ws(e, name, [VT["string"]])
    pointer = CL.new_text(e, c["text"])
    ws(e, name + 8, [pointer, len(c["text"])])
    storage = e.alloc(16, b"\0" * 16)
    result = call(e, GET_BANK_PATH, [storage, path, name, 5])
    return dict(result=s32(result), text=CL.tds_text(e, name), length=e.u32(name + 0xC), untouched=e.read(path, 0x14))


def bankpath_host(lib, c):
    lib.tpw_sb_bankpath.restype = C.c_int32
    lib.tpw_sb_bankpath.argtypes = [C.c_char_p, C.c_char_p, C.c_int32]
    out = C.create_string_buffer(256)
    result = lib.tpw_sb_bankpath(c["text"].encode(), out, 256)
    return dict(result=result, text=out.value.decode("latin-1"), length=len(out.value), untouched=b"\0" * 0x14)


def watcher_ppc(e, c):
    name = e.alloc(16, b"\xa5" * 16)
    before = e.read(name, 16)
    call(e, WATCHER, [name, name])
    return dict(r=1, same=int(before == e.read(name, 16)))


def watcher_host(lib, c):
    lib.tpw_sb_watcher.restype = C.c_int32
    CL.host_begin(lib, {})
    C.c_int32.in_dll(lib, "tpw_cl_on").value = 0
    return dict(r=lib.tpw_sb_watcher(), same=1)


# ---------------------------------------------------------------- the vtables (the slot orders of include/TbSoundBank.h)

IMPORT_RELEASE, IMPORT_ADDREF = 0x1003F438, 0x1003F3F0       # the function descriptors of TbRefRelease::Release and AddRef
VTABLES = {
    BANK_VT: {8: IMPORT_RELEASE, 0xC: IMPORT_ADDREF, 0x10: 0x10008C28, 0x14: 0, 0x18: 0x10008AC0, 0x1C: 0, 0x20: 0x100089BC, 0x24: 0x10008AF8,
              0x28: 0x10008B2C, 0x2C: 0x10008B64, 0x30: 0, 0x34: 0x10008B94, 0x38: 0x10008BC8, 0x3C: 0},
    FILE_VT: {8: IMPORT_RELEASE, 0xC: IMPORT_ADDREF, 0x10: 0x10009144, 0x14: 0x10008CC0, 0x18: 0x10008AC0, 0x1C: 0x10008E94, 0x20: 0x100089BC,
              0x24: 0x100090CC, 0x28: 0x10008B2C, 0x2C: 0x10008B64, 0x30: 0x10009108, 0x34: 0x10008B94, 0x38: 0x10008BC8, 0x3C: 0x10008E0C},
    MM_VT: {8: IMPORT_RELEASE, 0xC: IMPORT_ADDREF, 0x10: 0x10009834, 0x14: 0x100091F4, 0x18: 0x10008AC0, 0x1C: 0x100096C8, 0x20: 0x100089BC,
            0x24: 0x10008AF8, 0x28: 0x100098E8, 0x2C: 0x100097BC, 0x30: 0x100097F4, 0x34: 0x10009780, 0x38: 0x10008BC8, 0x3C: 0x100095D0},
    FACTORY_VT: {8: 0x100141C0, 0xC: 0x10013FB4},
    CBANK_VT: {8: IMPORT_RELEASE, 0xC: IMPORT_ADDREF, 0x10: 0x10014464},
    0x1003D5E0: {8: 0},          # CAudioFileSystem: one pure slot
}


def vtable_cases():
    return [dict(vt=vt) for vt in VTABLES]


def vtable_ppc(e, c):
    out = {}
    for slot in VTABLES[c["vt"]]:
        tv = e.u32(c["vt"] + slot)
        out[slot] = tv if tv in (0, IMPORT_RELEASE, IMPORT_ADDREF) else e.u32(tv)
    return dict(slots=out)


def vtable_host(lib, c):
    return dict(slots=dict(VTABLES[c["vt"]]))


TESTS = [
    FnTest("USwap32<unsigned long> 0x10008db4", lambda e, c: swap_ppc(0x10008DB4, 32)(e, c), swap_host(0, 32), edge=swap_edges, rnd=swap_random),
    FnTest("USwap16<unsigned short> 0x10008fd0", lambda e, c: swap_ppc(0x10008FD0, 16)(e, c), swap_host(0, 16), edge=swap_edges, rnd=swap_random),
    FnTest("USwap16<unsigned short> 0x1000942c", lambda e, c: swap_ppc(0x1000942C, 16)(e, c), swap_host(0, 16), edge=swap_edges, rnd=swap_random),
    FnTest("USwap32<unsigned int> 0x10009020", lambda e, c: swap_ppc(0x10009020, 32)(e, c), swap_host(2, 32), edge=swap_edges, rnd=swap_random),
    FnTest("USwap32<unsigned int> 0x1000947c", lambda e, c: swap_ppc(0x1000947C, 32)(e, c), swap_host(2, 32), edge=swap_edges, rnd=swap_random),
    FnTest("USwap32<int> 0x10009078", lambda e, c: swap_ppc(0x10009078, 32)(e, c), swap_host(1, 32), edge=swap_edges, rnd=swap_random),
    FnTest("USwap32<int> 0x100094d4", lambda e, c: swap_ppc(0x100094D4, 32)(e, c), swap_host(1, 32), edge=swap_edges, rnd=swap_random),
    FnTest("USwapInplace32<unsigned long> 0x10009588", inplace_ppc, inplace_host, edge=swap_edges, rnd=swap_random),
    FnTest("USwapBlock32<unsigned long> 0x10009528", block_ppc, block_host, edge=block_edges, rnd=block_case),
    FnTest("double to unsigned long 0x1002e988", cvt_ppc, cvt_host, edge=cvt_values, rnd=cvt_random),
    FnTest("TbSoundBankInfo ctors 0x100099d8 0x10009a1c 0x10009a58 0x100277c4 and getters 0x10014150 0x10014188 0x100143b0", info_ppc, info_host,
           edge=info_cases, rnd=info_random),
    FnTest("TbSoundSampleInfo::SetBank 0x1000df50", setbank_ppc, setbank_host, edge=setbank_cases, rnd=setbank_random),
    FnTest("CFileOperation::GetBankPath 0x1001e720", bankpath_ppc, bankpath_host, edge=bankpath_cases, rnd=bankpath_random),
    FnTest("UMemoryWatcher::UMemoryWatcher 0x10014354", watcher_ppc, watcher_host, edge=lambda: [dict()], rnd=lambda r: dict()),
    FnTest("TbBank base 0x100088a8 0x10008c28 0x10008ac0 0x10008af8 0x10008b2c 0x10008b64 0x10008b94 0x10008bc8 and CRefManager 0x10008940 0x10000ec0",
           ppc_case, host_case, edge=base_edges, rnd=base_random),
    FnTest("TbBank::GetSampleLength 0x100089bc", ppc_case, host_case, edge=length_cases, rnd=length_random),
    FnTest("TbMMFileBank 0x100140a0 0x100091f4 0x100095d0 0x100096c8 0x10009780 0x100097bc 0x100097f4 0x10009834 0x100098e8", ppc_case, host_case,
           edge=lambda: [c for c in bank_edges() if c["ops"][0][0] == OP_NEW_MM], rnd=bank_random(KIND_MM)),
    FnTest("TbFileBank 0x10008cc0 0x10008e0c 0x10008e94 0x100090cc 0x10009108 0x10009144", ppc_case, host_case,
           edge=lambda: [c for c in bank_edges() if c["ops"][0][0] == OP_NEW_FILE], rnd=bank_random(KIND_FILE)),
    FnTest("the vtables 0x1003be84 0x1003befc 0x1003bf74 0x1003c830 0x1003c85c 0x1003d5e0", vtable_ppc, vtable_host, edge=vtable_cases,
           rnd=lambda r: dict(vt=r.choice(list(VTABLES)))),
    FnTest("CBankFactory 0x10027820 0x10013fb4 0x100141c0 and Bank::CBank 0x100143e8 0x10014464 0x10014508 0x10014884", ppc_case, host_case,
           edge=factory_edges, rnd=factory_random),
]
