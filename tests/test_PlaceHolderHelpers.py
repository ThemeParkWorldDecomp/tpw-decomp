"""Unit w79g4, the placeholder helpers, against libtpw_unit_PlaceHolderHelpers.dylib (tests/unit_PlaceHolderHelpers.cpp), in the
program "sound symbols shared" (Code at 0x10000000, TOC base r2 = 0x1003abf0):
  src/PlaceHolderHelpers.cpp  CMapReference: constructor 0x1001af5c, GetSoundIds 0x1001f0dc, GetSoundPointer 0x1001f364;
      CPlaceHolderOneShotBranchingSentenceElement: constructor 0x10023d38, ChooseRandomSound 0x10023f04, destructor 0x10024114,
      GetClassID 0x100241c0; CPlaceHolderOneShotSentenceElement: destructor 0x10023dc0, constructor 0x10024218, Init 0x10024328,
      FillSampleInfo 0x10024438, Update 0x10024724, GetParameterValue 0x10024814, GetClassID 0x100248c0; CPlaceHolderOneShotThrowaway:
      destructor 0x10023e64, constructor 0x100242b4, FillSampleInfo 0x100267e8, GetClassID 0x10026898
  include/TbMapStreamer.h     sound::TbAudioEvent: constructor 0x1001ee64, GetType 0x1001c8a0, GetCategory 0x10017238, GetEvent
      0x1001eef4, GetPosition 0x1001ef2c (inline)
  include/PlaceHolderHelpers.h  TbAudioEventList::ListItem::ListItem 0x1001ece4 and the CAutoArrayPtr<ListItem> instance: operator
      T* 0x1001eaec, destructor 0x1001eb5c, counted constructor 0x1001ec10 (the template of include/snd_containers.h)
  CCatagoryContainer's six functions (0x1001f410 .. 0x1001fc30) were already translated by unit w70 (src/snd_containers.cpp) and are not
  part of this module.

Each function runs as the original in the emulator (at its address, the hidden TbError buffer is not needed: none of them returns one)
and as the host's translation, on a world built from the case. The base class CAudioPlaceHolder is REAL on both sides (the original's
code in the emulator, src/AudioPlaceHolder.cpp on the host), so the Element classes run over the real base functions and the real vtable
slots (the PowerPC objects keep their real Mac vtables; a wrong override would call a wrong slot). The world: a placeholder subject
(made by the real constructor of its class, then a preset state of 34 words written into the members), a parent placeholder (the real
base constructor and a preset: the sentence the Element belongs to), a tree of events, sounds, samples and branches (one block: Mac
layout with the pointer fields relocated, in the emulator's memory and in the host's arena), a category container whose AddRef and
Release record, a sound manager that is a recording fake (its getters, its Rolloff and SystemCommand virtuals; a command handed to
SystemCommand is decoded and recorded), bfl::maths::Random and RangedRandomSlow (answers from queues, the seed they were given is
recorded), the clock, TbMapStreamer::Instance (a block of bytes holding the category table for the CMapReference operations) and the
sample info. CPlaceHolderOneShot's constructor (unit w79g3) is the real one on the PowerPC side and the base constructor on the host.
The call logs, the answer, the state of the subject and the parent (all members, the extra data, the container's reference count,
the sample info, the sound data the swaps change and the two statics) are compared. NaN floats compare as one value.

Labels: 1 the subject, 2 the manager, 3 the volume control, 4 the category container, 5 the sample info, 6 the parent placeholder (in
the map reference world: the holder), 7 the streamer, 9 the array of a CAutoArrayPtr and 10 its holder, 11..13 the banks, 15 the
TbAudioEvent, 16 the category data, 17 the subject's extra data, 18 and 19 the parent's position and velocity, 21.. the events, 31..
the sounds (flat), 40.. the containers of the map reference world, 71.. the samples (flat).
"""
import ctypes as C
import math
import random
import struct

import calllog as CL
import ppcemu
import tpwtest as T
import test_AudioPlaceHolder as AP
from tpwtest import FnTest
from unicorn import UC_HOOK_CODE
from unicorn import ppc_const as P
from test_AudioPlaceHolder import (MASK, FILL, ONE, NSTATE, S_EVENT, S_SOUND, S_SAMPLE, S_POSX, S_POSY, S_POSZ, S_VELX, S_VELY, S_VELZ, S_ENDTIME,
                                   S_PRIORITY, S_HANDLE, S_CATAGORY, S_LISTHANDLE, S_FLAGS, S_PAUSETIME, S_MAXDIST, S_LASTSOUND, S_LASTSAMPLE,
                                   S_VCINDEX, S_FADE, S_VOLUME, S_PITCH, S_EXTRA, S_IDS, S_VALUES, S_LEFT, S_TOP, S_RIGHT, S_BOTTOM, S_CHILD,
                                   S_PARENT, F_PLAYING, F_LOOPING, F_LOOPFOREVER, F_NONPOS, F_FINISHED, F_S20, F_S40, F_MAXDIST, F_PROCVIRT,
                                   F_USERID, F_FIXEDVOL, F_OUTRANGE, F_BOUNDS, F_PAUSED, F_VCOFF, F_VOLSET, F_PITCHSET, F_EVENTMODE, F_FADING,
                                   F_FRESH, ALL_FLAGS, fbits, bits_f, canon, lbl, pack, f_bits, make_sample, make_sound, make_event,
                                   rand_events, rand_state, rpos, rbits, rsane, rword, rerr, queues_common, default_state, dump_info, rec_info,
                                   preset_holder, SPECIAL_FLOATS, safe_bits)

PROGRAM = "sound symbols shared"
SHIM_LIBRARY = "unit_PlaceHolderHelpers"

S_MGR, S_SEED = 0x1003F360, 0x1003F35C
VT_ELEMENT, VT_BRANCHING, VT_THROWAWAY, VT_EVENT = 0x1003D0D8, 0x1003CFF8, 0x1003D4F0, 0x1003CD8C

# the operations (the unit file's Op enum)
(K_MAPCTOR, K_IDS, K_PTR, K_ECTOR, K_BCTOR, K_TCTOR, K_EDTOR, K_BDTOR, K_TDTOR, K_INIT, K_FILL, K_UPDATE, K_GETPARAM, K_CHOOSE, K_ECLASS,
 K_BCLASS, K_TCLASS, K_TFILL, K_EVCTOR, K_EVTYPE, K_EVCAT, K_EVEVENT, K_EVPOS, K_ITEMCTOR, K_ARRCTOR, K_ARRCONV, K_ARRDTOR) = range(27)

ADDR = {
    K_MAPCTOR: 0x1001AF5C, K_IDS: 0x1001F0DC, K_PTR: 0x1001F364, K_ECTOR: 0x10024218, K_BCTOR: 0x10023D38, K_TCTOR: 0x100242B4,
    K_EDTOR: 0x10023DC0, K_BDTOR: 0x10024114, K_TDTOR: 0x10023E64, K_INIT: 0x10024328, K_FILL: 0x10024438, K_UPDATE: 0x10024724,
    K_GETPARAM: 0x10024814, K_CHOOSE: 0x10023F04, K_ECLASS: 0x100248C0, K_BCLASS: 0x100241C0, K_TCLASS: 0x10026898, K_TFILL: 0x100267E8,
    K_EVCTOR: 0x1001EE64, K_EVTYPE: 0x1001C8A0, K_EVCAT: 0x10017238, K_EVEVENT: 0x1001EEF4, K_EVPOS: 0x1001EF2C, K_ITEMCTOR: 0x1001ECE4,
    K_ARRCTOR: 0x1001EC10, K_ARRCONV: 0x1001EAEC, K_ARRDTOR: 0x1001EB5C,
}
NAME = {
    K_MAPCTOR: "CMapReference::CMapReference", K_IDS: "CMapReference::GetSoundIds", K_PTR: "CMapReference::GetSoundPointer",
    K_ECTOR: "CPlaceHolderOneShotSentenceElement::CPlaceHolderOneShotSentenceElement",
    K_BCTOR: "CPlaceHolderOneShotBranchingSentenceElement::CPlaceHolderOneShotBranchingSentenceElement",
    K_TCTOR: "CPlaceHolderOneShotThrowaway::CPlaceHolderOneShotThrowaway",
    K_EDTOR: "CPlaceHolderOneShotSentenceElement::~CPlaceHolderOneShotSentenceElement",
    K_BDTOR: "CPlaceHolderOneShotBranchingSentenceElement::~CPlaceHolderOneShotBranchingSentenceElement",
    K_TDTOR: "CPlaceHolderOneShotThrowaway::~CPlaceHolderOneShotThrowaway",
    K_INIT: "CPlaceHolderOneShotSentenceElement::Init", K_FILL: "CPlaceHolderOneShotSentenceElement::FillSampleInfo",
    K_UPDATE: "CPlaceHolderOneShotSentenceElement::Update", K_GETPARAM: "CPlaceHolderOneShotSentenceElement::GetParameterValue",
    K_CHOOSE: "CPlaceHolderOneShotBranchingSentenceElement::ChooseRandomSound",
    K_ECLASS: "CPlaceHolderOneShotSentenceElement::GetClassID", K_BCLASS: "CPlaceHolderOneShotBranchingSentenceElement::GetClassID",
    K_TCLASS: "CPlaceHolderOneShotThrowaway::GetClassID", K_TFILL: "CPlaceHolderOneShotThrowaway::FillSampleInfo",
    K_EVCTOR: "TbAudioEvent::TbAudioEvent", K_EVTYPE: "TbAudioEvent::GetType", K_EVCAT: "TbAudioEvent::GetCategory",
    K_EVEVENT: "TbAudioEvent::GetEvent", K_EVPOS: "TbAudioEvent::GetPosition", K_ITEMCTOR: "TbAudioEventList::ListItem::ListItem",
    K_ARRCTOR: "CAutoArrayPtr<TbAudioEventList::ListItem>::CAutoArrayPtr",
    K_ARRCONV: "CAutoArrayPtr<TbAudioEventList::ListItem>::operator TbAudioEventList::ListItem*",
    K_ARRDTOR: "CAutoArrayPtr<TbAudioEventList::ListItem>::~CAutoArrayPtr",
}
PLACEHOLDER_OPS = set(range(K_ECTOR, K_TFILL + 1))
CTOR_OPS = {K_ECTOR, K_BCTOR, K_TCTOR}
KIND = {K_ECTOR: "e", K_EDTOR: "e", K_INIT: "e", K_FILL: "e", K_UPDATE: "e", K_GETPARAM: "e", K_ECLASS: "e",
        K_BCTOR: "b", K_BDTOR: "b", K_CHOOSE: "b", K_BCLASS: "b", K_TCTOR: "t", K_TDTOR: "t", K_TCLASS: "t", K_TFILL: "t"}
CTOR_ADDR = {"e": ADDR[K_ECTOR], "b": ADDR[K_BCTOR], "t": ADDR[K_TCTOR]}
VT_OF = {"e": VT_ELEMENT, "b": VT_BRANCHING, "t": VT_THROWAWAY}

# the parameter words (the unit file's Param enum) and the result words
P_OP, P_MGR, P_SEED, P_BRANCHING, P_A, P_STATE, P_PSTATE, P_INFO, P_CAT, P_IMG = 0, 1, 2, 6, 8, 16, 52, 88, 100, 128
O_STATE, O_PSTATE, O_CAT, O_INFO, O_DATA, O_STATIC, O_ARRAY, O_BLOCK = 8, 44, 80, 84, 112, 130, 140, 200


# ---------------------------------------------------------------- the data image

class Image:
    """One block of map data in the Mac layout: fields (position, size, value, pointer?) and the positions that carry labels."""

    def __init__(self, pad=0):
        self.size = pad
        self.fields = []
        self.labels = []

    def alloc(self, n, align=1):
        self.size = (self.size + align - 1) // align * align
        pos = self.size
        self.size += n
        return pos

    def put(self, pos, size, value, ptr=False):
        self.fields.append((pos, size, value & MASK, ptr))

    def words(self):
        w = [self.size, len(self.fields), len(self.labels)]
        for pos, size, value, ptr in self.fields:
            w += [pos, size | (0x100 if ptr else 0), value]
        for label, pos in self.labels:
            w += [label, pos]
        return w


def tree_image(events, vc_index):
    """World A: the category data at 0, the events array, then per event its sounds and per sound its samples and branches."""
    im = Image()
    cat = im.alloc(24, 4)
    ev_pos = im.alloc(0x14 * len(events), 4)
    im.labels.append((16, cat))
    im.put(cat, 4, len(events))
    im.put(cat + 4, 4, ev_pos, True)
    im.put(cat + 8, 2, vc_index)
    sound_pos, flat_pos, sample_labels = [], [], 0
    for i, ev in enumerate(events):
        base = ev_pos + 0x14 * i
        so = im.alloc(0x2A * max(len(ev["sounds"]), 1), 4)
        im.labels.append((21 + i, base))
        im.put(base, 4, ev["id"])
        im.put(base + 4, 4, len(ev["sounds"]))
        im.put(base + 8, 4, so, True)
        im.put(base + 0xC, 4, ev["f0c"])
        im.put(base + 0x10, 2, ev["flags"])
        im.put(base + 0x12, 2, ev["f12"])
        sound_pos.append(so)
        for j in range(len(ev["sounds"])):
            flat_pos.append(so + 0x2A * j)
    flat = 0
    for i, ev in enumerate(events):
        for j, sd in enumerate(ev["sounds"]):
            base = sound_pos[i] + 0x2A * j
            nm = len(sd["samples"])
            sm = im.alloc(0x10 * max(nm, 1), 4)
            branches = sd.get("branches", [])
            br = im.alloc(8 * max(len(branches), 1), 4)
            im.labels.append((31 + flat, base))
            im.put(base, 4, nm | ((sd["hi"] & 0xFFFF) << 16))
            im.put(base + 4, 4, len(branches))
            im.put(base + 8, 4, sm, True)
            for k, key in enumerate(("vmin", "vmax", "pmin", "pmax")):
                im.put(base + 0xC + k, 1, sd[key])
            im.put(base + 0x10, 2, sd["f10"])
            im.put(base + 0x12, 2, sd["f12"])
            im.put(base + 0x16, 2, sd["f16"])
            im.put(base + 0x18, 2, sd["f18"])
            im.put(base + 0x1A, 2, sd["f1a"])
            im.put(base + 0x1C, 2, sd["f1c"])
            im.put(base + 0x1E, 4, sd["f1e"])
            im.put(base + 0x22, 4, sd["f22"])
            if branches:
                im.put(base + 0x26, 4, br, True)
            for k, (target, lo, hi, f04) in enumerate(branches):
                b = br + 8 * k
                im.put(b, 4, flat_pos[target % len(flat_pos)], True)
                im.put(b + 4, 2, f04)
                im.put(b + 6, 1, lo)
                im.put(b + 7, 1, hi)
            for k, smp in enumerate(sd["samples"]):
                a = sm + 0x10 * k
                im.labels.append((71 + sample_labels, a))
                sample_labels += 1
                im.put(a, 4, smp["f00"])
                im.put(a + 4, 4, smp["f04"])
                im.put(a + 8, 4, smp["f08"])
                im.put(a + 0xC, 2, smp["bank"])
            flat += 1
    return im


def build_image(e, L, im):
    """The image in the emulator's memory (big-endian words); returns the block address and the label -> address map."""
    block = e.alloc(max(im.size, 16), b"\0" * max(im.size, 16))
    for pos, size, value, ptr in im.fields:
        v = (value + block) & MASK if ptr else value
        e.write(block + pos, v.to_bytes(size, "big"))
    amap = {}
    for label, pos in im.labels:
        amap[label] = block + pos
        L.labels[block + pos] = label
    return block, amap


# ---------------------------------------------------------------- the world

class World:
    pass


def preset_state(e, L, w, obj, st, c, label_extra):
    """preset_holder (w79g2's) writes a state into a placeholder and its extra data; the parent word becomes the parent object."""
    extra = preset_holder(e, L, w, obj, st, c)
    if extra:
        e.write(extra + 0x1C, pack([w.parent if st[S_PARENT] else 0]))
        if not label_extra:
            L.labels.pop(extra, None)
    return extra


def dump_holder(e, L, obj):
    u = e.u32
    sx = lambda off: canon(u(obj + off))
    extra = u(obj + 0x5C)
    st = [lbl(L, u(obj + 4)), lbl(L, u(obj + 8)), lbl(L, u(obj + 0xC)), sx(0x10), sx(0x14), sx(0x18), sx(0x1C), sx(0x20), sx(0x24),
          u(obj + 0x28), u(obj + 0x2C), u(obj + 0x30), lbl(L, u(obj + 0x34)), u(obj + 0x38), u(obj + 0x3C), u(obj + 0x40), u(obj + 0x44),
          u(obj + 0x48), sx(0x4C), e.read(obj + 0x50, 1)[0], e.read(obj + 0x51, 1)[0], e.read(obj + 0x52, 1)[0], sx(0x54),
          struct.unpack(">H", e.read(obj + 0x58, 2))[0], struct.unpack(">H", e.read(obj + 0x5A, 2))[0], 1 if extra else 0]
    if extra:
        st += [struct.unpack("<I", e.read(extra, 4))[0], struct.unpack("<I", e.read(extra + 4, 4))[0], u(extra + 8), u(extra + 0xC),
               u(extra + 0x10), u(extra + 0x14), lbl(L, u(extra + 0x18)), lbl(L, u(extra + 0x1C))]
    else:
        st += [0] * 8
    return tuple(st)


def ppc_stub_setup(e):
    """The PowerPC side's fakes (the callees outside the unit): as unit w79g2's module installs them, minus the placeholder classes."""
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

    # operator new / delete: blocks filled with 0xA5; the extra data (0x20) is recorded
    def op_new(e):
        L, size = e.cl, e.r(3)
        block = e.alloc(max(size, 16), b"\xa5" * max(size, 16))
        if L.track and size == 0x20:
            L.rec("new", (size, L.newlab(block)))
        e.set_r(3, block)

    def op_delete(e):
        L, block = e.cl, e.r(3)
        if block != 0 and L.lab(block) > 0:
            L.rec("delete", (L.lab(block),))
            L.forget(block)
    e.stub("__nw__FUl", op_new)
    e.stub("__dl__FPv", op_delete)

    # the array runtime of the CAutoArrayPtr instance: an eight-byte cookie in front of the elements (the runtime itself is not in the dump)
    def new_array(e):
        size = e.r(3)
        e.set_r(3, e.alloc(max(size, 16), b"\xa5" * max(size, 16)))
    e.stub("__nwa__FUl", new_array)

    def construct_array(e, call):
        block, ctor, size, count = e.r(3), e.r(4), e.r(6), e.r(7)
        e.write(block, struct.pack(">II", size, count))
        for i in range(count):
            call(ctor, block + 8 + i * size, 1)
        return block + 8
    T.stub_cb(e, "__construct_new_array", construct_array)

    def destroy_array(e, call):
        L, pointer = e.cl, e.r(3)
        if pointer != 0 and L.lab(pointer) > 0:
            L.rec("delete", (L.lab(pointer),))
            L.forget(pointer)
        return None
    T.stub_cb(e, "__destroy_new_array", destroy_array)

    def clock(e):
        e.cl.rec("Clock", ())
        e.set_r(3, T.u32(e.cl.ret("Clock", 0)))
    e.stub("LbTime_GetClock__Fv", clock)

    # the in-binary callees outside the unit
    def random(e):
        L = e.cl
        L.rec("Random", (e.u32(e.r(3)),))
        e.set_r(3, T.u32(L.ret("Random", 0)))

    def ranged(e):
        L = e.cl
        L.rec("RangedRandom", (e.u32(e.r(3)), e.r(4)))
        e.set_r(3, T.u32(L.ret("RangedRandom", 0)))

    def instance(e):
        e.set_r(3, e.w.streamer)

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

    def mgr_bank(e):
        L = e.cl
        L.rec("Mgr.GetBank", (L.lab(e.r(3)), e.r(4)))
        answer = L.ret("Bank", 0)
        e.set_r(3, e.w.banks[answer - 1] if 1 <= answer <= 3 else 0)

    for address, fn in {0x10019AD0: random, 0x10018C30: ranged, 0x1001F6EC: instance, 0x1001966C: mgr_distmul, 0x1001954C: mgr_qsound,
                        0x10019758: mgr_vcontrol, 0x1002748C: mgr_bank}.items():
        T.stub_internal(e, address, fn)

    # the container's table: +8 Release, +0xC AddRef (they count in the word at +4 and record)
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

    # the manager's table: +0x18 SystemCommand (buffer r3, this r4, command r5), +0x44 GetRolloffFactorToMinMaxDistance
    CMD_VT = {0x1003C9F0: "SetSampleInfo", 0x1003CAE0: "Stop", 0x1003C9AC: "Pause", 0x1003C964: "Restart"}

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


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    lib = runner.lib
    lib.tpw_ph_run.argtypes = [C.c_int32, C.c_void_p, C.c_void_p]
    ppc_stub_setup(e)


# ---------------------------------------------------------------- cases

def make_case(op, **kw):
    c = dict(op=op, mgr=1, seed=0x0BADC0DE, state=default_state(), pstate=default_state(), a=[0] * 8, info=[0, 0, 0], events=[make_event()],
             vc=0, q={}, cats=[], layout=None)
    c.update(kw)
    c["q"] = {k: v if isinstance(v, list) else [v] for k, v in c["q"].items()}
    return c


def case_image(c):
    if c["layout"] is not None:
        return c["layout"]["image"]
    return tree_image(c["events"], c["vc"])


def params(c):
    p = [0] * 4096
    p[P_OP], p[P_MGR], p[P_SEED] = c["op"], c["mgr"], c["seed"]
    p[P_BRANCHING] = 1 if c.get("branching") else 0
    p[P_A:P_A + 8] = c["a"]
    p[P_STATE:P_STATE + NSTATE] = c["state"]
    p[P_PSTATE:P_PSTATE + NSTATE] = c["pstate"]
    p[P_INFO:P_INFO + 3] = c["info"]
    for i, (present, data) in enumerate(c["cats"]):
        p[P_CAT + 2 * i], p[P_CAT + 2 * i + 1] = present, data
    words = case_image(c).words()
    p[P_IMG:P_IMG + len(words)] = words
    return [v & MASK for v in p]


def sample_info(e, L, c):
    info = e.alloc(0xA0, b"\xa5" * 0xA0)
    T.call_ppc(e, 0x100158BC, [info])
    if c["info"][2]:
        e.write(info + 0xC, pack([c["info"][0]]))
        e.write(info + 0x14, pack([c["info"][1]]))
    L.labels[info] = 5
    return info


def ppc_run(e, c):
    op = c["op"]
    L = e.cl = CL.PpcLog({}, c["q"])
    L.track = False
    w = e.w = World()
    st, pst = c["state"], c["pstate"]
    a = [v & MASK for v in c["a"]]
    call = T.call_ppc
    im = case_image(c)
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
    w.banks = [e.alloc(16, b"\xa5" * 16) for _ in range(3)]
    for i, addr in enumerate(w.banks):
        L.labels[addr] = 11 + i
    w.streamer = e.alloc(0x40, b"\xa5" * 0x40)
    L.labels[w.streamer] = 7
    e.write(S_MGR, pack([w.manager if c["mgr"] else 0]))
    e.write(S_SEED, pack([c["seed"]]))
    ptr_label = lambda r3: lbl(L, r3)

    result = second = 0
    r_words = [0, 0, 0, 0, 0]
    blk = (0,) * 8
    arr = (0,) * 32
    subject = parent = 0
    holder = 0
    dump_flag = op in PLACEHOLDER_OPS
    buf = e.alloc(16, b"\xa5" * 16)

    if op in PLACEHOLDER_OPS:
        # the parent: the real base constructor, then the preset
        parent = e.alloc(0x80, b"\xa5" * 0x80)
        w.parent = parent
        call(e, 0x10017898, [parent])
        L.labels[parent] = 6
        L.labels[parent + 0x10] = 18
        L.labels[parent + 0x1C] = 19
        preset_state(e, L, w, parent, pst, c, False)
        subject = e.alloc(0x80, b"\xa5" * 0x80)
        kind = "b" if c.get("branching") and op in (K_INIT, K_FILL, K_UPDATE, K_GETPARAM) else KIND[op]
        if op not in CTOR_OPS:
            call(e, CTOR_ADDR[kind], [subject, parent] if kind != "t" else [subject])
            preset_state(e, L, w, subject, st, c, True)
            L.labels[subject] = 1
    elif op in (K_IDS, K_PTR):
        # the streamer: the category table at +0x1c (the array, its size), containers of 0x20 bytes whose +8 is the category data
        ncat = a[4]
        table = e.alloc(4 * max(ncat, 1), b"\0" * (4 * max(ncat, 1)))
        for i in range(ncat):
            present, data = c["cats"][i]
            if present:
                cont = e.alloc(0x20, pack([e.vt_container, 1, block + data if data else 0, FILL, FILL, FILL, FILL, FILL]))
                L.labels[cont] = 40 + i
                e.write(table + 4 * i, pack([cont]))
        e.write(w.streamer + 0x1C, pack([table if ncat else 0, ncat]))
        holder = e.alloc(0x80, b"\xa5" * 0x80)
        call(e, 0x10017898, [holder])
        e.write(holder + 4, pack([block + a[0] if a[0] else 0, block + a[1] if a[1] else 0, block + a[2] if a[2] else 0]))
        # the holder's event, sound and sample words are +4, +8, +0xc
        L.labels[holder] = 6
    info = sample_info(e, L, c)

    # the small objects
    if op in (K_EVTYPE, K_EVCAT, K_EVEVENT, K_EVPOS):
        pos = e.alloc(12, pack(a[2:5]))
        subject = e.alloc(0x30, b"\xa5" * 0x30)
        call(e, ADDR[K_EVCTOR], [subject, a[0], a[1], pos])
        L.labels[subject] = 15
    if op in (K_ARRCONV, K_ARRDTOR):
        holder = e.alloc(8, b"\xa5" * 8)
        if a[7]:
            e.write(holder, pack([0]))
        else:
            call(e, ADDR[K_ARRCTOR], [holder, a[0]])
        if e.u32(holder):
            L.labels[e.u32(holder)] = 9
        L.labels[holder] = 10

    # ---- the call
    del L.log[:]
    L.track = True
    ev_addr = amap.get(a[0], 0)
    if op == K_MAPCTOR:
        small = e.alloc(8, b"\xa5" * 8)
        call(e, ADDR[op], [small])
        blk = tuple(e.read(small, 4)) + (0, 0, 0, 0)
    elif op == K_IDS:
        cells = e.alloc(16, pack([FILL] * 4))
        small = e.alloc(8, b"\xa5" * 8)
        call(e, ADDR[op], [small, cells, cells + 4, cells + 8, cells + 12, holder])
        r_words = [e.u32(cells), e.u32(cells + 4), e.u32(cells + 8), e.u32(cells + 12), 0]
    elif op == K_PTR:
        small = e.alloc(8, b"\xa5" * 8)
        result = ptr_label(call(e, ADDR[op], [small, a[3], holder])[0])
    elif op in CTOR_OPS:
        kind = KIND[op]
        call(e, ADDR[op], [subject, parent] if kind != "t" else [subject])
        L.labels[subject] = 1
        vptr = e.u32(subject)
        code = e.u32(e.u32(vptr + 0x54))
        classid = call(e, code, [subject])[0]
        result = classid | (0x100 if vptr == VT_OF[kind] else 0) | (0x200 if e.read(subject + 0x60, 0x20) == b"\xa5" * 0x20 else 0)
    elif op in (K_EDTOR, K_BDTOR, K_TDTOR):
        call(e, ADDR[op], [subject, a[0]])
    elif op == K_INIT:
        result = call(e, ADDR[op], [subject, ev_addr if a[0] else 0, a[1]])[0] & 0xFF
    elif op in (K_FILL, K_TFILL):
        result = call(e, ADDR[op], [subject, a[0], info])[0] & 0xFF
    elif op == K_UPDATE:
        call(e, ADDR[op], [subject])
    elif op == K_GETPARAM:
        out = e.alloc(4, pack([FILL]))
        result = call(e, ADDR[op], [subject, a[0], out])[0] & 0xFF
        second = e.u32(out)
    elif op == K_CHOOSE:
        result = call(e, ADDR[op], [subject, a[0]])[0] & 0xFF
    elif op in (K_ECLASS, K_BCLASS, K_TCLASS):
        result = call(e, ADDR[op], [subject])[0]
    elif op == K_EVCTOR:
        pos = e.alloc(12, pack(a[2:5]))
        obj = e.alloc(0x30, b"\xa5" * 0x30)
        call(e, ADDR[op], [obj, a[0], a[1], pos])
        blk = (e.u32(obj + 4), e.u32(obj + 8), e.u32(obj + 0xC), e.u32(obj + 0x10), e.u32(obj + 0x14), e.u32(obj + 0x18), 0, 0)
        result = (0x100 if e.u32(obj) == VT_EVENT else 0) | (0x200 if e.read(obj + 0x1C, 0x14) == b"\xa5" * 0x14 else 0)
    elif op in (K_EVTYPE, K_EVCAT, K_EVEVENT):
        result = call(e, ADDR[op], [subject])[0] & MASK
    elif op == K_EVPOS:
        r3 = call(e, ADDR[op], [subject])[0]
        result = 1 if r3 == subject + 0xC else 0
        blk = (e.u32(r3), e.u32(r3 + 4), e.u32(r3 + 8), 0, 0, 0, 0, 0)
    elif op == K_ITEMCTOR:
        small = e.alloc(64, b"\xa5" * 64)
        call(e, ADDR[op], [small])
        blk = tuple(e.read(small, 8))
    elif op == K_ARRCTOR:
        holder = e.alloc(8, b"\xa5" * 8)
        call(e, ADDR[op], [holder, a[0]])
        pointer = e.u32(holder)
        L.labels[pointer] = 9
        result = 1 if pointer else 0
        words = [0] * 32
        data = e.read(pointer, a[0] * 8) if pointer and a[0] * 8 <= 128 else b""
        for i, byte in enumerate(data):
            words[i // 4] |= byte << (8 * (i % 4))
        arr = tuple(words)
    elif op == K_ARRCONV:
        result = ptr_label(call(e, ADDR[op], [holder])[0])
    elif op == K_ARRDTOR:
        call(e, ADDR[op], [holder, a[1]])
    L.track = False

    state = dump_holder(e, L, subject) if dump_flag and subject else (0,) * 34
    pstate = dump_holder(e, L, parent) if dump_flag and parent else (0,) * 34
    sd = []
    sound_labels = sorted(l for l in amap if 31 <= l < 71)
    for i in range(8):
        if i < len(sound_labels):
            base = amap[sound_labels[i]]
            sd += [struct.unpack("<I", e.read(base + 0xC, 4))[0], struct.unpack(">H", e.read(base + 0x10, 2))[0] | (struct.unpack(">H", e.read(base + 0x12, 2))[0] << 16)]
        else:
            sd += [0, 0]
    return dict(r=(result & MASK, second & MASK) + tuple(r_words), st=state, pst=pstate, cat=e.u32(w.container + 4), info=dump_info(e, L, info),
                data=tuple(sd), statics=(lbl(L, e.u32(S_MGR)), e.u32(S_SEED)), blk=blk, arr=arr, log=list(L.log))


def host_run(lib, c):
    p = (C.c_uint32 * 4096)(*params(c))
    out = (C.c_uint32 * 1024)()
    CL.host_begin(lib, {}, c["q"])
    lib.tpw_ph_run(c["op"], p, out)
    log = CL.host_end(lib)
    op = c["op"]
    if op == K_IDS:
        r_words = [out[0], out[1], out[2], out[3], 0]
        result = 0
    else:
        r_words = [0, 0, 0, 0, 0]
        result = out[0]
    if op in (K_MAPCTOR,):
        blk = tuple(out[O_BLOCK:O_BLOCK + 4]) + (0, 0, 0, 0)
    elif op == K_ITEMCTOR:
        blk = tuple(out[O_BLOCK:O_BLOCK + 8])
    else:
        blk = tuple(out[O_BLOCK:O_BLOCK + 8])
    return dict(r=(result & MASK, out[4]) + tuple(r_words), st=tuple(out[O_STATE:O_STATE + NSTATE]), pst=tuple(out[O_PSTATE:O_PSTATE + NSTATE]),
                cat=out[O_CAT], info=tuple(out[O_INFO:O_INFO + 24]), data=tuple(out[O_DATA:O_DATA + 16]), statics=(out[O_STATIC], out[O_STATIC + 1]),
                blk=blk, arr=tuple(out[O_ARRAY:O_ARRAY + 32]), log=log)


TESTS = []


# ---------------------------------------------------------------- randomisation

def add_branches(r, events, p):
    """Branch records for some sounds: (target sound, lowest byte, highest byte, the unused word)."""
    flat = [sd for ev in events for sd in ev["sounds"]]
    for sd in flat:
        sd["branches"] = []
        if r.random() < p:
            for _ in range(r.randrange(1, 4)):
                lo = r.choice([0, 0, 1, 5, 10, 50, 0x7F, 0xFF, r.randrange(256)])
                hi = min(r.choice([lo, lo + 1, 0xFF, 0x7F, 200, r.randrange(256)]), 255)
                sd["branches"].append((r.randrange(len(flat)), lo, hi, r.getrandbits(16)))


def base_case(r, op, **kw):
    """A random world: a tree, a subject state (an Element's extra data exists and names the parent) and a parent state (with extra data)."""
    events = rand_events(r, zero_samples=op == K_INIT)
    add_branches(r, events, 0.7 if op == K_CHOOSE else 0.3)
    st = rand_state(r, events, extra=True)
    pst = rand_state(r, events, extra=True)
    pst[S_PARENT] = 0
    pst[S_CHILD] = 0
    st[S_CHILD] = 0
    st[S_PARENT] = 1
    if KIND.get(op) == "t" and r.random() < 0.4:
        st[S_EXTRA] = 0
        st[S_PARENT] = 0
    c = make_case(op, events=events, state=st, pstate=pst, seed=r.choice([0, r.getrandbits(32)]), vc=r.randrange(0, 5), q=queues_common(r),
                  a=[rword(r) for _ in range(6)] + [0, 0], mgr=1 if r.random() < 0.9 else 0)
    c.update(kw)
    c["q"] = {k: v if isinstance(v, list) else [v] for k, v in c["q"].items()}
    return c


def rpick(r, values):
    """A Random() word whose high half is near one of the thresholds."""
    t = r.choice(list(values) + [v + 1 for v in values] + [max(v - 1, 0) for v in values] + [0, 0xFFFF, r.getrandbits(16)])
    return ((t & 0xFFFF) << 16) | r.getrandbits(16)


def branch_weights(events, flat_index_of, sound, parameter):
    """The running totals of the in-range branches of a sound (the sums the Mac's second loop compares)."""
    totals, acc = [], 0
    for target, lo, hi, _ in sound.get("branches", []):
        if lo <= parameter <= hi:
            acc = (acc + flat_index_of(target)["f1e"]) & MASK
            totals.append(acc)
    return totals


def gen_placeholder(op):
    def make(r):
        c = base_case(r, op)
        a, st, pst, events = c["a"], c["state"], c["pstate"], c["events"]
        flat = [sd for ev in events for sd in ev["sounds"]]
        if op in (K_INIT, K_FILL, K_UPDATE, K_GETPARAM) and r.random() < (0.4 if op == K_INIT else 0.2):
            c["branching"] = True   # the operation runs on a Branching element (its ChooseRandomSound is the virtual Init calls)
        if op == K_INIT:
            a[0] = 21 + r.randrange(len(events))
            a[1] = r.choice([0, 1000, rword(r), r.randrange(0, 100000)])
            c["q"]["Random"] = [r.getrandbits(32) for _ in range(3)]
            if r.random() < 0.3:
                st[S_SOUND] = 0
            if r.random() < 0.3:
                st[S_SAMPLE] = 0
            c["q"]["RangedRandom"] = [r.randrange(0, 300) for _ in range(4)]
        elif op == K_FILL:
            c["info"] = [r.choice([0, rword(r)]), r.choice([0, rword(r)]), r.randrange(3) == 0]
            if r.random() < 0.9 and not st[S_SAMPLE]:
                st[S_SAMPLE] = 71
            if r.random() < 0.9 and not st[S_SOUND]:
                st[S_SOUND] = 31
            a[0] = r.choice([0, 1000, rword(r), r.randrange(0, 100000)])
            if r.random() < 0.5:
                st[S_FLAGS] |= F_NONPOS
            if r.random() < 0.3:
                st[S_FLAGS] &= ~F_NONPOS
            if r.random() < 0.25:
                f22 = r.choice([fbits(2.0), fbits(1.0), fbits(4.0), 0, fbits(2.1), 0x40000001, 0x3FFFFFFF, 0x7FC00000, 0x80000000])
                for sd in flat:
                    sd["f22"] = f22
                c["q"]["DistMul"] = [r.choice([ONE, fbits(2.0), fbits(0.5)])]
                st[S_FLAGS] &= ~F_MAXDIST
            if r.random() < 0.2:
                st[S_FLAGS] |= F_MAXDIST
                st[S_MAXDIST] = r.choice([fbits(1.0), fbits(2.0), 0x40000001, fbits(50.0), 0x7FC00000, 0])
            if r.random() < 0.1:
                st[S_SOUND] = 0
            if r.random() < 0.1:
                st[S_SAMPLE] = 0
        elif op == K_UPDATE:
            c["mgr"] = 1
            if r.random() < 0.3:
                st[S_HANDLE] = 0
            elif st[S_HANDLE] == 0:
                st[S_HANDLE] = r.choice([1, 0x1234, r.getrandbits(32) | 1])
            if r.random() < 0.5:
                st[S_FLAGS] ^= F_NONPOS
        elif op == K_GETPARAM:
            if not st[S_SOUND]:
                st[S_SOUND] = 31
            sd = flat[0]
            a[0] = r.choice([sd["f18"], sd["f1c"], sd["f18"] | sd["f1c"], 1, 2, 4, 3, 0xFFFF, 0, rword(r), 0x10000 | sd["f18"]])
            if r.random() < 0.5:
                st[S_SOUND] = 31 + r.randrange(len(flat))
                sd = flat[st[S_SOUND] - 31]
                a[0] = r.choice([sd["f18"], sd["f1c"], sd["f18"] | sd["f1c"], rword(r)])
        elif op == K_CHOOSE:
            index_of = lambda t: flat[t % len(flat)]
            if r.random() < 0.6:
                pst[S_SOUND] = 31 + r.randrange(len(flat))
            else:
                pst[S_SOUND] = 0
                st[S_EVENT] = 21 + r.randrange(len(events))
            if pst[S_SOUND] == 0 and not st[S_EVENT]:
                st[S_EVENT] = 21
            sound = flat[pst[S_SOUND] - 31] if pst[S_SOUND] else events[st[S_EVENT] - 21]["sounds"][0]
            lo_hi = [(lo, hi) for _, lo, hi, _ in sound.get("branches", [])]
            parameter = r.choice([lo for lo, _ in lo_hi] + [hi for _, hi in lo_hi] + [lo - 1 for lo, _ in lo_hi] + [hi + 1 for _, hi in lo_hi] + [0, 0x7F, 0xFF]) \
                if lo_hi else r.randrange(256)
            parameter &= 0xFF
            values = bytearray(struct.pack("<I", pst[S_VALUES]))
            values[0] = parameter
            pst[S_VALUES] = struct.unpack("<I", bytes(values))[0]
            totals = branch_weights(events, index_of, sound, parameter)
            picks = [t for t in totals] + [t - 1 for t in totals] + [t + 1 for t in totals] + [0, 1, 0xFFFFFFFF, r.getrandbits(32)]
            c["q"]["RangedRandom"] = [r.choice(picks) & MASK for _ in range(2)]
            pst[S_FLAGS] = r.choice([0, F_S40, r.getrandbits(21)])
        elif op == K_TFILL:
            c["info"] = [r.choice([0, rword(r)]), r.choice([0, rword(r)]), r.randrange(3) == 0]
            if r.random() < 0.9 and not st[S_SAMPLE]:
                st[S_SAMPLE] = 71
            if r.random() < 0.9 and not st[S_SOUND]:
                st[S_SOUND] = 31
            a[0] = r.choice([0, 1000, rword(r), r.randrange(0, 100000)])
            if r.random() < 0.15:
                st[S_SOUND] = 0
            if r.random() < 0.1:
                st[S_SAMPLE] = 0
            if r.random() < 0.3:
                c["info"][1] |= 0x2000
                c["info"][2] = 1
        elif op in (K_EDTOR, K_BDTOR, K_TDTOR):
            a[0] = r.choice([0, 1, 2, 0xFFFFFFFF, 0x7FFF, 0x8000, 0xFFFF, 0x10000, 0x10001, 0xFFFF8000, 0x12345678])
        return c
    return make


# ---------------------------------------------------------------- the map reference world

def map_layout(cats):
    """cats: list of dict(present, data, count, events=[[samples per sound, ...] per event]). One block, padded by 16 bytes so that no record
    sits at position 0: the category data records, then the events arrays one after the other (adjacent), then the sounds and the samples.
    Returns the layout: the image, the category table [(present, data position)] and the positions of the records."""
    im = Image(pad=16)
    lay = dict(cat=[], ev=[], sound=[], sample=[], table=[])
    for c in cats:
        lay["cat"].append(im.alloc(24, 1) if c["data"] else 0)
    for i, c in enumerate(cats):
        lay["ev"].append(im.alloc(0x14 * len(c["events"]), 1) if c["data"] else 0)
    for i, c in enumerate(cats):
        lay["sound"].append([])
        for j, ev in enumerate(c["events"]):
            lay["sound"][i].append(im.alloc(0x2A * len(ev), 1) if c["data"] else 0)
    for i, c in enumerate(cats):
        lay["sample"].append([])
        for j, ev in enumerate(c["events"]):
            lay["sample"][i].append([])
            for k, nm in enumerate(ev):
                lay["sample"][i][j].append(im.alloc(0x10 * nm, 1) if c["data"] else 0)
    for i, c in enumerate(cats):
        lay["table"].append((1 if c["present"] else 0, lay["cat"][i]))
        if not c["data"]:
            continue
        im.put(lay["cat"][i], 4, c["count"] if c.get("count") is not None else len(c["events"]))
        im.put(lay["cat"][i] + 4, 4, lay["ev"][i], True)
        for j, ev in enumerate(c["events"]):
            base = lay["ev"][i] + 0x14 * j
            im.put(base + 4, 4, len(ev) if c.get("soundcounts") is None else c["soundcounts"][j])
            im.put(base + 8, 4, lay["sound"][i][j], True)
            for k, nm in enumerate(ev):
                sb = lay["sound"][i][j] + 0x2A * k
                im.put(sb, 4, nm | (c.get("hi", 0) << 16))
                im.put(sb + 8, 4, lay["sample"][i][j][k], True)
    im.alloc(64)   # the reads past the last record stay inside the block
    lay["image"] = im
    lay["size"] = im.size - 64
    lay["cats"] = cats
    return lay


def event_matches(lay, event):
    """Whether the Mac's search finds `event` (a block position) in some category: the sound pointer must not be null then (the code reads
    through it)."""
    for i, c in enumerate(lay["cats"]):
        if not c["present"] or not c["data"]:
            continue
        count = c["count"] if c.get("count") is not None else len(c["events"])
        if count != 0 and lay["ev"][i] <= event <= lay["ev"][i] + count * 0x14:
            return True
    return False


def ids_case(lay, ptrs, ncat=None):
    """A GetSoundIds case; None when the original would read through a null sound pointer."""
    if ptrs[1] == 0 and ptrs[0] != 0 and event_matches(lay, ptrs[0]):
        return None
    table = lay["table"] if ncat is None else ncat
    return make_case(K_IDS, layout=lay, cats=table, a=list(ptrs) + [0, len(table), 0, 0, 0])


def rand_cats(r):
    cats = []
    for _ in range(r.randrange(0, 5)):
        events = [[r.randrange(0, 4) for _ in range(r.randrange(0, 4))] for _ in range(r.randrange(0, 4))]
        c = dict(present=r.random() < 0.85, data=r.random() < 0.85, events=events, hi=r.choice([0, 0, 0, 1, 0xFFFF]))
        if r.random() < 0.15:
            c["count"] = r.choice([0, 1, len(events) + 1, len(events) + 3, 0xFFFFFFFF])
        if events and r.random() < 0.15:
            c["soundcounts"] = [r.choice([0, 1, len(ev), len(ev) + 2]) for ev in events]
        cats.append(c)
    return cats


def record_starts(lay):
    """Every position at which a record of the image starts (an end of an array is the start of what follows it): the event and the
    sound pointer of the holder are read through, so they must be at such a position (a field read across two records differs between
    the big-endian image and the little-endian one); the sample pointer is only subtracted and can be anywhere."""
    starts = set([lay["size"]])
    for i, c in enumerate(lay["cats"]):
        if not c["data"]:
            continue
        starts.add(lay["cat"][i])
        for j in range(len(c["events"]) + 1):
            starts.add(lay["ev"][i] + 0x14 * j)
        for j, ev in enumerate(c["events"]):
            for k in range(len(ev) + 1):
                starts.add(lay["sound"][i][j] + 0x2A * k)
            for k, nm in enumerate(ev):
                for m in range(nm + 1):
                    starts.add(lay["sample"][i][j][k] + 0x10 * m)
    return sorted(starts)


def positions(lay, cats, r):
    """Interesting holder pointers (block positions): the event and the sound at record starts (near the records of one random category,
    or any record), the sample anywhere near the samples."""
    size = lay["size"]
    starts = record_starts(lay)
    chosen = []
    usable = [i for i, c in enumerate(cats) if c["data"] and c["events"]]
    for kind in range(3):
        if not usable or r.random() < 0.1:
            chosen.append(0 if r.random() < 0.5 or kind == 2 else r.choice(starts))
            if kind == 2 and r.random() < 0.5:
                chosen[-1] = r.randrange(0, size + 8)
            continue
        i = r.choice(usable)
        j = r.randrange(len(cats[i]["events"]))
        ev = cats[i]["events"][j]
        if kind == 0:
            pos = r.choice([lay["ev"][i] + 0x14 * r.choice([j, j, len(cats[i]["events"])]), r.choice(starts)])
        elif kind == 1:
            pos = r.choice([lay["sound"][i][j] + 0x2A * r.choice([r.randrange(max(len(ev), 1)), len(ev)]), r.choice(starts)])
        else:
            k = r.randrange(max(len(ev), 1))
            n = ev[k] if ev else 0
            m = r.choice([0, 0, max(n - 1, 0), n, n + 1, 2, 3])
            delta = r.choice([0, 0, 1, -1, 7, 15, 16, 17, 31, 33])
            pos = max((lay["sample"][i][j][k] if ev else lay["sound"][i][j]) + 0x10 * m + delta, 1) if r.random() < 0.8 else r.randrange(0, size + 8)
        chosen.append(pos)
    return chosen


def gen_ids(r):
    while True:
        cats = rand_cats(r)
        lay = map_layout(cats)
        c = ids_case(lay, positions(lay, cats, r))
        if c is not None:
            return c


def gen_ptr(r):
    cats = rand_cats(r)
    lay = map_layout(cats)
    n = len(cats)
    a = [0, 0, 0, r.choice([0, 1, 2, 3, 4, n, n + 1, 0xFFFFFFFF, 0x80000000, max(n - 1, 0)]), n, 0, 0, 0]
    return make_case(K_PTR, layout=lay, cats=lay["table"], a=a)


def ids_edges():
    cases = []
    # one category: every event, sound and sample, at the start, in the middle, one past the end; the sample pointer at every skew
    for nsamples in ([0], [1], [2, 3], [3, 0, 1]):
        events = [nsamples, [1, 2], []]
        cats = [dict(present=True, data=True, events=events)]
        lay = map_layout(cats)
        for j in range(len(events) + 1):
            ev = lay["ev"][0] + 0x14 * j
            for k in range(len(events[j]) + 1 if j < len(events) else 1):
                snd = lay["sound"][0][j] + 0x2A * k if j < len(events) else lay["sound"][0][len(events) - 1]
                smp0 = lay["sample"][0][j][k] if j < len(events) and k < len(events[j]) else snd
                for m in range(-1, 4):
                    for delta in (0, 7, 15, 17):
                        cases.append(ids_case(lay, [ev, snd, smp0 + 0x10 * m + delta]))
    # the end of the events array (one past the last event) and just beyond; before the first event
    cats = [dict(present=True, data=True, events=[[1], [1]])]
    lay = map_layout(cats)
    end = lay["ev"][0] + 0x28
    for ev in (end, lay["ev"][0], lay["ev"][0] + 0x14, lay["cat"][0], lay["sound"][0][0], lay["sample"][0][0][0]):
        cases.append(ids_case(lay, [ev, lay["sound"][0][0], lay["sample"][0][0][0]]))
    # two adjacent categories: the one-past-the-end address of the first is the first event of the second (the first category wins)
    cats = [dict(present=True, data=True, events=[[1, 1], [2]]), dict(present=True, data=True, events=[[1], [3]])]
    lay = map_layout(cats)
    for i in range(2):
        for j in range(2):
            ev = lay["ev"][i] + 0x14 * j
            cases.append(ids_case(lay, [ev, lay["sound"][i][min(j, len(lay["sound"][i]) - 1)], lay["sample"][i][j][0]]))
    cases.append(ids_case(lay, [lay["ev"][1], lay["sound"][1][0], lay["sample"][1][0][0]]))
    cases.append(ids_case(lay, [lay["ev"][0] + 0x28, lay["sound"][1][0], lay["sample"][1][0][0]]))
    # missing pieces: each of the three pointers null, containers absent or without data, no categories, a zero event count
    for mask in range(1, 8):
        ptrs = [lay["ev"][1], lay["sound"][1][0], lay["sample"][1][0][0]]
        for bit in range(3):
            if mask & (1 << bit):
                ptrs[bit] = 0
        cases.append(ids_case(lay, ptrs))
    for table in ([(0, 0), (1, lay["cat"][1])], [(1, 0), (1, lay["cat"][0])], [(1, lay["cat"][0])], []):
        cases.append(ids_case(lay, [lay["ev"][1], lay["sound"][1][0], lay["sample"][1][0][0]], table))
    cats = [dict(present=True, data=True, events=[[1], [1]], count=0), dict(present=True, data=True, events=[[1], [1]])]
    lay2 = map_layout(cats)
    cases.append(ids_case(lay2, [lay2["ev"][0], lay2["sound"][0][0], lay2["sample"][0][0][0]]))
    cases.append(ids_case(lay2, [lay2["ev"][1], lay2["sound"][1][0], lay2["sample"][1][0][0]]))
    # counts that disagree with the arrays: clamped to count - 1; a zero sound or sample count gives -1
    cats = [dict(present=True, data=True, events=[[2, 2], [1]], count=1, soundcounts=[1, 0])]
    lay3 = map_layout(cats)
    for j in range(2):
        cases.append(ids_case(lay3, [lay3["ev"][0] + 0x14 * j, lay3["sound"][0][j] + 0x2A, lay3["sample"][0][j][0] + 0x30]))
    cats = [dict(present=True, data=True, events=[[0, 1]], soundcounts=[0], hi=0xFFFF)]
    lay4 = map_layout(cats)
    cases.append(ids_case(lay4, [lay4["ev"][0], lay4["sound"][0][0], lay4["sample"][0][0][0]]))
    cases.append(ids_case(lay4, [lay4["ev"][0], lay4["sound"][0][0] + 0x2A, lay4["sample"][0][0][1]]))
    # three categories, every event with each of its sounds and samples: the category index and the clamps
    cats = [dict(present=True, data=True, events=[[2, 1], [3]]), dict(present=False, data=True, events=[[1]]),
            dict(present=True, data=True, events=[[1, 2, 3]]), dict(present=True, data=True, events=[[0], [2, 2]])]
    lay6 = map_layout(cats)
    for i, c in enumerate(cats):
        for j, ev in enumerate(c["events"]):
            for k in range(len(ev)):
                for m in range(ev[k] + 1):
                    cases.append(ids_case(lay6, [lay6["ev"][i] + 0x14 * j, lay6["sound"][i][j] + 0x2A * k, lay6["sample"][i][j][k] + 0x10 * m]))
    # many sounds in one event: the sound index is the quotient by 42 (the pointer at the 41st sound and the ones after it)
    cats = [dict(present=True, data=True, events=[[0] * 45, [1]])]
    lay5 = map_layout(cats)
    for k in (0, 1, 2, 39, 40, 41, 42, 43, 44, 45):
        cases.append(ids_case(lay5, [lay5["ev"][0], lay5["sound"][0][0] + 0x2A * k, lay5["sample"][0][0][min(k, 44)]]))
    cases.append(ids_case(lay5, [lay5["ev"][0] + 0x14, lay5["sound"][0][1], lay5["sample"][0][1][0]]))
    # an event pointer that is not in any events array (before every array; a record of another kind): not found
    cases.append(ids_case(lay4, [lay4["cat"][0], lay4["sound"][0][0], lay4["sample"][0][0][0]]))
    return [c for c in cases if c is not None]


def ptr_edges():
    cases = []
    cats = [dict(present=True, data=True, events=[[1]]), dict(present=False, data=False, events=[]), dict(present=True, data=False, events=[])]
    lay = map_layout(cats)
    for n in range(0, 4):
        for index in (0, 1, 2, 3, 4, n - 1, n, n + 1, 0xFFFFFFFF, 0x80000000, 0x7FFFFFFF):
            cases.append(make_case(K_PTR, layout=lay, cats=lay["table"][:n], a=[0, 0, 0, index & MASK, n, 0, 0, 0]))
    return cases


# ---------------------------------------------------------------- the small classes

def gen_event(op):
    def make(r):
        pos = [rbits(r) for _ in range(3)]
        return make_case(op, a=[rword(r), rword(r)] + pos + [0, 0, 0])
    return make


def event_edges(op):
    cases = []
    for cat in (0, 1, 0xFFFFFFFF, 0x80000000, 0x12345678):
        for event in (0, 1, 0xFFFFFFFF, 0x7FFFFFFF, 0xDEADBEEF):
            for pos in ([ONE, 0x40000000, 0x40400000], [0, 0x80000000, 0x7F800000], [0x7FC00000, 0xFF800000, 1]):
                cases.append(make_case(op, a=[cat, event] + pos + [0, 0, 0]))
    return cases


def gen_array(op):
    def make(r):
        return make_case(op, a=[r.choice([0, 1, 2, 3, 5, 8, 16, r.randrange(0, 17)]), r.choice([0, 1, 2, 0xFFFFFFFF, 0x7FFF, 0x8000, 0xFFFF, 0x10000, r.getrandbits(32)]),
                                0, 0, 0, 0, 0, r.choice([0, 0, 0, 1])])
    return make


def array_edges(op):
    cases = []
    for count in (0, 1, 2, 3, 7, 8, 16):
        for flag in (0, 1, 0xFFFF, 0x8000, 0x7FFF, 0xFFFFFFFF, 0x10000):
            for null in (0, 1):
                cases.append(make_case(op, a=[count, flag, 0, 0, 0, 0, 0, null]))
    return cases


# ---------------------------------------------------------------- hand-written cases of the placeholder operations

def st_of(**kw):
    st = default_state()
    st[S_EXTRA] = 1
    st[S_PARENT] = 1
    for key, value in kw.items():
        st[globals()["S_" + key.upper()]] = value
    return st


def pst_of(**kw):
    st = default_state()
    st[S_EXTRA] = 1
    for key, value in kw.items():
        st[globals()["S_" + key.upper()]] = value
    return st


def edge_placeholder(op):
    r = random.Random(700 + op)
    W = lambda **kw: make_case(op, **kw)
    cases = []
    if op in CTOR_OPS or op in (K_ECLASS, K_BCLASS, K_TCLASS):
        for mgr in (0, 1):
            for cat in (0, 1):
                cases.append(W(mgr=mgr, pstate=pst_of(catagory=cat)))
    elif op in (K_EDTOR, K_BDTOR, K_TDTOR):
        for flag in (0, 1, 2, 0xFFFFFFFF, 0x7FFF, 0x8000, 0xFFFF, 0x10000, 0x10001):
            for extra in (0, 1):
                for cat in (0, 1):
                    st = st_of(extra=extra, catagory=cat, listhandle=0x12345678)
                    cases.append(W(state=st, a=[flag] + [0] * 7))
    elif op == K_INIT:
        one = [make_event([make_sound([make_sample(f08=1500)])])]
        two = [make_event([make_sound([make_sample(f00=1, f04=0x100, f08=500), make_sample(f00=2, f04=0xFFFF, f08=900)]),
                           make_sound([make_sample(f00=3, f04=0xFFFF, f08=100)], f1e=0xFFFF)], f12=3)]
        none = [make_event([make_sound([])])]
        for events in (one, two, none):
            for now in (0, 1000, 0xFFFFFF00, 0xFFFFFFFF):
                for flags in (0, F_LOOPING, 0xFFFFFFFF):
                    for pseudo in ((0, 0), (5, 20), (20, 5), (0xFFFF, 0)):
                        for branching in (False, True):
                            ev = [dict(events[0])]
                            ev[0]["sounds"] = [dict(sd, f10=pseudo[0], f12=pseudo[1]) for sd in events[0]["sounds"]]
                            if branching:
                                # the Branching element's ChooseRandomSound is the one Init calls: a branch to the first sound, in range
                                for sd in ev[0]["sounds"]:
                                    sd["branches"] = [(0, 0, 255, 0)]
                            cases.append(W(events=ev, state=st_of(flags=flags, vcindex=7), pstate=pst_of(vcindex=3, sound=31 if branching else 0), a=[21, now, 0, 0, 0, 0, 0, 0],
                                           q={"Random": [0x00010000, 0x7FFF0000, 0xFFFF0000], "RangedRandom": [3, 0, 99]}, branching=branching))
        # the Branching element's Init when the choice fails (no branch in range, no branches) and the sample is missing
        for branches, sound in (([], 31), ([(0, 5, 6, 0)], 31), ([(0, 0, 255, 0)], 0)):
            sd0 = make_sound([make_sample(f08=100)])
            sd0["branches"] = list(branches)
            cases.append(W(events=[make_event([sd0])], state=st_of(), pstate=pst_of(sound=sound, values=0), a=[21, 5, 0, 0, 0, 0, 0, 0], branching=True,
                           q={"Random": [0], "RangedRandom": [0]}))
    elif op == K_GETPARAM:
        for mask in (0, 1, 2, 3, 4, 0x10000):
            for f18 in (0, 1, 2, 3):
                for f1c in (0, 1, 2, 3):
                    for values in (0x00000000, 0x0A0B0C0D, 0xFFFFFFFF, 0x80000000):
                        cases.append(W(events=[make_event([make_sound(f18=f18, f1c=f1c)])], state=st_of(sound=31), pstate=pst_of(values=values),
                                       a=[mask] + [0] * 7))
    elif op == K_UPDATE:
        for handle in (0, 5):
            for flags in (0, F_NONPOS, 0xFFFFFFFF):
                for err in (0, -1):
                    cases.append(W(state=st_of(handle=handle, flags=flags), pstate=pst_of(posx=ONE, posy=0x40000000, posz=0x40400000, velx=ONE, priority=0x00050003),
                                   q={"SysErr": [err]}))
        nan = 0x7FC00000
        for pos in ([nan, 0, 0], [0x7F800000, 0, 0], [0, 0, 0xFF800000]):
            cases.append(W(state=st_of(handle=5), pstate=pst_of(posx=pos[0], posy=pos[1], posz=pos[2])))
    elif op == K_CHOOSE:
        def sd_with(weights_lo_hi, f1e):
            return make_sound(f1e=f1e)
        # sounds: 0 the branching sound, 1..3 targets with weights
        targets = [make_sound(f1e=w) for w in (10, 20, 30)]
        for branches in ([], [(1, 0, 255, 0)], [(1, 0, 10, 0), (2, 11, 20, 0), (3, 21, 255, 0)], [(1, 5, 5, 0), (2, 5, 5, 0), (3, 5, 5, 0)], [(1, 10, 5, 0)],
                         [(1, 0, 255, 0), (2, 0, 255, 0), (3, 0, 255, 0)]):
            sd0 = make_sound(samples=[make_sample()], f1e=1)
            sd0["branches"] = list(branches)
            events = [make_event([sd0] + [dict(t) for t in targets])]
            for parameter in (0, 4, 5, 6, 10, 11, 20, 21, 255):
                for pick in (0, 1, 9, 10, 11, 29, 30, 31, 59, 60, 61, 0xFFFFFFFF):
                    cases.append(W(events=events, state=st_of(event=21), pstate=pst_of(sound=31, values=parameter, flags=0, event=0),
                                   q={"RangedRandom": [pick]}))
        # the parent has no sound: the event's first sound is taken (with and without samples; only the low half of the count counts)
        for samples, hi in (([make_sample()], 0), ([], 0), ([], 1), ([], 0xFFFF), ([make_sample()], 0xFFFF)):
            sd0 = make_sound(samples=samples, hi=hi)
            sd0["branches"] = [(1, 0, 255, 0)]
            events = [make_event([sd0, make_sound(f1e=7)])]
            for pick in (0, 7, 8):
                cases.append(W(events=events, state=st_of(event=21), pstate=pst_of(sound=0, values=3), q={"RangedRandom": [pick]}))
        # the sound without branches clears both sounds
        cases.append(W(events=[make_event([make_sound()])], state=st_of(event=21, sound=31), pstate=pst_of(sound=31)))
        cases.append(W(events=[make_event([make_sound()])], state=st_of(event=21), pstate=pst_of(sound=0)))
    elif op in (K_FILL, K_TFILL):
        for length in (0, 500, 1000, 1001, 1500, 2000, 2500, 3000):
            for elapsed in (0, 1, 999, 1000, 1001, 1999, 2000, 2001, 5000):
                for flags in (0, F_LOOPING, F_NONPOS, F_LOOPFOREVER | F_PITCHSET, F_NONPOS | F_LOOPFOREVER):
                    st = st_of(flags=flags, endtime=100000, pausetime=0, sound=31, sample=71)
                    sd = make_sound([make_sample(f08=length)])
                    cases.append(W(events=[make_event([sd])], state=st, a=[(100000 - length - 250 + elapsed) & MASK] + [0] * 7,
                                   q={"Bank": [1], "Rolloff": [fbits(0.25)], "DistMul": [0x40000000], "QSound": [1]}))
        for f22 in (fbits(1.0), fbits(2.0), 0x40000001, fbits(2.1), fbits(0.0), 0x7FC00000):
            for mult in (ONE, fbits(2.0), fbits(0.5), 0):
                for maxflag in (0, F_MAXDIST):
                    st = st_of(flags=maxflag, endtime=100000, maxdist=fbits(2.0), sound=31, sample=71)
                    sd = make_sound([make_sample(f08=5000)], f22=f22)
                    cases.append(W(events=[make_event([sd])], state=st, a=[100000 - 5250 + 10] + [0] * 7,
                                   q={"Bank": [1], "Rolloff": [fbits(0.25)], "DistMul": [mult, mult], "QSound": [0]}))
        for qsound in (0, 1):
            for bank in (0, 1):
                for mgr in (0, 1):
                    cases.append(W(mgr=mgr, state=st_of(sound=31, sample=71, endtime=100000), a=[99000] + [0] * 7,
                                   q={"Bank": [bank], "QSound": [qsound], "DistMul": [ONE]}))
        for sound, sample in ((0, 71), (31, 0), (0, 0)):
            cases.append(W(state=st_of(sound=sound, sample=sample), q={"Bank": [1]}))
        if op == K_TFILL:
            for info in ([0, 0x2000, 1], [0, 0xFFFFFFFF, 1], [0xFFFFFFFF, 0, 1], [0, 0, 0]):
                cases.append(W(state=st_of(sound=31, sample=71, endtime=100000), a=[99000] + [0] * 7, info=info, q={"Bank": [1]}))
    return cases


def gen_edges(op):
    return lambda: edge_placeholder(op)


for _op in range(27):
    if _op in PLACEHOLDER_OPS:
        gen_fn, edge_fn = gen_placeholder(_op), (lambda op=_op: edge_placeholder(op))
    elif _op == K_IDS:
        gen_fn, edge_fn = gen_ids, ids_edges
    elif _op == K_PTR:
        gen_fn, edge_fn = gen_ptr, ptr_edges
    elif _op in (K_EVCTOR, K_EVTYPE, K_EVCAT, K_EVEVENT, K_EVPOS):
        gen_fn, edge_fn = gen_event(_op), (lambda op=_op: event_edges(op))
    elif _op in (K_ARRCTOR, K_ARRCONV, K_ARRDTOR):
        gen_fn, edge_fn = gen_array(_op), (lambda op=_op: array_edges(op))
    else:   # the two small constructors with nothing to vary
        gen_fn, edge_fn = (lambda r, op=_op: make_case(op)), (lambda: [])
    TESTS.append(FnTest("%s 0x%08x" % (NAME[_op], ADDR[_op]), ppc_run, host_run, edge=edge_fn, rnd=gen_fn))
