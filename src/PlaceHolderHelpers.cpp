// PlaceHolderHelpers.cpp: CMapReference and the one-shot sentence children of the sound library (unit w79g4, "sound symbols shared",
// the library's CSprocketsModule.cpp). See include/PlaceHolderHelpers.h for the classes, their vtables and the overrides.
//
// Conventions of this file. The parent of an Element is the sentence placeholder kept in the extra data's mParent; the Mac code
// reads it through the extra data on every use. Virtual calls the Mac makes through the object's vtable are virtual calls here
// (Init calls +0x3c, +0x40 and +0x50, FillSampleInfo +0x4c); the base's non-virtual members are called directly. The sound manager
// is the class static CAudioPlaceHolder::mpSoundManager (0x1003f360). The pointers of the map data that the Mac compares as 32-bit
// addresses are compared as arena offsets (the value a Ptr32 holds).
#include "PlaceHolderHelpers.h"

#include "TbMapStreamer.h"
#include "TbSoundManager.h"
#include "platform/arena.h"
#include "snd_commands.h"

namespace {

// A pointer into the map data as the Mac's 32-bit address: the arena offset, which is what a Ptr32 field of the data holds.
uint32_t Address(const void* pointer) {
    return pointer != nullptr ? tpw::ArenaOffset(pointer) : 0;
}

// The flag bits of the sample info's word at +0xc (include/TbSoundInfo.h calls it mField0C) the Element classes set.
constexpr uint32_t kInfoNonPositional = 0x1;
constexpr uint32_t kInfoPositional = 0x20;
constexpr uint32_t kInfoQSound = 0x1000;

// The roles of the bytes of a branch record (CSoundBranchData +6 and +7): the lowest and highest value of the parent's parameter
// byte that selects the branch.
uint8_t BranchLow(const CSoundBranchData& branch) { return branch.mF06[0]; }
uint8_t BranchHigh(const CSoundBranchData& branch) { return branch.mF06[1]; }

}  // namespace

// ---- CMapReference ---------------------------------------------------------------------------------------------------------

CMapReference::CMapReference() {}

// The compares are the listing's: the event address against the events array (unsigned, an address one past the end matches), then
// the three indexes by signed division (the compiler's multiply and shift for /20 and /42, shift and add-carry for /16), a negative
// index becomes 0 and an index at or past the count becomes count - 1 (signed compares; a sample count is its low 16 bits). The
// first match ends the search, so a one-past-the-end address of one category that is also the first event of the next gives the
// first category.
void CMapReference::GetSoundIds(int32_t& category, int32_t& event, int32_t& sound, int32_t& sample, CAudioPlaceHolder* holder) {
    CResize<CCatagoryContainer*>& containers = TbMapStreamer::Instance()->GetCatagoryDeque();
    category = 0;
    event = 0;
    sound = 0;
    sample = 0;
    const uint32_t sampleAddress = Address(holder->mSample);   // +0xc
    const uint32_t eventAddress = Address(holder->mEvent);     // +4
    const uint32_t soundAddress = Address(holder->mSound);     // +8
    if (sampleAddress == 0 || soundAddress == 0 || eventAddress == 0) {
        sample = 0;
        sound = 0;
        event = 0;
    }
    const int32_t count = static_cast<int32_t>(containers.Size());
    for (int32_t index = 0; index < count; ++index) {
        CCatagoryContainer* container = containers[static_cast<uint32_t>(index)];
        CCatagoryData* data = container != nullptr ? container->mData : nullptr;
        if (data != nullptr && data->mEventCount != 0) {
            const uint32_t events = data->mEvents.mOffset;
            if (eventAddress >= events && eventAddress <= events + data->mEventCount * 0x14) {
                event = static_cast<int32_t>(eventAddress - events) / 0x14;
                if (event < 0) event = 0;
                if (event >= static_cast<int32_t>(container->mData->mEventCount)) event = static_cast<int32_t>(container->mData->mEventCount) - 1;
                // the event record is read through the placeholder's own pointer (r30 holds the address, which is the record)
                const CEventData* eventRecord = holder->mEvent;
                sound = static_cast<int32_t>(soundAddress - eventRecord->mSounds.mOffset) / 0x2a;
                if (sound < 0) sound = 0;
                if (sound >= static_cast<int32_t>(eventRecord->mSoundCount)) sound = static_cast<int32_t>(eventRecord->mSoundCount) - 1;
                const CSoundData* soundRecord = holder->mSound;
                sample = static_cast<int32_t>(sampleAddress - soundRecord->mSamples.mOffset) / 16;
                if (sample < 0) sample = 0;
                const int32_t samples = static_cast<int32_t>(soundRecord->mSampleCount & 0xffff);
                if (sample >= samples) sample = samples - 1;
                category = index;
                return;
            }
        }
        ++category;
    }
    category = 0;
}

CCatagoryContainer* CMapReference::GetSoundPointer(uint32_t category, CAudioPlaceHolder*) {
    if (category < TbMapStreamer::Instance()->GetCatagoryDeque().Size()) {
        return TbMapStreamer::Instance()->GetCatagoryDeque()[category];
    }
    return nullptr;
}

// ---- CPlaceHolderOneShotThrowaway ------------------------------------------------------------------------------------------

CPlaceHolderOneShotThrowaway::CPlaceHolderOneShotThrowaway() : CPlaceHolderOneShot() {}

CPlaceHolderOneShotThrowaway::~CPlaceHolderOneShotThrowaway() {}

bool CPlaceHolderOneShotThrowaway::FillSampleInfo(uint32_t now, TbSoundSampleInfo& info) {
    if (!CAudioPlaceHolder::FillSampleInfo(now, info)) return false;
    info.mFlags &= ~0x2000u;   // rlwinm 0, 0x13, 0x11: clears flag 0x2000 (the completed callback request)
    info.mCompletedCallback = nullptr;
    return true;
}

uint32_t CPlaceHolderOneShotThrowaway::GetClassID() {
    return 8;
}

// ---- CPlaceHolderOneShotSentenceElement -------------------------------------------------------------------------------------

CPlaceHolderOneShotSentenceElement::CPlaceHolderOneShotSentenceElement(CAudioPlaceHolder* parent) : CPlaceHolderOneShotThrowaway() {
    AllocateExtraData();
    mExtra->mParent = parent;
}

CPlaceHolderOneShotSentenceElement::~CPlaceHolderOneShotSentenceElement() {}

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

// ---- CPlaceHolderOneShotBranchingSentenceElement ---------------------------------------------------------------------------

CPlaceHolderOneShotBranchingSentenceElement::CPlaceHolderOneShotBranchingSentenceElement(CAudioPlaceHolder* parent)
    : CPlaceHolderOneShotSentenceElement(parent) {}

CPlaceHolderOneShotBranchingSentenceElement::~CPlaceHolderOneShotBranchingSentenceElement() {}

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

uint32_t CPlaceHolderOneShotBranchingSentenceElement::GetClassID() {
    return 9;
}
