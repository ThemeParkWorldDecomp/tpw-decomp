"""Unit w79g2, CAudioPlaceHolder and its small classes, against libtpw_unit_AudioPlaceHolder.dylib (tests/unit_AudioPlaceHolder.cpp), in
the program "sound symbols shared" (Code at 0x10000000, TOC base r2 = 0x1003abf0):
  src/sound/AudioPlaceHolder.cpp, src/sound/AudioPlaceHolderList.cpp, src/sound/TbAudioBody.cpp, src/sound/TbSoundSampleCallback.cpp  TbSndCallback: SoundCallback 0x1000d48c, constructor 0x100179a8, CompletedCallback 0x1001baa4;
      CAudioPlaceHolder: SetSoundManager 0x100166b4, constructor 0x10017898, destructor 0x100179e0, ChangeDistPriority 0x10017aa4,
      SetPriorityAndVolume 0x10017d8c, UpdateParameter 0x10017f30, AllocateExtraData 0x10018034, UpdateParameterDepandants
      0x10018194, Update 0x100182a4, CompletedCallback 0x100185bc, Release 0x1001864c, SetFlagsFromEvent 0x100186b0,
      InitParamatisedControls 0x10018790, Init 0x10018874, GetParameterValue 0x10018a44, GetRandomVolume 0x10018ad0,
      GetRandomPitch 0x10018dc0, FillBasicSampleInfo 0x10018f34, GetPsuedoTime 0x10019060, FillSampleInfo 0x100191cc, GetVolume
      0x10019798, GetMaxDistance 0x1001984c, ChooseRandomSample 0x100198e8, GetNextSample 0x10019a10, VolumeControlIsOn
      0x10019b1c, ChooseRandomSound 0x10019ca0, GetNextSound 0x10019de4, SetVolume 0x10019ea0, SetVolumeAndFrequency 0x10019ee8,
      ConvertFrequencyToPitchIndex 0x1001a01c, SetVolumeAndPitch 0x1001a1bc, ProcessPositionAndPriority 0x1001a21c, InitFade
      0x1001a36c, SetOutOfRange 0x1001a3cc, Stop 0x1001a480, Pause 0x1001a544, Restart 0x1001a678, FinishedPlaying 0x1001a7b8,
      Save 0x1001a82c, WriteMagicHeader 0x1001aecc, GetClassID 0x1001afe4, Load 0x1001b020, ReadMagicHeader 0x1001b7ac,
      ModifyPositionForBoundingRectangle 0x1001b83c, PlaySample 0x1001ba48, GetHandle 0x1001c9b0, SetHandle 0x1001ccd0;
      CDistPriority: GetBasePriority 0x10016dc4, constructor 0x1001795c, SetDistance 0x10017ca4, SetBasePriority 0x10017d38,
      operator unsigned long 0x10017e9c, operator= 0x10017ee8; CAudioPlaceHolderExtraData constructor 0x100180cc

Each function runs as the original in the emulator (at its address, the hidden TbError buffer in r3 where the function returns
one) and as the host's translation, on a world built from the case: a placeholder (the real constructor, then a preset state of
34 words written into the members), a tree of events, sounds and samples (Mac layout in the emulator, Ptr32 records in the
arena on the host), a category container whose AddRef and Release record, a sound manager that is a recording fake (its getters,
its Rolloff and SystemCommand virtuals; a command handed to SystemCommand is decoded and recorded), the clock, the memory file
(LbFile_Write and LbFile_Read), bfl::maths::Random and RangedRandomSlow (answers from queues, the seed they were given is
recorded), CMapReference and the two sentence Element constructors (w79g4), TbMapStreamer::Instance and the fix-up call. The
placeholder's virtuals can be replaced by recording fakes through a hook mask (the slots are the Mac's: +0x10 the destructor
runs the real one after recording). The unit's own functions run as the original; the w77 info records run as the original.
The call logs, the answer, the placeholder's state (all members, the extra data, the container's reference count, the sample
info, the sound data the swaps change, the bytes written to the file and the two statics) are compared. NaN floats compare as one
value.

Labels: 1 the placeholder, 2 the manager, 3 the volume control, 4 the container, 5 the sample info, 6 the child placeholder, 7
the streamer, 8 the listener position, 9 and 10 the position and velocity arguments, 11..13 the banks, 14 the file, 16 the
category data, 21.. the events, 31.. the sounds (flat), 71.. the samples (flat).
"""
import ctypes as C
import math
import struct

import calllog as CL
import ppcemu
import tpwtest as T
from tpwtest import FnTest
from unicorn import UC_HOOK_CODE
from unicorn import ppc_const as P

PROGRAM = "sound symbols shared"
SHIM_LIBRARY = "unit_AudioPlaceHolder"
MASK = 0xFFFFFFFF
FILL = 0xA5A5A5A5
ONE = 0x3F800000

# statics of the class and the function-local static of ConvertFrequencyToPitchIndex
S_MGR, S_SEED = 0x1003F360, 0x1003F35C
S_GUARD, S_INVLN2 = 0x1003F356, 0x1003F358
REAL_VT = 0x1003CC04       # the placeholder's vtable
CB_VT = 0x1003CC5C         # TbSndCallback's vtable
CMD_VT = {0x1003C9F0: "SetSampleInfo", 0x1003CAE0: "Stop", 0x1003C9AC: "Pause", 0x1003C964: "Restart"}

# the parameter words (the unit file's Param enum)
(P_OP, P_MGR, P_HOOK, P_CLASS, P_CHILDCLASS, P_SEED, P_FILEN) = range(7)
P_STATE, P_A, P_LISTENER, P_ARGPOS, P_CHILD, P_INFO, P_SPEC = 8, 48, 56, 60, 72, 88, 128
# the state words (the unit file's State enum)
(S_EVENT, S_SOUND, S_SAMPLE, S_POSX, S_POSY, S_POSZ, S_VELX, S_VELY, S_VELZ, S_ENDTIME, S_PRIORITY, S_HANDLE, S_CATAGORY,
 S_LISTHANDLE, S_FLAGS, S_NEXT, S_PREVIOUS, S_PAUSETIME, S_MAXDIST, S_LASTSOUND, S_LASTSAMPLE, S_VCINDEX, S_FADE, S_VOLUME,
 S_PITCH, S_EXTRA, S_IDS, S_VALUES, S_LEFT, S_TOP, S_RIGHT, S_BOTTOM, S_CHILD, S_PARENT) = range(34)
NSTATE = 34
(O_R, O_R2, O_R3) = 0, 1, 2
O_STATE, O_CAT, O_INFO, O_FILE, O_DATA, O_STATIC, O_EXTRA = 8, 48, 52, 96, 224, 256, 260
(H_DTOR, H_INIT, H_SOUND, H_SAMPLE, H_PARAM, H_BASIC, H_PSEUDO, H_CLASS, H_DIST, H_DEP) = (1, 2, 4, 8, 0x10, 0x20, 0x40, 0x80, 0x100, 0x200)

# the flag bits of the placeholder (include/AudioPlaceHolder.h)
F_PLAYING, F_LOOPING, F_LOOPFOREVER, F_NONPOS, F_FINISHED = 1, 2, 4, 8, 0x10
F_S20, F_S40, F_MAXDIST, F_PROCVIRT, F_USERID, F_FIXEDVOL, F_OUTRANGE, F_BOUNDS = 0x20, 0x40, 0x80, 0x100, 0x200, 0x400, 0x800, 0x1000
F_PAUSED, F_VCOFF, F_VOLSET, F_PITCHSET, F_EVENTMODE, F_FADING, F_FRESH = 0x2000, 0x4000, 0x8000, 0x10000, 0x40000, 0x80000, 0x100000
ALL_FLAGS = [1 << i for i in range(21)]

(K_SETMGR, K_GETBASE, K_CTOR, K_PCTOR, K_DTOR, K_CHDIST, K_SETDIST, K_SETBASE, K_SETPV, K_P2W, K_PASSIGN, K_UPDPARAM, K_ALLOCX,
 K_XCTOR, K_UPDDEP, K_UPDATE, K_COMPLETED, K_RELEASE, K_SETFLAGS, K_INITCTRL, K_INIT, K_GETPARAM, K_RVOL, K_RPITCH, K_FILLBASIC,
 K_PSEUDO, K_FILL, K_GETVOL, K_MAXDIST, K_CHSAMPLE, K_NEXTSAMPLE, K_VCON, K_CHSOUND, K_NEXTSOUND, K_SETVOL, K_SETVF, K_CONVF,
 K_SETVP, K_PROCESS, K_INITFADE, K_OOR, K_STOP, K_PAUSE, K_RESTART, K_FINISHED, K_SAVE, K_WMAGIC, K_CLASSID, K_LOAD, K_RMAGIC,
 K_MODIFY, K_PLAY, K_GETHANDLE, K_SETHANDLE, K_SOUNDCB, K_CBCTOR, K_BASECOMPLETED) = range(57)

ADDR = {
    K_SETMGR: 0x100166B4, K_GETBASE: 0x10016DC4, K_CTOR: 0x10017898, K_PCTOR: 0x1001795C, K_DTOR: 0x100179E0, K_CHDIST: 0x10017AA4,
    K_SETDIST: 0x10017CA4, K_SETBASE: 0x10017D38, K_SETPV: 0x10017D8C, K_P2W: 0x10017E9C, K_PASSIGN: 0x10017EE8,
    K_UPDPARAM: 0x10017F30, K_ALLOCX: 0x10018034, K_XCTOR: 0x100180CC, K_UPDDEP: 0x10018194, K_UPDATE: 0x100182A4,
    K_COMPLETED: 0x100185BC, K_RELEASE: 0x1001864C, K_SETFLAGS: 0x100186B0, K_INITCTRL: 0x10018790, K_INIT: 0x10018874,
    K_GETPARAM: 0x10018A44, K_RVOL: 0x10018AD0, K_RPITCH: 0x10018DC0, K_FILLBASIC: 0x10018F34, K_PSEUDO: 0x10019060,
    K_FILL: 0x100191CC, K_GETVOL: 0x10019798, K_MAXDIST: 0x1001984C, K_CHSAMPLE: 0x100198E8, K_NEXTSAMPLE: 0x10019A10,
    K_VCON: 0x10019B1C, K_CHSOUND: 0x10019CA0, K_NEXTSOUND: 0x10019DE4, K_SETVOL: 0x10019EA0, K_SETVF: 0x10019EE8,
    K_CONVF: 0x1001A01C, K_SETVP: 0x1001A1BC, K_PROCESS: 0x1001A21C, K_INITFADE: 0x1001A36C, K_OOR: 0x1001A3CC, K_STOP: 0x1001A480,
    K_PAUSE: 0x1001A544, K_RESTART: 0x1001A678, K_FINISHED: 0x1001A7B8, K_SAVE: 0x1001A82C, K_WMAGIC: 0x1001AECC,
    K_CLASSID: 0x1001AFE4, K_LOAD: 0x1001B020, K_RMAGIC: 0x1001B7AC, K_MODIFY: 0x1001B83C, K_PLAY: 0x1001BA48,
    K_GETHANDLE: 0x1001C9B0, K_SETHANDLE: 0x1001CCD0, K_SOUNDCB: 0x1000D48C, K_CBCTOR: 0x100179A8, K_BASECOMPLETED: 0x1001BAA4,
}
NAME = {
    K_SETMGR: "CAudioPlaceHolder::SetSoundManager", K_GETBASE: "CDistPriority::GetBasePriority", K_CTOR: "CAudioPlaceHolder::CAudioPlaceHolder",
    K_PCTOR: "CDistPriority::CDistPriority", K_DTOR: "CAudioPlaceHolder::~CAudioPlaceHolder", K_CHDIST: "CAudioPlaceHolder::ChangeDistPriority",
    K_SETDIST: "CDistPriority::SetDistance", K_SETBASE: "CDistPriority::SetBasePriority",
    K_SETPV: "CAudioPlaceHolder::SetPriorityAndVolume", K_P2W: "CDistPriority::operator unsigned long",
    K_PASSIGN: "CDistPriority::operator=", K_UPDPARAM: "CAudioPlaceHolder::UpdateParameter",
    K_ALLOCX: "CAudioPlaceHolder::AllocateExtraData", K_XCTOR: "CAudioPlaceHolderExtraData::CAudioPlaceHolderExtraData",
    K_UPDDEP: "CAudioPlaceHolder::UpdateParameterDepandants", K_UPDATE: "CAudioPlaceHolder::Update",
    K_COMPLETED: "CAudioPlaceHolder::CompletedCallback", K_RELEASE: "CAudioPlaceHolder::Release",
    K_SETFLAGS: "CAudioPlaceHolder::SetFlagsFromEvent", K_INITCTRL: "CAudioPlaceHolder::InitParamatisedControls",
    K_INIT: "CAudioPlaceHolder::Init", K_GETPARAM: "CAudioPlaceHolder::GetParameterValue",
    K_RVOL: "CAudioPlaceHolder::GetRandomVolume", K_RPITCH: "CAudioPlaceHolder::GetRandomPitch",
    K_FILLBASIC: "CAudioPlaceHolder::FillBasicSampleInfo", K_PSEUDO: "CAudioPlaceHolder::GetPsuedoTime",
    K_FILL: "CAudioPlaceHolder::FillSampleInfo", K_GETVOL: "CAudioPlaceHolder::GetVolume",
    K_MAXDIST: "CAudioPlaceHolder::GetMaxDistance", K_CHSAMPLE: "CAudioPlaceHolder::ChooseRandomSample",
    K_NEXTSAMPLE: "CAudioPlaceHolder::GetNextSample", K_VCON: "CAudioPlaceHolder::VolumeControlIsOn",
    K_CHSOUND: "CAudioPlaceHolder::ChooseRandomSound", K_NEXTSOUND: "CAudioPlaceHolder::GetNextSound",
    K_SETVOL: "CAudioPlaceHolder::SetVolume", K_SETVF: "CAudioPlaceHolder::SetVolumeAndFrequency",
    K_CONVF: "CAudioPlaceHolder::ConvertFrequencyToPitchIndex", K_SETVP: "CAudioPlaceHolder::SetVolumeAndPitch",
    K_PROCESS: "CAudioPlaceHolder::ProcessPositionAndPriority", K_INITFADE: "CAudioPlaceHolder::InitFade",
    K_OOR: "CAudioPlaceHolder::SetOutOfRange", K_STOP: "CAudioPlaceHolder::Stop", K_PAUSE: "CAudioPlaceHolder::Pause",
    K_RESTART: "CAudioPlaceHolder::Restart", K_FINISHED: "CAudioPlaceHolder::FinishedPlaying",
    K_SAVE: "CAudioPlaceHolder::Save", K_WMAGIC: "CAudioPlaceHolder::WriteMagicHeader", K_CLASSID: "CAudioPlaceHolder::GetClassID",
    K_LOAD: "CAudioPlaceHolder::Load", K_RMAGIC: "CAudioPlaceHolder::ReadMagicHeader",
    K_MODIFY: "CAudioPlaceHolder::ModifyPositionForBoundingRectangle", K_PLAY: "CAudioPlaceHolder::PlaySample",
    K_GETHANDLE: "CAudioPlaceHolder::GetHandle", K_SETHANDLE: "CAudioPlaceHolder::SetHandle",
    K_SOUNDCB: "TbSndCallback::SoundCallback", K_CBCTOR: "TbSndCallback::TbSndCallback",
    K_BASECOMPLETED: "TbSndCallback::CompletedCallback",
}
# the operations that return a TbError through the hidden buffer (this moves to r4)
HIDDEN = {K_UPDATE, K_INIT, K_STOP, K_PAUSE, K_RESTART, K_SAVE, K_LOAD, K_PLAY}
# the operations whose answer is a float (compared as bits)
FLOAT_RESULT = {K_CHDIST, K_MAXDIST}
QUEUE_ERRORS = [0, -1, 1, 5, -2, 0x7FFFFFFF, -0x80000000]


def pack(words):
    return struct.pack(">%dI" % len(words), *[w & MASK for w in words])


def fbits(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bits_f(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def canon(bits):
    bits &= MASK
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x007FFFFF) else bits


def f1_bits(e):
    """FPR 1 as the single-precision bit pattern (every NaN one value)."""
    try:
        return canon(fbits(e.f(1)))
    except OverflowError:
        return 0x7F800000 if e.f(1) > 0 else 0xFF800000


def lbl(L, address):
    return L.lab(address) & MASK


def s8(b):
    return b - 256 if b > 127 else b


# ---------------------------------------------------------------- the data tree (events, sounds, samples)

def make_sample(f00=1, f04=0xFFFF, f08=2000, bank=0):
    return dict(f00=f00, f04=f04, f08=f08, bank=bank)


def make_sound(samples=None, hi=0, vmin=50, vmax=100, pmin=0, pmax=0, f10=0, f12=0, f16=0, f18=0, f1a=0, f1c=0, f1e=0xFFFF, f22=0x41200000):
    return dict(samples=samples if samples is not None else [make_sample()], hi=hi, vmin=vmin, vmax=vmax, pmin=pmin & 0xFF, pmax=pmax & 0xFF,
                f10=f10, f12=f12, f16=f16, f18=f18, f1a=f1a, f1c=f1c, f1e=f1e, f22=f22)


def make_event(sounds=None, id=7, f0c=5, flags=0, f12=0):
    return dict(sounds=sounds if sounds is not None else [make_sound()], id=id, f0c=f0c, flags=flags, f12=f12)


def spec_words(events):
    """The data tree as the flat words of the unit file's BuildWorld, depth first: [nEvents], then per event its record, its
    sounds' records, and after each sound record the sound's samples."""
    w = [len(events)]
    for ev in events:
        w += [ev["id"], len(ev["sounds"]), ev["f0c"], (ev["flags"] & 0xFFFF) | ((ev["f12"] & 0xFFFF) << 16)]
        for sd in ev["sounds"]:
            w += [len(sd["samples"]) | ((sd["hi"] & 0xFFFF) << 16), sd["vmin"] | (sd["vmax"] << 8) | (sd["pmin"] << 16) | (sd["pmax"] << 24),
                  (sd["f10"] & 0xFFFF) | ((sd["f12"] & 0xFFFF) << 16), (sd["f16"] & 0xFFFF) | ((sd["f18"] & 0xFFFF) << 16),
                  (sd["f1a"] & 0xFFFF) | ((sd["f1c"] & 0xFFFF) << 16), sd["f1e"], sd["f22"]]
            for sm in sd["samples"]:
                w += [sm["f00"], sm["f04"], sm["f08"], sm["bank"]]
    return w


def flat_sounds(events):
    return [sd for ev in events for sd in ev["sounds"]]


def flat_samples(events):
    return [sm for ev in events for sd in ev["sounds"] for sm in sd["samples"]]


def sound_label(events, e, s):
    return 31 + sum(len(events[i]["sounds"]) for i in range(e)) + s


def sample_label(events, e, s, m):
    return 71 + sum(len(sd["samples"]) for ev in events[:e] for sd in ev["sounds"]) + sum(len(sd["samples"]) for sd in events[e]["sounds"][:s]) + m


# ---------------------------------------------------------------- the PowerPC side

class World:
    pass


def build_data(e, L, events, vc_index):
    """The events, sounds and samples in the emulator; returns (catagory data address, events array, flat sound and sample addresses)."""
    ne = len(events)
    ev_addr = e.alloc(0x14 * max(ne, 1), b"\0" * (0x14 * max(ne, 1)))
    sound_addrs, sample_addrs = [], []
    for i, ev in enumerate(events):
        ns = len(ev["sounds"])
        so_addr = e.alloc(0x2A * max(ns, 1), b"\0" * (0x2A * max(ns, 1)))
        e.write(ev_addr + 0x14 * i, pack([ev["id"], ns, so_addr, ev["f0c"], (ev["flags"] & 0xFFFF) << 16 | (ev["f12"] & 0xFFFF)]))
        L.labels[ev_addr + 0x14 * i] = 21 + i
        for j, sd in enumerate(ev["sounds"]):
            nm = len(sd["samples"])
            sm_addr = e.alloc(0x10 * max(nm, 1), b"\0" * (0x10 * max(nm, 1)))
            base = so_addr + 0x2A * j
            e.write(base, pack([nm | ((sd["hi"] & 0xFFFF) << 16), 0, sm_addr]))
            e.write(base + 0xC, bytes([sd["vmin"], sd["vmax"], sd["pmin"], sd["pmax"]]))
            e.write(base + 0x10, struct.pack(">HH", sd["f10"] & 0xFFFF, sd["f12"] & 0xFFFF))
            e.write(base + 0x16, struct.pack(">HHHH", sd["f16"] & 0xFFFF, sd["f18"] & 0xFFFF, sd["f1a"] & 0xFFFF, sd["f1c"] & 0xFFFF))
            e.write(base + 0x1E, pack([sd["f1e"], sd["f22"]]))
            L.labels[base] = 31 + len(sound_addrs)
            sound_addrs.append(base)
            for k, sm in enumerate(sd["samples"]):
                a = sm_addr + 0x10 * k
                e.write(a, pack([sm["f00"], sm["f04"], sm["f08"], (sm["bank"] & 0xFFFF) << 16]))
                L.labels[a] = 71 + len(sample_addrs)
                sample_addrs.append(a)
    cat = e.alloc(0x18, pack([ne, ev_addr, (vc_index & 0xFFFF) << 16, 0, 0, 0]))
    L.labels[cat] = 16
    return cat, ev_addr, sound_addrs, sample_addrs


def ptr_of(labels_map, label):
    return labels_map.get(label, 0)


def preset_holder(e, L, w, obj, st, c):
    """Writes the preset state into a placeholder the real constructor made."""
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
        extra = e.alloc(0x20, b"\xa5" * 0x20)
        T.call_ppc(e, ADDR[K_XCTOR], [extra])
        e.write(extra, struct.pack("<I", st[S_IDS] & MASK))
        e.write(extra + 4, struct.pack("<I", st[S_VALUES] & MASK))
        e.write(extra + 8, pack([st[S_LEFT], st[S_TOP], st[S_RIGHT], st[S_BOTTOM]]))
        e.write(extra + 0x1C, pack([st[S_PARENT]]))
        e.write(obj + 0x5C, pack([extra]))
        L.labels[extra] = 17
        return extra
    return 0


def dump_holder(e, L, obj):
    u = e.u32
    sx = lambda off: canon(u(obj + off))
    extra = u(obj + 0x5C)
    st = [lbl(L, u(obj + 4)), lbl(L, u(obj + 8)), lbl(L, u(obj + 0xC)), sx(0x10), sx(0x14), sx(0x18), sx(0x1C), sx(0x20), sx(0x24),
          u(obj + 0x28), u(obj + 0x2C), u(obj + 0x30), lbl(L, u(obj + 0x34)), u(obj + 0x38), u(obj + 0x3C), u(obj + 0x40), u(obj + 0x44),
          u(obj + 0x48), sx(0x4C), e.read(obj + 0x50, 1)[0], e.read(obj + 0x51, 1)[0], e.read(obj + 0x52, 1)[0], sx(0x54),
          struct.unpack(">H", e.read(obj + 0x58, 2))[0], struct.unpack(">H", e.read(obj + 0x5A, 2))[0], 1 if extra else 0]
    if extra:
        child = u(extra + 0x18)
        st += [struct.unpack("<I", e.read(extra, 4))[0], struct.unpack("<I", e.read(extra + 4, 4))[0], u(extra + 8), u(extra + 0xC),
               u(extra + 0x10), u(extra + 0x14), (child if child and L.lab(child) <= 0 else lbl(L, child)), u(extra + 0x1C)]
    else:
        st += [0] * 8
    return tuple(st)


def dump_info(e, L, info):
    u = e.u32
    return tuple([u(info), lbl(L, u(info + 4)), lbl(L, u(info + 8)) if u(info + 0x14) & 4 else u(info + 8), u(info + 0xC), u(info + 0x10), u(info + 0x14), u(info + 0x18), u(info + 0x1C),
                  canon(u(info + 0x20)), u(info + 0x24), u(info + 0x28), lbl(L, u(info + 0x2C)), lbl(L, u(info + 0x30)),
                  canon(u(info + 0x34)), canon(u(info + 0x38)), canon(u(info + 0x3C)), u(info + 0x40), lbl(L, u(info + 0x44)),
                  lbl(L, u(info + 0x48)), u(info + 0x4C), lbl(L, u(info + 0x80)), u(info + 0x84), u(info + 0x88), u(info + 0x8C)])


def rec_info(e, L, name, handle, info):
    u = e.u32
    flags = u(info + 0x14)
    L.rec(name, [handle, u(info), L.lab(u(info + 4)), u(info + 0xC), u(info + 0x10), flags, u(info + 0x28),
                 u(info + 0x1C) if flags & 2 else 0, u(info + 0x24) if flags & 0x20 else 0, L.lab(u(info + 8)) if flags & 4 else 0,
                 u(info + 0x40) if flags & 0x40 else 0, L.lab(u(info + 0x48)) if flags & 0x2000 else 0])
    L.rec(name + ".more", [canon(u(info + 0x34)) if flags & 0x200 else 0, canon(u(info + 0x38)) if flags & 0x200 else 0,
                           canon(u(info + 0x3C)) if flags & 0x200 else 0, u(info + 0x4C) if flags & 0x1000 else 0,
                           L.lab(u(info + 0x80)) if flags & 0x40000 else 0, u(info + 0x84) if flags & 0x40000 else 0])
    pos, vel = u(info + 0x2C), u(info + 0x30)
    L.rec(name + ".pos", [L.lab(pos)] + [canon(u(pos + o)) if pos else 0 for o in (0, 4, 8)] + [L.lab(vel)]
          + [canon(u(vel + o)) if vel else 0 for o in (0, 4, 8)])


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    lib = runner.lib
    lib.tpw_ap_run.argtypes = [C.c_int32, C.c_void_p, C.c_char_p, C.c_void_p]
    e.vts = {}

    # ---- the MathLib imports TbSoundPosition::Valid makes (as in w77's module)
    def classify(e):
        v = e.f(1)
        bits = struct.unpack(">Q", struct.pack(">d", v))[0]
        if math.isnan(v):
            cls = 1 if bits & (1 << 51) else 0
        elif math.isinf(v):
            cls = 2
        elif v == 0.0:
            cls = 3
        elif abs(v) < 2.0 ** -126:
            cls = 5
        else:
            cls = 4
        e.set_r(3, cls)
    e.stub("__fpclassifyf", classify)
    e.stub("__isfinitef", lambda e: e.set_r(3, 0 if math.isnan(e.f(1)) or math.isinf(e.f(1)) else 1))

    def memcmp(e):
        a, b, n = e.read(e.r(3), e.r(5)), e.read(e.r(4), e.r(5)), e.r(5)
        e.set_r(3, T.u32((a > b) - (a < b)) if n else 0)
    e.stub("memcmp", memcmp)

    # ---- fctiwz of a value above INT32_MAX (or +infinity) gives 0x80000000 in Unicorn; the processor saturates to 0x7fffffff
    pending = {}

    def fctiwz_step(uc, address, size, user):
        if "target" in pending:
            target = pending.pop("target")
            old = uc.reg_read(ppcemu.FPR[target])
            uc.reg_write(ppcemu.FPR[target], (old & 0xFFFFFFFF00000000) | 0x7FFFFFFF)
        word = struct.unpack(">I", e.read(address, 4))[0]
        if word >> 26 == 63 and (word >> 1) & 0x3FF == 15:
            if e.f((word >> 11) & 31) >= 2147483648.0:
                pending["target"] = (word >> 21) & 31
    e.uc.hook_add(UC_HOOK_CODE, fctiwz_step, begin=ADDR[K_CONVF], end=ADDR[K_CONVF] + 208 - 1)

    # ---- C library: log with C semantics (the shared stub raises for 0)
    def c_log(x):
        if math.isnan(x):
            return x
        if x == 0.0:
            return -math.inf
        if x < 0.0:
            return math.nan
        if math.isinf(x):
            return x
        return math.log(x)
    e.stub("log", lambda e: e.set_f1(c_log(e.f(1))))

    # ---- operator new / delete: blocks filled with 0xA5; the extra data (0x20) and the sentence elements (0x60) are recorded
    def op_new(e):
        L, size = e.cl, e.r(3)
        block = e.alloc(max(size, 16), b"\xa5" * max(size, 16))
        if L.track and size in (0x20, 0x60):
            L.rec("new", (size, L.newlab(block)))
        e.set_r(3, block)

    def op_delete(e):
        L, block = e.cl, e.r(3)
        if block != 0 and L.lab(block) > 0:
            L.rec("delete", (L.lab(block),))
            L.forget(block)
    e.stub("__nw__FUl", op_new)
    e.stub("__dl__FPv", op_delete)

    # ---- the clock and the file
    def clock(e):
        e.cl.rec("Clock", ())
        e.set_r(3, T.u32(e.cl.ret("Clock", 0)))
    e.stub("LbTime_GetClock__Fv", clock)

    def file_write(e):   # (buffer r3, file r4, data r5, size r6, transferred r7)
        L, size = e.cl, e.r(6)
        data = e.read(e.r(5), size) if size else b""
        L.rec("File.Write", (L.lab(e.r(4)), size, int.from_bytes(data[:8], "little")))
        e.w.fileout += data
        if e.r(7):
            e.write(e.r(7), pack([size]))
        e.write(e.r(3), pack([L.ret("FileErr", 0)]))
    e.stub("LbFile_Write__FPvPCvUlPUl", file_write)

    def file_read(e):    # (buffer r3, file r4, data r5, size r6, transferred r7)
        L, size, w = e.cl, e.r(6), e.w
        data = bytes(w.filein[w.filepos + i] if w.filepos + i < len(w.filein) else 0 for i in range(size))
        w.filepos += size
        if size:
            e.write(e.r(5), data)
        L.rec("File.Read", (L.lab(e.r(4)), size, int.from_bytes(data[:8], "little")))
        if e.r(7):
            e.write(e.r(7), pack([size]))
        e.write(e.r(3), pack([L.ret("FileErr", 0)]))
    e.stub("LbFile_Read__FPvPvUlPUl", file_read)

    # ---- the in-binary callees outside the unit
    def random(e):
        L = e.cl
        L.rec("Random", (e.u32(e.r(3)),))
        e.set_r(3, T.u32(L.ret("Random", 0)))

    def ranged(e):
        L = e.cl
        L.rec("RangedRandom", (e.u32(e.r(3)), e.r(4)))
        e.set_r(3, T.u32(L.ret("RangedRandom", 0)))

    def mapref_ctor(e):
        e.cl.rec("MapRef.ctor", ())

    def mapref_ids(e):   # (this r3, four int& r4..r7, holder r8)
        L = e.cl
        L.rec("MapRef.GetSoundIds", (L.lab(e.r(8)),))
        for reg in (4, 5, 6, 7):
            e.write(e.r(reg), pack([L.ret("Ids", 0)]))

    def mapref_ptr(e):   # (this r3, category r4, holder r5)
        L = e.cl
        L.rec("MapRef.GetSoundPointer", (e.r(4), L.lab(e.r(5))))
        e.set_r(3, e.w.container if L.ret("SoundPointer", 0) != 0 else 0)

    def instance(e):
        e.set_r(3, e.w.streamer)

    def fixup(e):        # (streamer r3, index r4)
        L = e.cl
        L.rec("FixUp", (L.lab(e.r(3)), e.r(4)))
        e.set_r(3, T.u32(L.ret("FixUp", 0)))

    def mgr_listener(e):
        L = e.cl
        L.rec("Mgr.GetListenerPosition", (L.lab(e.r(3)),))
        e.set_r(3, e.w.listener if L.ret("ListenerOn", 0) != 0 else 0)

    def mgr_distmul(e):
        L = e.cl
        L.rec("Mgr.GetDistanceMultiplier", (L.lab(e.r(3)),))
        e.set_f1(bits_f(L.ret("DistMul", ONE)))

    def mgr_qsound(e):
        L = e.cl
        L.rec("Mgr.IsQSoundOn", (L.lab(e.r(3)),))
        e.set_r(3, T.u32(L.ret("QSound", 0)))

    def mgr_vcontrol(e):
        e.cl.rec("Mgr.GetVolumeControl", (e.cl.lab(e.r(3)),))
        e.set_r(3, e.w.vcontrol)

    def mgr_vcon(e):
        L = e.cl
        L.rec("Mgr.VolumeControlIsOn", (L.lab(e.r(3)), e.r(4) & 0xFF))
        e.set_r(3, 1 if L.ret("VCOn", 1) != 0 else 0)

    def mgr_bank(e):
        L = e.cl
        L.rec("Mgr.GetBank", (L.lab(e.r(3)), e.r(4)))
        answer = L.ret("Bank", 0)
        e.set_r(3, e.w.banks[answer - 1] if 1 <= answer <= 3 else 0)

    for address, fn in {0x10019AD0: random, 0x10018C30: ranged, 0x1001AF5C: mapref_ctor, 0x1001F0DC: mapref_ids, 0x1001F364: mapref_ptr,
                        0x1001F6EC: instance, 0x10023378: fixup, 0x10017CF4: mgr_listener, 0x1001966C: mgr_distmul,
                        0x1001954C: mgr_qsound, 0x10019758: mgr_vcontrol, 0x10016E60: mgr_vcon, 0x1002748C: mgr_bank}.items():
        T.stub_internal(e, address, fn)

    # the sentence Element constructors run the real base constructor (nested), then record
    def element(name):
        def stub(e):
            this, parent = e.r(3), e.r(4)

            def nested(e, call):
                call(("code", ADDR[K_CTOR]), this)
                e.cl.rec(name, (e.cl.lab(this), e.cl.lab(parent)))
                return this
            e.pending_cb = nested
            e.uc.reg_write(P.UC_PPC_REG_PC, ppcemu.RETURN_ADDR)
        return stub

    def nested_internal(address, fn):
        def hook(uc, addr, size, user):
            fn(e)
        e.uc.hook_add(UC_HOOK_CODE, hook, begin=address, end=address)
    nested_internal(0x10024218, element("Element.ctor"))
    nested_internal(0x10023D38, element("BranchingElement.ctor"))

    # ---- the container's table: +8 Release, +0xC AddRef (they count in the word at +4 and record)
    def cat_addref(e):
        L, this = e.cl, e.r(3)
        count = (e.u32(this + 4) + 1) & MASK
        e.write(this + 4, pack([count]))
        L.rec("Cat.AddRef", (L.lab(this), T.s32(count)))
        e.set_r(3, count)

    def cat_release(e):
        L, this = e.cl, e.r(3)
        count = (e.u32(this + 4) - 1) & MASK
        e.write(this + 4, pack([count]))
        L.rec("Cat.Release", (L.lab(this), T.s32(count)))
        e.set_r(3, count)
    words = [0] * 8
    words[2] = T.fake_function(e, cat_release)
    words[3] = T.fake_function(e, cat_addref)
    e.vt_container = T.persist_alloc(e, 32, pack(words))

    # ---- the manager's table: +0x18 SystemCommand (buffer r3, this r4, command r5), +0x44 GetRolloffFactorToMinMaxDistance
    def mgr_command(e):
        L, this, cmd = e.cl, e.r(4), e.r(5)
        L.rec("Mgr.SystemCommand", (L.lab(this),))
        kind = CMD_VT.get(e.u32(cmd), "?")
        handle = e.u32(cmd + 4)
        if kind == "SetSampleInfo":
            rec_info(e, L, "Cmd.SetSampleInfo", handle, e.u32(cmd + 8))
        else:
            L.rec("Cmd." + kind, (handle,))
        e.write(e.r(3), pack([L.ret("SysErr", 0)]))

    def mgr_rolloff(e):
        L = e.cl
        L.rec("Mgr.Rolloff", (L.lab(e.r(3)), f_bits(e.f(1)), f_bits(e.f(2))))
        e.set_f1(bits_f(L.ret("Rolloff", 0)))
    words = [0] * 20
    words[0x18 // 4] = T.fake_function(e, mgr_command)
    words[0x44 // 4] = T.fake_function(e, mgr_rolloff)
    e.vt_manager = T.persist_alloc(e, 80, pack(words))


def f_bits(x):
    try:
        return canon(fbits(x))
    except OverflowError:
        return 0x7F800000 if x > 0 else 0xFF800000


def holder_table(e, mask, child):
    """The placeholder's vtable with the slots of the hook mask replaced by recording fakes (cached)."""
    key = (mask, child)
    if key in e.vts:
        return e.vts[key]

    def v_dtor(e):
        L, this, flag = e.cl, e.r(3), e.r(4)
        L.rec("V.dtor", (L.lab(this),))

        def nested(e, call):
            call(("code", ADDR[K_DTOR]), this, flag)
            return this
        e.pending_cb = nested
        e.uc.reg_write(P.UC_PPC_REG_PC, ppcemu.RETURN_ADDR)

    def v_init(e):       # (buffer r3, this r4, container r5, event r6, position r7, f1)
        L = e.cl
        L.rec("V.Init", (L.lab(e.r(4)), L.lab(e.r(5)), L.lab(e.r(6)), L.lab(e.r(7)), f_bits(e.f(1))))
        e.write(e.r(3), pack([L.ret("V.Init", 0)]))

    def v_sound(e):
        L = e.cl
        L.rec("V.ChooseRandomSound", (L.lab(e.r(3)), e.r(4) & 0xFF))
        e.set_r(3, 1 if L.ret("V.ChooseRandomSound", 0) != 0 else 0)

    def v_sample(e):
        L = e.cl
        L.rec("V.ChooseRandomSample", (L.lab(e.r(3)), e.r(4) & 0xFF))
        answer = L.ret("V.ChooseRandomSample", 0)
        e.set_r(3, e.w.samples[answer - 1] if 0 < answer <= len(e.w.samples) else 0)

    def v_param(e):      # (this r3, mask r4, out r5)
        L = e.cl
        L.rec("V.GetParameterValue", (L.lab(e.r(3)), e.r(4)))
        e.write(e.r(5), pack([L.ret("V.GetParameterValue.out", 0)]))
        e.set_r(3, 1 if L.ret("V.GetParameterValue", 0) != 0 else 0)

    def v_basic(e):
        L = e.cl
        L.rec("V.FillBasicSampleInfo", (L.lab(e.r(3)), L.lab(e.r(4))))
        e.set_r(3, 1 if L.ret("V.FillBasicSampleInfo", 0) != 0 else 0)

    def v_pseudo(e):
        L = e.cl
        L.rec("V.GetPsuedoTime", (L.lab(e.r(3)),))
        e.set_r(3, T.u32(L.ret("V.GetPsuedoTime", 0)))

    def v_class(e):
        e.cl.rec("V.GetClassID", (e.cl.lab(e.r(3)),))
        e.set_r(3, e.w.child_class if e.r(3) == e.w.child else e.w.cls)

    def v_dist(e):
        e.cl.rec("V.ChangeDistPriority", (e.cl.lab(e.r(3)),))
        e.set_f1(bits_f(e.cl.ret("V.ChangeDistPriority", 0)))

    def v_dep(e):
        e.cl.rec("V.UpdateParameterDepandants", (e.cl.lab(e.r(3)),))

    table = bytearray(e.read(REAL_VT, 0x58))
    for bit, slot, fn in ((H_DTOR, 0x10, v_dtor), (H_INIT, 0x18, v_init), (H_DIST, 0x2C, v_dist), (H_DEP, 0x38, v_dep),
                          (H_SOUND, 0x3C, v_sound), (H_SAMPLE, 0x40, v_sample), (H_PARAM, 0x44, v_param), (H_BASIC, 0x4C, v_basic),
                          (H_PSEUDO, 0x50, v_pseudo)):
        if mask & bit:
            table[slot:slot + 4] = pack([T.fake_function(e, fn)])
    if (mask & H_CLASS) or child:
        table[0x54:0x58] = pack([T.fake_function(e, v_class)])
    e.vts[key] = T.persist_alloc(e, len(table), bytes(table))
    return e.vts[key]


# ---------------------------------------------------------------- the case

def default_state():
    st = [0] * NSTATE
    st[S_EVENT], st[S_SOUND], st[S_SAMPLE] = 21, 31, 71
    st[S_POSX], st[S_POSY], st[S_POSZ] = ONE, 0x40000000, 0x40400000
    st[S_LASTSOUND] = st[S_LASTSAMPLE] = 0xFF
    st[S_FADE] = ONE
    st[S_CATAGORY] = 1
    return st


def make_case(op, **kw):
    c = dict(op=op, mgr=1, hook=0, cls=3, childcls=7, seed=0x0BADC0DE, filein=b"", state=default_state(), a=[0] * 8,
             listener=[0, 0, 0], argpos=[ONE, 0x40000000, 0x40400000, 0, 0x3F800000, 0], child=[0, ONE, ONE, ONE, 0, 0x55],
             info=[0, 0, 0], events=[make_event()], vc=0, q={})
    c.update(kw)
    c["q"] = {k: v if isinstance(v, list) else [v] for k, v in c["q"].items()}
    return c


def params(c):
    p = [0] * 512
    p[P_OP], p[P_MGR], p[P_HOOK], p[P_CLASS], p[P_CHILDCLASS], p[P_SEED], p[P_FILEN] = c["op"], c["mgr"], c["hook"], c["cls"], c["childcls"], c["seed"], len(c["filein"])
    p[P_STATE:P_STATE + NSTATE] = c["state"]
    p[P_A:P_A + 8] = c["a"]
    p[P_A + 7] = c["vc"]
    p[P_LISTENER:P_LISTENER + 3] = c["listener"]
    p[P_ARGPOS:P_ARGPOS + 6] = c["argpos"]
    p[P_CHILD:P_CHILD + 6] = c["child"]
    p[P_INFO:P_INFO + 3] = c["info"]
    words = spec_words(c["events"])
    p[P_SPEC:P_SPEC + len(words)] = words
    return [v & MASK for v in p]


def ppc_run(e, c):
    op = c["op"]
    L = e.cl = CL.PpcLog({}, c["q"])
    L.track = False
    w = e.w = World()
    w.filein, w.filepos, w.fileout = c["filein"], 0, b""
    w.cls, w.child_class = c["cls"] & MASK, c["childcls"] & MASK
    st = c["state"]
    a = [v & MASK for v in c["a"]]
    # the data tree and the container
    events = c["events"]
    cat, ev_addr, sounds, samples = build_data(e, L, events, c["vc"])
    w.samples = samples
    w.amap = {}
    for i in range(len(events)):
        w.amap[21 + i] = ev_addr + 0x14 * i
    for i, addr in enumerate(sounds):
        w.amap[31 + i] = addr
    for i, addr in enumerate(samples):
        w.amap[71 + i] = addr
    w.container = e.alloc(0x20, pack([e.vt_container, 1, 0 if a[6] else cat, FILL, FILL, FILL, FILL, FILL]))
    L.labels[w.container] = 4
    w.manager = e.alloc(0x70, b"\xa5" * 0x70)
    e.write(w.manager, pack([e.vt_manager]))
    L.labels[w.manager] = 2
    w.vcontrol = e.alloc(8, b"\0" * 8)
    L.labels[w.vcontrol] = 3
    w.listener = e.alloc(12, pack(c["listener"]))
    L.labels[w.listener] = 8
    argpos = [e.alloc(12, pack(c["argpos"][3 * i:3 * i + 3])) for i in range(2)]
    L.labels[argpos[0]], L.labels[argpos[1]] = 9, 10
    w.banks = [e.alloc(16, b"\xa5" * 16) for _ in range(3)]
    for i, addr in enumerate(w.banks):
        L.labels[addr] = 11 + i
    w.streamer = e.alloc(0x40, b"\xa5" * 0x40)
    L.labels[w.streamer] = 7
    FILE = 0x7C0FF000
    L.labels[FILE] = 14
    e.write(S_MGR, pack([w.manager if c["mgr"] else 0]))
    e.write(S_SEED, pack([c["seed"]]))
    e.write(S_GUARD, b"\0")
    e.write(S_INVLN2, pack([0]))
    # the placeholder
    obj = e.alloc(0x80, b"\xa5" * 0x80)
    w.child = 0
    if op == K_CTOR:
        pass
    else:
        T.call_ppc(e, ADDR[K_CTOR], [obj])
        extra = preset_holder(e, L, w, obj, st, c)
        e.write(obj, pack([holder_table(e, c["hook"], False)]))
        if st[S_CHILD] and extra:
            child = e.alloc(0x80, b"\xa5" * 0x80)
            T.call_ppc(e, ADDR[K_CTOR], [child])
            cc = c["child"]
            e.write(child, pack([holder_table(e, 0, True)]))
            e.write(child + 0x3C, pack([cc[0]]))
            e.write(child + 0x10, pack(cc[1:4]))
            e.write(child + 0x4C, pack([cc[4]]))
            e.write(child + 0x38, pack([cc[5]]))
            L.labels[child] = 6
            e.write(extra + 0x18, pack([child]))
            w.child = child
        L.labels[obj] = 1
    # the sample info
    info = e.alloc(0xA0, b"\xa5" * 0xA0)
    T.call_ppc(e, 0x100158BC, [info])
    if c["info"][2]:
        e.write(info + 0xC, pack([c["info"][0]]))
        e.write(info + 0x14, pack([c["info"][1]]))
    L.labels[info] = 5
    block = e.alloc(256, b"\xa5" * 256)
    # the call
    del L.log[:]
    L.track = True
    result, second = 0, 0
    dump_holder_flag = True
    buf = e.alloc(4, pack([FILL]))
    extra_dump = (0,) * 8
    f = lambda bits: T.F(bits_f(bits))
    call = T.call_ppc
    A = ADDR[op]
    if op == K_SETMGR:
        call(e, A, [a[0] if a[0] != 1 else w.manager])
    elif op == K_GETBASE:
        result = call(e, A, [obj + 0x2C])[0] & MASK
    elif op == K_CTOR:
        call(e, A, [obj])
        result = int(e.u32(obj) == REAL_VT and e.read(obj + 0x60, 0x20) == b"\xa5" * 0x20)
    elif op == K_PCTOR:
        call(e, A, [obj + 0x2C])
    elif op == K_DTOR:
        # as `delete p` or an explicit call: through the vtable's destructor slot (a hooked slot records, then runs the real one)
        call(e, e.u32(e.u32(holder_table(e, c["hook"], False) + 0x10)), [obj, a[0]])
    elif op == K_CHDIST:
        call(e, A, [obj])
        result = f1_bits(e)
    elif op in (K_SETDIST, K_SETBASE):
        call(e, A, [obj + 0x2C, a[0]])
    elif op == K_SETPV:
        call(e, A, [obj, a[0]])
    elif op == K_P2W:
        result = call(e, A, [obj + 0x2C])[0] & MASK
    elif op == K_PASSIGN:
        call(e, A, [obj + 0x2C, a[0]])
    elif op == K_UPDPARAM:
        call(e, A, [obj, a[0], a[1]])
    elif op == K_ALLOCX:
        call(e, A, [obj])
    elif op == K_XCTOR:
        xb = e.alloc(0x40, b"\xa5" * 0x40)
        call(e, A, [xb])
        extra_dump = (struct.unpack("<I", e.read(xb, 4))[0], struct.unpack("<I", e.read(xb + 4, 4))[0], e.u32(xb + 8), e.u32(xb + 0xC),
                      e.u32(xb + 0x10), e.u32(xb + 0x14), e.u32(xb + 0x18), e.u32(xb + 0x1C))
        # the host's pointer members are eight bytes: the Mac's two pointer words must read as the filler or zero
        result = int(e.read(xb + 0x20, 0x20) == b"\xa5" * 0x20)
        dump_holder_flag = False
    elif op == K_UPDDEP:
        call(e, A, [obj])
    elif op == K_UPDATE:
        call(e, A, [buf, obj, argpos[0], argpos[1]])
        result = T.s32(e.u32(buf))
    elif op == K_COMPLETED:
        call(e, A, [obj, a[0], a[1], a[2]])
    elif op == K_RELEASE:
        call(e, A, [obj])
    elif op == K_SETFLAGS:
        call(e, A, [obj, f(a[0])])
    elif op == K_INITCTRL:
        call(e, A, [obj])
    elif op == K_INIT:
        call(e, A, [buf, obj, w.container if a[0] == 4 else a[0], w.amap.get(a[1], 0), argpos[0], f(a[2])])
        result = T.s32(e.u32(buf))
    elif op == K_GETPARAM:
        out = e.alloc(4, pack([FILL]))
        result = call(e, A, [obj, a[0], out])[0] & 0xFF
        second = e.u32(out)
    elif op in (K_RVOL, K_RPITCH, K_PSEUDO, K_GETVOL):
        result = call(e, A, [obj])[0] & MASK
    elif op == K_FILLBASIC:
        result = call(e, A, [obj, info])[0] & 0xFF
    elif op == K_FILL:
        result = call(e, A, [obj, a[0], info])[0] & 0xFF
    elif op == K_MAXDIST:
        call(e, A, [obj])
        result = f1_bits(e)
    elif op == K_CHSAMPLE or op == K_CHSOUND:
        r3 = call(e, A, [obj, a[0]])[0]
        result = lbl(L, r3) if op == K_CHSAMPLE else r3 & 0xFF
    elif op in (K_NEXTSAMPLE, K_NEXTSOUND):
        cell = e.alloc(4, pack([w.amap.get(a[2], 0)]))
        call(e, A, [obj, a[0], a[1], cell])
        result = lbl(L, e.u32(cell))
    elif op == K_VCON:
        result = call(e, A, [obj])[0] & 0xFF
    elif op == K_SETVOL:
        call(e, A, [obj, a[0]])
    elif op == K_SETVF:
        call(e, A, [obj, a[0], f(a[1])])
    elif op == K_CONVF:
        result = call(e, A, [obj, f(a[0])])[0] & MASK
    elif op == K_SETVP:
        call(e, A, [obj, a[0], a[1]])
    elif op == K_PROCESS:
        result = call(e, A, [obj])[0] & MASK
    elif op in (K_INITFADE, K_OOR, K_MODIFY):
        call(e, A, [obj])
    elif op in (K_STOP, K_PAUSE, K_RESTART):
        call(e, A, [buf, obj])
        result = T.s32(e.u32(buf))
    elif op == K_FINISHED:
        result = call(e, A, [obj, a[0]])[0] & 0xFF
    elif op == K_SAVE:
        call(e, A, [buf, obj, FILE, a[0]])
        result = T.s32(e.u32(buf))
    elif op == K_WMAGIC:
        call(e, A, [obj, FILE])
    elif op == K_CLASSID:
        result = call(e, A, [obj])[0] & MASK
    elif op == K_LOAD:
        call(e, A, [buf, obj, FILE, a[0]])
        result = T.s32(e.u32(buf))
    elif op == K_RMAGIC:
        result = call(e, A, [obj, FILE])[0] & 0xFF
    elif op == K_PLAY:
        call(e, A, [buf, obj, a[0]])
        result = T.s32(e.u32(buf))
    elif op == K_GETHANDLE:
        result = call(e, A, [obj])[0] & MASK
    elif op == K_SETHANDLE:
        result = call(e, A, [obj, a[0]])[0] & MASK
    elif op == K_SOUNDCB:
        call(e, A, [obj, a[0], a[1]])
    elif op == K_CBCTOR:
        call(e, A, [block])
        result = int(e.u32(block) == CB_VT and e.read(block + 4, 0x20) == b"\xa5" * 0x20)
        dump_holder_flag = False
    elif op == K_BASECOMPLETED:
        call(e, A, [block, a[0], a[1], a[2]])
        dump_holder_flag = False
    L.track = False
    state = dump_holder(e, L, obj) if dump_holder_flag else (0,) * 34
    catcount = e.u32(w.container + 4)
    sd = []
    for i in range(8):
        if i < len(sounds):
            base = sounds[i]
            sd += [struct.unpack("<I", e.read(base + 0xC, 4))[0], struct.unpack(">H", e.read(base + 0x10, 2))[0] | (struct.unpack(">H", e.read(base + 0x12, 2))[0] << 16)]
        else:
            sd += [0, 0]
    return dict(r=result & MASK, r2=second & MASK, st=state, cat=catcount, info=dump_info(e, L, info), file=w.fileout, data=tuple(sd),
                statics=(lbl(L, e.u32(S_MGR)), e.u32(S_SEED)), extra=extra_dump, log=list(L.log))


def host_run(lib, c):
    p = (C.c_uint32 * 512)(*params(c))
    out = (C.c_uint32 * 512)()
    CL.host_begin(lib, {}, c["q"])
    lib.tpw_ap_run(c["op"], p, c["filein"] if c["filein"] else b"", out)
    log = CL.host_end(lib)
    n = out[O_FILE]
    raw = b"".join(struct.pack("<I", out[O_FILE + 1 + i]) for i in range(120))
    return dict(r=out[O_R] & MASK, r2=out[O_R2], st=tuple(out[O_STATE:O_STATE + NSTATE]), cat=out[O_CAT], info=tuple(out[O_INFO:O_INFO + 24]),
                file=raw[:n], data=tuple(out[O_DATA:O_DATA + 16]), statics=(out[O_STATIC], out[O_STATIC + 1]), extra=tuple(out[O_EXTRA:O_EXTRA + 8]),
                log=log)


TESTS = []


# ---------------------------------------------------------------- randomisation

SPECIAL_FLOATS = [0.0, -0.0, 1.0, -1.0, 0.5, 2.0, 2.1, 3.0, 96.0, 127.0, 1e20, -1e20, 1e-30, 3.4e38, -3.4e38, math.inf, -math.inf, math.nan]
EVENT_FLAGS = [0, 4, 8, 0x200, 0x4000, 0x204, 0x4008, 0x420C, 0xFFFF, 0x10, 0x0100, 0x8000]


def safe_bits(b):
    """A float bit pattern without signalling NaNs (the emulator may quiet them)."""
    b &= MASK
    if (b & 0x7F800000) == 0x7F800000 and (b & 0x007FFFFF):
        return 0x7FC00000
    return b


def rbits(r):
    k = r.random()
    if k < 0.45:
        return fbits(r.uniform(-1000.0, 1000.0))
    if k < 0.75:
        return fbits(r.choice(SPECIAL_FLOATS))
    return safe_bits(r.getrandbits(32))


def rsane(r, lo=-1000.0, hi=1000.0):
    return fbits(r.uniform(lo, hi))


def rpos(r, sane=False):
    return [rsane(r) if sane else rbits(r) for _ in range(3)]


def rword(r):
    return r.choice([0, 1, 2, 0xFF, 0x100, 0xFFFF, 0x10000, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, r.getrandbits(32), r.getrandbits(32), r.getrandbits(16)])


def rerr(r):
    return r.choice(QUEUE_ERRORS)


def rlength(r):
    return r.choice([0, 1, 250, 999, 1000, 1001, 1500, 2000, 5000, 60000, r.randrange(0, 1 << 20), r.randrange(0, 4000)])


def rand_sound(r, samples_max=4, zero_samples=False):
    n = r.randrange(0 if zero_samples else 1, samples_max + 1)
    acc, samples = 0, []
    for _ in range(n):
        acc += r.randrange(0, 0x6000)
        samples.append(make_sample(f00=r.randrange(0, 60), f04=acc if r.random() < 0.85 else r.getrandbits(32), f08=rlength(r), bank=r.randrange(0, 4)))
    vmin, vmax = r.choice([(r.randrange(256), r.randrange(256)), (50, 100), (100, 50), (0, 0), (255, 0), (7, 7), (0, 255), (r.randrange(256), r.randrange(256))])
    pmin, pmax = r.choice([(r.randrange(-128, 128), r.randrange(-128, 128)), (-10, 10), (10, -10), (0, 0), (127, -128), (-128, 127), (5, 5)])
    f10, f12 = r.choice([(0, 0), (0, 5), (5, 0), (10, 3), (3, 10), (0xFFFF, 0), (r.getrandbits(16), r.getrandbits(16)), (7, 7), (r.randrange(0, 2000), r.randrange(0, 2000))])
    return make_sound(samples, hi=r.choice([0, 0, 0, 0, 0, 1, 0xFFFF, r.getrandbits(16)]), vmin=vmin, vmax=vmax, pmin=pmin, pmax=pmax, f10=f10, f12=f12, f16=r.choice([0, 1, 0x101, r.getrandbits(16)]),
                      f18=r.choice([0, 1, 2, 4, 3, 6, 7, r.getrandbits(16)]), f1a=r.choice([0, 2, 0x202, r.getrandbits(16)]),
                      f1c=r.choice([0, 1, 2, 4, 3, 6, 7, r.getrandbits(16)]), f1e=r.choice([0, 1, 100, 0x8000, 0xFFFF, r.randrange(0, 0x6000), r.getrandbits(32)]),
                      f22=r.choice([0, 0x41200000, fbits(r.uniform(0.0, 300.0)), safe_bits(r.getrandbits(32))]))


def rand_events(r, max_events=2, zero_samples=False):
    events = []
    for _ in range(r.randrange(1, max_events + 1)):
        sounds = [rand_sound(r, zero_samples=zero_samples) for _ in range(r.randrange(1, 5))]
        events.append(make_event(sounds, id=r.randrange(0, 100), f0c=r.choice([0, 5, 0x100, 0xFFFF, 0x10000, 0x12345, r.getrandbits(32)]),
                                 flags=r.choice(EVENT_FLAGS + [r.getrandbits(16)]), f12=r.choice([0, 0, 1, 5, 0x1FF, r.getrandbits(16)])))
    return events


def rand_state(r, events, event=True, sound=True, sample=True, extra=None, handle=None, flags=None):
    """A consistent random preset: the event, one of its sounds and one of that sound's samples (each may be left out)."""
    st = default_state()
    ei = r.randrange(len(events))
    si = r.randrange(len(events[ei]["sounds"]))
    sd = events[ei]["sounds"][si]
    mi = r.randrange(len(sd["samples"])) if sd["samples"] else None
    st[S_EVENT] = 21 + ei if event else 0
    st[S_SOUND] = sound_label(events, ei, si) if sound else 0
    st[S_SAMPLE] = sample_label(events, ei, si, mi) if sample and mi is not None else 0
    if not event and r.random() < 0.5:
        st[S_EVENT] = 0
    st[S_POSX], st[S_POSY], st[S_POSZ] = rpos(r, sane=r.random() < 0.8)
    st[S_VELX], st[S_VELY], st[S_VELZ] = rpos(r, sane=r.random() < 0.8)
    st[S_ENDTIME] = r.choice([0, 250, 2250, r.randrange(0, 100000), r.getrandbits(32), 0xFFFFFF00])
    st[S_PRIORITY] = rword(r)
    st[S_HANDLE] = handle if handle is not None else r.choice([0, 0, 1, 0x1234, 0xFFFFFFFF, r.getrandbits(32)])
    st[S_CATAGORY] = r.randrange(2)
    st[S_LISTHANDLE] = r.choice([0, 1, 0x10001, 0x5, 0xFFFF0003, r.getrandbits(32)])
    st[S_FLAGS] = flags if flags is not None else r.choice([0, r.getrandbits(21), r.getrandbits(21) & r.getrandbits(21), r.choice(ALL_FLAGS), r.getrandbits(32)])
    st[S_PAUSETIME] = r.choice([0, 0, 1, 1000, r.randrange(0, 100000), r.getrandbits(32)])
    st[S_MAXDIST] = r.choice([0, rbits(r), fbits(r.uniform(1.0, 200.0))])
    st[S_LASTSOUND] = r.choice([0xFF, 0, 1, 2, 3, 0x7F, 0x80, r.getrandbits(8)])
    st[S_LASTSAMPLE] = r.choice([0xFF, 0, 1, 2, 3, 0x7F, 0x80, r.getrandbits(8)])
    st[S_VCINDEX] = r.choice([0, 1, 2, 3, 4, 5, 8, 0xFF, r.getrandbits(8)])
    st[S_FADE] = r.choice([0, ONE, fbits(r.uniform(0.0, 1.2)), fbits(0.95), fbits(0.9), rbits(r)])
    st[S_VOLUME] = r.choice([0, 1, 100, 127, 0xFFFF, r.getrandbits(16)])
    st[S_PITCH] = r.choice([0, 1, 0xFFFF, 0x7FFF, 0x8000, r.getrandbits(16)])
    st[S_EXTRA] = r.randrange(2) if extra is None else int(extra)
    if st[S_EXTRA]:
        st[S_IDS] = r.choice([0, 0x01010100, 0x03020100, r.getrandbits(32), 0x00020100])
        st[S_VALUES] = r.choice([0, 0x01020304, r.getrandbits(32), 0x00506070])
        st[S_LEFT], st[S_TOP], st[S_RIGHT], st[S_BOTTOM] = [r.choice([0, 5, -5, 100, -100, 1 << 20, -(1 << 20), r.randrange(-300, 300)]) & MASK for _ in range(4)]
        st[S_PARENT] = r.choice([0, 0x1234, r.getrandbits(32)])
    return st


def rpick(r, values):
    """A Random() word whose high half is near one of the thresholds."""
    t = r.choice(list(values) + [v + 1 for v in values] + [max(v - 1, 0) for v in values] + [0, 0xFFFF, r.getrandbits(16)])
    return ((t & 0xFFFF) << 16) | r.getrandbits(16)


def queues_common(r):
    q = {
        "ListenerOn": [r.choice([0, 1, 1, 1])], "DistMul": [r.choice([ONE, ONE, 0x40000000, 0x3F000000, 0, rbits(r), rsane(r, 0.1, 5.0)]) for _ in range(3)],
        "QSound": [r.randrange(2)], "VCOn": [r.choice([0, 1, 1])], "Bank": [r.choice([0, 1, 2, 3, 1, 1])],
        "Rolloff": [rsane(r, -10.0, 10.0)], "SysErr": [rerr(r) for _ in range(3)], "Clock": [r.choice([0, 1000, r.randrange(0, 100000), 0xFFFFFF00, r.getrandbits(32)])],
        "RangedRandom": [r.randrange(0, 300) for _ in range(4)], "Random": [r.getrandbits(32) for _ in range(3)],
        "Ids": [r.choice([0, 1, 2, 3, 5, 0x7FFFFFFF, -1]) for _ in range(8)], "FixUp": [r.choice([0, 1, 2, 7])],
        "SoundPointer": [r.randrange(2)], "FileErr": [r.choice([0, 0, -1])],
        "V.Init": [rerr(r)], "V.ChooseRandomSound": [r.randrange(2)], "V.ChooseRandomSample": [r.choice([0, 0, 1, 2, 3])],
        "V.GetParameterValue": [r.randrange(2) for _ in range(3)], "V.GetParameterValue.out": [r.choice([0, 1, 50, 100, 255, 300, r.getrandbits(32)]) for _ in range(3)],
        "V.FillBasicSampleInfo": [r.randrange(2)], "V.GetPsuedoTime": [r.choice([0, 1, 500, -1, r.randrange(-100, 100000)])],
        "V.ChangeDistPriority": [r.choice([0, rsane(r, 0.0, 100000.0), rbits(r)])],
    }
    return q


ZERO_SAMPLE_OPS = {K_CHSAMPLE, K_NEXTSAMPLE, K_INIT, K_LOAD}


def base_case(r, op, **kw):
    events = rand_events(r, zero_samples=op in ZERO_SAMPLE_OPS)
    c = make_case(op, events=events, state=rand_state(r, events), hook=r.choice([0, 0, 0, r.getrandbits(10)]), cls=r.choice([0, 3, 4, 6, 7, 9, 12]),
                  childcls=r.choice([0, 4, 6, 7, 9, 11]), seed=r.choice([0, r.getrandbits(32)]), vc=r.randrange(0, 5), q=queues_common(r),
                  listener=rpos(r, sane=True), argpos=rpos(r, sane=r.random() < 0.8) + rpos(r, sane=r.random() < 0.8),
                  a=[rword(r) for _ in range(6)] + [0, 0], mgr=1 if r.random() < 0.96 else 0)
    if r.random() < 0.05:
        c["argpos"][3] = 0x7FC00000
    # now and then the event, the sound or the sample is missing (the operations that need one put it back)
    for index, p_null in ((S_EVENT, 0.05), (S_SOUND, 0.07), (S_SAMPLE, 0.08)):
        if r.random() < p_null:
            c["state"][index] = 0
    c.update(kw)
    c["q"] = {k: v if isinstance(v, list) else [v] for k, v in c["q"].items()}
    return c


def ensure_sound(c):
    st = c["state"]
    if not st[S_SOUND]:
        st[S_SOUND] = 31
    return c


def ensure_event(c):
    if not c["state"][S_EVENT]:
        c["state"][S_EVENT] = 21
    return c


def ensure_extra(c, r):
    st = c["state"]
    if not st[S_EXTRA]:
        st[S_EXTRA] = 1
        st[S_IDS] = r.choice([0x03020100, r.getrandbits(32)])
        st[S_VALUES] = r.getrandbits(32)
        st[S_LEFT], st[S_TOP], st[S_RIGHT], st[S_BOTTOM] = [r.randrange(-300, 300) & MASK for _ in range(4)]
    return c


def ensure_mgr(c):
    c["mgr"] = 1
    return c


# ---------------------------------------------------------------- per-operation random cases

def gen(op):
    """The random case builder of one operation."""
    def make(r):
        c = base_case(r, op)
        a = c["a"]
        st = c["state"]
        events = c["events"]
        if op in (K_SETMGR,):
            a[0] = r.choice([0, 1, 0x1234560, rword(r)])
        elif op in (K_GETBASE, K_P2W, K_PCTOR):
            pass
        elif op in (K_SETDIST, K_SETBASE):
            a[0] = r.choice([0, 1, 0xFFFF, 0x10000, 0x12345, rword(r)])
        elif op == K_PASSIGN:
            a[0] = rword(r)
        elif op in (K_DTOR, K_RELEASE):
            c["hook"] |= r.choice([0, H_DTOR])
            a[0] = r.choice([0, 1, 2, 0xFFFFFFFF, 0x7FFF, 0x8000, 0xFFFF, 0x10000, 0x10001, 0xFFFF8000])
        elif op in (K_CHDIST, K_PROCESS):
            ensure_event(c)
            ensure_mgr(c)
            if op == K_PROCESS:
                if st[S_FLAGS] & F_BOUNDS or r.random() < 0.3:
                    ensure_extra(c, r)
                    c["q"]["ListenerOn"] = [1]
                if r.random() < 0.7:
                    st[S_FLAGS] &= ~F_FRESH
                if st[S_FLAGS] & F_BOUNDS:
                    c["q"]["ListenerOn"] = [1]
        elif op == K_SETPV:
            ensure_mgr(c)
            a[0] = rword(r)
        elif op == K_UPDPARAM:
            ids = [(st[S_IDS] >> (8 * i)) & 0xFF for i in range(4)] if st[S_EXTRA] else [0]
            a[0] = r.choice(ids + [r.getrandbits(8), 0, 1, 2])
            a[1] = rword(r)
            c["hook"] |= r.choice([0, H_DEP])
        elif op == K_UPDDEP:
            ensure_sound(c)
            if st[S_EXTRA] == 0 and r.random() < 0.5:
                ensure_extra(c, r)
            c["hook"] &= ~(H_DEP | H_PARAM)
            if r.random() < 0.4:
                # the parameters answer: percentages that scale a range far past 16 bits
                ensure_extra(c, r)
                c["hook"] |= H_PARAM
                c["q"]["V.GetParameterValue"] = [r.randrange(2) for _ in range(3)]
                c["q"]["V.GetParameterValue.out"] = [r.choice([0, 100, 1000, 0x40000000, 0x10000000, 0xFFFFFFFF, r.getrandbits(32)]) for _ in range(3)]
        elif op == K_UPDATE:
            ensure_mgr(c)
            if r.random() < 0.4:
                st[S_POSX], st[S_POSY], st[S_POSZ] = c["argpos"][:3]
            if r.random() < 0.4:
                st[S_VELX], st[S_VELY], st[S_VELZ] = c["argpos"][3:6]
        elif op in (K_COMPLETED, K_SOUNDCB, K_BASECOMPLETED):
            a[0] = r.choice([0, rword(r)])
            a[1] = r.choice([st[S_LISTHANDLE], st[S_LISTHANDLE], 0, rword(r), st[S_LISTHANDLE] ^ 1])
            a[2] = r.choice([0, 0, 1, 0xFFFFFFFF, 2, rword(r)])
        elif op == K_SETFLAGS:
            ensure_event(c)
            a[0] = r.choice([0, 0x80000000, rbits(r), fbits(r.uniform(1.0, 100.0))])
        elif op == K_INITCTRL:
            ensure_sound(c)
        elif op == K_INIT:
            ensure_mgr(c)
            a[0] = 4
            ei = r.randrange(len(events))
            a[1] = 21 + ei
            a[2] = r.choice([0, 0x80000000, rbits(r), fbits(r.uniform(1.0, 100.0))])
            c["hook"] &= ~H_INIT
            if st[S_SOUND] == 0 and c["hook"] & H_SOUND:
                c["q"]["V.ChooseRandomSound"] = [0]
            if st[S_SOUND] == 0 and r.random() < 0.5:
                c["hook"] &= ~H_SOUND
            if r.random() < 0.3:
                st[S_SOUND] = 0
                c["hook"] &= ~H_SOUND
                c["q"]["V.ChooseRandomSound"] = [0]
            # a hooked ChooseRandomSound that says yes needs a sound to initialise the controls from
            if (c["hook"] & H_SOUND) and c["q"]["V.ChooseRandomSound"][0] != 0:
                ensure_sound(c)
            st[S_SAMPLE] = st[S_SAMPLE] if r.random() < 0.5 else 0
        elif op == K_GETPARAM:
            ensure_sound(c)
            ensure_extra(c, r)
            sd = events[0]["sounds"][0]
            a[0] = r.choice([sd["f18"], sd["f1c"], sd["f18"] | sd["f1c"], 1, 2, 4, 3, 0xFFFF, 0, rword(r)])
        elif op in (K_RVOL, K_RPITCH, K_PSEUDO):
            if r.random() < 0.85 or op == K_PSEUDO:
                ensure_sound(c)
            if r.random() < 0.6:
                ensure_extra(c, r)
            c["hook"] &= ~H_PARAM
            c["hook"] |= r.choice([0, 0, H_PARAM])
            if c["hook"] & H_PARAM:
                ensure_extra(c, r)
        elif op == K_FILLBASIC:
            ensure_mgr(c)
            if not st[S_SAMPLE]:
                st[S_SAMPLE] = 71
            c["info"] = [r.choice([0, 0x200, 0x4, 0xC0, rword(r)]), r.choice([0, rword(r)]), r.randrange(2)]
        elif op == K_FILL:
            ensure_mgr(c)
            c["info"] = [r.choice([0, rword(r)]), r.choice([0, rword(r)]), r.randrange(3) == 0]
            if r.random() < 0.9 and not st[S_SAMPLE]:
                st[S_SAMPLE] = 71
            if r.random() < 0.9:
                ensure_sound(c)
            a[0] = r.choice([0, 1000, st[S_ENDTIME], (st[S_ENDTIME] + st[S_PAUSETIME]) & MASK, ((st[S_ENDTIME] + st[S_PAUSETIME]) & MASK) + 1,
                             r.getrandbits(32), r.randrange(0, 100000)])
            if r.random() < 0.5:
                # a time inside the sample's cycle
                length = events[0]["sounds"][0]["samples"][0]["f08"] if events[0]["sounds"][0]["samples"] else 1000
                a[0] = (st[S_ENDTIME] - (length + 250) + r.randrange(0, 3 * max(length, 1))) & MASK
            c["hook"] &= ~(H_PSEUDO | H_BASIC)
            c["hook"] |= r.choice([0, 0, H_PSEUDO]) | r.choice([0, 0, H_BASIC])
            if r.random() < 0.1:
                c["mgr"] = 0
            if r.random() < 0.25:
                # the distance edges: a maximum distance of exactly the minimum
                f22 = r.choice([fbits(2.0), fbits(1.0), fbits(4.0), 0, fbits(2.1), 0x40000001, 0x3FFFFFFF])
                for ev in events:
                    for sd in ev["sounds"]:
                        sd["f22"] = f22
                c["q"]["DistMul"] = [r.choice([ONE, fbits(2.0), fbits(0.5)])]
                st[S_FLAGS] &= ~F_MAXDIST
            if r.random() < 0.15:
                st[S_SOUND] = 0
            if r.random() < 0.12:
                st[S_SAMPLE] = 0
        elif op == K_GETVOL:
            ensure_sound(c)
            c["hook"] &= ~H_PARAM
            if r.random() < 0.4:
                ensure_extra(c, r)
                c["hook"] |= H_PARAM
                c["q"]["V.GetParameterValue"] = [1]
                c["q"]["V.GetParameterValue.out"] = [r.choice([0, 100, 1000, 0x40000000, 0x10000000, 0xFFFFFFFF, r.getrandbits(32)])]
        elif op == K_MAXDIST:
            ensure_mgr(c)
            if r.random() < 0.2:
                st[S_SOUND] = 0
        elif op in (K_CHSAMPLE, K_NEXTSAMPLE):
            a[0] = r.randrange(2)
            sums = [sm["f04"] for ev in events for sd in ev["sounds"] for sm in sd["samples"]]
            c["q"]["Random"] = [rpick(r, sums)]
            if op == K_NEXTSAMPLE:
                ensure_sound(c)
                nsm = len(events[0]["sounds"][0]["samples"])
                a[1] = r.choice([st[S_LASTSAMPLE], st[S_LASTSAMPLE], 0, 1, 2, 3, 0x7F, 0x80, 0xFF, 0xFE, r.getrandbits(8)])
                a[2] = r.choice([0, 71, 72, 73, 74])
                if r.random() < 0.7:
                    st[S_LASTSAMPLE] = a[1] & 0xFF
            c["hook"] &= ~H_SAMPLE
        elif op in (K_CHSOUND, K_NEXTSOUND):
            a[0] = r.randrange(2)
            sums = []
            for ev in events:
                acc = 0
                for sd in ev["sounds"]:
                    acc += sd["f1e"]
                    sums.append(acc & MASK)
            c["q"]["Random"] = [rpick(r, sums)]
            if op == K_NEXTSOUND:
                ensure_event(c)
                a[1] = r.choice([st[S_LASTSOUND], st[S_LASTSOUND], 0, 1, 2, 3, 0x7F, 0x80, 0xFF, 0xFE, r.getrandbits(8)])
                a[2] = r.choice([0, 31, 32, 33, 34])
                if r.random() < 0.7:
                    st[S_LASTSOUND] = a[1] & 0xFF
            elif r.random() < 0.1:
                st[S_EVENT] = 0
            c["hook"] &= ~(H_SOUND | H_PARAM)
        elif op == K_VCON:
            ensure_mgr(c)
            if r.random() < 0.1:
                st[S_EVENT] = 0
        elif op == K_SETVOL:
            pass
        elif op == K_SETVF:
            ensure_sound(c)
            a[0] = r.choice([0, 1, 100, 127, 255, 0xFFFF, 0x10000, 0x7FFFFFFF, 0xFFFFFFFF, rword(r)])
            a[1] = r.choice([ONE, 0, 0x80000000, 0x40000000, 0x3F000000, 0x42C80000, rbits(r), fbits(r.uniform(0.1, 4.0))])
        elif op == K_CONVF:
            a[0] = r.choice([ONE, 0, 0x80000000, 0x40000000, 0x3F000000, 0x43DC0000, fbits(440.0), 0x7F800000, 0xFF800000, 0x7FC00000, 1,
                             rbits(r), fbits(r.uniform(0.05, 8.0)), fbits(r.uniform(0.5, 2.0))])
        elif op == K_SETVP:
            a[1] = r.choice([0, 1, 0xFFFFFFFF, 0x8000, 0x7FFF, 0xFFFF, 0x10000, rword(r)])
        elif op == K_SETVOL:
            pass
        elif op in (K_INITFADE, K_OOR):
            ensure_mgr(c)
        elif op in (K_STOP, K_PAUSE, K_RESTART):
            ensure_mgr(c)
        elif op == K_FINISHED:
            a[0] = r.choice([(st[S_ENDTIME] + st[S_PAUSETIME]) & MASK, ((st[S_ENDTIME] + st[S_PAUSETIME]) & MASK) + 1,
                             ((st[S_ENDTIME] + st[S_PAUSETIME]) & MASK) - 1, 0, 0xFFFFFFFF, rword(r)])
        elif op == K_MODIFY:
            ensure_mgr(c)
            ensure_extra(c, r)
            c["q"]["ListenerOn"] = [1]
            c["listener"] = r.choice([rpos(r, sane=True), [fbits(float(v)) for v in (r.randrange(-300, 300), 0, r.randrange(-300, 300))], rpos(r)])
        elif op == K_SAVE:
            c["hook"] |= H_CLASS
            c["cls"] = r.choice([0, 3, 7, 9, 0xFFFFFFFF])
            if r.random() < 0.5 and st[S_EXTRA]:
                st[S_CHILD] = 1
            ensure_mgr(c)
            c["q"]["Ids"] = [r.choice([1, 2, 3, 0, -1, 5]) for _ in range(8)]
            a[0] = r.choice([0, 1, rword(r)])
        elif op == K_LOAD:
            c = gen_load(r, c)
        elif op == K_RMAGIC:
            c["filein"] = r.choice([bytes([0x91, 0xD5, 0x12, 0x00]), bytes([0x91, 0xD5, 0x12, 0x01]), bytes([0, 0, 0, 0]), b"", bytes([0x00, 0x12, 0xD5, 0x91]),
                                    bytes(r.getrandbits(8) for _ in range(4)), bytes([0x91, 0xD5, 0x12, 0x00]) + b"xyz"])
        elif op in (K_WMAGIC, K_CLASSID, K_PLAY, K_GETHANDLE, K_CBCTOR):
            c["hook"] &= ~H_CLASS if op == K_CLASSID else c["hook"]
            a[0] = rword(r)
        elif op == K_SETHANDLE:
            a[0] = rword(r)
        elif op == K_PCTOR:
            pass
        elif op == K_UPDDEP:
            pass
        elif op == K_ALLOCX or op == K_XCTOR or op == K_CTOR:
            pass
        elif op == K_SETVP:
            pass
        return c
    return make


# ---------------------------------------------------------------- Load: files

MAGIC_BYTES = struct.pack("<I", 0x12D591)


def le(x):
    return struct.pack("<I", x & MASK)


def block_bytes(pos, maxdist, ids, extra, bounded, handle, child_class, magics=(MAGIC_BYTES,) * 3):
    """One placeholder's saved block as Save writes it after the class id (and Load reads it)."""
    out = magics[0] + le(pos[0]) + le(pos[1]) + le(pos[2]) + le(0) + le(maxdist) + b"".join(le(i) for i in ids) + magics[1]
    if extra is None:
        out += le(0)
    else:
        out += le(1) + bytes(extra["ids"]) + bytes(extra["values"]) + le(extra["top"]) + le(extra["left"]) + le(extra["right"]) + le(extra["bottom"])
    out += magics[2] + bytes([bounded]) + le(handle) + le(child_class)
    return out


def gen_load(r, c):
    """A case for Load: a (mostly) good file for the world's tree, with queue answers that resolve or fail."""
    events = c["events"]
    ensure_mgr(c)
    st = c["state"]
    st[S_CHILD] = 0
    c["hook"] &= ~(H_INIT | H_SOUND | H_SAMPLE | H_CLASS)
    c["hook"] |= H_CLASS
    c["cls"] = r.choice([0, 3, 4, 6, 7, 9])

    def ids():
        ei = r.randrange(len(events))
        si = r.randrange(len(events[ei]["sounds"]))
        mi = r.randrange(max(len(events[ei]["sounds"][si]["samples"]), 1))
        return [r.choice([1, 2, 5, 0x7FFFFFFF]), ei, si, mi]

    def one(child_class, magics):
        extra = None
        if r.random() < 0.6:
            extra = dict(ids=bytes(r.getrandbits(8) for _ in range(4)), values=bytes(r.getrandbits(8) for _ in range(4)),
                         top=r.randrange(-300, 300), left=r.randrange(-300, 300), right=r.randrange(-300, 300), bottom=r.randrange(-300, 300))
        return block_bytes(rpos(r, sane=True), r.choice([0, rsane(r, 1.0, 100.0), rbits(r)]), ids(), extra, r.choice([0, 1, 2, 0xFF]), r.getrandbits(32),
                           child_class, magics)
    magics = [MAGIC_BYTES] * 3
    if r.random() < 0.25:
        magics[r.randrange(3)] = r.choice([le(0x12D590), le(0x12D591 ^ 0x80000000), le(0), bytes([0x00, 0x12, 0xD5, 0x91]), le(0x12D591)[:3] + b"\x01"])
    child_class = r.choice([0, 0, 7, 9])
    data = one(child_class, magics)
    if child_class:
        data += le(child_class) + one(r.choice([0, 0, 7, 9]) if r.random() < 0.1 else 0, [MAGIC_BYTES] * 3)
    if r.random() < 0.1:
        data = data[:r.randrange(0, len(data))]
    c["filein"] = data
    c["q"]["FixUp"] = [r.choice([0, 1, 1, 1, 2]), r.choice([0, 1, 1, 2])]
    c["q"]["SoundPointer"] = [r.choice([0, 1, 1, 1]), r.choice([0, 1, 1])]
    if r.random() < 0.1:
        c["a"][6] = 1
    # an Init that is hooked cannot reach a missing sound; the real Init needs the tree, which the file indexes
    if r.random() < 0.3:
        c["hook"] |= H_INIT
    c["a"][0] = r.choice([0, 1, rword(r)])
    return c


def edge_cases(op):
    """Hand-written cases for an operation (the random ones follow)."""
    r = __import__("random").Random(700 + op)
    cases = [gen(op)(__import__("random").Random(9000 + 31 * op + i)) for i in range(24)]
    W = lambda **kw: make_case(op, **kw)
    if op in (K_CONVF,):
        for b in [ONE, 0, 0x80000000, 0x40000000, 0x3F000000, 0x3E800000, 0x43DC0000, fbits(440.0), fbits(0.001), fbits(1000.0), 0x7F800000, 0xFF800000,
                  0x7FC00000, 1, 0x00800000, 0x3F800001, 0x3F7FFFFF, 0xBF800000, fbits(1.0594631), fbits(0.9438743), fbits(1.5), fbits(0.75)]:
            cases.append(W(a=[b] + [0] * 7))
    elif op == K_SETVF:
        for vol in (0, 1, 127, 128, 0xFFFF, 0x10000, 0xFFFFFFFF):
            for sd in (make_sound(vmin=0), make_sound(vmin=100), make_sound(vmin=127), make_sound(vmin=128), make_sound(vmin=255)):
                for b in (ONE, fbits(2.0), fbits(0.5)):
                    cases.append(W(events=[make_event([sd])], a=[vol, b, 0, 0, 0, 0, 0, 0]))
    elif op in (K_DTOR, K_RELEASE):
        for flag in (0, 1, 2, 0xFFFFFFFF, 0x7FFF, 0x8000, 0xFFFF, 0x10000, 0x10001):
            for extra in (0, 1):
                for cat in (0, 1):
                    for hook in (0, H_DTOR):
                        st = default_state()
                        st[S_EXTRA], st[S_CATAGORY], st[S_LISTHANDLE] = extra, cat, 0x12345678
                        cases.append(W(state=st, a=[flag] + [0] * 7, hook=hook))
    elif op == K_COMPLETED:
        for flags in (0, F_S40, F_VCOFF, F_OUTRANGE, F_PLAYING | F_S40, F_PLAYING, F_PLAYING | F_FINISHED | F_VCOFF | F_OUTRANGE, 0xFFFFFFFF, F_FINISHED):
            for reason in (0, 1, 0xFFFFFFFF):
                for same in (1, 0):
                    st = default_state()
                    st[S_FLAGS], st[S_LISTHANDLE], st[S_HANDLE] = flags, 0x10005, 0x77
                    cases.append(W(state=st, a=[0, 0x10005 if same else 0x10006, reason, 0, 0, 0, 0, 0]))
    elif op == K_VCON:
        for on in (0, 1):
            for flags in (0, F_VCOFF, F_PLAYING, F_PLAYING | F_VCOFF, 0xFFFFFFFF):
                for ef in (0, 4, 8, 0xC, 0x200, 0x4000):
                    for handle in (0, 5):
                        for event in (0, 21):
                            st = default_state()
                            st[S_FLAGS], st[S_HANDLE], st[S_EVENT] = flags, handle, event
                            cases.append(W(events=[make_event(flags=ef)], state=st, q={"VCOn": [on], "SysErr": [0]}))
    elif op == K_FINISHED:
        for flags in (0, F_LOOPING, F_PLAYING, F_PLAYING | F_LOOPING):
            for end, pause in ((1000, 0), (1000, 500), (0xFFFFFF00, 0x200), (0, 0), (5, 0xFFFFFFFF)):
                for dnow in (-1, 0, 1, 2):
                    st = default_state()
                    st[S_FLAGS], st[S_ENDTIME], st[S_PAUSETIME] = flags, end, pause
                    cases.append(W(state=st, a=[((end + pause) + dnow) & MASK] + [0] * 7))
    elif op in (K_CHSOUND, K_CHSAMPLE):
        for weights in ([100, 200, 300], [0, 0, 0], [0xFFFF], [10, 10, 10, 10], [0xFFFF, 0xFFFF], [1, 2]):
            for pick in (0, 99, 100, 101, 199, 200, 201, 299, 300, 301, 0xFFFF, 10, 20, 30, 40, 1, 2, 3):
                for rotate in (0, 1):
                    if op == K_CHSOUND:
                        sounds = [make_sound(f1e=wt) for wt in weights]
                        events = [make_event(sounds)]
                    else:
                        samples = []
                        acc = 0
                        for wt in weights:
                            acc += wt
                            samples.append(make_sample(f00=len(samples) + 1, f04=acc))
                        events = [make_event([make_sound(samples)])]
                    st = default_state()
                    st[S_LASTSOUND] = st[S_LASTSAMPLE] = r.choice([0xFF, 0, 1, 2])
                    cases.append(W(events=events, state=st, a=[rotate] + [0] * 7, q={"Random": [(pick << 16) | 0x1234]}))
    elif op in (K_NEXTSOUND, K_NEXTSAMPLE):
        for count in (0, 1, 2, 3, 4):
            for idx in (0, 1, 2, 3, 0x7F, 0x80, 0xFF, 0xFE):
                for last in (idx, (idx + 1) & 0xFF, 0xFF):
                    for rotate in (0, 1):
                        if op == K_NEXTSOUND:
                            events = [make_event([make_sound() for _ in range(max(count, 1))])]
                            events[0]["sounds"] = events[0]["sounds"][:4]
                        else:
                            events = [make_event([make_sound([make_sample(f00=i + 1) for i in range(count)])])]
                        st = default_state()
                        st[S_LASTSOUND] = st[S_LASTSAMPLE] = last
                        cases.append(W(events=events, state=st, a=[rotate, idx, 31 if op == K_NEXTSOUND else 71, 0, 0, 0, 0, 0]))
    elif op in (K_RVOL, K_RPITCH):
        for lo, hi in ((0, 0), (10, 20), (20, 10), (255, 0), (0, 255), (7, 8), (8, 7), (100, 100), (127, 128), (128, 127)):
            for extra in (0, 1):
                for hook in (0, H_PARAM):
                    for out in (0, 50, 100, 255, 1000, 0xFFFFFFFF):
                        sd = make_sound(vmin=lo, vmax=hi, pmin=lo - 128 if op == K_RPITCH else 0, pmax=hi - 128 if op == K_RPITCH else 0)
                        st = default_state()
                        st[S_EXTRA] = extra
                        st[S_VALUES] = 0x00506000
                        cases.append(W(events=[make_event([sd])], state=st, hook=hook,
                                       q={"V.GetParameterValue": [1], "V.GetParameterValue.out": [out], "RangedRandom": [3]}))
    elif op == K_PSEUDO:
        for lo, hi in ((0, 0), (0, 5), (5, 0), (10, 20), (20, 10), (7, 7), (0xFFFF, 0), (0, 0xFFFF), (1, 0xFFFF), (0xFFFF, 1)):
            for extra in (0, 1):
                for hook in (0, H_PARAM):
                    for out in (0, 50, 100, 255, 0x1000000, 0xFFFFFFFF):
                        st = default_state()
                        st[S_EXTRA] = extra
                        cases.append(W(events=[make_event([make_sound(f10=lo, f12=hi)])], state=st, hook=hook,
                                       q={"V.GetParameterValue": [1], "V.GetParameterValue.out": [out], "RangedRandom": [3]}))
    elif op == K_FILL:
        for length in (0, 500, 1000, 1001, 1500, 2000, 2500, 3000):
            for elapsed in (0, 1, 500, 999, 1000, 1001, 1250, 1499, 1500, 1501, 1999, 2000, 2001, 2999, 3001, 5000):
                for flags in (0, F_LOOPING, F_NONPOS, F_LOOPFOREVER | F_PITCHSET):
                    st = default_state()
                    st[S_FLAGS], st[S_ENDTIME], st[S_PAUSETIME] = flags, 100000, 0
                    sd = make_sound([make_sample(f08=length)])
                    cases.append(W(events=[make_event([sd])], state=st, a=[(100000 - length - 250 + elapsed) & MASK] + [0] * 7,
                                   q={"Bank": [1], "Rolloff": [fbits(0.25)], "DistMul": [0x40000000], "QSound": [1]}, listener=[0, 0, 0]))
        # the maximum distance against the minimum of 2.0: below, equal and above, with and without the event's own distance
        for f22 in (fbits(1.0), fbits(2.0), 0x40000001, fbits(2.1), fbits(0.0)):
            for mult in (ONE, fbits(2.0), fbits(0.5), 0):
                for maxflag in (0, F_MAXDIST):
                    st = default_state()
                    st[S_FLAGS], st[S_ENDTIME], st[S_MAXDIST] = maxflag, 100000, fbits(2.0)
                    sd = make_sound([make_sample(f08=5000)], f22=f22)
                    cases.append(W(events=[make_event([sd])], state=st, a=[100000 - 5250 + 10] + [0] * 7,
                                   q={"Bank": [1], "Rolloff": [fbits(0.25)], "DistMul": [mult, mult], "QSound": [0]}))
    elif op in (K_SAVE,):
        for extra in (0, 1):
            for child in (0, 1):
                for cls in (0, 3, 7):
                    for bounded in (0, F_BOUNDS):
                        st = default_state()
                        st[S_EXTRA], st[S_CHILD], st[S_FLAGS], st[S_LISTHANDLE] = extra, child and extra, bounded, 0x20003
                        cases.append(W(state=st, hook=H_CLASS, cls=cls, childcls=r.choice([7, 9]), q={"Ids": [1, 2, 3, 4, 5, 6, 7, 8]}))
        cases.append(W(hook=H_CLASS, cls=3, q={"Ids": [0, 2, 3, 4]}))
    elif op == K_LOAD:
        events = [make_event([make_sound([make_sample(), make_sample(f00=2)]), make_sound()]), make_event()]
        good = block_bytes([ONE, 0x40000000, 0x40400000], fbits(5.0), [1, 0, 1, 0], None, 0, 0x10003, 0)
        for fix in (0, 1):
            for ptr in (0, 1):
                for mags in range(4):
                    magics = [MAGIC_BYTES] * 3
                    if mags:
                        magics[mags - 1] = le(0x12D590)
                    data = block_bytes([ONE, 0x40000000, 0x40400000], fbits(5.0), [1, 0, 1, 0], None, 0, 0x10003, 0, magics) if False else block_bytes(
                        [ONE, 0x40000000, 0x40400000], fbits(5.0), [1, 0, 1, 0], dict(ids=b"\x01\x02\x03\x04", values=b"\x05\x06\x07\x08", top=1, left=2, right=3, bottom=4), 1, 3, 0, magics)
                    cases.append(W(events=events, hook=H_CLASS, cls=3, filein=data, q={"FixUp": [fix], "SoundPointer": [ptr], "Random": [0x00010000]}, a=[0, 0, 0, 0, 0, 0, 0, 0]))
        cases.append(W(events=events, hook=H_CLASS, cls=4, filein=b"", q={"FixUp": [1]}))
    return cases


for _op in range(57):
    if _op not in NAME:
        continue
    TESTS.append(FnTest("%s 0x%08x" % (NAME[_op], ADDR[_op]), ppc_run, host_run, edge=(lambda op=_op: edge_cases(op)), rnd=gen(_op)))
