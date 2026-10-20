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
#include "PlaceHolderTypes.h"

#include <cstring>
#include <new>

#include "LbTime.h"
#include "PlaceHolderHelpers.h"
#include "TbSoundManager.h"
#include "platform/arena.h"

namespace {

// The Mac address of a record of the sound data.
uint32_t Address(const void* pointer) {
    return pointer != nullptr ? tpw::ArenaOffset(pointer) : 0;
}

uint8_t BranchLow(const CSoundBranchData& branch) { return branch.mF06[0]; }
uint8_t BranchHigh(const CSoundBranchData& branch) { return branch.mF06[1]; }

constexpr uint32_t kSoundRecord = 0x2a;   // sizeof(CSoundData) on the Mac

// operator new(0x60, slot) and the constructor: the slot itself, or NULL when there is no slot (nothing is constructed then).
template <class T>
T* Construct(CAudioPlaceHolder* slot) {
    static_assert(sizeof(T) == sizeof(CAudioPlaceHolder), "a placeholder subclass adds no data member (the list's slot holds one)");
    return slot != nullptr ? new (slot) T : nullptr;
}

// The tail all eight four-argument CreateObject statics share: Init through the vtable (+0x18) with the container, the sound data,
// the event's position (+0xc) and its float (+0x18); the object when Init did not fail, else NULL (the object stays constructed).
CAudioPlaceHolder* InitObject(CAudioPlaceHolder* object, CCatagoryContainer* container, void* sound, const sound::TbAudioEvent& event) {
    if (object == nullptr) return nullptr;
    const TbError error = object->Init(container, sound, event.mPosition, event.mW);
    return static_cast<int32_t>(error) >= 0 ? object : nullptr;
}

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

// ---- CPlaceHolderOneShot -----------------------------------------------------------------------------------------------------

CPlaceHolderOneShot::CPlaceHolderOneShot() : CAudioPlaceHolder() {}

CPlaceHolderOneShot::~CPlaceHolderOneShot() {}

TbError CPlaceHolderOneShot::PlaySample(uint32_t now) {
    return StartSample(this, now, false);
}

uint32_t CPlaceHolderOneShot::GetClassID() {
    return 3;
}

void CPlaceHolderOneShot::CreateObject(CCatagoryContainer* container, CAudioPlaceHolder* slot, void* sound, const sound::TbAudioEvent& event) {
    InitObject(Construct<CPlaceHolderOneShot>(slot), container, sound, event);
}

void CPlaceHolderOneShot::CreateObject(CAudioPlaceHolder* slot) {
    Construct<CPlaceHolderOneShot>(slot);
}

// ---- CPlaceHolderOneShotSentence ---------------------------------------------------------------------------------------------

CPlaceHolderOneShotSentence::CPlaceHolderOneShotSentence() : CPlaceHolderOneShot() {
    AllocateExtraData();
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

uint32_t CPlaceHolderOneShotSentence::GetClassID() {
    return 4;
}

CPlaceHolderOneShotSentenceElement* CPlaceHolderOneShotSentence::GetElement() {
    return new CPlaceHolderOneShotSentenceElement(this);
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

void CPlaceHolderOneShotSentence::CreateObject(CAudioPlaceHolder* slot) {
    Construct<CPlaceHolderOneShotSentence>(slot);
}

// ---- CPlaceHolderOneShotBranchingSentence ------------------------------------------------------------------------------------

CPlaceHolderOneShotBranchingSentence::CPlaceHolderOneShotBranchingSentence() : CPlaceHolderOneShotSentence() {}

CPlaceHolderOneShotBranchingSentence::~CPlaceHolderOneShotBranchingSentence() {}

uint32_t CPlaceHolderOneShotBranchingSentence::GetClassID() {
    return 6;
}

CPlaceHolderOneShotSentenceElement* CPlaceHolderOneShotBranchingSentence::GetElement() {
    return new CPlaceHolderOneShotBranchingSentenceElement(this);
}

void CPlaceHolderOneShotBranchingSentence::CreateObject(CCatagoryContainer* container, CAudioPlaceHolder* slot, void* sound,
                                                        const sound::TbAudioEvent& event) {
    InitObject(Construct<CPlaceHolderOneShotBranchingSentence>(slot), container, sound, event);
}

void CPlaceHolderOneShotBranchingSentence::CreateObject(CAudioPlaceHolder* slot) {
    Construct<CPlaceHolderOneShotBranchingSentence>(slot);
}

// ---- CPlaceHolderSentence ----------------------------------------------------------------------------------------------------

CPlaceHolderSentence::CPlaceHolderSentence() : CAudioPlaceHolder() {}

CPlaceHolderSentence::~CPlaceHolderSentence() {}

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

TbError CPlaceHolderSentence::PlaySample(uint32_t now) {
    return StartSample(this, now, true);
}

bool CPlaceHolderSentence::FillBasicSampleInfo(TbSoundSampleInfo& info) {
    if (!CAudioPlaceHolder::FillBasicSampleInfo(info)) return false;
    info.SetCallback(this);
    return true;
}

uint32_t CPlaceHolderSentence::GetClassID() {
    return 5;
}

void CPlaceHolderSentence::CreateObject(CCatagoryContainer* container, CAudioPlaceHolder* slot, void* sound, const sound::TbAudioEvent& event) {
    InitObject(Construct<CPlaceHolderSentence>(slot), container, sound, event);
}

void CPlaceHolderSentence::CreateObject(CAudioPlaceHolder* slot) {
    Construct<CPlaceHolderSentence>(slot);
}

// ---- CPlaceHolderLinearSentence ----------------------------------------------------------------------------------------------

CPlaceHolderLinearSentence::CPlaceHolderLinearSentence() : CPlaceHolderSentence() {}

CPlaceHolderLinearSentence::~CPlaceHolderLinearSentence() {}

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

void CPlaceHolderLinearSentence::CreateObject(CCatagoryContainer* container, CAudioPlaceHolder* slot, void* sound, const sound::TbAudioEvent& event) {
    InitObject(Construct<CPlaceHolderLinearSentence>(slot), container, sound, event);
}

void CPlaceHolderLinearSentence::CreateObject(CAudioPlaceHolder* slot) {
    Construct<CPlaceHolderLinearSentence>(slot);
}

// ---- CPlaceHolderBranchingSentence -------------------------------------------------------------------------------------------

CPlaceHolderBranchingSentence::CPlaceHolderBranchingSentence() : CPlaceHolderSentence() {}

CPlaceHolderBranchingSentence::~CPlaceHolderBranchingSentence() {}

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

uint32_t CPlaceHolderBranchingSentence::GetClassID() {
    return 1;
}

void CPlaceHolderBranchingSentence::CreateObject(CCatagoryContainer* container, CAudioPlaceHolder* slot, void* sound,
                                                 const sound::TbAudioEvent& event) {
    InitObject(Construct<CPlaceHolderBranchingSentence>(slot), container, sound, event);
}

void CPlaceHolderBranchingSentence::CreateObject(CAudioPlaceHolder* slot) {
    Construct<CPlaceHolderBranchingSentence>(slot);
}

// ---- CPlaceHolderSentenceShuffle ---------------------------------------------------------------------------------------------

namespace {

// The shuffle of a placeholder is kept in the extra data's +0x1c word.
CPlaceHolderSentenceShuffle::CListShuffle* ShuffleOf(CAudioPlaceHolder* holder) {
    return reinterpret_cast<CPlaceHolderSentenceShuffle::CListShuffle*>(holder->mExtra->mParent);
}

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

void CPlaceHolderSentenceShuffle::CreateObject(CCatagoryContainer* container, CAudioPlaceHolder* slot, void* sound,
                                               const sound::TbAudioEvent& event) {
    InitObject(Construct<CPlaceHolderSentenceShuffle>(slot), container, sound, event);
}

void CPlaceHolderSentenceShuffle::CreateObject(CAudioPlaceHolder* slot) {
    Construct<CPlaceHolderSentenceShuffle>(slot);
}

// ---- CListSentence -----------------------------------------------------------------------------------------------------------

namespace {

// The list data of a CListSentence is kept in the extra data's +0x1c word.
CListSentence::CListData* ListDataOf(CAudioPlaceHolder* holder) {
    return reinterpret_cast<CListSentence::CListData*>(holder->mExtra->mParent);
}

}  // namespace

CListSentence::CListData::CListData(const TbAudioEventList& list) : mCount(list.mCount), mIndex(0), mItems(mCount) {
    std::memcpy(static_cast<TbAudioEventList::ListItem*>(mItems), list.mItems, static_cast<std::size_t>(mCount << 3));
}

CListSentence::CListData::~CListData() {}

CListSentence::CListSentence(const TbAudioEventList* list) : CPlaceHolderSentence() {
    AllocateExtraData();
    mExtra->mParent = reinterpret_cast<CAudioPlaceHolder*>(new CListData(*list));
}

CListSentence::~CListSentence() {
    delete ListDataOf(this);
}

// Loop flag 2 first; then the entry at the cursor (the cursor advances even past the end) names a category and an event: the event is
// mapped through the category container's data, and the base's ChooseRandomSound (a direct call) picks a sound of it. False when the
// cursor is past the end, the category has no container or data, or the mapper finds no event.
bool CListSentence::ChooseRandomSound(bool) {
    mFlags |= kLooping;
    TbAudioEventList::ListItem item;
    CListData* data = ListDataOf(this);
    const uint32_t index = data->mIndex;
    data->mIndex = index + 1;
    if (index < data->mCount) {
        item.mCategory = static_cast<TbAudioEventList::ListItem*>(data->mItems)[index].mCategory;
        item.mEvent = static_cast<TbAudioEventList::ListItem*>(data->mItems)[index].mEvent;
        CCatagoryContainer* container = TbMapStreamer::Instance()->GetCatagoryDeque()[item.mCategory];
        if (container != nullptr && container->mData != nullptr) {
            const sound::TbAudioEvent event(item.mCategory, item.mEvent, mPosition);
            CEventData* mapped = TbEventMapper::MapEvent(container->mData, event);
            if (mapped != nullptr) {
                mEvent = mapped;
                return CAudioPlaceHolder::ChooseRandomSound(false);
            }
        }
    }
    return false;
}

void CListSentence::CreateObject(CCatagoryContainer* container, CAudioPlaceHolder* slot, void* sound, const sound::TbAudioEvent& event) {
    CreateObject(slot, static_cast<const TbAudioEventList*>(&event));
    InitObject(slot, container, sound, event);
}

void CListSentence::CreateObject(CAudioPlaceHolder* slot, const TbAudioEventList* list) {
    if (slot != nullptr) new (slot) CListSentence(list);
}
