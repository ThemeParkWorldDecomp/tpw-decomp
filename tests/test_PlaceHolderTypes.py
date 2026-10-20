"""Unit w79g3, the placeholder subclasses, against libtpw_unit_PlaceHolderTypes.dylib (tests/unit_PlaceHolderTypes.cpp), in the program
"sound symbols shared" (Code at 0x10000000, TOC base r2 = 0x1003abf0): the 67 functions of src/PlaceHolderTypes.cpp, listed in FUNCS below with
their addresses (CPlaceHolderOneShot, CPlaceHolderOneShotSentence, CPlaceHolderOneShotBranchingSentence, CPlaceHolderSentence,
CPlaceHolderLinearSentence, CPlaceHolderBranchingSentence, CPlaceHolderSentenceShuffle with CListShuffle, CListSentence with CListData: the
constructors, destructors, class ids, the eight four-argument and eight one-argument CreateObject statics, and the overrides).

Each function runs as the original in the emulator and as the host's translation, on a world built from the case. The virtual overrides
are called THROUGH THE OBJECT'S VTABLE on both sides (the PowerPC side reads the slot of the object's real Mac vtable, the host makes a
virtual call through a CAudioPlaceHolder*), so a wrong slot map or a wrong class chain shows as a different function running; the
constructors are checked through the vptr on the PowerPC side and through a virtual GetClassID on both. The base class CAudioPlaceHolder
(w79g2) and the Elements of w79g4 are REAL on both sides, so the subclasses run over the real base functions. The world: a subject of
the class (made by the real constructor, then a preset state of 34 words written into the members, the extra data updated in place), a
chain of Element children for the one-shot sentences, a tree of events, sounds, samples and branches (one block: Mac layout with the
pointer fields relocated, in the emulator's memory and in the host's arena), a category container whose AddRef and Release record, a
category table for CListSentence, an event list, a sound manager that is a recording fake (its getters, Rolloff, PlaySample,
ConstructSample and SystemCommand), TbEventMapper::MapEvent (answers from a queue), bfl::maths::Random and RangedRandomSlow (queues; the
seed they were given is recorded), the clock, and the sample info. The call logs, the answer, the state of the subject and of the first
three children (all members, the extra data), the aux words (the list data or the shuffle), the sound records the shuffle moves, the
container's reference count, the sample info and the two statics are compared. NaN floats compare as one value.

Labels: 1 the subject, 2 the manager, 3 the volume control, 4 the category container, 5 the sample info, 7 the streamer, 8 the subject's
mHandle field, 9 the array of a CListData, 10 the standalone list data or shuffle (and the subject's shuffle), 11..13 the banks, 14 the
event list, 16 the category data, 17 the subject's extra data, 21.. the events, 31.. the sounds, 40..43 the category containers, 50..53
the chained children, 55..58 their extra data, 71.. the samples, 480..483 the children's mHandle fields, 150 and up the objects made.
"""
import ctypes as C
import math
import random
import struct

import calllog as CL
import ppcemu
import tpwtest as T
from tpwtest import FnTest
from unicorn import ppc_const as P
import test_AudioPlaceHolder as AP
from test_AudioPlaceHolder import (MASK, FILL, ONE, NSTATE, S_EVENT, S_SOUND, S_SAMPLE, S_POSX, S_POSY, S_POSZ, S_VELX, S_VELY, S_VELZ, S_ENDTIME,
                                   S_PRIORITY, S_HANDLE, S_CATAGORY, S_LISTHANDLE, S_FLAGS, S_NEXT, S_PREVIOUS, S_PAUSETIME, S_MAXDIST,
                                   S_LASTSOUND, S_LASTSAMPLE, S_VCINDEX, S_FADE, S_VOLUME, S_PITCH, S_EXTRA, S_IDS, S_VALUES, S_LEFT, S_TOP,
                                   S_RIGHT, S_BOTTOM, S_CHILD, S_PARENT, F_PLAYING, F_LOOPING, F_LOOPFOREVER, F_NONPOS, F_FINISHED, F_S20,
                                   F_S40, F_MAXDIST, F_FIXEDVOL, F_VCOFF, F_PAUSED, ALL_FLAGS, fbits, bits_f, canon, lbl, pack, f_bits,
                                   make_sample, make_sound, make_event, rand_events, rand_state, rword, queues_common, default_state,
                                   dump_info, rpos, rbits)
import test_PlaceHolderHelpers as PH
from test_PlaceHolderHelpers import tree_image, build_image, World, add_branches, dump_holder

PROGRAM = "sound symbols shared"
SHIM_LIBRARY = "unit_PlaceHolderTypes"

S_MGR, S_SEED = 0x1003F360, 0x1003F35C
FIRST_NEW = 150

OS, OSS, OSBS, SEN, LIN, BRA, SHU, LST = range(8)
KIND_NAME = ["CPlaceHolderOneShot", "CPlaceHolderOneShotSentence", "CPlaceHolderOneShotBranchingSentence", "CPlaceHolderSentence",
             "CPlaceHolderLinearSentence", "CPlaceHolderBranchingSentence", "CPlaceHolderSentenceShuffle", "CListSentence"]
CTOR = [0x1001DFD0, 0x1001DEB8, 0x1001DE3C, 0x1001E0AC, 0x1001E278, 0x1001E3FC, 0x10024D24, 0x1001E974]
VT = [0x1003D384, 0x1003D438, 0x1003CD00, 0x1003D580, 0x1003D2F4, 0x1003D240, 0x1003D18C, 0x1003CE00]
CREATE1 = [0x1001E194, 0x1001E11C, 0x1001DDBC, 0x1001E03C, 0x1001E204, 0x1001E384, 0x10024F68, 0x1001E8E4]
CREATE4 = [0x100259A0, 0x1002669C, 0x10025C38, 0x10026C24, 0x100256F4, 0x10025504, 0x10024E64, 0x1001E7E8]
DTOR = [0x1001DF38, 0x10025D48, 0x1001E5E8, 0x1001E2E8, 0x10025904, 0x10025654, 0x10024DB0, 0x1001EF68]
CLASSID = [0x10025BF8, 0x100267A0, 0x1001E598, 0x1001F09C, 0x100258BC, 0x1002560C, 0x1002515C, 0x1001F09C]
EXPECT_ID = [3, 4, 6, 5, 2, 1, 10, 5]
ELEMENT_CTOR, BRANCHING_ELEMENT_CTOR = 0x10024218, 0x10023D38

# the operations (the unit file's Op enum)
K_CTOR, K_DTOR, K_CLASSID, K_CREATE1, K_CREATE4 = 0, 8, 16, 24, 32
(K_GETELEMENT, K_LDCTOR, K_LDDTOR, K_SHCTOR, K_SHINIT, K_SHSHUFFLE, K_SHNEXT, K_SETLISTINFO, K_CHOOSE, K_ASSIGN, K_UPDPARAM, K_PLAY, K_STOP,
 K_UPDATE, K_UPDDEP, K_FIRSTCHILD, K_PROCESS, K_ADDELEMENT, K_REMOVE, K_CALLBACK, K_BASIC) = range(40, 61)

# the parameter words and the result words (the unit file's enums)
P_OP, P_MGR, P_SEED, P_KIND, P_NCHILD, P_NCAT, P_BKIND, P_A, P_STATE, P_CHILD, P_INFO = 0, 1, 2, 3, 4, 5, 6, 8, 24, 60, 198
P_LIST, P_CAT, P_EVT, P_SHUF, P_IMG = 202, 222, 232, 240, 256
O_R, O_STATE, O_CHILD, O_CAT, O_INFO, O_STATIC, O_SOUNDS, O_AUX, O_SLOT, O_BLOCK = 0, 8, 44, 184, 186, 210, 214, 336, 360, 392

# name, address, op, the kinds the function is run on, the vtable slot it is called through (None: by address)
FUNCS = [
    ("CPlaceHolderOneShotBranchingSentence::CreateObject(void*)", 0x1001DDBC, K_CREATE1 + OSBS, [OSBS], None),
    ("CPlaceHolderOneShotBranchingSentence::CPlaceHolderOneShotBranchingSentence", 0x1001DE3C, K_CTOR + OSBS, [OSBS], None),
    ("CPlaceHolderOneShotSentence::CPlaceHolderOneShotSentence", 0x1001DEB8, K_CTOR + OSS, [OSS], None),
    ("CPlaceHolderOneShot::~CPlaceHolderOneShot", 0x1001DF38, K_DTOR + OS, [OS], 0x10),
    ("CPlaceHolderOneShot::CPlaceHolderOneShot", 0x1001DFD0, K_CTOR + OS, [OS], None),
    ("CPlaceHolderSentence::CreateObject(void*)", 0x1001E03C, K_CREATE1 + SEN, [SEN], None),
    ("CPlaceHolderSentence::CPlaceHolderSentence", 0x1001E0AC, K_CTOR + SEN, [SEN], None),
    ("CPlaceHolderOneShotSentence::CreateObject(void*)", 0x1001E11C, K_CREATE1 + OSS, [OSS], None),
    ("CPlaceHolderOneShot::CreateObject(void*)", 0x1001E194, K_CREATE1 + OS, [OS], None),
    ("CPlaceHolderLinearSentence::CreateObject(void*)", 0x1001E204, K_CREATE1 + LIN, [LIN], None),
    ("CPlaceHolderLinearSentence::CPlaceHolderLinearSentence", 0x1001E278, K_CTOR + LIN, [LIN], None),
    ("CPlaceHolderSentence::~CPlaceHolderSentence", 0x1001E2E8, K_DTOR + SEN, [SEN], 0x10),
    ("CPlaceHolderBranchingSentence::CreateObject(void*)", 0x1001E384, K_CREATE1 + BRA, [BRA], None),
    ("CPlaceHolderBranchingSentence::CPlaceHolderBranchingSentence", 0x1001E3FC, K_CTOR + BRA, [BRA], None),
    ("CPlaceHolderOneShotBranchingSentence::GetElement", 0x1001E4F8, K_GETELEMENT, [OSBS], 0x58),
    ("CPlaceHolderOneShotBranchingSentence::GetClassID", 0x1001E598, K_CLASSID + OSBS, [OSBS], 0x54),
    ("CPlaceHolderOneShotBranchingSentence::~CPlaceHolderOneShotBranchingSentence", 0x1001E5E8, K_DTOR + OSBS, [OSBS], 0x10),
    ("CListSentence::CreateObject(CCatagoryContainer*, ...)", 0x1001E7E8, K_CREATE4 + LST, [LST], None),
    ("CListSentence::CreateObject(void*, const TbAudioEventList*)", 0x1001E8E4, K_CREATE1 + LST, [LST], None),
    ("CListSentence::CListSentence", 0x1001E974, K_CTOR + LST, [LST], None),
    ("CListSentence::CListData::CListData", 0x1001EA2C, K_LDCTOR, [LST], None),
    ("CListSentence::ChooseRandomSound", 0x1001ED20, K_CHOOSE, [LST], 0x3C),
    ("CListSentence::~CListSentence", 0x1001EF68, K_DTOR + LST, [LST], 0x10),
    ("CListSentence::CListData::~CListData", 0x1001F008, K_LDDTOR, [LST], None),
    ("CPlaceHolderSentence::GetClassID", 0x1001F09C, K_CLASSID + SEN, [SEN, LST], 0x54),
    ("CPlaceHolderSentenceShuffle::CListShuffle::CListShuffle", 0x10024910, K_SHCTOR, [SHU], None),
    ("CPlaceHolderSentenceShuffle::CListShuffle::Init", 0x100249D4, K_SHINIT, [SHU], None),
    ("CPlaceHolderSentenceShuffle::CListShuffle::Shuffle", 0x10024A50, K_SHSHUFFLE, [SHU], None),
    ("CPlaceHolderSentenceShuffle::CListShuffle::GetNextSound", 0x10024C5C, K_SHNEXT, [SHU], None),
    ("CPlaceHolderSentenceShuffle::CPlaceHolderSentenceShuffle", 0x10024D24, K_CTOR + SHU, [SHU], None),
    ("CPlaceHolderSentenceShuffle::~CPlaceHolderSentenceShuffle", 0x10024DB0, K_DTOR + SHU, [SHU], 0x10),
    ("CPlaceHolderSentenceShuffle::CreateObject(CCatagoryContainer*, ...)", 0x10024E64, K_CREATE4 + SHU, [SHU], None),
    ("CPlaceHolderSentenceShuffle::CreateObject(void*)", 0x10024F68, K_CREATE1 + SHU, [SHU], None),
    ("CPlaceHolderSentenceShuffle::SetListInfo", 0x10024FE0, K_SETLISTINFO, [SHU], None),
    ("CPlaceHolderSentenceShuffle::ChooseRandomSound", 0x100250B8, K_CHOOSE, [SHU], 0x3C),
    ("CPlaceHolderSentenceShuffle::GetClassID", 0x1002515C, K_CLASSID + SHU, [SHU], 0x54),
    ("CPlaceHolderBranchingSentence::AssignSoundToNextBranch", 0x100251A4, K_ASSIGN, [BRA], None),
    ("CPlaceHolderBranchingSentence::UpdateParameter", 0x10025380, K_UPDPARAM, [BRA], 0x34),
    ("CPlaceHolderBranchingSentence::ChooseRandomSound", 0x10025468, K_CHOOSE, [BRA], 0x3C),
    ("CPlaceHolderBranchingSentence::CreateObject(CCatagoryContainer*, ...)", 0x10025504, K_CREATE4 + BRA, [BRA], None),
    ("CPlaceHolderBranchingSentence::GetClassID", 0x1002560C, K_CLASSID + BRA, [BRA], 0x54),
    ("CPlaceHolderBranchingSentence::~CPlaceHolderBranchingSentence", 0x10025654, K_DTOR + BRA, [BRA], 0x10),
    ("CPlaceHolderLinearSentence::CreateObject(CCatagoryContainer*, ...)", 0x100256F4, K_CREATE4 + LIN, [LIN], None),
    ("CPlaceHolderLinearSentence::ChooseRandomSound", 0x100257F8, K_CHOOSE, [LIN], 0x3C),
    ("CPlaceHolderLinearSentence::GetClassID", 0x100258BC, K_CLASSID + LIN, [LIN], 0x54),
    ("CPlaceHolderLinearSentence::~CPlaceHolderLinearSentence", 0x10025904, K_DTOR + LIN, [LIN], 0x10),
    ("CPlaceHolderOneShot::CreateObject(CCatagoryContainer*, ...)", 0x100259A0, K_CREATE4 + OS, [OS], None),
    ("CPlaceHolderOneShot::PlaySample", 0x10025A9C, K_PLAY, [OS], 0x1C),
    ("CPlaceHolderOneShot::GetClassID", 0x10025BF8, K_CLASSID + OS, [OS], 0x54),
    ("CPlaceHolderOneShotBranchingSentence::CreateObject(CCatagoryContainer*, ...)", 0x10025C38, K_CREATE4 + OSBS, [OSBS], None),
    ("CPlaceHolderOneShotSentence::~CPlaceHolderOneShotSentence", 0x10025D48, K_DTOR + OSS, [OSS], 0x10),
    ("CPlaceHolderOneShotSentence::Stop", 0x10025E28, K_STOP, [OSS, OSBS], 0x20),
    ("CPlaceHolderOneShotSentence::Update", 0x10025F18, K_UPDATE, [OSS, OSBS], 0x30),
    ("CPlaceHolderOneShotSentence::UpdateParameter", 0x10026028, K_UPDPARAM, [OSS, OSBS], 0x34),
    ("CPlaceHolderOneShotSentence::UpdateParameterDepandants", 0x100260C4, K_UPDDEP, [OSS, OSBS], 0x38),
    ("CPlaceHolderOneShotSentence::PlaySample", 0x10026168, K_PLAY, [OSS, OSBS], 0x1C),
    ("CPlaceHolderOneShotSentence::GetFirstChild", 0x10026228, K_FIRSTCHILD, [OSS, OSBS], None),
    ("CPlaceHolderOneShotSentence::ProcessChildPlaceHolders", 0x100262E8, K_PROCESS, [OSS, OSBS], None),
    ("CPlaceHolderOneShotSentence::AddNewElementTo", 0x100263FC, K_ADDELEMENT, [OSS, OSBS], None),
    ("CPlaceHolderOneShotSentence::GetElement", 0x10026514, K_GETELEMENT, [OSS], 0x58),
    ("CPlaceHolderOneShotSentence::RemoveCompleteChildPlaceHolders", 0x100265AC, K_REMOVE, [OSS, OSBS], None),
    ("CPlaceHolderOneShotSentence::CreateObject(CCatagoryContainer*, ...)", 0x1002669C, K_CREATE4 + OSS, [OSS], None),
    ("CPlaceHolderOneShotSentence::GetClassID", 0x100267A0, K_CLASSID + OSS, [OSS], 0x54),
    ("CPlaceHolderSentence::SoundCallback", 0x100268E0, K_CALLBACK, [SEN, LIN, BRA, SHU, LST], 0x08),
    ("CPlaceHolderSentence::FillBasicSampleInfo", 0x100269F8, K_BASIC, [SEN, LIN, BRA, SHU, LST], 0x4C),
    ("CPlaceHolderSentence::PlaySample", 0x10026AAC, K_PLAY, [SEN, LIN, BRA, SHU, LST], 0x1C),
    ("CPlaceHolderSentence::CreateObject(CCatagoryContainer*, ...)", 0x10026C24, K_CREATE4 + SEN, [SEN], None),
]
assert len(FUNCS) == 67


# ---------------------------------------------------------------- the PowerPC side

def rec_info(e, L, name, handle, info):
    """AP.rec_info, but a position or velocity that is not a labelled object (a dead local of a non-positional Element) is not read."""
    u = e.u32
    flags = u(info + 0x14)
    L.rec(name, [handle, u(info), L.lab(u(info + 4)), u(info + 0xC), u(info + 0x10), flags, u(info + 0x28),
                 u(info + 0x1C) if flags & 2 else 0, u(info + 0x24) if flags & 0x20 else 0, L.lab(u(info + 8)) if flags & 4 else 0,
                 u(info + 0x40) if flags & 0x40 else 0, L.lab(u(info + 0x48)) if flags & 0x2000 else 0])
    L.rec(name + ".more", [canon(u(info + 0x34)) if flags & 0x200 else 0, canon(u(info + 0x38)) if flags & 0x200 else 0,
                           canon(u(info + 0x3C)) if flags & 0x200 else 0, u(info + 0x4C) if flags & 0x1000 else 0,
                           L.lab(u(info + 0x80)) if flags & 0x40000 else 0, u(info + 0x84) if flags & 0x40000 else 0])
    pos, vel = u(info + 0x2C), u(info + 0x30)
    read_pos, read_vel = pos and L.lab(pos) > 0, vel and L.lab(vel) > 0
    L.rec(name + ".pos", [L.lab(pos)] + [canon(u(pos + o)) if read_pos else 0 for o in (0, 4, 8)] + [L.lab(vel)]
          + [canon(u(vel + o)) if read_vel else 0 for o in (0, 4, 8)])


def rec_words(e, L, name, info):
    """The sample callback, as the unit file's RecInfoCallback (flag 0x800 of the info's flags)."""
    L.rec(name + ".callback", (L.lab(e.u32(info + 0x44)) if e.u32(info + 0x14) & 0x800 else 0,))


def ppc_stub_setup_types(e):
    """The PowerPC side's fakes: w79g4's set, plus the manager calls, the event mapper and the recorded sizes of operator new."""
    PH.ppc_stub_setup(e)

    # operator new: blocks filled with 0xA5; the extra data (0x20), a placeholder (0x60), the shuffle (0x10) and the list data (0xc) recorded
    def op_new(e):
        L, size = e.cl, e.r(3)
        block = e.alloc(max(size, 16), b"\xa5" * max(size, 16))
        if L.track and size in (0x20, 0x60, 0x10, 0xC):
            L.rec("new", (size, L.newlab(block)))
        e.set_r(3, block)
    e.stub("__nw__FUl", op_new)

    def mgr_listener(e):
        L = e.cl
        L.rec("Mgr.GetListenerPosition", (L.lab(e.r(3)),))
        e.set_r(3, e.w.listener if L.ret("ListenerOn", 0) != 0 else 0)

    def mgr_vcon(e):
        L = e.cl
        L.rec("Mgr.VolumeControlIsOn", (L.lab(e.r(3)), e.r(4) & 0xFF))
        e.set_r(3, 1 if L.ret("VCOn", 1) != 0 else 0)

    def fixup(e):
        L = e.cl
        L.rec("FixUp", (L.lab(e.r(3)), e.r(4)))
        e.set_r(3, T.u32(L.ret("FixUp", 0)))

    def play_sample(e):      # (manager r3, info r4, control r5)
        L = e.cl
        L.rec("Mgr.PlaySample", (L.lab(e.r(3)), L.lab(e.r(5))))
        rec_info(e, L, "Mgr.PlaySample.info", 0, e.r(4))
        rec_words(e, L, "Mgr.PlaySample.info", e.r(4))
        e.write(e.r(5), pack([L.ret("PlayControl", 0x4321)]))
        e.set_r(3, T.u32(L.ret("PlaySample", 0)))

    def construct_sample(e):   # (manager r3, handle r4, control r5, info r6)
        L = e.cl
        L.rec("Mgr.ConstructSample", (L.lab(e.r(3)), e.r(4)))
        rec_info(e, L, "Mgr.ConstructSample.info", 0, e.r(6))
        rec_words(e, L, "Mgr.ConstructSample.info", e.r(6))
        e.set_r(3, T.u32(L.ret("ConstructSample", 0)))

    def map_event(e):        # (mapper r3, category data r4, event r5)
        L = e.cl
        event = e.r(5)
        L.rec("MapEvent", (L.lab(e.r(4)), e.u32(event + 4), e.u32(event + 8), AP.canon(e.u32(event + 0xC)), AP.canon(e.u32(event + 0x10)),
                           AP.canon(e.u32(event + 0x14))))
        answer = L.ret("Mapped", 0)
        e.set_r(3, e.w.amap.get(answer, 0) if 21 <= answer < 31 else 0)

    for address, fn in {0x10017CF4: mgr_listener, 0x10016E60: mgr_vcon, 0x10023378: fixup, 0x10027D94: play_sample, 0x10027C90: construct_sample,
                        0x10026D24: map_event}.items():
        T.stub_internal(e, address, fn)


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    lib = runner.lib
    lib.tpw_pt_run.argtypes = [C.c_int32, C.c_void_p, C.c_void_p]
    ppc_stub_setup_types(e)


def ppc_preset(e, L, w, obj, st, extra_label):
    """Writes a preset state into a placeholder the real constructor made. The extra data its constructor made is updated in place (its
    +0x18 and +0x1c words stay); else it is made here."""
    w_ = lambda off, words: e.write(obj + off, pack(words))
    amap = w.amap
    w_(4, [amap.get(st[S_EVENT], 0), amap.get(st[S_SOUND], 0), amap.get(st[S_SAMPLE], 0)])
    w_(0x10, [st[S_POSX], st[S_POSY], st[S_POSZ], st[S_VELX], st[S_VELY], st[S_VELZ], st[S_ENDTIME], st[S_PRIORITY], st[S_HANDLE]])
    w_(0x34, [w.container if st[S_CATAGORY] else 0, st[S_LISTHANDLE], st[S_FLAGS]])
    w_(0x48, [st[S_PAUSETIME], st[S_MAXDIST]])
    e.write(obj + 0x50, bytes([st[S_LASTSOUND] & 0xFF, st[S_LASTSAMPLE] & 0xFF, st[S_VCINDEX] & 0xFF]))
    w_(0x54, [st[S_FADE]])
    e.write(obj + 0x58, struct.pack(">HH", st[S_VOLUME] & 0xFFFF, st[S_PITCH] & 0xFFFF))
    if st[S_EXTRA]:
        extra = e.u32(obj + 0x5C)
        if not extra:
            extra = e.alloc(0x20, b"\xa5" * 0x20)
            T.call_ppc(e, 0x100180CC, [extra])
            e.write(obj + 0x5C, pack([extra]))
        e.write(extra, struct.pack("<I", st[S_IDS] & MASK))
        e.write(extra + 4, struct.pack("<I", st[S_VALUES] & MASK))
        e.write(extra + 8, pack([st[S_LEFT], st[S_TOP], st[S_RIGHT], st[S_BOTTOM]]))
        if extra_label:
            L.labels[extra] = extra_label


def ppc_sounds(e, amap, block):
    """The sound records of event 21 (up to 12) as ten words each, as the unit file's DumpSounds."""
    out = [0] * 120
    event = amap.get(21)
    if not event:
        return tuple(out)
    u = e.u32
    count = min(u(event + 4), 12)
    base = u(event + 8)
    for i in range(count):
        s = base + 0x2A * i
        f10, f12 = struct.unpack(">HH", e.read(s + 0x10, 4))
        f16, f18 = struct.unpack(">HH", e.read(s + 0x16, 4))
        f1a, f1c = struct.unpack(">HH", e.read(s + 0x1A, 4))
        words = [u(s), u(s + 4), (u(s + 8) - block) & MASK if u(s + 8) else 0, int.from_bytes(e.read(s + 0xC, 4), "little"), f10 | (f12 << 16),
                 f16 | (f18 << 16), f1a | (f1c << 16), u(s + 0x1E), canon(u(s + 0x22)), (u(s + 0x26) - block) & MASK if u(s + 0x26) else 0]
        out[i * 10:i * 10 + 10] = words
    return tuple(out)


def vcall_class(e, obj):
    """GetClassID through the object's vtable (+0x54)."""
    return T.call_ppc(e, e.u32(e.u32(e.u32(obj) + 0x54)), [obj])[0] & MASK


def ppc_chain(e, L, subject):
    """The first three children of the subject's chain (label and 34 state words each) and the chain length (up to 8)."""
    flat = [0] * (35 * 3)
    extra = e.u32(subject + 0x5C)
    child = e.u32(extra + 0x18) if extra else 0
    n = 0
    while child and n < 8:
        if n < 3:
            flat[n * 35] = lbl(L, child) | (vcall_class(e, child) << 16)
            flat[n * 35 + 1:n * 35 + 35] = dump_holder(e, L, child)
        extra = e.u32(child + 0x5C)
        child = e.u32(extra + 0x18) if extra else 0
        n += 1
    return tuple(flat), n


def ppc_run(e, c):
    op, kind = c["op"], c["kind"]
    L = e.cl = CL.PpcLog({}, c["q"], first_new=FIRST_NEW)
    L.track = False
    w = e.w = World()
    call = T.call_ppc
    st = c["state"]
    a = [v & MASK for v in c["a"]]
    im = c["image"]
    block, amap = build_image(e, L, im)
    w.amap = amap
    cat = amap.get(16, 0)
    w.container = e.alloc(0x20, pack([e.vt_container, 1, 0 if a[6] else cat, FILL, FILL, FILL, FILL, FILL]))
    L.labels[w.container] = 4
    w.manager = e.alloc(0x70, b"\xa5" * 0x70)
    e.write(w.manager, pack([e.vt_manager]))
    L.labels[w.manager] = 2
    w.vcontrol = e.alloc(8, b"\0" * 8)
    L.labels[w.vcontrol] = 3
    w.listener = e.alloc(12, pack(a[8:11]))
    w.banks = [e.alloc(16, b"\xa5" * 16) for _ in range(3)]
    for i, addr in enumerate(w.banks):
        L.labels[addr] = 11 + i
    w.streamer = e.alloc(0x40, b"\xa5" * 0x40)
    L.labels[w.streamer] = 7
    e.write(S_MGR, pack([w.manager if c["mgr"] else 0]))
    e.write(S_SEED, pack([c["seed"]]))

    # the category table of the streamer (for CListSentence)
    ncat = len(c["cats"])
    table = e.alloc(4 * max(ncat, 1), b"\0" * (4 * max(ncat, 1)))
    for i, (present, data) in enumerate(c["cats"]):
        if present:
            cont = e.alloc(0x20, pack([e.vt_container, 1, cat if data else 0, FILL, FILL, FILL, FILL, FILL]))
            L.labels[cont] = 40 + i
            e.write(table + 4 * i, pack([cont]))
    e.write(w.streamer + 0x1C, pack([table if ncat else 0, ncat]))

    # the event list: vptr, category, event, position, the float, the items' address and count
    count, items = c["list"]
    item_words = []
    for cat_, event_ in items:
        item_words += [cat_, event_]
    item_words += [0] * (16 - len(item_words))
    items_addr = e.alloc(64, pack(item_words))
    evt = c["evt"]
    elist = e.alloc(0x30, pack([FILL, evt[0], evt[1], evt[2], evt[3], evt[4], evt[5], items_addr, count]))
    L.labels[elist] = 14

    standalone = K_LDCTOR <= op <= K_SHNEXT
    create_op = K_CREATE1 <= op < K_GETELEMENT
    ctor_op = K_CTOR <= op < K_DTOR
    subject = slot = shuffle = data = 0
    children = []
    if create_op:
        slot = e.alloc(0x80, b"\xa5" * 0x80)
        e.write(slot + 0x38, pack([st[S_LISTHANDLE]]))
        e.write(slot + 0x40, pack([st[S_NEXT], st[S_PREVIOUS]]))
        if a[7]:
            slot = 0
    elif not standalone and not ctor_op:
        subject = e.alloc(0x80, b"\xa5" * 0x80)
        call(e, CTOR[kind], [subject, elist] if kind == LST else [subject])
        ppc_preset(e, L, w, subject, st, 17)
        if kind == LST:
            made = e.u32(e.u32(subject + 0x5C) + 0x1C)
            L.labels[made] = 10
            L.labels[e.u32(made + 8)] = 9
        L.labels[subject] = 1
        L.labels[subject + 0x30] = 8
        L.labels[subject + 0x10] = 18
        L.labels[subject + 0x1C] = 19
        previous = subject
        for i in range(min(c["nchild"], 4)):
            child = e.alloc(0x80, b"\xa5" * 0x80)
            call(e, BRANCHING_ELEMENT_CTOR if c["bkind"] else ELEMENT_CTOR, [child, subject])
            ppc_preset(e, L, w, child, c["children"][i], 55 + i)
            L.labels[child] = 50 + i
            L.labels[child + 0x30] = 480 + i
            L.labels[child + 0x10] = 490 + 2 * i
            L.labels[child + 0x1C] = 491 + 2 * i
            e.write(e.u32(previous + 0x5C) + 0x18, pack([child]))
            previous = child
            children.append(child)
        shuf = c["shuf"]
        if kind == SHU and shuf[0]:
            shuffle = e.alloc(0x20, b"\xa5" * 0x20)
            e.write(shuffle, pack([amap.get(shuf[1], 0), shuf[2], shuf[3]]))
            e.write(shuffle + 0xC, bytes([shuf[5] & 0xFF, shuf[6] & 0xFF, shuf[4] & 0xFF]))
            e.write(e.u32(subject + 0x5C) + 0x1C, pack([shuffle]))
            L.labels[shuffle] = 10
        if kind == LST and shuf[0]:
            d = e.u32(e.u32(subject + 0x5C) + 0x1C)
            e.write(d + 4, pack([shuf[3]]))
    if standalone:
        if op in (K_SHINIT, K_SHSHUFFLE, K_SHNEXT):
            shuf = c["shuf"]
            shuffle = e.alloc(0x20, b"\xa5" * 0x20)
            e.write(shuffle, pack([amap.get(shuf[1], 0), shuf[2], shuf[3]]))
            e.write(shuffle + 0xC, bytes([shuf[5] & 0xFF, shuf[6] & 0xFF, shuf[4] & 0xFF]))
            L.labels[shuffle] = 10
        if op == K_LDDTOR:
            data = e.alloc(0x10, b"\xa5" * 0x10)
            call(e, 0x1001EA2C, [data, elist])
            L.labels[data] = 10
            L.labels[e.u32(data + 8)] = 9
    info = PH.sample_info(e, L, c)
    buf = e.alloc(16, b"\xa5" * 16)

    def vcall(obj, slot_, args):
        vptr = e.u32(obj)
        return call(e, e.u32(e.u32(vptr + slot_)), args)

    # ---- the call
    del L.log[:]
    L.track = True
    result = 0
    dump_subject = bool(subject)
    returned = 0
    ev_addr = amap.get(a[0], 0)
    if op < K_DTOR:                       # constructors
        subject = e.alloc(0x80, b"\xa5" * 0x80)
        call(e, CTOR[kind], [subject, elist] if kind == LST else [subject])
        L.labels[subject] = 1
        vptr = e.u32(subject)
        classid = vcall(subject, 0x54, [subject])[0]
        result = classid | (0x100 if vptr == VT[kind] else 0) | (0x200 if e.read(subject + 0x60, 0x20) == b"\xa5" * 0x20 else 0)
        dump_subject = True
    elif op < K_CLASSID:                  # destructors, through the vtable
        vcall(subject, 0x10, [subject, a[0]])
        dump_subject = False
    elif op < K_CREATE1:
        result = vcall(subject, 0x54, [subject])[0]
    elif op < K_CREATE4:
        call(e, CREATE1[kind], [slot, elist] if kind == LST else [slot])
    elif op < K_GETELEMENT:
        call(e, CREATE4[kind], [w.container, slot, ev_addr, elist])
    elif op == K_GETELEMENT:
        returned = vcall(subject, 0x58, [subject])[0]
        result = lbl(L, returned)
    elif op == K_LDCTOR:
        data = e.alloc(0x10, b"\xa5" * 0x10)
        call(e, 0x1001EA2C, [data, elist])
        L.labels[data] = 10
        L.labels[e.u32(data + 8)] = 9
    elif op == K_LDDTOR:
        call(e, 0x1001F008, [data, a[0]])
    elif op == K_SHCTOR:
        shuffle = e.alloc(0x20, b"\xa5" * 0x20)
        call(e, 0x10024910, [shuffle, ev_addr])
        L.labels[shuffle] = 10
    elif op == K_SHINIT:
        call(e, 0x100249D4, [shuffle, ev_addr])
    elif op == K_SHSHUFFLE:
        call(e, 0x10024A50, [shuffle])
    elif op == K_SHNEXT:
        result = lbl(L, call(e, 0x10024C5C, [shuffle, a[0]])[0])
    elif op == K_SETLISTINFO:
        call(e, 0x10024FE0, [subject, ev_addr])
    elif op == K_CHOOSE:
        result = vcall(subject, 0x3C, [subject, 1 if a[0] else 0])[0] & 0xFF
    elif op == K_ASSIGN:
        result = call(e, 0x100251A4, [subject, a[0]])[0] & 0xFF
    elif op == K_UPDPARAM:
        vcall(subject, 0x34, [subject, a[0], a[1]])
    elif op == K_PLAY:
        vcall(subject, 0x1C, [buf, subject, a[0]])
        result = e.u32(buf)
    elif op == K_STOP:
        vcall(subject, 0x20, [buf, subject])
        result = e.u32(buf)
    elif op == K_UPDATE:
        pos = e.alloc(12, pack(a[0:3]))
        vel = e.alloc(12, pack(a[3:6]))
        vcall(subject, 0x30, [buf, subject, pos, vel])
        result = e.u32(buf)
    elif op == K_UPDDEP:
        vcall(subject, 0x38, [subject])
    elif op == K_FIRSTCHILD:
        returned = call(e, 0x10026228, [subject, a[0]])[0]
        result = lbl(L, returned)
    elif op == K_PROCESS:
        call(e, 0x100262E8, [subject, a[0]])
    elif op == K_ADDELEMENT:
        parent = subject if a[1] == 0 else (children[a[1] - 1] if a[1] <= len(children) else 0)
        result = call(e, 0x100263FC, [subject, parent, a[0]])[0] & 0xFF
    elif op == K_REMOVE:
        call(e, 0x100265AC, [subject, a[0]])
    elif op == K_CALLBACK:
        vcall(subject, 0x08, [subject, a[0], a[1]])
    elif op == K_BASIC:
        result = vcall(subject, 0x4C, [subject, info])[0] & 0xFF
    L.track = False

    state = (0,) * 34
    chain, length = (0,) * 105, 0
    aux = [0] * 24
    slotwords = [0] * 42
    if dump_subject and subject:
        state = dump_holder(e, L, subject)
        chain, length = ppc_chain(e, L, subject)
        extra = e.u32(subject + 0x5C)
        if kind == LST and extra and e.u32(extra + 0x1C) and op != K_SETLISTINFO:
            d = e.u32(extra + 0x1C)
            aux[0], aux[1], aux[2] = lbl(L, d), e.u32(d), e.u32(d + 4)
            array = e.u32(d + 8)
            for i in range(min(e.u32(d), 8)):
                aux[3 + 2 * i], aux[4 + 2 * i] = e.u32(array + 8 * i), e.u32(array + 8 * i + 4)
        if kind == SHU and extra and e.u32(extra + 0x1C):
            shuffle = e.u32(extra + 0x1C)
    chain = list(chain) + [0] * 35
    if returned:
        chain[3 * 35:4 * 35] = [lbl(L, returned) | (vcall_class(e, returned) << 16)] + list(dump_holder(e, L, returned))
    if shuffle:
        aux[12:19] = [lbl(L, shuffle), lbl(L, e.u32(shuffle)), e.u32(shuffle + 4), e.u32(shuffle + 8), e.read(shuffle + 0xC, 1)[0],
                      e.read(shuffle + 0xD, 1)[0], e.read(shuffle + 0xE, 1)[0]]
    if data and op == K_LDCTOR:
        aux[0], aux[1], aux[2] = lbl(L, data), e.u32(data), e.u32(data + 4)
        array = e.u32(data + 8)
        for i in range(min(e.u32(data), 8)):
            aux[3 + 2 * i], aux[4 + 2 * i] = e.u32(array + 8 * i), e.u32(array + 8 * i + 4)
    if create_op and slot:
        slotwords[0] = lbl(L, slot)
        slotwords[1:4] = [e.u32(slot + 0x38), e.u32(slot + 0x40), e.u32(slot + 0x44)]
        if a[5]:
            vptr = e.u32(slot)
            result = (0x100 if vptr == VT[kind] else 0) | (0x200 if e.read(slot + 0x60, 0x20) == b"\xa5" * 0x20 else 0)
            slotwords[4] = vcall(slot, 0x54, [slot])[0]
            slotwords[8:42] = dump_holder(e, L, slot)
    r = (result & MASK, 0, length, 0, 0)
    return dict(r=r, st=state, chain=tuple(chain[:140]), cat=e.u32(w.container + 4), info=dump_info(e, L, info),
                statics=(lbl(L, e.u32(S_MGR)), e.u32(S_SEED)), aux=tuple(aux), slot=tuple(slotwords), sounds=ppc_sounds(e, amap, block),
                log=list(L.log))


# ---------------------------------------------------------------- the host side

def params(c):
    p = [0] * 4096
    p[P_OP], p[P_MGR], p[P_SEED], p[P_KIND], p[P_NCHILD], p[P_NCAT], p[P_BKIND] = c["op"], c["mgr"], c["seed"], c["kind"], c["nchild"], len(c["cats"]), c["bkind"]
    p[P_A:P_A + 16] = c["a"]
    p[P_STATE:P_STATE + NSTATE] = c["state"]
    for i, ch in enumerate(c["children"][:4]):
        p[P_CHILD + 34 * i:P_CHILD + 34 * i + 34] = ch
    p[P_INFO:P_INFO + 3] = c["info"]
    count, items = c["list"]
    p[P_LIST] = count
    for i, (cat_, event_) in enumerate(items[:8]):
        p[P_LIST + 1 + 2 * i], p[P_LIST + 2 + 2 * i] = cat_, event_
    for i, (present, data) in enumerate(c["cats"][:4]):
        p[P_CAT + 2 * i], p[P_CAT + 2 * i + 1] = present, data
    p[P_EVT:P_EVT + 6] = c["evt"]
    p[P_SHUF:P_SHUF + 7] = c["shuf"]
    words = c["image"].words()
    p[P_IMG:P_IMG + len(words)] = words
    return [v & MASK for v in p]


def host_run(lib, c):
    p = (C.c_uint32 * 4096)(*params(c))
    out = (C.c_uint32 * 1024)()
    CL.host_begin(lib, {}, c["q"], first_new=FIRST_NEW)
    lib.tpw_pt_run(c["op"], p, out)
    log = CL.host_end(lib)
    r = (out[O_R], 0, out[O_R + 2], 0, 0)
    return dict(r=r, st=tuple(out[O_STATE:O_STATE + NSTATE]), chain=tuple(out[O_CHILD:O_CHILD + 140]), cat=out[O_CAT],
                info=tuple(out[O_INFO:O_INFO + 24]), statics=(out[O_STATIC], out[O_STATIC + 1]), aux=tuple(out[O_AUX:O_AUX + 24]),
                slot=tuple(out[O_SLOT:O_SLOT + 42]), sounds=tuple(out[O_SOUNDS:O_SOUNDS + 120]), log=log)


# ---------------------------------------------------------------- the cases

def dstate():
    st = default_state()
    st[S_EXTRA] = 1
    return st


def sound_count(events):
    return sum(len(ev["sounds"]) for ev in events)


def make_case(op, kind, **kw):
    events = kw.pop("events", None) or [make_event()]
    c = dict(op=op, kind=kind, mgr=1, seed=0x0BADC0DE, state=default_state(), children=[], nchild=0, bkind=0, cats=[], a=[0] * 16,
             info=[0, 0, 0], list=(0, []), evt=[0, 0, ONE, 0x40000000, 0x40400000, 0], shuf=[0] * 7, events=events, vc=0, q={})
    c["state"][S_EXTRA] = 1
    c.update(kw)
    c["image"] = tree_image(c["events"], c["vc"])
    c["q"] = {k: v if isinstance(v, list) else [v] for k, v in c["q"].items()}
    c["nchild"] = len(c["children"])
    return c


def sstate(r, events, **kw):
    """A random state with extra data, no child and no parent, and an event."""
    st = rand_state(r, events, extra=True)
    st[S_EXTRA] = 1
    st[S_CHILD] = 0
    st[S_PARENT] = 0
    return st


def queues_types(r, events):
    q = queues_common(r)
    q["PlaySample"] = [r.choice([0, 1, 0x1234, 0xFFFFFFFF]) for _ in range(3)]
    q["PlayControl"] = [r.choice([0x4321, 0, 7]) for _ in range(2)]
    q["ConstructSample"] = [r.choice([0, 0x55, 0xFFFF0000]) for _ in range(2)]
    q["Mapped"] = [r.choice([0, 21, 21 + len(events) - 1, 21 + r.randrange(len(events))]) for _ in range(3)]
    return q


def rand_tree(r, kind, bkind=0):
    events = rand_events(r, max_events=2)
    if kind in (BRA, OSBS) or bkind:
        add_branches(r, events, 0.6)
    return events


def rand_list(r, ncat=None, max_items=8):
    n = r.choice([0, 1, 2, 3, 5, 8, r.randrange(0, max_items + 1)])
    top = max(ncat if ncat else 1, 1)
    return n, [(r.randrange(top) if ncat else r.choice([0, 1, 2, r.getrandbits(32)]), r.choice([0, 1, 5, r.getrandbits(32)])) for _ in range(n)]


def rand_cats(r):
    return [(1 if r.random() < 0.85 else 0, 1 if r.random() < 0.85 else 0) for _ in range(r.randrange(1, 5))]


def rand_evt(r):
    return [r.choice([0, 1, 7, rword(r)]), r.choice([0, 1, 9, rword(r)])] + [PH.rsane(r) if hasattr(PH, "rsane") else AP.rsane(r) for _ in range(3)] \
        + [r.choice([0, ONE, 0x40000000, fbits(r.uniform(0.0, 50.0))])]


def world_case(r, op, kind, nchild=None, bkind=None, **kw):
    """A random world around the subject: a tree, the subject's state, children (for the one-shot sentences), queues, a list and categories."""
    if bkind is None:
        bkind = 1 if kind == OSBS else 0
    events = rand_tree(r, kind, bkind)
    st = sstate(r, events)
    if kind in (OSS, OSBS) and not bkind:
        st[S_IDS] &= ~0xFF
    if nchild is None:
        nchild = r.choice([0, 1, 1, 2, 3]) if kind in (OSS, OSBS) else 0
    children = []
    for _ in range(nchild):
        cs = sstate(r, events)
        if r.random() < 0.7:
            cs[S_ENDTIME] = r.choice([0, 100, 5000, 100000, r.getrandbits(32), 0xFFFFFFFF])
        children.append(cs)
    cats = rand_cats(r) if kind == LST else []
    c = make_case(op, kind, events=events, state=st, children=children, bkind=bkind, cats=cats, seed=r.choice([0, r.getrandbits(32)]),
                  vc=r.randrange(0, 5), q=queues_types(r, events), a=[rword(r) for _ in range(6)] + [0] * 10,
                  mgr=1 if r.random() < 0.92 else 0, list=rand_list(r, len(cats) if cats else None), evt=rand_evt(r))
    c["a"][8:11] = [AP.rsane(r) for _ in range(3)]
    c["state"][S_FLAGS] = r.choice([c["state"][S_FLAGS], c["state"][S_FLAGS] & ~(F_PLAYING | F_FINISHED), c["state"][S_FLAGS] & ~(F_FINISHED | F_S40)])
    c.update(kw)
    c["image"] = tree_image(c["events"], c["vc"])
    c["q"] = {k: v if isinstance(v, list) else [v] for k, v in c["q"].items()}
    c["nchild"] = len(c["children"])
    return c


def ensure_sound(c):
    st = c["state"]
    if not st[S_SOUND]:
        st[S_SOUND] = 31
    if not st[S_EVENT]:
        st[S_EVENT] = 21
    return c


# ---- generators per operation

def kind_pick(kinds):
    return lambda r: r.choice(kinds)


def gen_ctor(kind):
    def make(r):
        c = world_case(r, K_CTOR + kind, kind, nchild=0)
        c["list"] = rand_list(r, None)
        return c
    return make


def edge_ctor(kind):
    def make():
        cases = []
        for mgr in (0, 1):
            for n in (0, 1, 3, 8):
                c = make_case(K_CTOR + kind, kind, mgr=mgr, list=(n, [(i, i * 3) for i in range(n)]))
                cases.append(c)
        return cases
    return make


def gen_dtor(kind):
    def make(r):
        c = world_case(r, K_DTOR + kind, kind)
        c["a"][0] = r.choice([0, 1, 2, 0xFFFFFFFF, 0x7FFF, 0x8000, 0xFFFF, 0x10000, 0x10001, 0xFFFF8000, 0x12345678])
        if kind == SHU and r.random() < 0.6:
            c["shuf"] = [1, 21, r.getrandbits(32), r.randrange(0, 4), r.randrange(2), 1, 2]
        if kind == BRA or kind == LIN or kind == SEN or kind == OS:
            if r.random() < 0.3:
                c["state"][S_EXTRA] = 0
        return c
    return make


def edge_dtor(kind):
    def make():
        cases = []
        for flag in (0, 1, 2, 0xFFFFFFFF, 0x7FFF, 0x8000, 0xFFFF, 0x10000, 0x10001):
            for cat in (0, 1):
                for nchild in ((0, 1, 3) if kind in (OSS, OSBS) else (0,)):
                    st = default_state()
                    st[S_EXTRA] = 1
                    st[S_CATAGORY] = cat
                    st[S_LISTHANDLE] = 0x12345678
                    kids = [default_state() for _ in range(nchild)]
                    for k in kids:
                        k[S_EXTRA] = 1
                        k[S_CATAGORY] = cat
                    c = make_case(K_DTOR + kind, kind, state=st, children=kids, a=[flag] + [0] * 15, bkind=1 if kind == OSBS else 0)
                    if kind == SHU:
                        c["shuf"] = [1, 21, 5, 1, 0, 1, 2]
                    cases.append(c)
        return cases
    return make


def gen_classid(kinds):
    def make(r):
        return world_case(r, K_CLASSID + r.choice(kinds), r.choice(kinds))
    return make


def make_classid_case(kind):
    return make_case(K_CLASSID + kind, kind)


def gen_classid_kinds(kinds):
    def make(r):
        kind = r.choice(kinds)
        c = world_case(r, K_CLASSID + kind, kind)
        return c
    return make


def edge_classid(kinds):
    return lambda: [make_case(K_CLASSID + k, k, mgr=m) for k in kinds for m in (0, 1)]


def gen_create1(kind):
    def make(r):
        c = world_case(r, K_CREATE1 + kind, kind, nchild=0)
        c["a"][5] = 1
        c["a"][7] = 1 if r.random() < 0.1 else 0
        c["state"][S_LISTHANDLE] = r.choice([0, 0x10003, r.getrandbits(32)])
        c["state"][S_NEXT] = r.choice([0, r.getrandbits(32)])
        c["state"][S_PREVIOUS] = r.choice([0, r.getrandbits(32)])
        return c
    return make


def edge_create1(kind):
    def make():
        cases = []
        for n in ((0, 1, 4, 8) if kind == LST else (0,)):
            for null in (0, 1):
                st = default_state()
                st[S_LISTHANDLE], st[S_NEXT], st[S_PREVIOUS] = 0x00050007, 0x11110000, 0x22220000
                cases.append(make_case(K_CREATE1 + kind, kind, state=st, a=[0, 0, 0, 0, 0, 1, 0, null] + [0] * 8,
                                       list=(n, [(i, 100 + i) for i in range(n)])))
        return cases
    return make


def gen_create4(kind):
    def make(r):
        c = world_case(r, K_CREATE4 + kind, kind, nchild=0)
        events = c["events"]
        c["a"][0] = 21 + r.randrange(len(events))
        c["a"][5] = 1
        c["a"][6] = 0
        c["a"][7] = 1 if r.random() < 0.08 else 0
        if kind == BRA:
            for ev in events:
                ev["f12"] = r.choice([1, 2, 5, 0x100])
            c["events"] = events
        if kind == LST:
            # valid category numbers, mapped events from the tree
            n = len(c["cats"])
            c["list"] = rand_list(r, n)
        c["state"][S_LISTHANDLE] = r.choice([0, 0x10003, r.getrandbits(32)])
        c["state"][S_NEXT] = r.choice([0, r.getrandbits(32)])
        c["state"][S_PREVIOUS] = r.choice([0, r.getrandbits(32)])
        c["image"] = tree_image(c["events"], c["vc"])
        return c
    return make


def edge_create4(kind):
    def make():
        cases = []
        r = random.Random(4000 + kind)
        for n in range(6):
            c = gen_create4(kind)(r)
            c["a"][7] = 1 if n == 5 else 0
            cases.append(c)
        return cases
    return make


def gen_getelement(kinds):
    def make(r):
        kind = r.choice(kinds)
        c = world_case(r, K_GETELEMENT, kind, nchild=r.choice([0, 1]))
        return c
    return make


def gen_ldctor(r):
    c = world_case(r, K_LDCTOR, LST, nchild=0)
    c["list"] = rand_list(r, None)
    return c


def edge_ldctor():
    cases = []
    for n in (0, 1, 2, 7, 8):
        cases.append(make_case(K_LDCTOR, LST, list=(n, [(i + 1, 0x100 + 7 * i) for i in range(n)])))
    return cases


def gen_lddtor(r):
    c = world_case(r, K_LDDTOR, LST, nchild=0)
    c["list"] = rand_list(r, None)
    c["a"][0] = r.choice([0, 1, 2, 0xFFFFFFFF, 0x7FFF, 0x8000, 0xFFFF, 0x10000, 0x12345678])
    return c


def edge_lddtor():
    cases = []
    for flag in (0, 1, 0xFFFFFFFF, 0x7FFF, 0x8000, 0xFFFF, 0x10000):
        for n in (0, 1, 5):
            cases.append(make_case(K_LDDTOR, LST, a=[flag] + [0] * 15, list=(n, [(i, i) for i in range(n)])))
    return cases


def shuffle_events(r, n, uniq=True):
    sounds = []
    for i in range(n):
        s = make_sound([make_sample(f08=100 + i)], f1e=1000 + 17 * i, f10=i, f12=100 + i, f16=i * 3, f18=i * 5, f1a=i * 7, f1c=i * 11,
                       vmin=i, vmax=200 - i, pmin=i, pmax=i + 1, hi=i)
        if i % 2 == 0:
            s["branches"] = [(i, i, i + 1, i)]   # a branch pointer in the record (its top bytes move with the record)
        sounds.append(s)
    return [make_event(sounds, id=7, f12=r.choice([0, 3]) if r is not None else 3)]


def gen_shuf(op):
    def make(r):
        n = r.choice([0, 1, 2, 3, 4, 5, 6, 8, 9, r.randrange(1, 10)])
        events = shuffle_events(r, max(n, 1))
        if n == 0:
            events[0]["sounds"] = []
        events.append(make_event([make_sound([make_sample()], f1e=300 + i) for i in range(r.randrange(1, 4))], id=9))
        picks = [0, 1, n - 1, n, n + 1, n - 2, 2, 3, 0xFFFFFFFF, r.randrange(0, 12), r.getrandbits(32)]
        q = queues_types(r, events)
        q["RangedRandom"] = [r.choice(picks) & MASK for _ in range(4)]
        q["Clock"] = [r.getrandbits(32), r.getrandbits(32)]
        c = make_case(op, SHU, events=events, q=q, a=[r.choice([21, 22]), r.choice([0, 1, 2]), 0, 0, 0, 0, 0, 0] + [0] * 8,
                      shuf=[1, r.choice([21, 21, 22]), r.getrandbits(32), r.choice([0, 1, 2, n, n + 1, r.randrange(0, 12)]), r.randrange(2), 1, 2])
        if op == K_SHNEXT:
            c["a"][0] = r.choice([0, 1, 2, 0xFFFFFFFF])
        if op in (K_SHINIT, K_SHSHUFFLE, K_SHNEXT) and n == 0:
            c["a"][0] = 21
            c["shuf"][1] = 21
        return c
    return make


def edge_shuf(op):
    def make():
        cases = []
        for n in (1, 2, 3, 4, 5, 6, 9):
            events = shuffle_events(None, n)
            for pick in sorted(set([0, 1, 2, n - 2, n - 1, n, n + 1, 0xFFFFFFFF])):
                for toggle in (0, 1):
                    q = {"RangedRandom": [pick & MASK], "Clock": [0x1234, 0x5678]}
                    c = make_case(op, SHU, events=events, q=q, a=[21, 1] + [0] * 14, shuf=[1, 21, 0xCAFE, 0, toggle, 1, 2])
                    cases.append(c)
        for idx in (0, 2, 3, 4, 5):
            for flag in (0, 1):
                events = shuffle_events(None, 4)
                cases.append(make_case(op, SHU, events=events, q={"RangedRandom": [2]}, a=[flag if op == K_SHNEXT else 21, 1] + [0] * 14, shuf=[1, 21, 99, idx, 1, 1, 2]))
        # strides other than 1 and 2 (the constructor sets 1 and 2; a preset shows the bytes are read)
        events = shuffle_events(None, 9)
        for steps in ((1, 2), (3, 1), (2, 2)):
            cases.append(make_case(op, SHU, events=events, q={"RangedRandom": [4]}, a=[21] + [0] * 15, shuf=[1, 21, 7, 0, 0, steps[0], steps[1]]))
        return cases
    return make


def gen_setlistinfo(r):
    n = r.choice([0, 1, 2, 4, 6])
    events = shuffle_events(r, max(n, 1))
    events.append(make_event([make_sound([make_sample()], f1e=77 + i) for i in range(r.randrange(1, 4))], id=9))
    q = queues_types(r, events)
    q["RangedRandom"] = [r.randrange(0, 8) for _ in range(3)]
    q["Clock"] = [r.getrandbits(32)]
    st = sstate(r, events)
    shuf = [0] * 7
    if r.random() < 0.7:
        shuf = [1, r.choice([21, 22]), r.getrandbits(32), r.randrange(0, 4), r.randrange(2), 1, 2]
    return make_case(K_SETLISTINFO, SHU, events=events, q=q, state=st, a=[r.choice([21, 22])] + [0] * 15, shuf=shuf)


def edge_setlistinfo():
    cases = []
    events = shuffle_events(None, 4) + [make_event([make_sound(), make_sound()], id=9)]
    for present in (0, 1):
        for stored in (21, 22, 0):
            for arg in (21, 22):
                cases.append(make_case(K_SETLISTINFO, SHU, events=events, q={"RangedRandom": [1], "Clock": [5]}, a=[arg] + [0] * 15,
                                       shuf=[present, stored, 3, 2, 1, 1, 2]))
    return cases


def gen_choose(kinds):
    def make(r):
        kind = r.choice(kinds)
        c = world_case(r, K_CHOOSE, kind, nchild=0)
        st, events = c["state"], c["events"]
        flat = [sd for ev in events for sd in ev["sounds"]]
        c["a"][0] = r.randrange(2)
        if kind == LIN:
            if r.random() < 0.15:
                st[S_SOUND] = 0
            if r.random() < 0.1:
                st[S_EVENT] = 0
            if r.random() < 0.5:
                # the last sound or one before it
                e_i = st[S_EVENT] - 21 if st[S_EVENT] else 0
                base = sum(len(ev["sounds"]) for ev in events[:e_i])
                st[S_SOUND] = 31 + base + r.randrange(len(events[e_i]["sounds"])) if events else 0
            if r.random() < 0.25:
                st[S_SOUND] = 31 + r.randrange(max(sound_count(events), 1))   # a sound of another event: a negative distance
            if r.random() < 0.5:
                st[S_FLAGS] ^= F_LOOPFOREVER
        elif kind == BRA:
            ensure_sound(c)
            if r.random() < 0.3:
                st[S_SOUND] = 0
            lo_hi = [(lo, hi) for sd in flat for _, lo, hi, _ in sd.get("branches", [])]
            param = r.choice([lo for lo, _ in lo_hi] + [hi for _, hi in lo_hi] + [lo - 1 for lo, _ in lo_hi] + [hi + 1 for _, hi in lo_hi] + [0, 0x7F, 0xFF]) if lo_hi \
                else r.randrange(256)
            vals = bytearray(struct.pack("<I", st[S_VALUES]))
            vals[0] = param & 0xFF
            st[S_VALUES] = struct.unpack("<I", bytes(vals))[0]
            c["q"]["RangedRandom"] = [r.choice([0, 1, 5, 50, 100, 0xFFFFFFFF, r.randrange(0, 300), r.getrandbits(32)]) for _ in range(3)]
        elif kind == SHU:
            st[S_EVENT] = 21
            c["events"] = shuffle_events(r, r.randrange(1, 9))
            c["q"]["RangedRandom"] = [r.choice([0, 1, 2, 3, 7, 8, 0xFFFFFFFF]) for _ in range(3)]
            c["q"]["Clock"] = [r.getrandbits(32)]
            if r.random() < 0.5:
                c["shuf"] = [1, 21, r.getrandbits(32), r.randrange(0, 11), r.randrange(2), 1, 2]
            c["image"] = tree_image(c["events"], c["vc"])
        elif kind == LST:
            n = len(c["cats"])
            c["list"] = rand_list(r, n)
            if r.random() < 0.7:
                c["shuf"] = [1, 0, 0, r.randrange(0, c["list"][0] + 2), 0, 0, 0]
            if r.random() < 0.2:
                c["a"][6] = 1
        return c
    return make


def edge_choose(kinds):
    def make():
        cases = []
        r = random.Random(500 + sum(kinds))
        if kinds == [LIN]:
            # every (event, sound) pair of two events, loop flag on and off: the distance can be negative or past the end
            events = [make_event([make_sound(samples=[]) for _ in range(3)]), make_event([make_sound(samples=[]) for _ in range(2)])]
            for event in (21, 22, 0):
                for sound in range(0, 6):
                    for flags in (0, F_LOOPFOREVER):
                        st = default_state()
                        st[S_EXTRA] = 1
                        st[S_EVENT] = event
                        st[S_SOUND] = 31 + sound if sound < 5 else 0
                        st[S_FLAGS] = flags
                        cases.append(make_case(K_CHOOSE, LIN, events=events, state=st, a=[1] + [0] * 15))
        for kind in kinds:
            for i in range(14):
                c = gen_choose([kind])(r)
                cases.append(c)
        return cases
    return make


def gen_assign(r):
    c = world_case(r, K_ASSIGN, BRA, nchild=0)
    events = c["events"]
    ensure_sound(c)
    st = c["state"]
    flat = [sd for ev in events for sd in ev["sounds"]]
    if r.random() < 0.3:
        st[S_SOUND] = 0
    lo_hi = [(lo, hi) for sd in flat for _, lo, hi, _ in sd.get("branches", [])]
    c["a"][0] = r.choice([lo for lo, _ in lo_hi] + [hi for _, hi in lo_hi] + [lo - 1 for lo, _ in lo_hi] + [hi + 1 for _, hi in lo_hi] + [0, 0x7F, 0xFF, 0x100, 0x101]) \
        if lo_hi else r.choice([0, 5, 0x7F, 0xFF, 0x100])
    c["a"][0] &= MASK
    c["q"]["RangedRandom"] = [r.choice([0, 1, 5, 10, 11, 29, 30, 31, 100, 0xFFFFFFFF, r.randrange(0, 300), r.getrandbits(32)]) for _ in range(3)]
    return c


def edge_assign():
    cases = []
    targets = [make_sound(f1e=w) for w in (10, 20, 30)]
    for branches in ([], [(1, 0, 255, 0)], [(1, 0, 10, 0), (2, 11, 20, 0), (3, 21, 255, 0)], [(1, 5, 5, 0), (2, 5, 5, 0), (3, 5, 5, 0)], [(1, 10, 5, 0)],
                     [(1, 0, 255, 0), (2, 0, 255, 0), (3, 0, 255, 0)]):
        sd0 = make_sound(samples=[make_sample()], f1e=1)
        sd0["branches"] = list(branches)
        events = [make_event([sd0] + [dict(t) for t in targets])]
        for param in (0, 4, 5, 6, 10, 11, 20, 21, 255, 256, 0x105):
            for pick in (0, 1, 9, 10, 11, 29, 30, 31, 59, 60, 61, 0xFFFFFFFF):
                st = default_state()
                st[S_EXTRA] = 1
                st[S_EVENT], st[S_SOUND] = 21, 31
                st[S_VALUES] = 0x0A0B0C0D
                cases.append(make_case(K_ASSIGN, BRA, events=events, state=st, a=[param] + [0] * 15, q={"RangedRandom": [pick]}))
    # no sound yet: the event's first sound, with and without samples (only the low half of the count counts)
    for samples, hi in (([make_sample()], 0), ([], 0), ([], 1), ([], 0xFFFF), ([make_sample()], 0xFFFF)):
        sd0 = make_sound(samples=samples, hi=hi)
        sd0["branches"] = [(1, 0, 255, 0)]
        events = [make_event([sd0, make_sound(f1e=7)])]
        for pick in (0, 7, 8):
            st = default_state()
            st[S_EXTRA] = 1
            st[S_EVENT], st[S_SOUND] = 21, 0
            cases.append(make_case(K_ASSIGN, BRA, events=events, state=st, a=[3] + [0] * 15, q={"RangedRandom": [pick]}))
    cases.append(make_case(K_ASSIGN, BRA, events=[make_event([make_sound()])], state=dstate(), a=[3] + [0] * 15))
    return cases


def gen_updparam(kinds):
    def make(r):
        kind = r.choice(kinds)
        c = world_case(r, K_UPDPARAM, kind, nchild=r.choice([0, 1, 2]) if kind in (OSS, OSBS) else 0)
        ensure_sound(c)
        st = c["state"]
        c["a"][0] = r.choice([0, 1, 2, 3, 4, 5, 0xFF, 0x1FF, r.getrandbits(8), (st[S_IDS] & 0xFF), (st[S_IDS] >> 8) & 0xFF, (st[S_IDS] >> 16) & 0xFF])
        c["a"][1] = r.choice([0, 1, 50, 0x7F, 0xFF, 0x100, r.getrandbits(32)])
        if r.random() < 0.6:
            st[S_FLAGS] |= F_S40
        else:
            st[S_FLAGS] &= ~F_S40
        vals = bytearray(struct.pack("<I", st[S_VALUES]))
        vals[0] = r.choice([0, 0x7F, 5, r.getrandbits(8)])
        st[S_VALUES] = struct.unpack("<I", bytes(vals))[0]
        if kind == BRA:
            c["q"]["RangedRandom"] = [r.choice([0, 1, 5, 50, 100, r.randrange(0, 300), r.getrandbits(32)]) for _ in range(3)]
        return c
    return make


def edge_updparam(kinds):
    def make():
        cases = []
        r = random.Random(900 + sum(kinds))
        for kind in kinds:
            for flags in (0, F_S40):
                for byte0 in (0, 0x7F, 0x80, 5):
                    for ident in (0, 1, 2, 3):
                        c = gen_updparam([kind])(r)
                        c["state"][S_FLAGS] = (c["state"][S_FLAGS] & ~F_S40) | flags
                        vals = bytearray(struct.pack("<I", c["state"][S_VALUES]))
                        vals[0] = byte0
                        c["state"][S_VALUES] = struct.unpack("<I", bytes(vals))[0]
                        c["a"][0] = ident
                        cases.append(c)
        return cases
    return make


def gen_play(kinds):
    def make(r):
        kind = r.choice(kinds)
        c = world_case(r, K_PLAY, kind, nchild=r.choice([0, 1, 2, 3]) if kind in (OSS, OSBS) else 0)
        st = c["state"]
        if kind in (OSS, OSBS):
            ensure_sound(c)
        elif r.random() < 0.9:
            ensure_sound(c)
            if not st[S_SAMPLE]:
                st[S_SAMPLE] = 71
        c["a"][0] = r.choice([0, 100, 5000, 100000, r.getrandbits(32), 0xFFFFFFFF])
        if r.random() < 0.5:
            st[S_FLAGS] &= ~(F_PLAYING | F_FINISHED | F_S40)
        if r.random() < 0.3:
            st[S_FLAGS] ^= F_LOOPFOREVER
        if r.random() < 0.15:
            st[S_FLAGS] |= F_NONPOS
        c["q"]["ListenerOn"] = [r.randrange(2)]
        return c
    return make


def edge_play(kinds):
    def make():
        cases = []
        for kind in kinds:
            for mgr in (0, 1):
                for sound in ((31,) if kind in (OSS, OSBS) else (0, 31)):
                    for flags in (0, F_PLAYING, F_FINISHED, F_PLAYING | F_FINISHED, F_LOOPFOREVER, F_S40, F_NONPOS):
                        for ret in (0, 1):
                            st = default_state()
                            st[S_EXTRA] = 1
                            st[S_SOUND] = sound
                            st[S_SAMPLE] = 71
                            st[S_FLAGS] = flags
                            st[S_ENDTIME] = 100000
                            cases.append(make_case(K_PLAY, kind, mgr=mgr, state=st, a=[99000] + [0] * 15, q={"PlaySample": [ret], "PlayControl": [0x4321],
                                                   "Bank": [1], "QSound": [0], "DistMul": [ONE]}))
        return cases
    return make


def gen_chain_op(op):
    def make(r):
        kind = r.choice([OSS, OSBS])
        c = world_case(r, op, kind, nchild=r.choice([0, 1, 2, 3]))
        st = c["state"]
        ensure_sound(c)
        if op != K_REMOVE:
            c["mgr"] = 1   # the base's Stop, UpdateParameterDepandants and Update need the manager
        c["a"][0] = r.choice([0, 100, 5000, 100000, r.getrandbits(32), 0xFFFFFFFF])
        for ch in c["children"]:
            ch[S_ENDTIME] = r.choice([0, 99, 100, 101, 5000, 100000, r.getrandbits(32), 0xFFFFFFFF])
            if r.random() < 0.5:
                ch[S_HANDLE] = 0
            if r.random() < 0.5:
                ch[S_FLAGS] &= ~(F_PLAYING | F_FINISHED)
        if r.random() < 0.5:
            st[S_FLAGS] &= ~(F_FINISHED | F_S40)
        c["a"][0] = r.choice([0, 99, 100, 101, c["a"][0]])
        if op == K_ADDELEMENT:
            c["a"][1] = r.randrange(0, len(c["children"]) + 1)
        if op in (K_FIRSTCHILD, K_PROCESS):
            if r.random() < 0.3:
                st[S_FLAGS] ^= F_S20
            if r.random() < 0.3 and kind == OSBS:
                st[S_IDS] = (st[S_IDS] & ~0xFF) | r.choice([0, 1, 2])
        if op == K_UPDATE:
            c["a"][:6] = [rbits(r) for _ in range(6)]
            c["a"][:3] = [AP.rsane(r) for _ in range(3)] if r.random() < 0.8 else c["a"][:3]
            if r.random() < 0.5:
                st[S_FLAGS] ^= F_NONPOS
            for ch in c["children"]:
                if r.random() < 0.6:
                    ch[S_HANDLE] = r.choice([1, 0x1234, r.getrandbits(32) | 1])
        c["q"]["RangedRandom"] = [r.choice([0, 1, 2, 5, 17, r.randrange(0, 300)]) for _ in range(4)]
        return c
    return make


def edge_chain_op(op):
    def make():
        cases = []
        r = random.Random(1200 + op)
        for kind in (OSS, OSBS):
            for n in range(0, 4):
                for i in range(5):
                    cases.append(gen_chain_op(op)(r))
        # end times against the clock: the child list compared at, below and above `now`
        for now in (99, 100, 101):
            for ends in ([100], [99, 101], [100, 100, 100], [101, 99, 100], [0, 0xFFFFFFFF]):
                kids = []
                for e_ in ends:
                    k = default_state()
                    k[S_EXTRA] = 1
                    k[S_ENDTIME] = e_
                    kids.append(k)
                st = default_state()
                st[S_EXTRA] = 1
                st[S_IDS] = 0
                cases.append(make_case(op, OSS, state=st, children=kids, a=[now, 0] + [0] * 14, q={"Bank": [1], "RangedRandom": [0], "Random": [0x10000]}))
        return cases
    return make


def gen_callback(r):
    kind = r.choice([SEN, LIN, BRA, SHU, LST])
    c = world_case(r, K_CALLBACK, kind, nchild=0)
    st = c["state"]
    c["a"][0] = r.choice([0, 1, 0x1234, 0xFFFFFFFF, r.getrandbits(32)])
    c["a"][1] = r.getrandbits(32)
    if r.random() < 0.8:
        ensure_sound(c)
    elif r.random() < 0.5:
        st[S_SOUND] = 0
    if kind == LIN and r.random() < 0.2:
        st[S_EVENT] = st[S_SOUND] = 0   # no event: the choice leaves the sound alone and answers true
    if kind == BRA:
        ensure_sound(c)
        c["q"]["RangedRandom"] = [r.choice([0, 1, 5, 50, 100, r.randrange(0, 300), r.getrandbits(32)]) for _ in range(3)]
    if kind == SHU:
        st[S_EVENT] = 21
        c["events"] = shuffle_events(r, r.randrange(1, 9))
        st[S_SOUND] = 31
        st[S_SAMPLE] = 71
        c["q"]["RangedRandom"] = [r.choice([0, 1, 2, 3, 7, 8]) for _ in range(3)]
        c["q"]["Clock"] = [r.getrandbits(32)]
        c["image"] = tree_image(c["events"], c["vc"])
    if kind == LST:
        c["list"] = rand_list(r, len(c["cats"]))
        if r.random() < 0.7:
            c["shuf"] = [1, 0, 0, r.randrange(0, c["list"][0] + 2), 0, 0, 0]
    if kind == LIN and r.random() < 0.5:
        st[S_FLAGS] ^= F_LOOPFOREVER
    c["q"]["V.ChooseRandomSample"] = [0]
    return c


def gen_basic(r):
    kind = r.choice([SEN, LIN, BRA, SHU, LST])
    c = world_case(r, K_BASIC, kind, nchild=0)
    c["info"] = [r.choice([0, rword(r)]), r.choice([0, rword(r)]), r.randrange(3) == 0]
    if r.random() < 0.8:
        ensure_sound(c)
    c["state"][S_FLAGS] |= r.choice([0, 0x200, 0x40000, 0x400])
    return c


def edge_basic():
    cases = []
    for kind in (SEN, LIN, BRA, SHU, LST):
        for flags in (0, 0x200, 0x40000, 8, 4):
            for bank in (0, 1, 2):
                st = default_state()
                st[S_EXTRA] = 1
                st[S_FLAGS] = flags
                cases.append(make_case(K_BASIC, kind, state=st, q={"Bank": [bank], "QSound": [0]}))
    return cases


def edge_callback():
    cases = []
    for kind in (SEN, LIN):
        for mgr in (0, 1):
            for sound in (0, 31):
                for rnd in (0, 1):
                    for basic in (0, 1):
                        for ret in (0, 1):
                            st = default_state()
                            st[S_EXTRA] = 1
                            st[S_SOUND] = sound
                            events = [make_event([make_sound(), make_sound()])]
                            cases.append(make_case(K_CALLBACK, kind, mgr=mgr, state=st, events=events, a=[0x1234, 0x5678] + [0] * 14,
                                                   q={"V.ChooseRandomSound": [rnd], "V.FillBasicSampleInfo": [basic], "ConstructSample": [ret], "Bank": [1]}))
    return cases


# ---------------------------------------------------------------- the tests

def make_tests():
    tests = []
    for name, addr, op, kinds, slot in FUNCS:
        if K_CTOR <= op < K_DTOR:
            kind = op - K_CTOR
            gen, edge = gen_ctor(kind), edge_ctor(kind)
        elif K_DTOR <= op < K_CLASSID:
            kind = op - K_DTOR
            gen, edge = gen_dtor(kind), edge_dtor(kind)
        elif K_CLASSID <= op < K_CREATE1:
            gen, edge = gen_classid_kinds(kinds), edge_classid(kinds)
        elif K_CREATE1 <= op < K_CREATE4:
            kind = op - K_CREATE1
            gen, edge = gen_create1(kind), edge_create1(kind)
        elif K_CREATE4 <= op < K_GETELEMENT:
            kind = op - K_CREATE4
            gen, edge = gen_create4(kind), edge_create4(kind)
        elif op == K_GETELEMENT:
            gen, edge = gen_getelement(kinds), (lambda kinds=kinds: [make_case(K_GETELEMENT, k) for k in kinds])
        elif op == K_LDCTOR:
            gen, edge = gen_ldctor, edge_ldctor
        elif op == K_LDDTOR:
            gen, edge = gen_lddtor, edge_lddtor
        elif op in (K_SHCTOR, K_SHINIT, K_SHSHUFFLE, K_SHNEXT):
            gen, edge = gen_shuf(op), edge_shuf(op)
        elif op == K_SETLISTINFO:
            gen, edge = gen_setlistinfo, edge_setlistinfo
        elif op == K_CHOOSE:
            gen, edge = gen_choose(kinds), edge_choose(kinds)
        elif op == K_ASSIGN:
            gen, edge = gen_assign, edge_assign
        elif op == K_UPDPARAM:
            gen, edge = gen_updparam(kinds), edge_updparam(kinds)
        elif op == K_PLAY:
            gen, edge = gen_play(kinds), edge_play(kinds)
        elif op in (K_STOP, K_UPDATE, K_UPDDEP, K_FIRSTCHILD, K_PROCESS, K_ADDELEMENT, K_REMOVE):
            gen, edge = gen_chain_op(op), edge_chain_op(op)
        elif op == K_CALLBACK:
            gen, edge = gen_callback, edge_callback
        elif op == K_BASIC:
            gen, edge = gen_basic, edge_basic
        else:
            raise AssertionError(name)
        tests.append(FnTest("%s 0x%08x" % (name, addr), ppc_run, host_run, edge=edge, rnd=gen))
    return tests


TESTS = make_tests()
