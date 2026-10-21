// PlaceHolderHelpers.h: the placeholder helpers of the sound library (unit w79g4, "sound symbols shared", Code at 0x10000000, TOC r2 =
// 0x1003abf0; the code is in src/sound/AudioPlaceHolder.cpp, src/sound/PlaceHolderSentence.cpp with the addresses):
//   CMapReference                                 resolves a placeholder's event, sound and sample pointers to the four ids the save game
//                                                 stores and back, through the map streamer's category table
//   CPlaceHolderOneShotThrowaway                  a one-shot placeholder that is not told when a sample ends
//   CPlaceHolderOneShotSentenceElement            the children of the one-shot sentences (made by Load and by w79g3's AddNewElementTo)
//   CPlaceHolderOneShotBranchingSentenceElement   the child of a branching one-shot sentence
//   TbAudioEventList::ListItem                    the pair CListSentence keeps per entry; the CAutoArrayPtr instance over it is the
//                                                 template of snd_containers.h (0x1001eaec operator T*, 0x1001eb5c destructor, 0x1001ec10
//                                                 counted constructor), ListItem::ListItem 0x1001ece4 (4, empty)
// Unit w79g2's CAudioPlaceHolder::Save and Load use CMapReference and make the two Element classes. The sound library's own
// TbAudioEvent (0x1001ee64 and the four getters) is sound::TbAudioEvent in TbMapStreamer.h.
//
// Class chain (Mac vtables, each 0x58 bytes; r2 + the ctor's addi): CAudioPlaceHolder 0x1003cc04 <- CPlaceHolderOneShot (w79g3) 0x1003d384
// <- Throwaway 0x1003d4f0 (r2 + 0x2900) <- SentenceElement 0x1003d0d8 (r2 + 0x24e8) <- BranchingSentenceElement 0x1003cff8 (r2 + 0x2408).
// All are 0x60 bytes (no member of their own). Overrides:
//   Throwaway   +0x10 destructor, +0x48 FillSampleInfo, +0x54 GetClassID (8)
//   Element     +0x10 destructor, +0x44 GetParameterValue, +0x48 FillSampleInfo, +0x54 GetClassID (7); Init and Update are NOT virtual
//               (the vtable keeps the base's Init +0x18 and Update +0x30); they hide the base's overloads here
//   Branching   +0x10 destructor, +0x3c ChooseRandomSound, +0x54 GetClassID (9)
// The Element classes keep their parent (the sentence placeholder) in the extra data's +0x1c (mParent): the constructor stores it,
// and every override reads the parent's position, velocity, priority, volume control index and parameter bytes through it.
#pragma once
#include <cstdint>

#include "AudioPlaceHolder.h"
#include "PlaceHolderTypes.h"
#include "snd_containers.h"

class CCatagoryContainer;

// The map reference: resolves a placeholder's event, sound and sample pointers to the four ids the save game stores, and back.
// One word on the Mac (a 4-byte object on Save's and Load's stack); the constructor writes nothing.
class CMapReference {
public:
    CMapReference();   // 0x1001af5c (4): empty
    // 0x1001f0dc (504): the index of the first category container whose events array contains the placeholder's event (an end
    // address one past the array counts), then the indexes of the event, sound and sample inside that category (each clamped into
    // 0 .. count - 1). A placeholder without an event, a sound or a sample, or an event outside every category, gives the first
    // id 0, which CAudioPlaceHolder::Save treats as failure (so does a match in the first category: id 0 is also category 0).
    void GetSoundIds(int32_t& category, int32_t& event, int32_t& sound, int32_t& sample, CAudioPlaceHolder* holder);
    // 0x1001f364 (96): the category container for a (fixed up) category id; NULL when it is not in the table. `holder` is unused.
    CCatagoryContainer* GetSoundPointer(uint32_t category, CAudioPlaceHolder* holder);
    uint32_t mF00;   // never read or written
};

// TbAudioEventList::ListItem (the pair of words CListSentence keeps per entry; ListItem::ListItem 0x1001ece4, 4, empty) moved to
// include/PlaceHolderTypes.h with the rest of TbAudioEventList (unit w79g3's CListData holds the array); the CAutoArrayPtr instance over
// it is still w79a's template of include/snd_containers.h (0x1001eaec operator T*, 0x1001eb5c destructor, 0x1001ec10 counted constructor).

// ---- CPlaceHolderOneShotThrowaway --------------------------------------------------------------------------------------------
class CPlaceHolderOneShotThrowaway : public CPlaceHolderOneShot {
public:
    CPlaceHolderOneShotThrowaway();                // 0x100242b4 (56): CPlaceHolderOneShot's constructor, then vtable 0x1003d4f0
    ~CPlaceHolderOneShotThrowaway() override;      // 0x10023e64 (100), slot +0x10: vtable 0x1003d4f0, the base destructor (flag 0); a positive flag deletes
    // 0x100267e8 (88), slot +0x48: the base's FillSampleInfo (a direct call); on success the "completed callback" request (flag
    // 0x2000 of the info's flags) is dropped and the callback cleared, so the end of the sample is not reported.
    bool FillSampleInfo(uint32_t now, TbSoundSampleInfo& info) override;
    uint32_t GetClassID() override;                // 0x10026898 (8), slot +0x54: 8
};

// ---- CPlaceHolderOneShotSentenceElement ---------------------------------------------------------------------------------------
// Init and Update hide the base's virtual overloads of the same names (the binary's members are separate functions).
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Woverloaded-virtual"
class CPlaceHolderOneShotSentenceElement : public CPlaceHolderOneShotThrowaway {
public:
    explicit CPlaceHolderOneShotSentenceElement(CAudioPlaceHolder* parent);   // 0x10024218 (92): Throwaway's constructor, vtable 0x1003d0d8, extra data, parent
    ~CPlaceHolderOneShotSentenceElement() override;   // 0x10023dc0 (100), slot +0x10
    // 0x10024328 (192): not virtual. Stores the event, SetFlagsFromEvent(0.0), takes the parent's volume control index, picks a sound
    // and a sample (the virtuals +0x3c and +0x40, rotate false) and sets mEndTime = now + GetPsuedoTime() (the virtual +0x50). The
    // sample is stored even when the pick failed. False when there is no sound or no sample.
    bool Init(CEventData* event, uint32_t now);
    // 0x10024438 (588), slot +0x48: fills `info` for starting the child's sample from the parent's position, velocity and priority.
    // False without a sample, a sound or the manager, or when FillBasicSampleInfo (virtual +0x4c) fails. A non-positional child
    // hands the info the addresses of two LOCAL positions (0, 1, 0) and (0, 0, 0) (they are dead once this returns, as on the Mac).
    bool FillSampleInfo(uint32_t now, TbSoundSampleInfo& info) override;
    // 0x10024724 (172): not virtual. Tells a playing sample the parent's position and velocity.
    void Update();
    // 0x10024814 (92), slot +0x44: the parent's parameter bytes (+5 and +6 of its extra data) for the sound's two masks.
    bool GetParameterValue(uint32_t mask, uint32_t& value) override;
    uint32_t GetClassID() override;                // 0x100248c0 (8), slot +0x54: 7
};
#pragma clang diagnostic pop

// ---- CPlaceHolderOneShotBranchingSentenceElement ------------------------------------------------------------------------------
class CPlaceHolderOneShotBranchingSentenceElement : public CPlaceHolderOneShotSentenceElement {
public:
    explicit CPlaceHolderOneShotBranchingSentenceElement(CAudioPlaceHolder* parent);   // 0x10023d38 (60): the Element's constructor, vtable 0x1003cff8
    ~CPlaceHolderOneShotBranchingSentenceElement() override;   // 0x10024114 (100), slot +0x10
    // 0x10023f04 (440), slot +0x3c: the branch of the parent's sound that the parent's parameter byte (extra +4) selects, weighted
    // by the branches' sounds (CSoundData +0x1e); the choice is stored in this and in the parent. `rotate` is unused.
    bool ChooseRandomSound(bool rotate) override;
    uint32_t GetClassID() override;                // 0x100241c0 (8), slot +0x54: 9
};
