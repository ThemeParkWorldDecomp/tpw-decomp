// ListSentence.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of ListSentence code, named after it.

#include "helpers/PlaceHolderSentenceHelpers.h"
#include "LbTime.h"
#include "PlaceHolderHelpers.h"
#include "PlaceHolderTypes.h"
#include "platform/arena.h"
#include "TbDynamicStringTemplate.h"
#include "TbSoundBank.h"
#include "TbSoundManager.h"
#include <cstddef>
#include <cstring>
#include <new>

// The sound library's sample banks (file TbMMFileBank.cpp of the Mac's "sound symbols shared" library, with the factory and
// the bank handle from ChannelState.cpp). See include/TbSoundBank.h for the layouts, the vtables and the file format.
//
// Every allocation goes through the nothrow forms of operator new, new[] and delete[] (the library's own allocator pair on
// the Mac: __nw__FUl, __nwa__FUl and __dla__FPv), so that a failed allocation is a NULL result as it is there.

// ---- CFileOperation ----------------------------------------------------------------------------------------------------

// 0x1001e720 (56 bytes)
int32_t CFileOperation::GetBankPath(bfl::FileSystem::CFileStorage& storage, TbDynamicStringTemplate<char>& path,
                                    const TbDynamicStringTemplate<char>& name, int32_t flags) {
    const_cast<TbDynamicStringTemplate<char>&>(name) += "HD.sdt";
    return 1;
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

uint32_t CPlaceHolderSentence::GetClassID() {
    return 5;
}

// ---- CListSentence -----------------------------------------------------------------------------------------------------------

namespace {

// The list data of a CListSentence is kept in the extra data's +0x1c word.
CListSentence::CListData* ListDataOf(CAudioPlaceHolder* holder) {
    return reinterpret_cast<CListSentence::CListData*>(holder->mExtra->mParent);
}

}  // namespace

void CListSentence::CreateObject(CCatagoryContainer* container, CAudioPlaceHolder* slot, void* sound, const sound::TbAudioEvent& event) {
    CreateObject(slot, static_cast<const TbAudioEventList*>(&event));
    InitObject(slot, container, sound, event);
}

void CListSentence::CreateObject(CAudioPlaceHolder* slot, const TbAudioEventList* list) {
    if (slot != nullptr) new (slot) CListSentence(list);
}
CListSentence::CListSentence(const TbAudioEventList* list) : CPlaceHolderSentence() {
    AllocateExtraData();
    mExtra->mParent = reinterpret_cast<CAudioPlaceHolder*>(new CListData(*list));
}

CListSentence::CListData::CListData(const TbAudioEventList& list) : mCount(list.mCount), mIndex(0), mItems(mCount) {
    std::memcpy(static_cast<TbAudioEventList::ListItem*>(mItems), list.mItems, static_cast<std::size_t>(mCount << 3));
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

CListSentence::~CListSentence() {
    delete ListDataOf(this);
}

CListSentence::CListData::~CListData() {}
