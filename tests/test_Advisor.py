"""Differential test for the advisor: src/game/advisor/Advisor.cpp (the state machine), src/game/TPCamera.cpp, src/game/advisor/TPAdvisor.cpp (the model and its four
slots), src/game/TPEmbed.cpp, src/game/TPMesh.cpp, src/game/rides/RideAnim.cpp (the small callees), ScaleLMatrixNoTrans (src/game/LMatrix.cpp) and SND_DuckVolume
(src/game/Sound.cpp, src/game/park/ParkSound.cpp), against libtpw_unit_Advisor.dylib (tests/unit_Advisor.cpp).

A case is a SCRIPT over a small world: up to three advisor meshes ("variants": the mesh header with its nodes, mini nodes
and embedded entries as a file image, the ride set that is its instance with a group of animations and time controls, the
mesh record, the type set) that the script installs in the four slots by hand or by calling LoadAdvisor. One operation
calls a function of the unit or sets something up (the clock, a field of the advisor's state, the lip-sync data, a node's
flags). After every operation the answer, the whole state (the statics of Advisor.cpp, the slots and their lights, the
words the code changes in the instances: flags, matrix pointer, node flags, time control words, the root matrix), the
number of calls the fakes have recorded and the heap traffic are compared; the call logs (every fake's arguments) are
compared at the end.

The world is built by ONE piece of code over a Side (the PowerPC side writes big-endian words at addresses of the
emulator's scratch memory, with the Mac's record layouts; the host side writes little-endian words and native pointers
at the offsets the library reports, the file images in the arena as Ptr32 fields need), so both sides hold the same
numbers. Every object has a label (a fixed number) so that pointers compare as labels.

The fakes (both sides, see unit_Advisor.cpp): the clock (the case's `clock` operation sets what ReadFixed and
GetPauseFlag answer), rand (MSL's generator from the case's seed, or the queue "Rand"), the sound wrappers and the effect
manager (answers from queues, default: a started sound has handle 1 and keeps playing, EM_SetEmitersOn answers 1), the
ride set loader (the case queues the sets it hands out), and the animation model: SetCurRideAnim queues the animation in
time control 0 and the next (anim & 3) + 1 AnimateRideSet calls run it.

Not reproduced (the original faults or reads uninitialised memory): ticks with a speaking advisor and no ride set; a
mouth count of 0 while the mouth is open; a sequence of 20 words without its end; node numbers past the node table;
animation lengths indexed by 0 or a number past 20; a name that is not found in a mesh whose mini nodes are counted in the
node count (the search then reads on past the full nodes, as the original does).
"""
import ctypes as C
import pathlib
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_Advisor"
MASK = 0xFFFFFFFF
DEAD = 0xDEADBEEF

# ---- the original's addresses
STATE, STATE_SIZE = 0x102B354C, 0x168
STORE, SLOT_SIZE, SLOTS = 0x102B56FC, 0xBC, 4
IN_USE, BUFFER, BUFFER_SIZE = 0x102B36B4, 0x102B36B8, 1000
ADVISOR_ON = 0x1038071C + 0x34
SOUND_SETTINGS, SOUND_SETTINGS_SIZE = 0x102F7C40, 0x40
BANKS, THEME_BANK = 0x102F7C94, 0x102F7CB4
CLOCK = 0x1037F048
CANNED = 0x1027D944

ADDR = dict(
    ADV_Pause=0x10007D54, ADV_UnPause=0x10007DD4, ADV_Init=0x10007E5C, ADV_Close=0x10007FC4,
    GenerateAnimSequence=0x1000804C, SetCannedSequence=0x100081F4, SetAdvisorHatHand=0x10008330,
    DressAdvisor=0x100084A0, ADV_Tick2=0x100091D8, ADV_Tick=0x10009298,
    LoadAdvisor=0x10031BC0, DisableAdvisorDisplay=0x10031DE8, TickAdvisor=0x10031ED0, DestroyAdvisor=0x10031F38,
    StartAdvisorAnimation=0x10031FF4, EnableAdvisorAccessory=0x100320A8, DisableAdvisorAccessory=0x10032178,
    GetAdvisorRideAnim=0x1003224C, ctor=0x10032454,
    FindDataNodeIndexByID=0x101FFDC8, TempHack=0x101FFF90, SetMeshInUISpace=0x1006553C,
    ScaleLMatrixNoTrans=0x100C1C3C, GetDeferredAnim=0x100C7A74, GetAnimLength=0x100C7BB4, SND_DuckVolume=0x100DEAC8)
# Real on the PowerPC side: the unit's functions, SetMatrixIdentity (linked with ScaleLMatrixNoTrans) and the light's
# constructor with the two functions it calls (CMapLinkBase's constructor, CLight::SetNormal and SetColor).
REAL = set(ADDR.values()) | {0x100C0E28, 0x10007A98, 0x10007B7C, 0x1000784C, 0x10007964}

# ---- the layouts. A name is "kind.field"; the Mac's offsets here, the host's from the library.
STATE_FIELDS = [   # (field, Mac offset, words, pointer)
    ("mFlags", 0x00, 1, 0), ("mBaseTime", 0x04, 1, 0), ("mEndTime", 0x08, 1, 0), ("mSpeaking", 0x0C, 1, 0),
    ("mMouthOpen", 0x10, 1, 0), ("mWhich", 0x14, 1, 0), ("mSoundSet", 0x18, 1, 0), ("mSound", 0x1C, 1, 0),
    ("mSoundHandle", 0x20, 1, 0), ("mHat", 0x24, 1, 0), ("mHand", 0x28, 1, 0), ("mDressAfterAnim", 0x2C, 1, 0),
    ("mDressNow", 0x30, 1, 0), ("mSoundTime", 0x34, 1, 0), ("mHeadNode", 0x38, 2, 0), ("mMouthNodes", 0x40, 10, 0),
    ("mAnimLengths", 0x68, 20, 0), ("mSequence", 0xB8, 20, 0), ("mSequencePos", 0x108, 1, 0),
    ("mAnimIndex", 0x10C, 1, 0), ("mMouthCount", 0x110, 1, 0), ("mMouthTime", 0x114, 1, 0), ("mKeyTime", 0x118, 1, 0),
    ("mClockPaused", 0x11C, 1, 0), ("mPauseStart", 0x120, 1, 0), ("mPausedTotal", 0x124, 1, 0),
    ("mMouthWanted", 0x128, 1, 0), ("mMouthShown", 0x12C, 1, 0), ("mMouthChange", 0x130, 1, 0),
    ("mSyncSize", 0x134, 1, 0), ("mSyncPos", 0x138, 1, 0), ("mSyncActive", 0x13C, 1, 0), ("mSyncData", 0x140, 1, 1),
    ("mAccessoriesOn", 0x144, 4, 0), ("mAccessoriesOff", 0x154, 4, 0), ("mPausedSound", 0x164, 1, 0)]
LIGHT_FIELDS = [("mColor", 4, 4), ("mPosition", 0x14, 3), ("mF20", 0x20, 3), ("mNormal", 0x2C, 3), ("mData", 0x38, 3),
                ("mRadius", 0x44, 1), ("mRadiusSquared", 0x48, 1), ("mInverseRadiusSquared", 0x4C, 1),
                ("mFlags", 0x50, 1), ("mColorWord", 0x54, 1)]
MAC = {"slot.mSet": 0, "slot.mInstance": 4, "slot.mMatrix": 8, "slot.size": SLOT_SIZE, "slot.light": 0x48,
       "set.mFlags": 4, "set.mHeads": 8, "set.mTimeCount": 0xE, "set.mTimeControls": 0x10, "set.mGroups": 0x18,
       "set.mFC0": 0xC0, "set.mFC4": 0xC4, "set.mMatrix": 0xC8, "set.size": 0x100,
       "group.mFlags": 0, "group.mCount": 4, "group.mEntries": 8, "group.size": 12,
       "entry.mIndex": 0, "entry.mAnimation": 4, "entry.size": 8, "mesh.mF00": 0, "mesh.mHeader": 4, "mesh.size": 0x40,
       "control.mF24": 0x24, "control.mF28": 0x28, "control.size": 0x38, "sound.mChanged": 0x28, "sound.mF2C": 0x2C}
for _name, _off, _n, _p in STATE_FIELDS:
    MAC["state." + _name] = _off

# Strings the nodes can be named: the six names the code looks for (in the cases the file uses) and others.
NAMES = ["Bug Head", "Mouth - Normal", "Mouth - Aah", "Mouth - Eee", "Mouth - Ooh", "Mouth - Sss", "BUG HEAD",
         "mouth - normal", "MOUTH - AAH", "Mouth - EEE", "mouth - ooh", "MOUTH - SSS", "Body", "Right hand", "Hardhat",
         "bug hea", "Mouth - Normal ", "Mouth Normal", "", "x"]
PATHS = [None, "Data:Global:Advisor", ""]
FOLDERS = [None, "Data:Global", "Textures"]
VARIANTS = [None, "", "Halloween", "A" * 40]
ANIM_STRIDE = 0x48


def f32bits(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def canon(word):
    """NaNs compare as one value (a signalling NaN is quietened differently by the two sides)."""
    word &= MASK
    return 0x7FC00000 if word & 0x7F800000 == 0x7F800000 and word & 0x007FFFFF else word


def align(x, a):
    return (x + a - 1) & ~(a - 1)


# =============================================================================================== the sides


class Side:
    """What both sides share: the world's building and dumping. A subclass supplies the memory (rd, wr, alloc), the
    layout (off, psize, big), the labels and the function calls."""

    def pack(self, fmt, *values):
        return struct.pack(("<", ">")[self.big] + fmt, *values)

    def unpack(self, fmt, data):
        return struct.unpack(("<", ">")[self.big] + fmt, data)

    def u32(self, address):
        return self.unpack("I", self.rd(address, 4))[0]

    def w32(self, address, value):
        self.wr(address, self.pack("I", value & MASK))

    def u16(self, address):
        return self.unpack("H", self.rd(address, 2))[0]

    def w16(self, address, value):
        self.wr(address, self.pack("H", value & 0xFFFF))

    def ptr(self, address):
        return self.unpack("I" if self.psize == 4 else "Q", self.rd(address, self.psize))[0]

    def wptr(self, address, value):
        self.wr(address, self.pack("I" if self.psize == 4 else "Q", value))

    def lab(self, value):
        return 0 if value == 0 else self.labels.get(value, -1)

    def new_native(self, label, size):
        address = self.alloc(size, False)
        self.name(address, label)
        return address

    # ---- the world
    def build_variant(self, k, spec):
        """The advisor mesh `spec` describes, as variant k: returns a dict of its addresses."""
        nodes, minis = spec["nodes"], spec["minis"]
        n, m = len(nodes), len(minis)
        every = spec["all"]
        records = max(every, n, 1)
        strings = {}
        for _, text in nodes + minis:
            strings.setdefault(text, 0)
        strings.setdefault("ext", 0)
        offset = 0xB8
        for text in strings:
            strings[text] = offset
            offset += len(text) + 1
        nodes_at = align(offset, 16)
        minis_at = nodes_at + records * 0xA0
        embedded_at = minis_at + max(m, 1) * 0x58
        root_at = embedded_at + max(len(spec["embedded"]), 1) * 0x14
        groups = spec["groups"]
        frames = []                          # (group, index) -> the frames word, in file order
        for kind in range(12):
            for frames_word in groups[kind][2]:
                frames.append(frames_word)
        frames_at = root_at + 0x58
        total = frames_at + max(len(frames), 1) * ANIM_STRIDE + 0x40
        image = self.alloc(total, True)
        header = image
        fp = lambda rel: self.fp(image, rel)   # noqa: E731
        self.w32(header + 0x30, spec["hflags"])
        self.w16(header + 0x42, every)
        self.w16(header + 0x44, n)
        self.w16(header + 0x46, spec["f46"])
        self.w16(header + 0x48, len(spec["embedded"]))
        self.w32(header + 0x70, fp(nodes_at))
        self.w32(header + 0x74, fp(minis_at))
        self.w32(header + 0x78, fp(root_at))
        self.w32(header + 0x7C, fp(embedded_at))
        for text, at in strings.items():
            self.wr(image + at, text.encode("latin-1") + b"\0")
        for i in range(records):
            flags, text = nodes[i] if i < n else (spec["ext_flags"], "ext")
            self.w32(image + nodes_at + i * 0xA0, flags)
            self.w32(image + nodes_at + i * 0xA0 + 0x54, fp(strings[text]))
        for i, (flags, text) in enumerate(minis):
            self.w32(image + minis_at + i * 0x58, flags)
            self.w32(image + minis_at + i * 0x58 + 0x54, fp(strings[text]))
        for i, (flags, ident) in enumerate(spec["embedded"]):
            self.w32(image + embedded_at + i * 0x14, flags)
            self.w32(image + embedded_at + i * 0x14 + 4, ident)
        self.w32(image + root_at, 0x200)
        for i, word in enumerate(spec["root"]):
            self.w32(image + root_at + 0x10 + 4 * i, word)
        for i, word in enumerate(frames):
            self.w32(image + frames_at + i * ANIM_STRIDE + 8, word)
        self.name(image, 80 + k)
        mesh = self.new_native(70 + k, self.off("mesh.size"))
        self.w32(mesh + self.off("mesh.mF00"), spec["mesh_flags"])
        self.wptr(mesh + self.off("mesh.mHeader"), header)
        tc = len(spec["controls"])
        controls = self.new_native(90 + k, max(tc, 1) * self.off("control.size"))
        for c, (kind, index) in enumerate(spec["controls"]):
            self.w32(controls + c * self.off("control.size") + self.off("control.mF24"), kind)
            self.w32(controls + c * self.off("control.size") + self.off("control.mF28"), index)
        inst = self.new_native(60 + k, self.off("set.size"))
        self.w32(inst + self.off("set.mFlags"), spec["setflags"])
        self.wptr(inst + self.off("set.mHeads"), mesh)
        self.w16(inst + self.off("set.mTimeCount"), tc)
        self.wptr(inst + self.off("set.mTimeControls"), controls)
        self.w16(inst + self.off("set.mFC0"), spec["fc0"][0])
        self.w16(inst + self.off("set.mFC0") + 2, spec["fc0"][1])
        self.w16(inst + self.off("set.mFC4"), spec["fc4"][0])
        self.w16(inst + self.off("set.mFC4") + 2, spec["fc4"][1])
        position = 0
        for kind in range(12):
            gflags, count, words = groups[kind]
            base = inst + self.off("set.mGroups") + kind * self.off("group.size")
            self.w32(base + self.off("group.mFlags"), gflags)
            self.w32(base + self.off("group.mCount"), count)
            entries = self.alloc(max(len(words), 1) * self.off("entry.size"), False)
            for j, _ in enumerate(words):
                at = entries + j * self.off("entry.size")
                self.w32(at + self.off("entry.mIndex"), j)
                self.wptr(at + self.off("entry.mAnimation"), image + frames_at + (position + j) * ANIM_STRIDE)
            position += len(words)
            self.wptr(base + self.off("group.mEntries"), entries)
        type_set = self.new_native(50 + k, self.off("set.size"))
        self.w32(type_set + self.off("set.mFlags"), 0x1234)
        return dict(spec=spec, image=image, header=header, mesh=mesh, controls=controls, inst=inst, type=type_set,
                    nodes_at=nodes_at, minis_at=minis_at, root_at=root_at, records=records, tc=tc)

    def install(self, slot, variant):
        base = self.slot(slot)
        v = self.variants[variant] if variant is not None else None
        self.wptr(base + self.off("slot.mSet"), v["type"] if v else 0)
        self.wptr(base + self.off("slot.mInstance"), v["inst"] if v else 0)

    def set_state(self, name, index, value):
        self.w32(self.state_address + self.off("state." + name) + 4 * index, value)

    def set_sync(self, mode, values):
        """The lip-sync data: none (0), the static buffer (1) or a block of its own (2: labelled, one per case)."""
        field = self.state_address + self.off("state.mSyncData")
        if mode == 0:
            self.wptr(field, 0)
            return
        if mode == 1:
            self.wptr(field, self.buffer_address)
            self.w32(self.in_use_address, 1)
            for i, word in enumerate(values):
                self.w32(self.buffer_address + 4 * i, word)
            return
        block = self.alloc_plain(max(len(values), 1) * 4)
        self.name(block, 100 + self.blocks)
        self.blocks += 1
        for i, word in enumerate(values):
            self.w32(block + 4 * i, word)
        self.wptr(field, block)

    # ---- one operation
    def apply(self, item):
        """Runs one script item; returns the answer (None for a set-up item)."""
        op = item[0]
        a = list(item[1:])
        if op == "clock":
            self.set_clock(a[0], a[1])
        elif op == "state":
            self.set_state(a[0], a[1], a[2])
        elif op == "install":
            self.install(a[0], a[1])
        elif op == "node":
            v = self.variants[a[0]]
            self.w32(v["image"] + v["nodes_at"] + a[1] * 0xA0, a[2])
        elif op == "mini":
            v = self.variants[a[0]]
            self.w32(v["image"] + v["minis_at"] + a[1] * 0x58, a[2])
        elif op == "control":
            v = self.variants[a[0]]
            at = v["controls"] + a[1] * self.off("control.size")
            self.w32(at + self.off("control.mF24"), a[2])
            self.w32(at + self.off("control.mF28"), a[3])
        elif op == "setflags":
            self.w32(self.variants[a[0]]["inst"] + self.off("set.mFlags"), a[1])
        elif op == "sync":
            self.set_sync(a[0], a[1:])
        elif op == "on":
            self.set_advisor_on(a[0])
        elif op == "banks":
            self.set_banks(*a)
        elif op == "matrix":
            for i, word in enumerate(a):
                self.w32(self.scratch + 4 * i, word)
        else:
            return self.run(op, a)
        return None

    def run(self, op, a):
        v = lambda i: self.variants[a[i]]   # noqa: E731
        simple = dict(ADV_Init=1, ADV_Close=2, ADV_Pause=3, ADV_UnPause=4, ADV_Tick=5, ADV_Tick2=6, DressAdvisor=8)
        if op in simple:
            return self.call(simple[op], [])
        if op == "TickAdvisor":
            return self.call(7, [a[0]])
        if op == "SetAdvisorHatHand":
            return self.call(9, [a[0]])
        if op == "GenerateAnimSequence":
            return self.call(10, [a[0]])
        if op == "SetCannedSequence":
            return self.call(11, [a[0], a[1]])
        if op == "LoadAdvisor":
            # (slot, variant, texts, load, instance): the sets the fakes hand out are queued only when they are used
            slot, variant, texts, load, instance = a
            if load:
                self.push("LoadSet", v(1)["type"])
            if instance:
                self.push("InstanceSet", v(1)["inst"])
            self.push("LoadRideAnimSet", load)
            self.push("InstanceRideAnimSet", instance)
            return self.call(12, [texts[0], texts[1], texts[2], slot])
        if op == "DestroyAdvisor":
            return self.call(13, [a[0]])
        if op == "DisableAdvisorDisplay":
            return self.call(14, [a[0]])
        if op == "StartAdvisorAnimation":
            return self.call(15, [a[0], a[1]])
        if op == "EnableAdvisorAccessory":
            return self.call(16, [a[0], a[1]])
        if op == "DisableAdvisorAccessory":
            return self.call(17, [a[0], a[1]])
        if op == "GetAdvisorRideAnim":
            return self.call(18, [a[0]])
        if op == "SND_DuckVolume":
            return self.call(19, [a[0]])
        if op == "FindDataNodeIndexByID":
            return self.call(20, [v(0)["inst"], a[1], a[2]])
        if op == "TempHack":
            return self.call(21, [v(0)["inst"], a[1], a[2]])
        if op == "SetMeshInUISpace":
            return self.call(22, [v(0)["inst"] + self.off("set.mHeads")])
        if op == "GetDeferredAnim":
            return self.call(23, [v(0)["inst"], a[1], a[2]])
        if op == "GetAnimLength":
            return self.call(24, [v(0)["inst"], a[1], a[2]])
        if op == "ScaleLMatrixNoTrans":
            return self.call(25, [self.scratch, a[0], a[1], a[2]])
        raise ValueError(op)

    # ---- everything the code may have changed
    def dump(self):
        out = []
        word = lambda block, at: struct.unpack_from(("<", ">")[self.big] + "I", block, at)[0]   # noqa: E731
        base = self.state_address
        state_end = max(self.off("state." + name) + 4 * words for name, _, words, _ in STATE_FIELDS)
        block = self.rd(base, state_end + 8)
        for name, mac, words, is_pointer in STATE_FIELDS:
            for i in range(words):
                at = self.off("state." + name) + 4 * i
                if is_pointer:
                    out.append(self.lab(struct.unpack_from(("<", ">")[self.big] + ("I" if self.psize == 4 else "Q"), block, at)[0]))
                else:
                    out.append(word(block, at))
        out.append(self.u32(self.in_use_address))
        slot_size = self.off("slot.size")
        for s in range(SLOTS):
            slot = self.slot(s)
            block = self.rd(slot, slot_size)
            pointer = lambda at: struct.unpack_from(("<", ">")[self.big] + ("I" if self.psize == 4 else "Q"), block, at)[0]   # noqa: E731
            out.append(self.lab(pointer(self.off("slot.mSet"))))
            out.append(self.lab(pointer(self.off("slot.mInstance"))))
            matrix = self.off("slot.mMatrix")
            out += [canon(word(block, matrix + 4 * i)) for i in range(16)]
            body = self.light_body(s)
            lblock = self.rd(body, 0x60) if self.big else block
            for name, mac, words in LIGHT_FIELDS:
                at = self.light_offset(name, mac)
                out += [canon(word(lblock, at + 4 * i)) for i in range(words)]
            at = self.light_offset("mNext", 0x58)
            out.append(self.lab(struct.unpack_from(("<", ">")[self.big] + ("I" if self.psize == 4 else "Q"), lblock, at)[0]))
        for k in sorted(self.variants):
            v = self.variants[k]
            inst = v["inst"]
            out.append(self.u32(inst + self.off("set.mFlags")))
            out.append(self.lab(self.ptr(inst + self.off("set.mMatrix"))))
            out.append(self.u32(v["mesh"] + self.off("mesh.mF00")))
            out.append(self.u32(v["header"] + 0x30))
            nodes = self.rd(v["image"] + v["nodes_at"], v["records"] * 0xA0)
            out += [word(nodes, i * 0xA0) for i in range(v["records"])]
            m = len(v["spec"]["minis"])
            if m:
                minis = self.rd(v["image"] + v["minis_at"], m * 0x58)
                out += [word(minis, i * 0x58) for i in range(m)]
            if v["tc"]:
                size = self.off("control.size")
                controls = self.rd(v["controls"], v["tc"] * size)
                for c in range(v["tc"]):
                    out += [word(controls, c * size + self.off("control.mF24")), word(controls, c * size + self.off("control.mF28"))]
            root = self.rd(v["image"] + v["root_at"] + 0x10, 0x40)
            out += [canon(word(root, 4 * i)) for i in range(16)]
        scratch = self.rd(self.scratch, 0x40)
        out += [canon(word(scratch, 4 * i)) for i in range(16)]
        out += [self.u32(self.sound_address + self.off("sound.mChanged")),
                self.u32(self.sound_address + self.off("sound.mF2C"))]
        return out + self.heap()      # blocks operator new, new[], delete and delete[] have handled


class PpcSide(Side):
    big = True
    psize = 4

    SAVED = [(STATE, STATE_SIZE), (STORE, SLOT_SIZE * SLOTS), (IN_USE, 4), (BUFFER, BUFFER_SIZE), (ADVISOR_ON, 1),
             (SOUND_SETTINGS, SOUND_SETTINGS_SIZE), (BANKS, 0x28), (THEME_BANK, 4)]

    def __init__(self, e, case):
        self.e = e
        e.heap_in_scratch = True
        del e.alloc_log[:]
        self.saved = [(a, e.read(a, n)) for a, n in self.SAVED]
        self.labels = {}
        self.blocks = 0
        self.variants = {}
        self.clock_now, self.clock_paused = 0, 0
        self.rand_state = case["seed"] & MASK
        self.anim_left = 0
        self.state_address, self.buffer_address, self.in_use_address = STATE, BUFFER, IN_USE
        self.sound_address = SOUND_SETTINGS
        # zero statics, as the game's BSS is at the start
        for a, n in self.SAVED:
            e.write(a, b"\0" * n)
        self.system = e.data_import("gei_System")
        self.map = e.alloc(16, b"\xa5" * 16)
        e.write(e.data_import("gei_MapWho"), struct.pack(">I", self.map))
        self.scratch = e.alloc(0x40, b"\xa5" * 0x40)
        fixed = {self.system: 1, self.map: 2, CLOCK: 3, self.scratch: 4, BUFFER: 5}
        for s in range(SLOTS):
            fixed.update({STORE + s * SLOT_SIZE: 10 + s, STORE + s * SLOT_SIZE + 4: 20 + s,
                          STORE + s * SLOT_SIZE + 8: 30 + s, STORE + s * SLOT_SIZE + 0x50: 40 + s})
        self.L = e.cl = CL.PpcLog(fixed, case.get("q"), first_new=200)
        self.labels = self.L.labels
        self.stubs = self.make_stubs()
        self.old = self.stubs.activate(e)
        # the static initialiser: each slot's constructor (its calls are not part of the case)
        for s in range(SLOTS):
            T.call_ppc(e, ADDR["ctor"], [STORE + s * SLOT_SIZE])
        del self.L.log[:]

    def close(self):
        CL.Stubs.deactivate(self.e, self.old)
        for a, data in self.saved:
            self.e.write(a, data)

    # ---- layout, memory, labels
    def off(self, key):
        return MAC[key]

    def rd(self, address, n):
        return self.e.read(address, n)

    def wr(self, address, data):
        self.e.write(address, data)

    def alloc(self, size, file):
        return self.e.alloc(size, b"\xa5" * size)

    alloc_plain = lambda self, size: self.alloc(size, False)   # noqa: E731

    def fp(self, image, rel):
        return image + rel

    def name(self, address, label):
        self.L.labels[address] = label

    def slot(self, s):
        return STORE + s * SLOT_SIZE

    def light_body(self, s):
        return self.u32(self.slot(s) + MAC["slot.light"])

    def light_offset(self, name, mac):
        return mac

    def push(self, key, value):
        self.L.queues.setdefault(key, []).append(value)

    def set_clock(self, now, paused):
        self.clock_now, self.clock_paused = now & MASK, paused & MASK

    def set_advisor_on(self, value):
        self.e.write(ADVISOR_ON, bytes([value & 0xFF]))

    def set_banks(self, b3, b5, b8):
        self.w32(BANKS + 0xC, b3)
        self.w32(BANKS + 0x14, b5)
        self.w32(THEME_BANK, b8)

    # ---- the fakes
    def queue_animation(self, set_address, kind, anim):
        """SetCurRideAnim's model: the animation goes into time control 0 (words 0x24 and 0x28)."""
        if set_address == 0:
            return
        controls = self.u32(set_address + 0x10)
        count = self.u16(set_address + 0xE)
        if controls == 0 or count == 0:
            return
        self.w32(controls + 0x24, kind)
        self.w32(controls + 0x28, anim)
        self.anim_left = 0 if kind == 0xC else (anim & 3) + 1

    def make_stubs(self):
        S, R, e, L = CL.Stubs(), CL.recorder, self.e, self.L
        S.real.update(REAL)
        ret = lambda key, dflt: (lambda e, L: L.ret(key, dflt) & MASK)   # noqa: E731

        S.internal(0x1014733C, R("ReadFixed", "L", ret=lambda e, L: self.clock_now))
        S.internal(0x101474B8, R("GetPauseFlag", "L", ret=lambda e, L: self.clock_paused))
        S.imp("UpdateLights__7CSystemFv", R("UpdateLights", "L"))
        S.imp("Link__7CMapWhoFP12CMapLinkBasei", R("Link", "LLi", ret=lambda e, L: 1))
        S.imp("Unlink__7CMapWhoFP12CMapLinkBasei", R("Unlink", "LLi", ret=lambda e, L: 1))
        S.internal(0x100680FC, R("SetupDirectory", "sss", ret=lambda e, L: 1))

        def load(e):
            name, flags, out, loaded, count = e.cstr(e.r(3)), e.r(4), e.r(5), e.r(6), e.r(7)
            L.rec("LoadRideAnimSet", [flags, L.lab(out), L.lab(loaded), count], [name[:159]])
            result = L.ret("LoadRideAnimSet", 1) & MASK
            if result:
                self.w32(out, L.ret("LoadSet"))
            e.set_r(3, result)
        S.internal(0x1006840C, load)

        def instance(e):
            s, heap, flags, out, count = e.r(3), e.r(4), e.r(5), e.r(6), e.r(7)
            L.rec("InstanceRideAnimSet", [L.lab(s), L.lab(heap), flags, L.lab(out), count])
            if s == 0:
                e.set_r(3, 0)
                return
            result = L.ret("InstanceRideAnimSet", 1) & MASK
            if result:
                self.w32(out, L.ret("InstanceSet"))
            e.set_r(3, result)
        S.internal(0x10062E1C, instance)
        S.internal(0x100C7768, R("InitRideAnimSet", "Liii", ret=lambda e, L: 1))
        S.internal(0x10063338, R("DestroyRideAnimSetInstance", "LLu"))
        S.internal(0x1006A5E8, R("DestroyRideAnimSet", "Li"))
        S.internal(0x10065364, R("IsMeshDrawEnabled", "L", ret=ret("IsMeshDrawEnabled", 1)))
        S.internal(0x100653A4, R("DisableMeshDraw", "L"))
        S.internal(0x1006548C, R("DisableMeshDrawRange", "Luu"))

        def anim_value(e, L):
            value = L.ret("SetCurRideAnim", 0) & MASK
            self.queue_animation(e.r(3), e.r(4), e.r(5))
            return value
        S.internal(0x100C6670, R("SetCurRideAnim", "Luuufu", ret=anim_value))

        def animate(e, L):
            if self.anim_left > 0:
                self.anim_left -= 1
                s = e.r(3)
                if self.anim_left == 0 and s:
                    controls, count = self.u32(s + 0x10), self.u16(s + 0xE)
                    if controls and count:
                        self.w32(controls + 0x24, 0xC)
        S.internal(0x100C74D4, R("AnimateRideSet", "Liu", then=animate))
        S.internal(0x100DD9FC, R("SOUND_SendEvent", "iiiiii", ret=ret("SOUND_SendEvent", 1)))
        S.internal(0x100DDB18, R("SOUND_PauseSound", "i"))
        S.internal(0x100DDBB8, R("SOUND_UnpauseSound", "i"))
        S.internal(0x100DDF60, R("SOUND_MoveSoundEffect", "iiii", ret=lambda e, L: L.ret("SOUND_MoveSoundEffect", e.r(3)) & MASK))
        S.internal(0x1007A3D0, R("EM_Init"))
        S.internal(0x1007A454, R("EM_SetEmitersOn", "iii", ret=ret("EM_SetEmitersOn", 1)))
        S.internal(0x1007A558, R("EM_SetEffectTranslation", "iifff"))
        S.internal(0x1007A63C, R("EM_SetEffectOrbit", "iifff"))
        S.internal(0x1007A6B8, R("EM_Tick"))
        S.internal(0x100BEC30, R("PAR_InitEmitter", "iiii", ret=lambda e, L: 7))
        S.internal(0x101E188C, R("UpdateIslandLobbyKeyDisplay"))

        def ctrace(e):
            fmt = e.cstr(e.r(4))
            L.rec("CTRACE", [e.r(3), T.s32(e.r(5)) if "%d" in fmt else 0], [fmt[:159]])
        S.internal(0x1006FCBC, ctrace)

        def stricmp(e):
            a, b = e.cstr(e.r(3)), e.cstr(e.r(4))
            L.rec("_stricmp", [], [a[:159], b[:159]])
            la, lb = a.lower(), b.lower()
            e.set_r(3, 0 if la == lb else T.u32(-1 if la < lb else 1))
        S.imp("_stricmp__10NS_MacDozeFPCcPCc", stricmp)

        def rand(e):
            queued = L.ret("Rand", -1)
            if queued >= 0:
                value = queued
            else:
                self.rand_state = (self.rand_state * 0x41C64E6D + 0x3039) & MASK
                value = (self.rand_state >> 16) & 0x7FFF
            L.rec("rand", [value])
            e.set_r(3, value)
        S.imp("rand", rand)

        def free(e):
            if e.r(3) and L.lab(e.r(3)) > 0:
                L.rec("free", [L.lab(e.r(3))])
        S.imp("free", free)
        return S

    # ---- calls
    def call(self, op, a):
        e = self.e
        c = lambda name, *args: T.call_ppc(e, ADDR[name], [x & MASK for x in args])[0]   # noqa: E731
        names = {1: "ADV_Init", 2: "ADV_Close", 3: "ADV_Pause", 4: "ADV_UnPause", 5: "ADV_Tick", 6: "ADV_Tick2",
                 8: "DressAdvisor"}
        if op in names:
            c(names[op])
            return 0
        if op == 7:
            c("TickAdvisor", a[0])
        elif op == 9:
            c("SetAdvisorHatHand", a[0])
        elif op == 10:
            return c("GenerateAnimSequence", a[0]) & MASK
        elif op == 11:
            return c("SetCannedSequence", a[0], a[1]) & MASK
        elif op == 12:
            texts = []
            for text in a[:3]:
                texts.append(0 if text is None else e.alloc(len(text) + 1, text.encode("latin-1") + b"\0"))
            c("LoadAdvisor", texts[0], texts[1], texts[2], a[3])
        elif op == 13:
            c("DestroyAdvisor", a[0])
        elif op == 14:
            c("DisableAdvisorDisplay", a[0])
        elif op == 15:
            return c("StartAdvisorAnimation", a[0], a[1]) & MASK
        elif op == 16:
            c("EnableAdvisorAccessory", a[0], a[1])
        elif op == 17:
            c("DisableAdvisorAccessory", a[0], a[1])
        elif op == 18:
            return self.lab(c("GetAdvisorRideAnim", a[0]))
        elif op == 19:
            c("SND_DuckVolume", a[0])
        elif op == 20:
            return c("FindDataNodeIndexByID", a[0], a[1], a[2]) & MASK
        elif op == 21:
            return c("TempHack", a[0], a[1], a[2]) & MASK
        elif op == 22:
            c("SetMeshInUISpace", a[0])
        elif op == 23:
            kind, index = e.alloc(4, struct.pack(">I", DEAD)), e.alloc(4, struct.pack(">I", DEAD))
            c("GetDeferredAnim", a[0], kind if a[2] & 1 else 0, index if a[2] & 2 else 0, a[1])
            return (e.u32(index) << 32) | e.u32(kind)
        elif op == 24:
            return c("GetAnimLength", a[0], a[1], a[2]) & MASK
        elif op == 25:
            point = e.alloc(12, struct.pack(">III", a[1] & MASK, a[2] & MASK, a[3] & MASK))
            c("ScaleLMatrixNoTrans", a[0], point)
        return 0

    def log(self):
        return self.L.log

    def heap(self):
        return [sum(1 for kind, _, _ in self.e.alloc_log if kind == k) for k in ("new", "new[]", "delete", "delete[]")]


class HostSide(Side):
    big = False
    psize = 8
    LAYOUT = {}

    def __init__(self, lib, case):
        self.lib = lib
        lib.tpw_ad_begin.argtypes = [C.c_uint32]
        for fn in ("tpw_ad_alloc", "tpw_ad_state", "tpw_ad_slot", "tpw_ad_linkbase", "tpw_ad_buffer", "tpw_ad_in_use",
                   "tpw_ad_system", "tpw_ad_map", "tpw_ad_clock", "tpw_ad_banks", "tpw_ad_theme_bank",
                   "tpw_ad_advisor_on", "tpw_ad_sound_settings"):
            getattr(lib, fn).restype = C.c_void_p
        lib.tpw_ad_alloc.argtypes = [C.c_int32, C.c_int32]
        lib.tpw_ad_slot.argtypes = lib.tpw_ad_linkbase.argtypes = [C.c_int32]
        lib.tpw_ad_arena_base.restype = C.c_int64
        lib.tpw_ad_op.argtypes = [C.c_int32, C.POINTER(C.c_int64)]
        lib.tpw_ad_op.restype = C.c_int64
        lib.tpw_ad_construct.argtypes = [C.c_void_p]
        if not HostSide.LAYOUT:
            lib.tpw_ad_layout.restype = C.c_char_p
            for item in lib.tpw_ad_layout().decode().split(";"):
                if item:
                    key, value = item.split("=")
                    HostSide.LAYOUT[key] = int(value)
        C.c_int32.in_dll(lib, "tpw_cl_on").value = 0
        lib.tpw_ad_begin(case["seed"] & MASK)
        self.arena = lib.tpw_ad_arena_base()
        counts = (C.c_int32 * 4).in_dll(lib, "tpw_cl_heap")
        self.heap0 = [counts[i] for i in range(4)]
        self.labels = {}
        self.blocks = 0
        self.variants = {}
        self.keep = []
        self.state_address = lib.tpw_ad_state()
        self.buffer_address = lib.tpw_ad_buffer()
        self.in_use_address = lib.tpw_ad_in_use()
        self.sound_address = lib.tpw_ad_sound_settings()
        self.scratch = self.alloc(0x40, False)
        fixed = {1: lib.tpw_ad_system(), 2: lib.tpw_ad_map(), 3: lib.tpw_ad_clock(), 4: self.scratch,
                 5: self.buffer_address}
        for s in range(SLOTS):
            base = lib.tpw_ad_slot(s)
            fixed.update({10 + s: base, 20 + s: base + self.off("slot.mInstance"), 30 + s: base + self.off("slot.mMatrix"),
                          40 + s: lib.tpw_ad_linkbase(s)})
        for label, address in fixed.items():
            self.labels[address] = label
        CL.host_begin(lib, fixed, case.get("q"), first_new=200)

    def close(self):
        C.c_int32.in_dll(self.lib, "tpw_cl_on").value = 0

    def off(self, key):
        return HostSide.LAYOUT[key]

    def rd(self, address, n):
        return C.string_at(address, n)

    def wr(self, address, data):
        C.memmove(address, data, len(data))

    def alloc(self, size, file):
        address = self.lib.tpw_ad_alloc(size, 1 if file else 0)
        self.wr(address, b"\xa5" * size)
        return address

    alloc_plain = lambda self, size: self.alloc(size, False)   # noqa: E731

    def fp(self, image, rel):
        return image + rel - self.arena

    def name(self, address, label):
        self.labels[address] = label
        self.lib.tpw_cl_label(label, address)

    def slot(self, s):
        return self.lib.tpw_ad_slot(s)

    def light_body(self, s):
        return self.slot(s)

    def light_offset(self, name, mac):
        return HostSide.LAYOUT["light." + name]

    def push(self, key, value):
        self.lib.tpw_cl_queue(key.encode(), value)

    def set_clock(self, now, paused):
        C.c_int32.in_dll(self.lib, "tpw_ad_clock_now").value = T.s32(now)
        C.c_int32.in_dll(self.lib, "tpw_ad_clock_paused").value = T.s32(paused)

    def set_advisor_on(self, value):
        self.wr(self.lib.tpw_ad_advisor_on(), bytes([value & 0xFF]))

    def set_banks(self, b3, b5, b8):
        banks = self.lib.tpw_ad_banks()
        self.wr(banks + 0xC, struct.pack("<I", b3 & MASK))
        self.wr(banks + 0x14, struct.pack("<I", b5 & MASK))
        self.wr(self.lib.tpw_ad_theme_bank(), struct.pack("<I", b8 & MASK))

    def call(self, op, a):
        keep = []
        args = []
        for x in a:
            if isinstance(x, str) or x is None:
                if x is None:
                    args.append(0)
                else:
                    buf = C.create_string_buffer(x.encode("latin-1"))
                    keep.append(buf)
                    args.append(C.addressof(buf))
            else:
                args.append(x)
        args += [0] * (8 - len(args))
        array = (C.c_int64 * 8)(*[x if x < (1 << 63) else x - (1 << 64) for x in args])
        result = self.lib.tpw_ad_op(op, array)
        if op in (10, 11, 15, 20, 21, 24):
            return result & MASK
        if op == 23:
            return result & 0xFFFFFFFFFFFFFFFF
        return result

    def log(self):
        return CL.host_end(self.lib)

    def heap(self):
        counts = (C.c_int32 * 4).in_dll(self.lib, "tpw_cl_heap")
        return [counts[i] - self.heap0[i] for i in range(4)]


# =============================================================================================== running a case


def run_case(side, case):
    """Builds the case's world and runs its script on one side: the answers, the state after each step, the number of
    fake calls after each step, and the call log."""
    for k, spec in enumerate(case["variants"]):
        side.variants[k] = side.build_variant(k, spec)
    answers, states, marks = [], [], []
    for item in case["script"]:
        if item[0] == "LoadAdvisor":
            item = list(item)
            item[3] = [PATHS[item[3][0]], FOLDERS[item[3][1]], VARIANTS[item[3][2]]]
        answers.append(side.apply(tuple(item)))
        states.append(side.dump())
        marks.append(len(side.log()) if isinstance(side, PpcSide) else C.c_int32.in_dll(side.lib, "tpw_cl_n").value)
    return dict(answers=answers, states=states, marks=marks, heap=side.heap(), log=side.log())


def advisor_ppc(e, case):
    side = PpcSide(e, case)
    try:
        return run_case(side, case)
    finally:
        side.close()


def advisor_host(lib, case):
    side = HostSide(lib, case)
    try:
        return run_case(side, case)
    finally:
        side.close()


def setup(runner):
    T.install_std_stubs(runner.emu)


# =============================================================================================== the worlds

ADVISOR_FILES = pathlib.Path(__file__).resolve().parent.parent.parent / "extracted" / "wads" / "global" / "advisor.wad.d"
EMBEDDED_IDS = [5, 6, 7, 8, 9, 0xA, 0xB, 0xC, 0xD, 0xE, 0x13, 0x14, 0x15, 0x16, 0, 1, 0xFFFFFFFF, 0x20]
EMBEDDED_FLAGS = [0x401, 0x411, 0x400, 0x411, 0x1, 0x10, 0x3DA1F82, 0x3DA1F83, 0, 0x800, 0x400001, 0xFFFFFFFF]
NODE_FLAGS = [0, 1, 0x10, 0x11, 0x200, 0x8000, 0xFFFFFFFF, 0x80000010, 0x5A5A5A5A]
SPECIAL_FLOATS = [0.0, -0.0, 1.0, -1.0, 0.5, 0.015, 0.011250000447, 0.001, 3.4028234663852886e38, 1e-30, float("inf"),
                  float("nan")]


def blank_groups():
    return {kind: (0, 0, []) for kind in range(12)}


def make_spec(nodes, minis=(), embedded=(), **kw):
    spec = dict(nodes=list(nodes), minis=list(minis), embedded=list(embedded), f46=0, hflags=1, mesh_flags=1, setflags=0,
                fc0=(0, 0), fc4=(0, 0), controls=[(0xC, 0)], ext_flags=0, groups=blank_groups(),
                root=[f32bits(1.0 if i % 5 == 0 else 0.0) for i in range(16)])
    spec.update(kw)
    spec.setdefault("all", len(spec["nodes"]) + len(spec["minis"]))
    if "all" not in kw:
        spec["all"] = len(spec["nodes"]) + len(spec["minis"])
    return spec


def real_spec():
    """The advisor mesh and its 15 animations as the disc has them (read now; None when the files are not there)."""
    path = ADVISOR_FILES / "Advisor.MD2"
    if not path.is_file():
        return None
    data = path.read_bytes()
    u = lambda o, f: struct.unpack_from("<" + f, data, o)[0]   # noqa: E731

    def text(o):
        return data[o:data.index(b"\0", o)].decode("latin-1")

    n, every, f46, e = u(0x44, "H"), u(0x42, "H"), u(0x46, "H"), u(0x48, "H")
    nodes_at, minis_at, embedded_at = u(0x70, "I"), u(0x74, "I"), u(0x7C, "I")
    nodes = [(u(nodes_at + i * 0xA0, "I"), text(u(nodes_at + i * 0xA0 + 0x54, "I"))) for i in range(n)]
    minis = [(u(minis_at + i * 0x58, "I"), text(u(minis_at + i * 0x58 + 0x54, "I"))) for i in range(every - n)]
    embedded = [(u(embedded_at + i * 0x14, "I"), u(embedded_at + i * 0x14 + 4, "I")) for i in range(e)]
    frames = []
    for i in range(1, 16):
        animation = ADVISOR_FILES / f"Advisorm{i}.MD2"
        if not animation.is_file():
            return None
        raw = animation.read_bytes()
        frames.append(struct.unpack_from("<I", raw, struct.unpack_from("<I", raw, 0x98)[0] + 8)[0])
    groups = blank_groups()
    groups[5] = (1, len(frames), frames)
    root = [u(u(0x78, "I") + 0x10 + 4 * i, "I") for i in range(16)]
    return make_spec(nodes, minis, embedded, f46=f46, hflags=u(0x30, "I"), groups=groups, root=root, all=every)


def random_floatbits(rnd):
    if rnd.random() < 0.3:
        return f32bits(rnd.choice(SPECIAL_FLOATS))
    if rnd.random() < 0.5:
        return rnd.getrandbits(32)
    return f32bits(rnd.uniform(-100.0, 100.0))


def random_spec(rnd, complete=None):
    """A made-up advisor mesh. `complete` (default: usually) puts the six names the code looks for among its nodes."""
    complete = rnd.random() < 0.8 if complete is None else complete
    n = rnd.randint(1, 14)
    m = rnd.choice([0, 1, 2, 4, 6])
    texts = rnd.sample(range(len(NAMES)), min(n, len(NAMES))) + [rnd.randrange(len(NAMES)) for _ in range(max(0, n - len(NAMES)))]
    names = [NAMES[i] for i in texts]
    if complete:
        n = max(n, 6)
        names = (names + ["x"] * n)[:n]
        for text, place in zip(["Bug Head", rnd.choice(["Mouth - Normal", "mouth - normal"]), "MOUTH - AAH", "Mouth - Eee",
                                rnd.choice(["Mouth - Ooh", "mouth - ooh"]), "Mouth - Sss"], rnd.sample(range(n), 6)):
            names[place] = text
    nodes = [(rnd.choice(NODE_FLAGS), names[i]) for i in range(n)]
    minis = [(rnd.choice(NODE_FLAGS), rnd.choice(NAMES)) for _ in range(m)]
    every = n + m if complete and rnd.random() < 0.5 else n
    if not complete:
        every = n
    if every > n:                  # the search reads past the full nodes only when a name is missing: not here
        pass
    e = rnd.randint(0, min(8, n + m))
    embedded = [(rnd.choice(EMBEDDED_FLAGS), rnd.choice(EMBEDDED_IDS)) for _ in range(e)]
    f46 = rnd.choice([n + m - e, n + m - e, rnd.randint(0, n + m - e)])
    groups = blank_groups()
    for kind in range(12):
        flags = rnd.choice([0, 1, 2, 3, 3, 0x10, 0xFFFFFFFF])
        count = rnd.choice([0, 1, 2, 3, 15] if kind == 5 else [0, 1, 2, 3])
        if kind == 5 and rnd.random() < 0.7:
            count = rnd.randint(8, 20)
        frames = [rnd.choice([rnd.randint(0, 400), rnd.randint(0, 0x7FFFFFFF), rnd.getrandbits(32), 30, 0, 1])
                  for _ in range(count)]
        groups[kind] = (flags, count if flags & 3 or rnd.random() < 0.5 else rnd.choice([0, 7, 100]), frames)
        if not (flags & 3) and groups[kind][1] != len(frames):
            groups[kind] = (flags, groups[kind][1], frames)
    controls = [(rnd.choice([0xC, 0xC, 0xC, 5, 0, 11, 0xD]), rnd.randrange(20)) for _ in range(rnd.choice([0, 1, 1, 1, 2, 3]))]
    return make_spec(nodes, minis, embedded, f46=f46, hflags=rnd.choice([0, 1, 0x200, 0xFFFFFFFF]),
                     mesh_flags=rnd.choice([0, 1, 1, 0xFFFFFFFE]), setflags=rnd.choice([0, 0x4000, 0x4, 0xFFFFFFFF, 0x4004]),
                     fc0=(rnd.getrandbits(16), rnd.getrandbits(16)), fc4=(rnd.getrandbits(16), rnd.getrandbits(16)),
                     controls=controls, groups=groups, ext_flags=rnd.choice(NODE_FLAGS), all=every,
                     root=[random_floatbits(rnd) for _ in range(16)])


# =============================================================================================== the scripts

HATS = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 0x13, 0x14, 0x15, 0x16, 0xFFFFFFFF, 99, 0xFFFFFFFE]
EFFECT_BITS = [0, 0x10000, 0x20000, 0x40000, 0x80000, 0x100000, 0x200000, 0x60000, 0x3E0000, 0x7F0000, 0x30000,
               0x220000, 0x400000, 0x1000000, 0x180000, 0x140000, 0x1C0000, 0x120000]
REAL_SPEC = []


def real():
    if not REAL_SPEC:
        REAL_SPEC.append(real_spec())
    return REAL_SPEC[0]


def pick_spec(rnd, allow_real=True):
    spec = real() if allow_real and rnd.random() < 0.35 else None
    return spec if spec is not None else random_spec(rnd)


def queues(rnd):
    """The case's answers for the fakes that look at queues (the last value of each repeats)."""
    return {"IsMeshDrawEnabled": [rnd.choice([0, 1, 1]) for _ in range(rnd.randint(1, 4))],
            "SetCurRideAnim": [rnd.choice([0, 1000, 1666, 0xFFFFFFFF]) for _ in range(rnd.randint(1, 4))],
            "SOUND_SendEvent": [rnd.choice([0, 1, 5, 0x7FFFFFFF]) for _ in range(rnd.randint(1, 5))],
            "SOUND_MoveSoundEffect": [rnd.choice([0, 1, 2]) for _ in range(rnd.randint(1, 6))] + [0],
            "EM_SetEmitersOn": [rnd.choice([0, 1, 3]) for _ in range(rnd.randint(1, 4))]}


def model_case(rnd):
    """LoadAdvisor, DestroyAdvisor and the rest of TPAdvisor.cpp over four slots and up to three meshes."""
    k = rnd.randint(1, 3)
    variants = [pick_spec(rnd) for _ in range(k)]
    installed = [None] * SLOTS
    script = []
    for _ in range(rnd.randint(6, 30)):
        r = rnd.random()
        slot = rnd.choice([0, 0, 1, 2, 3])
        if r < 0.22:
            variant = rnd.randrange(k)
            load, instance = rnd.random() < 0.9, rnd.random() < 0.9
            if installed[slot] is None:
                load = instance = True
            script.append(("LoadAdvisor", slot, variant, (rnd.randrange(3), rnd.randrange(3), rnd.randrange(4)), load, instance))
            if instance:
                installed[slot] = variant
        elif r < 0.30:
            script.append(("DestroyAdvisor", slot))
            installed[slot] = None
        elif r < 0.38:
            variant = rnd.randrange(k)
            script.append(("install", slot, variant))
            installed[slot] = variant
        elif r < 0.42:
            script.append(("install", slot, None))
            installed[slot] = None
        elif r < 0.50:
            script.append(("DisableAdvisorDisplay", slot))
        elif r < 0.60:
            script.append(("TickAdvisor", slot))
        elif r < 0.68:
            script.append(("StartAdvisorAnimation", rnd.choice([0, 1, 2, 5, 15, 16, 0xFFFFFFFF, rnd.getrandbits(32)]), slot))
        elif r < 0.84 and installed[slot] is not None:
            script.append((rnd.choice(["EnableAdvisorAccessory", "DisableAdvisorAccessory"]), rnd.choice(EMBEDDED_IDS), slot))
        elif r < 0.88:
            script.append(("GetAdvisorRideAnim", slot))
        elif r < 0.92 and installed[slot] is not None:
            script.append(("control", installed[slot], 0, rnd.choice([0xC, 0xC, 5, 3]), rnd.randrange(16)))
        elif r < 0.96 and installed[slot] is not None:
            script.append(("setflags", installed[slot], rnd.choice([0, 0x4000, 0x4004, 4, rnd.getrandbits(32)])))
        else:
            script.append(("TickAdvisor", slot))
    return dict(seed=rnd.getrandbits(32), variants=variants, q=queues(rnd), script=script)


def sync_values(rnd):
    values = [rnd.choice([0, 100, 250, 1000, 1500, 2999, 3000, 12345, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFE])
              for _ in range(rnd.randint(0, 12))]
    return values + [0xFFFFFFFF]


def speak(script, rnd, now, which, free_first=False):
    """The state ADV_Call leaves behind (ADV_Call is not translated): a line begins at the advisor time `now`."""
    base = now - 200
    for name, value in (("mWhich", which), ("mSoundSet", rnd.randrange(2)), ("mSound", rnd.randrange(1, 300)),
                        ("mHat", rnd.choice(HATS)), ("mHand", rnd.choice([0, 0, 0, 5, 14, 0xFFFFFFFF])),
                        ("mBaseTime", base), ("mMouthOpen", rnd.randrange(2)), ("mSyncActive", 0),
                        ("mSoundTime", rnd.choice([0, base + 1000, base + 1000, base + rnd.randrange(0, 3000)])),
                        ("mEndTime", base + rnd.randrange(300, 15000)),
                        ("mKeyTime", rnd.choice([0, 0, base + rnd.randrange(400, 8000)])),
                        ("mSoundHandle", rnd.choice([0, 0, 1, 77])), ("mFlags", rnd.choice(EFFECT_BITS)),
                        ("mDressNow", rnd.choice([0, 0, 1])), ("mDressAfterAnim", rnd.choice([0, 0, 1])),
                        ("mMouthChange", base), ("mMouthTime", base + rnd.randrange(0, 600))):
        script.append(("state", name, 0, value & MASK))
    mode = rnd.choice([0, 1, 2, 2])
    values = sync_values(rnd)
    script.append(("sync", mode) + tuple(values))
    script.append(("state", "mSyncPos", 0, 0))
    script.append(("state", "mSyncSize", 0, len(values) * 4))
    script.append(("state", "mSyncActive", 0, rnd.choice([1, 1, 0]) if mode else rnd.choice([0, 1])))
    if rnd.random() < 0.5:
        script.append(("GenerateAnimSequence", rnd.choice([0, 500, 3000, 12000, 30000, rnd.randrange(60000)])))
    else:
        script.append(("SetCannedSequence", rnd.randrange(14), rnd.choice([0, 3000, 12000])))
    script.append(("state", "mSpeaking", 0, 1))


def machine_case(rnd):
    """ADV_Init, then lines and ticks with the clock moving (and paused) over two installed advisors."""
    variants = [pick_spec(rnd), pick_spec(rnd)]
    script = [("install", 0, 0), ("install", 1, 1)]
    script += [("on", rnd.choice([1, 1, 1, 0])), ("banks", rnd.randrange(1, 90), rnd.randrange(1, 90), rnd.randrange(1, 90)),
               ("ADV_Init",)]
    now = rnd.choice([0, 1000, 123456, 0x7FFFF000, 0xFFFFF000])
    paused = 0
    for _ in range(rnd.randint(15, 90)):
        r = rnd.random()
        if r < 0.08:
            speak(script, rnd, now, rnd.randrange(2))
            if rnd.random() < 0.4:
                script += [("state", "mSoundHandle", 0, rnd.randint(1, 5)), ("ADV_Pause",)]
        elif r < 0.12:
            paused = 1 - paused
        elif r < 0.14:
            script.append((rnd.choice(["ADV_Pause", "ADV_UnPause", "ADV_Pause", "ADV_UnPause", "ADV_UnPause"]),))
            if rnd.random() < 0.5:
                script.append(("state", "mPausedSound", 0, rnd.choice([0, 1, 1, 7])))
        elif r < 0.15:
            script.append(("ADV_Close",))
        elif r < 0.17:
            script.append(("DressAdvisor",))
        elif r < 0.19:
            script.append(("SetAdvisorHatHand", rnd.choice(HATS)))
        elif r < 0.21:
            script.append(("state", rnd.choice(["mAccessoriesOn", "mAccessoriesOff"]), rnd.randrange(4), rnd.choice(HATS)))
        elif r < 0.23:
            script.append(("state", "mFlags", 0, rnd.choice(EFFECT_BITS)))
        elif r < 0.25:
            script.append(("on", rnd.randrange(2)))
        else:
            now = (now + rnd.choice([0, 16, 31, 100, 250, 600, 2000, 20000])) & MASK
            script.append(("clock", now, paused))
            script.append(("ADV_Tick",))
            if rnd.random() < 0.7:
                script.append(("TickAdvisor", rnd.choice([0, 0, 1])))
            if rnd.random() < 0.7:
                script.append(("ADV_Tick2",))
    return dict(seed=rnd.getrandbits(32), variants=variants, q=queues(rnd) | {"Rand": rnd.sample(range(10), 10) * 2 if rnd.random() < 0.3 else []},
                script=script)


def leaf_case(rnd):
    """The small callees one by one over random meshes."""
    k = rnd.randint(1, 2)
    variants = [pick_spec(rnd) for _ in range(k)]
    script = []
    for _ in range(rnd.randint(4, 20)):
        v = rnd.randrange(k)
        spec = variants[v]
        r = rnd.random()
        ids = [i for _, i in spec["embedded"]] + EMBEDDED_IDS
        if r < 0.20:
            mask = rnd.choice([0x400, 0, 1, 2, 0x10, 0x3DA1F83, 0x3DA1F82, 0x800, 0xFFFFFFFF, rnd.getrandbits(32), 0x1F83])
            script.append(("FindDataNodeIndexByID", v, mask, rnd.choice(ids)))
        elif r < 0.40:
            script.append(("TempHack", v, rnd.getrandbits(32), rnd.choice([0xFFFFFFFF, 0, 1, 2, 3, 4, 5, 6, 7, 0xFFFFFFFE, rnd.getrandbits(32)])))
        elif r < 0.48:
            script.append(("SetMeshInUISpace", v))
        elif r < 0.62:
            script.append(("GetDeferredAnim", v, rnd.randrange(max(len(spec["controls"]), 1)), rnd.randrange(4)))
        elif r < 0.80:
            script.append(("GetAnimLength", v, rnd.choice([5, 5, 5, rnd.randrange(13)]), rnd.choice([0, 1, 14, 15, 19, rnd.randrange(24)])))
        elif r < 0.92:
            script.append(("matrix",) + tuple(random_floatbits(rnd) for _ in range(16)))
            script.append(("ScaleLMatrixNoTrans", random_floatbits(rnd), random_floatbits(rnd), random_floatbits(rnd)))
        else:
            script.append(("SND_DuckVolume", rnd.choice([0, 1, -1, 7, 0x7FFFFFFF, rnd.getrandbits(32)]) & MASK))
    return dict(seed=1, variants=variants, q={}, script=script)


def idle_case(rnd):
    """No advisor loaded, or not the one the state names: the ticks, the dressing and the start-up do what they can."""
    variants = [pick_spec(rnd), pick_spec(rnd)]
    installed = [rnd.random() < 0.5 for _ in range(2)]
    script = [("install", s, s) for s in range(2) if installed[s]]
    script += [("on", rnd.randrange(2)), ("banks", 3, 5, 8)]
    now = rnd.randrange(0, 100000)
    which = 0
    for _ in range(rnd.randint(8, 40)):
        r = rnd.random()
        if r < 0.15:
            script.append(("ADV_Init",))
            which = 0                      # it clears the state
        elif r < 0.30:
            which = rnd.randrange(4)
            script.append(("state", "mWhich", 0, which))
        elif r < 0.40:
            script.append(("state", "mFlags", 0, rnd.choice(EFFECT_BITS)))
            script.append(("state", "mSpeaking", 0, rnd.randrange(2)))
        elif r < 0.55:
            script.append(("DressAdvisor",))
        elif r < 0.65:
            # the accessories are looked up in the advisor the state names: it has to be there
            if which < 2 and installed[which]:
                script.append(("SetAdvisorHatHand", rnd.choice(HATS)))
        elif r < 0.75:
            script.append(("ADV_Close",))
        elif r < 0.80:
            script.append(("ADV_Pause",))
        elif r < 0.85:
            script.append(("ADV_UnPause",))
        elif r < 0.90:
            script.append(("TickAdvisor", rnd.randrange(4)))
        elif r < 0.95:
            script.append(("sync", rnd.choice([0, 1, 2]), 5, 0xFFFFFFFF))
            script.append(("state", "mSyncActive", 0, rnd.randrange(2)))
        else:
            # the tick is only safe when nothing is speaking, or the advisor the state names is loaded
            now = (now + rnd.choice([0, 33, 500, 5000])) & MASK
            script.append(("clock", now, rnd.randrange(2)))
            script.append(("state", "mSpeaking", 0, 0))
            script.append(("state", "mDressAfterAnim", 0, 0))
            script.append(("ADV_Tick",))
            script.append(("ADV_Tick2",))
    return dict(seed=rnd.getrandbits(32), variants=variants, q=queues(rnd), script=script)


def sequence_case(rnd):
    """GenerateAnimSequence and SetCannedSequence over random animation lengths and random picks."""
    lengths = [rnd.choice([0, 1, 33, 333, 666, 1666, 2500, 5000, 20000, rnd.randrange(30000), 0x7FFFFFFF, 0xFFFFFFFF, 0x80000000])
               for _ in range(20)]
    script = [("state", "mAnimLengths", i, lengths[i]) for i in range(20)]
    script.append(("state", "mSoundTime", 0, rnd.choice([0, 5000])))
    for _ in range(rnd.randint(1, 5)):
        if rnd.random() < 0.6:
            script.append(("GenerateAnimSequence", rnd.choice([0, 1, 200, 1500, 4000, 9000, 25000, 70000, 0x7FFFFFFF, 0xFFFFFFFF,
                                                               rnd.randrange(40000)] + lengths[:10])))
        else:
            script.append(("SetCannedSequence", rnd.randrange(14), rnd.choice([0, 0, 1, 700, 6000, rnd.randrange(40000)])))
        script.append(("state", "mSequencePos", 0, rnd.randrange(3)))
    picks = [rnd.randrange(32768) for _ in range(60)]
    return dict(seed=rnd.getrandbits(32), variants=[], q={"Rand": picks if rnd.random() < 0.7 else []}, script=script)


def dress_case(rnd):
    """DressAdvisor over odd animation lengths (the effects take one as a float), effect bits, hats and hands."""
    variants = [pick_spec(rnd), pick_spec(rnd)]
    script = [("install", 0, 0), ("install", 1, 1), ("ADV_Init",)]
    for _ in range(rnd.randint(2, 8)):
        for i in range(20):
            if rnd.random() < 0.5:
                script.append(("state", "mAnimLengths", i, rnd.choice([0, 1, 16777217, 16777219, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF,
                                                                       33333, rnd.getrandbits(32), rnd.randrange(100000)])))
        for name, value in (("mAnimIndex", rnd.randrange(20)), ("mFlags", rnd.choice(EFFECT_BITS) | rnd.choice([0, 0, 1, 0x8000])),
                            ("mSpeaking", rnd.randrange(2)), ("mWhich", rnd.randrange(2)), ("mHat", rnd.choice(HATS)),
                            ("mHand", rnd.choice(HATS))):
            script.append(("state", name, 0, value))
        script.append(("DressAdvisor",))
    return dict(seed=rnd.getrandbits(32), variants=variants, q=queues(rnd), script=script)


def pause_cases():
    """ADV_Pause and ADV_UnPause for every combination of what they test."""
    script = [("banks", 1, 2, 3)]
    for speaking in (0, 1):
        for on in (0, 1):
            for handle in (0, 9):
                for paused in (0, 1):
                    script += [("state", "mSpeaking", 0, speaking), ("on", on), ("state", "mSoundHandle", 0, handle),
                               ("state", "mPausedSound", 0, paused), ("ADV_Pause",), ("ADV_UnPause",),
                               ("state", "mPausedSound", 0, paused), ("ADV_UnPause",), ("ADV_Pause",)]
    return dict(seed=2, variants=[], q={}, script=script)


def timer_cases():
    """Each of ADV_Tick's times one millisecond before, at and after the advisor clock (the tests are `<= 0` and `> 0`)."""
    spec = real() or random_spec(random.Random(3), True)
    for base in (1000, 0, 0x7FFFFFFF, 0xFFFFFFF0):
        for field in ("end", "sound", "key", "mouth", "change"):
            for delta in (-1, 0, 1):
                far = (base + 0x1000) & MASK
                at = (base + delta) & MASK
                script = [("install", 0, 0), ("install", 1, 1), ("banks", 4, 5, 6), ("ADV_Init",),
                          ("state", "mSpeaking", 0, 1), ("state", "mSoundHandle", 0, 0), ("state", "mEndTime", 0, far),
                          ("state", "mWhich", 0, 0), ("state", "mMouthCount", 0, 5)]
                script.append(("sync", 2, 1000, 3000, 0xFFFFFFFF))
                if field == "end":
                    script.append(("state", "mEndTime", 0, at))
                elif field == "sound":
                    script += [("state", "mSoundTime", 0, at), ("state", "mSound", 0, 9)]
                elif field == "key":
                    script.append(("state", "mKeyTime", 0, at))
                elif field == "mouth":
                    script += [("state", "mSyncActive", 0, 1), ("state", "mMouthOpen", 0, 1), ("state", "mMouthTime", 0, at)]
                else:
                    script += [("state", "mSyncActive", 0, 1), ("state", "mMouthOpen", 0, 1), ("state", "mMouthTime", 0, far),
                               ("state", "mMouthChange", 0, at)]
                script += [("clock", base, 0), ("ADV_Tick",), ("clock", (base + 1) & MASK, 0), ("ADV_Tick",)]
                yield dict(seed=11, variants=[spec, spec], q={}, script=script)


def edge_cases():
    yield pause_cases()
    yield from timer_cases()
    r = random.Random(7)
    real_one = real()
    specs = [real_one] if real_one is not None else []
    specs.append(random_spec(r, True))
    mouth = lambda spec: spec                                          # noqa: E731
    for spec in specs:
        # the leaves on the real mesh, every id and animation
        script = []
        for ident in [0xFFFFFFFF, 0, 1, 2, 3, 4, 5, 6]:
            script.append(("TempHack", 0, 0, ident))
        for mask in (0x400, 0, 0x3DA1F82, 0x1):
            for ident in (0x5, 0x6, 0xC, 0xE, 0x13, 0x14, 0x15, 0x16, 0xFFFFFFFF, 0x17):
                script.append(("FindDataNodeIndexByID", 0, mask, ident))
        for kind in (5, 0, 11, 12):
            for index in range(0, 22):
                script.append(("GetAnimLength", 0, kind, index))
        for want in range(4):
            script.append(("GetDeferredAnim", 0, 0, want))
        script += [("SetMeshInUISpace", 0), ("SND_DuckVolume", 0), ("SND_DuckVolume", 1)]
        yield dict(seed=1, variants=[spec], q={}, script=script)
        # LoadAdvisor, ADV_Init, every hat and effect, a line of every canned sequence, then unloaded
        script = [("on", 1), ("banks", 11, 22, 33)]
        for variant_text in range(4):
            script.append(("LoadAdvisor", 0, 0, (1, 0, variant_text), True, True))
        script += [("ADV_Init",), ("GetAdvisorRideAnim", 0), ("GetAdvisorRideAnim", 1)]
        for hat in HATS:
            for flags in (0, 0x10000):
                script += [("state", "mFlags", 0, flags), ("SetAdvisorHatHand", hat)]
        for effect in EFFECT_BITS:
            for speaking in (0, 1):
                script += [("state", "mFlags", 0, effect), ("state", "mSpeaking", 0, speaking), ("state", "mAnimIndex", 0, 3),
                           ("DressAdvisor",)]
        for row in range(14):
            for length in (0, 5000):
                script.append(("SetCannedSequence", row, length))
        for length in (0, 1, 500, 2000, 9999, 30000, 70000):
            script.append(("GenerateAnimSequence", length))
        script += [("StartAdvisorAnimation", 3, 0), ("StartAdvisorAnimation", 3, 2), ("DisableAdvisorDisplay", 0),
                   ("DisableAdvisorDisplay", 0), ("TickAdvisor", 0), ("DestroyAdvisor", 0), ("DestroyAdvisor", 0),
                   ("TickAdvisor", 0), ("ADV_Init",), ("ADV_Close",)]
        yield dict(seed=5, variants=[spec], q={"SetCurRideAnim": [1666, 33], "Rand": list(range(0, 10)) * 4}, script=script)


def ctor_ppc(e, case):
    """The slot's constructor (0x10032454, with the light's) over storage filled with the case's byte."""
    e.heap_in_scratch = True
    log = e.cl = CL.PpcLog({e.data_import("gei_System"): 1})
    stubs = CL.Stubs()
    stubs.real.update(REAL)
    stubs.imp("UpdateLights__7CSystemFv", CL.recorder("UpdateLights", "L"))
    old = stubs.activate(e)
    try:
        storage = e.alloc(SLOT_SIZE, bytes([case["poison"]]) * SLOT_SIZE)
        T.call_ppc(e, ADDR["ctor"], [storage])
        body = e.u32(storage + MAC["slot.light"])
        words = lambda at, n: [canon(e.u32(at + 4 * i)) for i in range(n)]   # noqa: E731
        pattern = case["poison"] * 0x01010101
        # the set, the instance (untouched?) and the matrix (untouched, word by word)
        out = [int(e.u32(storage) == pattern), int(e.u32(storage + 4) == pattern)] + words(storage + MAC["slot.mMatrix"], 16)
        for name, mac, count in LIGHT_FIELDS:
            out += words(body + mac, count)
        out.append(e.u32(body + 0x58))
        return dict(fields=out, log=log.log)
    finally:
        CL.Stubs.deactivate(e, old)


def ctor_host(lib, case):
    side = HostSide(lib, dict(seed=1))
    try:
        storage = lib.tpw_ad_alloc(HostSide.LAYOUT["slot.size"] + 16, 0)
        C.memset(storage, case["poison"], HostSide.LAYOUT["slot.size"] + 16)
        lib.tpw_ad_construct(storage)
        word = lambda at: struct.unpack("<I", C.string_at(storage + at, 4))[0]   # noqa: E731
        pattern = bytes([case["poison"]]) * 8
        out = [int(C.string_at(storage + HostSide.LAYOUT["slot.mSet"], 8) == pattern),
               int(C.string_at(storage + HostSide.LAYOUT["slot.mInstance"], 8) == pattern)]
        out += [canon(word(HostSide.LAYOUT["slot.mMatrix"] + 4 * i)) for i in range(16)]
        for name, mac, count in LIGHT_FIELDS:
            out += [canon(word(HostSide.LAYOUT["light." + name] + 4 * i)) for i in range(count)]
        out.append(word(HostSide.LAYOUT["light.mNext"]))
        return dict(fields=out, log=side_log(side))
    finally:
        side.close()


def side_log(side):
    return CL.host_end(side.lib)


def ctor_cases():
    for poison in (0xA5, 0x00, 0xFF):
        yield dict(poison=poison)


TESTS = [
    FnTest("Advisor model", advisor_ppc, advisor_host, edge=lambda: (c for c in []), rnd=model_case),
    FnTest("Advisor machine", advisor_ppc, advisor_host, edge=edge_cases, rnd=machine_case),
    FnTest("Advisor idle", advisor_ppc, advisor_host, edge=lambda: (c for c in []), rnd=idle_case),
    FnTest("Advisor sequences", advisor_ppc, advisor_host, edge=lambda: (c for c in []), rnd=sequence_case),
    FnTest("Advisor dressing", advisor_ppc, advisor_host, edge=lambda: (c for c in []), rnd=dress_case),
    FnTest("Advisor leaves", advisor_ppc, advisor_host, edge=lambda: (c for c in []), rnd=leaf_case),
    FnTest("Advisor slot constructor", ctor_ppc, ctor_host, edge=ctor_cases, rnd=lambda r: dict(poison=r.choice([0xA5, 0, 0xFF, 0x5A]))),
]
