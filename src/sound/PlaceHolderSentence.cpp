// PlaceHolderSentence.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of PlaceHolderSentence code, named after it.

#include "AudioPlaceHolder.h"
#include "bfl_FileSystem.h"
#include "CSprocketsModule.h"
#include "helpers/PlaceHolderSentenceHelpers.h"
#include "LbTime.h"
#include "PlaceHolderHelpers.h"
#include "PlaceHolderTypes.h"
#include "platform/arena.h"
#include "snd_commands.h"
#include "snd_containers.h"
#include "TbMapStreamer.h"
#include "TbMpegBase.h"
#include "TbSoundCodecs.h"
#include "TbSoundManager.h"
#include "TbSoundPipeChannel.h"
#include <cmath>
#include <cstring>
#include <new>

// PlaceHolderHelpers.cpp: CMapReference and the one-shot sentence children of the sound library (unit w79g4, "sound symbols shared",
// the library's CSprocketsModule.cpp). See include/PlaceHolderHelpers.h for the classes, their vtables and the overrides.
//
// Conventions of this file. The parent of an Element is the sentence placeholder kept in the extra data's mParent; the Mac code
// reads it through the extra data on every use. Virtual calls the Mac makes through the object's vtable are virtual calls here
// (Init calls +0x3c, +0x40 and +0x50, FillSampleInfo +0x4c); the base's non-virtual members are called directly. The sound manager
// is the class static CAudioPlaceHolder::mpSoundManager (0x1003f360). The pointers of the map data that the Mac compares as 32-bit
// addresses are compared as arena offsets (the value a Ptr32 holds).

namespace {

// The flag bits of the sample info's word at +0xc (include/TbSoundInfo.h calls it mField0C) the Element classes set.
constexpr uint32_t kInfoNonPositional = 0x1;
constexpr uint32_t kInfoPositional = 0x20;
constexpr uint32_t kInfoQSound = 0x1000;

// The roles of the bytes of a branch record (CSoundBranchData +6 and +7): the lowest and highest value of the parent's parameter
// byte that selects the branch.
uint8_t BranchLow(const CSoundBranchData& branch) { return branch.mF06[0]; }
uint8_t BranchHigh(const CSoundBranchData& branch) { return branch.mF06[1]; }

}  // namespace

// ---- CPlaceHolderOneShotBranchingSentenceElement ---------------------------------------------------------------------------

CPlaceHolderOneShotBranchingSentenceElement::CPlaceHolderOneShotBranchingSentenceElement(CAudioPlaceHolder* parent)
    : CPlaceHolderOneShotSentenceElement(parent) {}

CPlaceHolderOneShotSentenceElement::~CPlaceHolderOneShotSentenceElement() {}

CPlaceHolderOneShotThrowaway::~CPlaceHolderOneShotThrowaway() {}

// The parent's sound is taken first; without one the event's first sound is the choice when it has samples (the parent gets it
// too), otherwise the branches of this->mSound are weighed: the branches whose byte range holds the parent's parameter byte add
// their sounds' weights (mF1E), a random number below the sum (RangedRandomSlow on a copy of the seed) picks the first branch
// whose running total reaches it. No branch in range sets the parameter byte to 0x7f and the parent's flag 0x40 and answers false
// (this->mSound stays); no sound at all (a sound without branches) clears this and the parent's sound.
bool CPlaceHolderOneShotBranchingSentenceElement::ChooseRandomSound(bool) {
    CAudioPlaceHolder* parent = mExtra->mParent;
    const uint8_t parameter = parent->mExtra->mParamValue[0];
    mSound = parent->mSound;
    if (mSound == nullptr && mEvent != nullptr) {
        mSound = mEvent->mSounds.get();
        if ((mSound->mSampleCount & 0xffff) != 0) {
            parent->mSound = mSound;
            return true;
        }
    }
    if (mSound->mBranchCount == 0) {
        mSound = nullptr;
        parent->mSound = mSound;
        return false;
    }
    uint32_t matches = 0;
    uint32_t total = 0;
    const uint32_t branches = mSound->mBranchCount;
    for (uint32_t index = 0; index < branches; ++index) {
        const CSoundBranchData& branch = mSound->mBranches.get()[index];
        if (BranchLow(branch) <= parameter && BranchHigh(branch) >= parameter) {
            ++matches;
            total += branch.mSound->mF1E;
        }
    }
    if (matches == 0) {
        parent->mExtra->mParamValue[0] = 0x7f;
        parent->mFlags |= kSubclass40;
        return false;
    }
    int32_t seed = mRandomSeed;
    const uint32_t pick = bfl::maths::RangedRandomSlow(seed, total);
    uint32_t running = 0;
    for (uint32_t index = 0; index < branches; ++index) {
        const CSoundBranchData& branch = mSound->mBranches.get()[index];
        if (BranchLow(branch) <= parameter && BranchHigh(branch) >= parameter) {
            running += branch.mSound->mF1E;
            if (running >= pick) {
                mSound = mSound->mBranches.get()[index].mSound.get();
                parent->mSound = mSound;
                return true;
            }
        }
    }
    return false;
}

CPlaceHolderOneShotBranchingSentenceElement::~CPlaceHolderOneShotBranchingSentenceElement() {}

uint32_t CPlaceHolderOneShotBranchingSentenceElement::GetClassID() {
    return 9;
}
// ---- CPlaceHolderOneShotSentenceElement -------------------------------------------------------------------------------------

CPlaceHolderOneShotSentenceElement::CPlaceHolderOneShotSentenceElement(CAudioPlaceHolder* parent) : CPlaceHolderOneShotThrowaway() {
    AllocateExtraData();
    mExtra->mParent = parent;
}

// ---- CPlaceHolderOneShotThrowaway ------------------------------------------------------------------------------------------

CPlaceHolderOneShotThrowaway::CPlaceHolderOneShotThrowaway() : CPlaceHolderOneShot() {}

bool CPlaceHolderOneShotSentenceElement::Init(CEventData* event, uint32_t now) {
    mEvent = event;
    SetFlagsFromEvent(0.0f);   // 0x100332c0
    mVolumeControlIndex = mExtra->mParent->mVolumeControlIndex;
    if (!ChooseRandomSound(false)) return false;
    mSample = ChooseRandomSample(false);
    if (mSample == nullptr) return false;
    mEndTime = now + static_cast<uint32_t>(GetPsuedoTime());
    return true;
}

// The Mac's order: the basic info, the volume, the volume control, the pitch, the priority (the PARENT's), then the position
// (a non-positional child gets two local positions (0, 1, 0) and (0, 0, 0), a positional one the parent's with a distance record
// whose minimum is 2.0 and whose maximum is the greater of the child's and 2.1) and the loop request.
bool CPlaceHolderOneShotSentenceElement::FillSampleInfo(uint32_t, TbSoundSampleInfo& info) {
    if (mSample == nullptr || mSound == nullptr || mpSoundManager == nullptr) return false;
    if (!FillBasicSampleInfo(info)) return false;
    info.SetVolume(GetRandomVolume());
    info.SetVolumeControl(*mpSoundManager->GetVolumeControl(), mVolumeControlIndex);
    info.SetPitch(GetRandomPitch());
    info.SetPriority(mExtra->mParent->mPriority);
    TbSoundPosition position(0, 1, 0);
    TbSoundPosition velocity(0, 0, 0);
    if ((mFlags & kNonPositional) != 0) {
        info.SetPosition(position);
        info.SetVelocity(velocity);
        info.mField0C |= kInfoNonPositional;
    } else {
        const float minimum = 2.0f;                 // 0x100332bc
        float maximum = GetMaxDistance();
        if (maximum <= minimum) maximum = 2.1f;     // 0x100332b8
        const float rolloff = mpSoundManager->GetRolloffFactorToMinMaxDistance(minimum, maximum);
        const TbSoundDistance distance(minimum, maximum, rolloff);
        info.SetDistance(distance);
        info.SetPosition(mExtra->mParent->mPosition);
        info.SetVelocity(mExtra->mParent->mVelocity);
        info.mField0C |= kInfoPositional;
        if (mpSoundManager->IsQSoundOn()) info.mField0C |= kInfoQSound;
    }
    if ((mFlags & kLoopForever) != 0) info.SetLoopCount(9999);
    return true;
}

void CPlaceHolderOneShotSentenceElement::Update() {
    if (mHandle == nullptr) return;
    TbSoundSampleInfo info;
    if ((mFlags & kNonPositional) == 0) info.mField0C |= kInfoPositional;
    info.SetPosition(mExtra->mParent->mPosition);
    info.SetVelocity(mExtra->mParent->mVelocity);
    SystemCommand::SetSampleInfo command(mHandle, info);
    mpSoundManager->SystemCommand(command);
}

bool CPlaceHolderOneShotSentenceElement::GetParameterValue(uint32_t mask, uint32_t& value) {
    if ((mSound->mF18 & mask) != 0) {
        value = mExtra->mParent->mExtra->mParamValue[1];
        return true;
    }
    if ((mSound->mF1C & mask) != 0) {
        value = mExtra->mParent->mExtra->mParamValue[2];
        return true;
    }
    return false;
}

uint32_t CPlaceHolderOneShotSentenceElement::GetClassID() {
    return 7;
}

// PlaceHolderTypes.cpp: the kinds of playing sound built on CAudioPlaceHolder (unit w79g3, "sound symbols shared", the library's
// CSprocketsModule.cpp): CPlaceHolderOneShot, CPlaceHolderOneShotSentence and ...BranchingSentence, CPlaceHolderSentence and its
// Linear, Branching, Shuffle and CListSentence subclasses. See include/PlaceHolderTypes.h for the class tree, the vtable slots and the
// extra data's +0x1c word.
//
// Conventions of this file. The listing's operation order is kept (the call logs of the test compare it). A virtual call the Mac makes
// through the object's vtable is a virtual call here; a call the Mac makes by address (the base's functions) is a qualified call.
// Mac addresses of the sound data that the code compares or subtracts are compared as arena offsets (the 32-bit value a Ptr32 field
// holds), because the Mac does that arithmetic on 32-bit addresses. The objects are constructed in place into the list's slot
// (operator new(0x60, slot) returns the slot); every constructor leaves the list's words alone. The destructors' vtable stores and
// the base call with flag 0 are the compiler's chain; the flag-1 deleting form is `delete`.

namespace {

// The Mac address of a record of the sound data.
uint32_t Address(const void* pointer) {
    return pointer != nullptr ? tpw::ArenaOffset(pointer) : 0;
}

constexpr uint32_t kSoundRecord = 0x2a;   // sizeof(CSoundData) on the Mac

// The flags and the sample info the sentences share: the placeholder starts its sample with the manager (OneShot and Sentence).
TbError StartSample(CAudioPlaceHolder* holder, uint32_t now, bool sentence) {
    TbSoundManager* manager = CAudioPlaceHolder::mpSoundManager;
    if (manager != nullptr && holder->mSound != nullptr) {
        if ((holder->mFlags & CAudioPlaceHolder::kPlaying) != 0 || (holder->mFlags & CAudioPlaceHolder::kFinished) != 0) return TbError(0);
        holder->InitFade();
        holder->mFlags |= CAudioPlaceHolder::kPlaying;
        TbSoundSampleInfo info;
        if (holder->FillSampleInfo(now, info)) {   // virtual +0x48
            if (sentence && (holder->mFlags & CAudioPlaceHolder::kLoopForever) != 0) info.SetLoopCount(1);
            if (manager->PlaySample(info, holder->mHandle) == nullptr) {
                holder->mFlags &= ~CAudioPlaceHolder::kPlaying;
                holder->mHandle = nullptr;
            }
            return TbError(0);
        }
        holder->mFlags |= CAudioPlaceHolder::kFinished;
    }
    return TbError(-1);
}

}  // namespace

// Like the branching Element's walk (unit w79g4) but on this placeholder: without a sound the event's first sound is taken (done when it
// has samples); a sound without branches is dropped; the branches whose byte range holds `parameter` add their sounds' weights (a
// 32-bit wrap), none in range sets the parameter byte to 0x7f and flag 0x40 (false), else a draw below the sum (RangedRandomSlow on a
// copy of the seed) picks the first branch whose running total reaches it.
bool CPlaceHolderBranchingSentence::AssignSoundToNextBranch(uint32_t parameter) {
    if (mSound == nullptr && mEvent != nullptr) {
        mSound = mEvent->mSounds.get();
        if ((mSound->mSampleCount & 0xffff) != 0) return true;
    }
    CSoundData* sound = mSound;
    const uint32_t branches = sound->mBranchCount;
    if (branches == 0) {
        mSound = nullptr;
        return false;
    }
    uint32_t matches = 0;
    uint32_t total = 0;
    for (uint32_t index = 0; index < branches; ++index) {
        const CSoundBranchData& branch = sound->mBranches.get()[index];
        if (BranchLow(branch) <= parameter && BranchHigh(branch) >= parameter) {
            ++matches;
            total += branch.mSound->mF1E;
        }
    }
    if (matches == 0) {
        mExtra->mParamValue[0] = 0x7f;
        mFlags |= kSubclass40;
        return false;
    }
    int32_t seed = mRandomSeed;
    const uint32_t pick = bfl::maths::RangedRandomSlow(seed, total);
    uint32_t running = 0;
    for (uint32_t index = 0; index < branches; ++index) {
        const CSoundBranchData& branch = sound->mBranches.get()[index];
        if (BranchLow(branch) <= parameter && BranchHigh(branch) >= parameter) {
            running += branch.mSound->mF1E;
            if (running >= pick) {
                mSound = sound->mBranches.get()[index].mSound.get();
                break;
            }
        }
    }
    return true;
}

void CPlaceHolderBranchingSentence::UpdateParameter(uint8_t id, uint32_t value) {
    CAudioPlaceHolder::UpdateParameter(id, value);
    if ((mFlags & kSubclass40) != 0 && mExtra->mParamValue[0] != 0x7f) {
        if (ChooseRandomSound(true) && mSound != nullptr) {
            mSample = CAudioPlaceHolder::ChooseRandomSample(true);
            if (mSample != nullptr) mFlags &= ~kSubclass40;
        }
    }
}

bool CPlaceHolderBranchingSentence::ChooseRandomSound(bool) {
    const bool chosen = AssignSoundToNextBranch(mExtra->mParamValue[0]);
    if (chosen) InitParamatisedControls();
    return chosen;
}

void CPlaceHolderBranchingSentence::CreateObject(CCatagoryContainer* container, CAudioPlaceHolder* slot, void* sound,
                                                 const sound::TbAudioEvent& event) {
    InitObject(Construct<CPlaceHolderBranchingSentence>(slot), container, sound, event);
}

uint32_t CPlaceHolderBranchingSentence::GetClassID() {
    return 1;
}

CPlaceHolderBranchingSentence::~CPlaceHolderBranchingSentence() {}

void CPlaceHolderLinearSentence::CreateObject(CCatagoryContainer* container, CAudioPlaceHolder* slot, void* sound, const sound::TbAudioEvent& event) {
    InitObject(Construct<CPlaceHolderLinearSentence>(slot), container, sound, event);
}

// The index of the new sound is the signed 32-bit address difference from the event's first sound divided by 42 (truncating), compared
// as unsigned with the event's sound count.
bool CPlaceHolderLinearSentence::ChooseRandomSound(bool) {
    if (mEvent != nullptr) {
        if (mSound == nullptr) {
            mSound = mEvent->mSounds.get();
        } else {
            const int32_t difference = static_cast<int32_t>(Address(mSound) + kSoundRecord - mEvent->mSounds.mOffset);
            mSound = mSound + 1;
            const uint32_t index = static_cast<uint32_t>(difference / static_cast<int32_t>(kSoundRecord));
            if (index >= mEvent->mSoundCount) {
                if ((mFlags & kLoopForever) == 0) return false;
                mSound = mEvent->mSounds.get();
            }
        }
    }
    return true;
}

uint32_t CPlaceHolderLinearSentence::GetClassID() {
    return 2;
}

CPlaceHolderLinearSentence::~CPlaceHolderLinearSentence() {}

void CPlaceHolderOneShot::CreateObject(CCatagoryContainer* container, CAudioPlaceHolder* slot, void* sound, const sound::TbAudioEvent& event) {
    InitObject(Construct<CPlaceHolderOneShot>(slot), container, sound, event);
}

TbError CPlaceHolderOneShot::PlaySample(uint32_t now) {
    return StartSample(this, now, false);
}

uint32_t CPlaceHolderOneShot::GetClassID() {
    return 3;
}

void CPlaceHolderOneShotBranchingSentence::CreateObject(CCatagoryContainer* container, CAudioPlaceHolder* slot, void* sound,
                                                        const sound::TbAudioEvent& event) {
    InitObject(Construct<CPlaceHolderOneShotBranchingSentence>(slot), container, sound, event);
}

// The children are deleted through their deleting destructors, each successor read before its predecessor goes.
CPlaceHolderOneShotSentence::~CPlaceHolderOneShotSentence() {
    CAudioPlaceHolder* child = mExtra->mChild;
    while (child != nullptr) {
        CAudioPlaceHolder* next = child->mExtra->mChild;
        delete child;
        child = next;
    }
}

TbError CPlaceHolderOneShotSentence::Stop() {
    CAudioPlaceHolder* child = mExtra->mChild;
    while (child != nullptr) {
        CAudioPlaceHolder* next = child->mExtra->mChild;
        child->Stop();
        delete child;
        child = next;
    }
    mExtra->mChild = nullptr;
    return CAudioPlaceHolder::Stop();
}

// Only the position is taken (the velocity argument is not used); a non-positional sentence puts itself at the integer (0, 1, 0), a
// positional one prepares the info bit 0x20 of a local record nothing reads; then each child updates itself (Element::Update, a direct call).
TbError CPlaceHolderOneShotSentence::Update(const TbSoundPosition& position, const TbSoundPosition&) {
    mPosition = position;
    TbSoundSampleInfo info;
    if ((mFlags & kNonPositional) != 0) {
        mPosition.Set(0, 1, 0);
    } else {
        info.mField0C |= 0x20;
    }
    for (CAudioPlaceHolder* child = mExtra->mChild; child != nullptr; child = child->mExtra->mChild) {
        static_cast<CPlaceHolderOneShotSentenceElement*>(child)->Update();
    }
    return TbError(0);
}

void CPlaceHolderOneShotSentence::UpdateParameter(uint8_t id, uint32_t value) {
    CAudioPlaceHolder::UpdateParameter(id, value);
    if ((mFlags & kSubclass40) != 0 && mExtra->mParamValue[0] != 0x7f) mFlags &= ~kSubclass40;
}

void CPlaceHolderOneShotSentence::UpdateParameterDepandants() {
    for (CAudioPlaceHolder* child = mExtra->mChild; child != nullptr; child = child->mExtra->mChild) {
        child->UpdateParameterDepandants();
    }
}

TbError CPlaceHolderOneShotSentence::PlaySample(uint32_t now) {
    if ((mFlags & kFinished) == 0 && (mFlags & kSubclass40) == 0) {
        ProcessChildPlaceHolders(now);
        RemoveCompleteChildPlaceHolders(now);
    }
    return TbError(0);
}

// With no child yet: a sentence whose first parameter id is set drops its sound once (flag 0x20), then the first child is made.
CAudioPlaceHolder* CPlaceHolderOneShotSentence::GetFirstChild(uint32_t now) {
    if (mExtra->mChild == nullptr) {
        if (mExtra->mParamId[0] != 0 && (mFlags & kSubclass20) == 0) {
            mFlags |= kSubclass20;
            mSound = nullptr;
        }
        AddNewElementTo(this, now);
    }
    return mExtra->mChild;
}

// Each child plays (virtual +0x1c); one whose end time has passed (now > end, unsigned) and which has no successor gets one.
void CPlaceHolderOneShotSentence::ProcessChildPlaceHolders(uint32_t now) {
    if ((mFlags & kFinished) == 0) {
        CAudioPlaceHolder* child = GetFirstChild(now);
        while (child != nullptr) {
            CAudioPlaceHolder* current = child;
            child = current->mExtra->mChild;
            current->PlaySample(now);
            if (now > current->mEndTime && current->mExtra->mChild == nullptr) AddNewElementTo(current, now);
        }
    }
}

// The new child is chained after `parent` (virtual +0x58); a child that cannot start is deleted and unchained.
bool CPlaceHolderOneShotSentence::AddNewElementTo(CAudioPlaceHolder* parent, uint32_t now) {
    parent->mExtra->mChild = GetElement();
    if (static_cast<CPlaceHolderOneShotSentenceElement*>(parent->mExtra->mChild)->Init(mEvent, now)) {
        InitParamatisedControls();
        return true;
    }
    delete parent->mExtra->mChild;
    parent->mExtra->mChild = nullptr;
    return false;
}

CPlaceHolderOneShotSentenceElement* CPlaceHolderOneShotSentence::GetElement() {
    return new CPlaceHolderOneShotSentenceElement(this);
}

// A child whose end time has passed (now >= end, unsigned) is unchained from its predecessor and deleted.
void CPlaceHolderOneShotSentence::RemoveCompleteChildPlaceHolders(uint32_t now) {
    CAudioPlaceHolder* previous = this;
    CAudioPlaceHolder* child = mExtra->mChild;
    while (child != nullptr) {
        if (now < child->mEndTime) {
            previous = child;
            child = child->mExtra->mChild;
        } else {
            CAudioPlaceHolder* next = child->mExtra->mChild;
            previous->mExtra->mChild = next;
            delete child;
            child = next;
        }
    }
}

void CPlaceHolderOneShotSentence::CreateObject(CCatagoryContainer* container, CAudioPlaceHolder* slot, void* sound, const sound::TbAudioEvent& event) {
    InitObject(Construct<CPlaceHolderOneShotSentence>(slot), container, sound, event);
}

uint32_t CPlaceHolderOneShotSentence::GetClassID() {
    return 4;
}

bool CPlaceHolderOneShotThrowaway::FillSampleInfo(uint32_t now, TbSoundSampleInfo& info) {
    if (!CAudioPlaceHolder::FillSampleInfo(now, info)) return false;
    info.mFlags &= ~0x2000u;   // rlwinm 0, 0x13, 0x11: clears flag 0x2000 (the completed callback request)
    info.mCompletedCallback = nullptr;
    return true;
}

uint32_t CPlaceHolderOneShotThrowaway::GetClassID() {
    return 8;
}

// The sample `sample` ended: the next sound and sample are picked (virtual +0x3c and +0x40, rotate true; the sample is stored before it
// is tested) and, when the basic info can be filled (virtual +0x4c), the manager makes the next sample from it.
void CPlaceHolderSentence::SoundCallback(void* sample, void*) {
    if (ChooseRandomSound(true) && mpSoundManager != nullptr && mSound != nullptr) {
        mSample = ChooseRandomSample(true);
        if (mSample != nullptr) {
            TbSoundSampleInfo info;
            void* control = nullptr;
            if (FillBasicSampleInfo(info)) mpSoundManager->ConstructSample(sample, control, info);
        }
    }
}

bool CPlaceHolderSentence::FillBasicSampleInfo(TbSoundSampleInfo& info) {
    if (!CAudioPlaceHolder::FillBasicSampleInfo(info)) return false;
    info.SetCallback(this);
    return true;
}

TbError CPlaceHolderSentence::PlaySample(uint32_t now) {
    return StartSample(this, now, true);
}

void CPlaceHolderSentence::CreateObject(CCatagoryContainer* container, CAudioPlaceHolder* slot, void* sound, const sound::TbAudioEvent& event) {
    InitObject(Construct<CPlaceHolderSentence>(slot), container, sound, event);
}

// ---- CPlaceHolderSentenceShuffle ---------------------------------------------------------------------------------------------

namespace {

void SetShuffle(CAudioPlaceHolder* holder, CPlaceHolderSentenceShuffle::CListShuffle* shuffle) {
    holder->mExtra->mParent = reinterpret_cast<CAudioPlaceHolder*>(shuffle);
}

}  // namespace

CPlaceHolderSentenceShuffle::CListShuffle::CListShuffle(CEventData* event) : mEvent(nullptr), mSeed(0), mIndex(0) {
    mToggle = 0;
    mStep[0] = 1;
    mStep[1] = 2;
    mSeed = LbTime_GetClock();
    Init(event);
}

void CPlaceHolderSentenceShuffle::CListShuffle::Init(CEventData* event) {
    mEvent = event;
    Shuffle();
}

// Starts at the pair (p, q) = (the record before the random one, the random one) and swaps the 42-byte records at p and q, p moving
// down one record and q forward by one or two records alternately, while p is not before the array and q is before its last record.
// The compares are unsigned 32-bit address compares (arena offsets here); the draw is RangedRandomSlow on a COPY of mSeed.
void CPlaceHolderSentenceShuffle::CListShuffle::Shuffle() {
    mIndex = 0;
    int32_t seed = static_cast<int32_t>(mSeed);
    const uint32_t pick = bfl::maths::RangedRandomSlow(seed, mEvent->mSoundCount);
    const uint32_t first = mEvent->mSounds.mOffset;
    uint32_t p = first + (pick - 1) * kSoundRecord;
    uint32_t q = first + pick * kSoundRecord;
    for (;;) {
        const uint32_t base = mEvent->mSounds.mOffset;
        if (p < base) break;
        if (!(q < base + (mEvent->mSoundCount - 1) * kSoundRecord)) break;
        uint8_t* low = tpw::gArenaBase + p;
        uint8_t* high = tpw::gArenaBase + q;
        uint8_t saved[kSoundRecord];
        std::memcpy(saved, low, kSoundRecord);
        std::memcpy(low, high, kSoundRecord);
        std::memcpy(high, saved, kSoundRecord);
        p -= kSoundRecord;
        const uint8_t toggle = mToggle;
        mToggle = toggle == 0 ? 1 : 0;
        q += mStep[toggle == 0 ? 1 : 0] * kSoundRecord;
    }
}

CSoundData* CPlaceHolderSentenceShuffle::CListShuffle::GetNextSound(uint32_t reshuffle) {
    if (mIndex >= mEvent->mSoundCount) {
        if (reshuffle == 0) return nullptr;
        Shuffle();
    }
    const uint32_t index = mIndex;
    CSoundData* sound = mEvent->mSounds.get() + index;
    mIndex = index + 1;
    return sound;
}

CPlaceHolderSentenceShuffle::CPlaceHolderSentenceShuffle() : CPlaceHolderSentence() {
    AllocateExtraData();
    SetShuffle(this, nullptr);
}

CPlaceHolderSentenceShuffle::~CPlaceHolderSentenceShuffle() {
    if (ShuffleOf(this) != nullptr) delete ShuffleOf(this);
}

void CPlaceHolderSentenceShuffle::CreateObject(CCatagoryContainer* container, CAudioPlaceHolder* slot, void* sound,
                                               const sound::TbAudioEvent& event) {
    InitObject(Construct<CPlaceHolderSentenceShuffle>(slot), container, sound, event);
}

void CPlaceHolderSentenceShuffle::CreateObject(CAudioPlaceHolder* slot) {
    Construct<CPlaceHolderSentenceShuffle>(slot);
}
void CPlaceHolderSentenceShuffle::SetListInfo(CEventData* event) {
    CListShuffle* shuffle = ShuffleOf(this);
    if (shuffle == nullptr) {
        shuffle = new CListShuffle(event);
        SetShuffle(this, shuffle);
    } else if (shuffle->mEvent != event) {
        shuffle->Init(event);
    }
}

bool CPlaceHolderSentenceShuffle::ChooseRandomSound(bool) {
    SetListInfo(mEvent);
    mSound = ShuffleOf(this)->GetNextSound(1);
    return mSound != nullptr;
}

uint32_t CPlaceHolderSentenceShuffle::GetClassID() {
    return 10;
}

// Unit v51112: small functions of the sound program's AudioPlaceHolder, TbSoundSample*, CSprocketsModule ranges (see the header block).

// ---- LB_MIN / LB_MAX ---------------------------------------------------------------------------------------------------------

const uint32_t& LbMaxULong(const uint32_t& a, const uint32_t& b) {
    if (a > b) return a;
    return b;
}
const float& LbMinFloat(const float& a, const float& b) {
    if (a < b) return a;
    return b;
}
const uint32_t& LbMinUInt(const uint32_t& a, const uint32_t& b) {
    if (a < b) return a;
    return b;
}
const int32_t& LbMaxLong(const int32_t& a, const int32_t& b) {
    if (a > b) return a;
    return b;
}
const int32_t& LbMinInt(const int32_t& a, const int32_t& b) {
    if (a < b) return a;
    return b;
}

TbRect* TbRectAssign(TbRect* dest, const TbRect* source) {
    dest->left = source->left;
    dest->top = source->top;
    dest->right = source->right;
    dest->bottom = source->bottom;
    return dest;
}

void* OperatorNewPlacementSound(uint32_t, void* where) { return where; }

namespace TbSoundSampleStoreSound {
TbSoundPipeChannel* IteratorIncrement(ChannelIterator* self, uint32_t elementSize) {
    uint8_t* next = reinterpret_cast<uint8_t*>(self->mCurrent) + elementSize;
    self->mCurrent = reinterpret_cast<TbSoundPipeChannel*>(next);
    if (next < reinterpret_cast<uint8_t*>(self->mEnd)) return self->mCurrent;
    return nullptr;
}
}  // namespace TbSoundSampleStoreSound

void* CFormatToWaveFormatEx(CFormat* self) {
    self->FillWaveFormatEx();
    return reinterpret_cast<uint8_t*>(self) + 4;
}

namespace tbstd {
float logf(float x) { return static_cast<float>(::log(static_cast<double>(x))); }
}  // namespace tbstd

// ---- CAutoArrayPtr<CAudioPlaceHolder*> ---------------------------------------------------------------------------------------

// The runtime's heap glue (new[] 0x1002ebfc, delete[] 0x1002ebcc, delete 0x1002ea94): faked by the test library.
extern "C" void* SoundHeapNewPointers(uint32_t count);
extern "C" void SoundHeapDeleteArray(void* block);
extern "C" void SoundHeapDelete(void* block);

// Lead 2026-10-20 (v51112): the host bodies of that glue and of the seed's (0x1002ebfc is operator new[], 0x1002ebcc operator
// delete[], 0x1002ea94 operator delete and 0x1002eaac LbTime_GetClock, as src/AudioPlaceHolderList.cpp says). They are weak,
// so the test library's recording fakes take their place there; the launcher has no other definition of these unmangled names.
extern "C" __attribute__((weak)) void* SoundHeapNewPointers(uint32_t count) { return ::operator new[](count * sizeof(void*)); }
extern "C" __attribute__((weak)) void SoundHeapDeleteArray(void* block) { ::operator delete[](block); }
extern "C" __attribute__((weak)) void SoundHeapDelete(void* block) { ::operator delete(block); }
CAutoArrayPtr<CAudioPlaceHolder*>* CAutoArrayPtrPlaceHolderCtor(CAutoArrayPtr<CAudioPlaceHolder*>* self, uint32_t count) {
    self->mData = static_cast<CAudioPlaceHolder**>(SoundHeapNewPointers(count));
    return self;
}

CAutoArrayPtr<CAudioPlaceHolder*>* CAutoArrayPtrPlaceHolderDtor(CAutoArrayPtr<CAudioPlaceHolder*>* self, int16_t deleteFlag) {
    if (self != nullptr) {
        if (self->mData != nullptr) SoundHeapDeleteArray(self->mData);
        if (deleteFlag > 0) SoundHeapDelete(self);
    }
    return self;
}

CAudioPlaceHolder** CAutoArrayPtrPlaceHolderGet(const CAutoArrayPtr<CAudioPlaceHolder*>* self) { return self->mData; }

// ---- files -------------------------------------------------------------------------------------------------------------------

int32_t CFileWriteMain(bfl::FileSystem::CFile* self, const void* source, uint32_t size) {
    if (self->mImplementation == nullptr) return -1;
    return self->mImplementation->Write(source, size);
}

namespace bfl::FileSystem {
// 0x10020c50 (104)
CDiscFileStorage* CDiscFileStorage::DestroySound(CDiscFileStorage* self, int16_t deleteFlag) {
    if (self) {
        self->CFileStorage::~CFileStorage();
        if (deleteFlag > 0) SoundHeapDelete(self);
    }
    return self;
}
}  // namespace bfl::FileSystem

// CSprocketsChannel (unit v5263): the Sound Sprockets channel object, 0x1002a29c .. 0x1002b0b8 of the sound library.
// The host class is CSprocketsChannelMac: platform/sound_module.h owns the name CSprocketsChannel.

using namespace SamsUtilities;

// 0x1002a29c (112)
CSprocketsChannelMac::CSprocketsChannelMac() {
    USoundChannel_Construct(this, 0x10c);
    mVtable10 = 0x1003e0dc;
    mVtable4 = 0x1003e0e4;
    mVtable8 = 0x1003e0f4;
    mPipe = nullptr;
    mBuffer = nullptr;
    mBufferSize = 0;
    m3D = 0;
    mFlag49 = 0;
}

// 0x1002a33c (132): the deleting form (flag > 0) goes through operator delete.
CSprocketsChannelMac::~CSprocketsChannelMac() {
    mVtable10 = 0x1003e0dc;
    mVtable4 = 0x1003e0e4;
    mVtable8 = 0x1003e0f4;
    Cleanup();
    USoundChannel_Destruct(this, 0);
}

void CSprocketsChannelMac::operator delete(void* object) { USoundChannel_Free(object); }

// 0x1002a3f0 (168)
void CSprocketsChannelMac::Cleanup() {
    if (mSndChannel != nullptr) {
        SndCommand command;
        command.cmd = 0x2e;
        command.param1 = 0;
        command.param2 = 0;
        SndDoImmediate_(mSndChannel, &command);
        command.cmd = 4;
        SndDoImmediate_(mSndChannel, &command);
        command.cmd = 3;
        SndDoImmediate_(mSndChannel, &command);
    }
    if (mBuffer != nullptr) {
        DisposeBuffer_(mBuffer);
        mBuffer = nullptr;
        mBufferSize = 0;
    }
}

// 0x1002a4cc (184)
void CSprocketsChannelMac::Callback(SndCommand* command) {
    UCommandPlay play;
    if (mPipe != nullptr && mBuffer != nullptr) {
        if (static_cast<USoundDescription*>(mDescription) != nullptr) {
            if (TbSoundPipeChannel::StreamerCallback(mBuffer, mBufferSize, mPipe)) {
                play.SetImmediate(1);
                SendToChannel(this, play);
            }
        }
    }
    USoundChannel_Callback(this, command);
}

// 0x1002a8a0 (124)
void CSprocketsChannelMac::Play() {
    UCommandPlay play;
    if (mPipe != nullptr) {
        if (TbSoundPipeChannel::StreamerCallback(mBuffer, mBufferSize, mPipe)) {
            play.SetImmediate(1);
            SendToChannel(this, play);
        }
    }
}

// 0x1002b0b8 (112)
void CSprocketsChannelMac::Set3D(uint8_t on) {
    m3D = on;
    UCommandSoundType command(static_cast<SoundType>(on != 0 ? 1 : 0));
    command.SetImmediate(1);
    SendToChannel(this, command);
}
