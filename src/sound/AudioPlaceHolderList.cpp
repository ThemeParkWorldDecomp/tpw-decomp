// AudioPlaceHolderList.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of AudioPlaceHolderList code, named after it.

#include "AudioPlaceHolder.h"
#include "AudioPlaceHolderList.h"
#include "LbFile.h"
#include "LbTime.h"
#include "PlaceHolderHelpers.h"
#include "PlaceHolderTypes.h"
#include "ppc_math.h"
#include "snd_commands.h"
#include "TbMapStreamer.h"
#include "TbSoundBank.h"
#include "TbSoundManager.h"
#include <cmath>
#include <cstring>
#include <new>

// AudioPlaceHolderList.cpp: CAudioPlaceHolderList, the pool and the priority list of the sound placeholders (unit w79g1, "sound
// symbols shared", file CSprocketsModule.cpp of the library). See include/AudioPlaceHolderList.h for the layout, the handles and
// the pool; the function addresses are in the header.
//
// Conventions of this file. The listing's operation order is kept (the call logs of the test compare it). The virtual calls on a
// placeholder are `lwz r12, 0(this); lwz r12, SLOT(r12); bl 0x1002f154` on the Mac: Release is slot +0x14, PlaySample +0x1c, Stop
// +0x20, Update +0x30, UpdateParameter +0x34 (all declared by include/AudioPlaceHolder.h in that order). Answers a virtual
// returns through a hidden TbError buffer are dropped where the listing drops them. The file's virtuals are Read +0x10, Write
// +0x14, Seek +0x18 and GetPos +0x20 (bfl::FileSystem::CFile). The words of a save block are written with
// SamsUtilities::USwap32 (include/TbSoundBank.h: the file is little-endian, so the host writes native words). The runtime's
// allocator pair: 0x1002ebfc is operator new[] and 0x1002ebcc operator delete[] (CMemBlock::CreateObject allocates the block with the
// first and the list's destructor frees it with the second, without running ~CMemBlock); 0x1002eaac is LbTime_GetClock.

namespace {

constexpr uint32_t kSlotsPerBlock = 100;     // the literal 100 (0x64) of AddNewBlock, Handle2Ptr and CMemBlock::CreateObject's caller
constexpr uint32_t kMaxChannels = 0x1e;      // the words at 0x1003cc6c (constructor) and 0x1003cc70 (SetNumberOfChannels)
constexpr uint32_t kPriorityEntries = 0x1f;  // the array of the constructor, cleared by 0x7c bytes (31 words of four bytes)

// The slot: the host placeholder (the Mac's 0x60 bytes are the host's sizeof). The block header is padded so that the first slot
// has the placeholder's alignment (the Mac's header is 0xc bytes).
constexpr std::size_t kSlotSize = sizeof(CAudioPlaceHolder);
constexpr std::size_t kBlockHeader = (sizeof(CAudioPlaceHolderList::CMemBlock) + alignof(CAudioPlaceHolder) - 1) / alignof(CAudioPlaceHolder) *
                                     alignof(CAudioPlaceHolder);
static_assert(kBlockHeader % alignof(CAudioPlaceHolder) == 0, "the first slot is aligned");

// The handle words are 32 bits on the Mac. A handle the list makes is zero-extended; the compares use the low half.
uint32_t Word(const void* handle) {
    return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(handle));
}

void* HandleOf(uint32_t word) {
    return reinterpret_cast<void*>(static_cast<uintptr_t>(word));
}

// 0x1001e470 (24): LB_MAX<long, long>
int32_t LbMax(int32_t a, int32_t b) {
    return a > b ? a : b;
}

// One word of the save block, written or read through the file's virtuals (the answers are not looked at).
template <class T>
void WriteWord(bfl::FileSystem::CFile* file, T value) {
    const T swapped = ::SamsUtilities::USwap32<T>(value);
    file->Write(&swapped, 4);
}

template <class T>
void ReadWord(bfl::FileSystem::CFile* file, T& value) {
    file->Read(&value, 4);
    value = ::SamsUtilities::USwap32<T>(value);
}

}  // namespace

// ---- construction and destruction --------------------------------------------------------------------------------------------

// 0x1001bb40 (140)
CAudioPlaceHolderList::CAudioPlaceHolderList(uint32_t channels)
    : mCriticalSection(), mBlocks(nullptr), mFree(nullptr), mUsed(nullptr), mBlockCount(0), mNextSlotIndex(0), mHandleGeneration(0),
      mPriorityList(kPriorityEntries), mLowestPriority(0), mNumberOfChannels(LB_MIN(channels, kMaxChannels)) {}

// 0x1001bd68 (64)
uint32_t CAudioPlaceHolderList::SetNumberOfChannels(uint32_t channels) {
    mNumberOfChannels = LB_MIN(channels, kMaxChannels);
    return mNumberOfChannels;
}

// 0x1001be30 (200): every used placeholder is released (virtual +0x14; the next link is read after the call, as the listing does),
// then the blocks are freed one by one with the runtime's delete[] (no ~CMemBlock). The priority array and the lock go with the
// members. The deleting form (flag > 0) is the host's `delete`.
CAudioPlaceHolderList::~CAudioPlaceHolderList() {
    for (CAudioPlaceHolder* holder = mUsed; holder != nullptr; holder = holder->mNext) holder->Release();
    while (mBlocks != nullptr) {
        CMemBlock* block = mBlocks;
        mBlocks = block->GetNext();
        ::operator delete[](static_cast<void*>(block));
    }
}

// 0x1001bf2c (8)
CAudioPlaceHolderList::CMemBlock* CAudioPlaceHolderList::CMemBlock::GetNext() const {
    return mNext;
}

// ---- the tick ----------------------------------------------------------------------------------------------------------------

// 0x1001bf78 (340). The next link is read before the placeholder is looked at, so removing it is safe. A placeholder flagged 0x100
// is stopped and goes no further (the flag is tested after FinishedPlaying, which can clear the playing bit); one flagged 0x40
// never joins the priority list. Every entry of the priority list (up to the first NULL) plays with the tick's clock.
void CAudioPlaceHolderList::ProcessPlaceHolders() {
    InitialisePriorityList();
    CAudioPlaceHolder* next = mUsed;
    const uint32_t now = LbTime_GetClock();
    while (next != nullptr) {
        CAudioPlaceHolder* holder = next;
        next = holder->mNext;
        if ((holder->mFlags & 0x10) != 0 || holder->FinishedPlaying(now)) {
            RemovePlaceHolder(holder);
            continue;
        }
        if ((holder->mFlags & 0x100) != 0) {
            holder->Stop();
        } else if ((holder->mFlags & 0x40) == 0) {
            if (holder->VolumeControlIsOn() && holder->ProcessPositionAndPriority()) AddToHiPriorityList(holder);
        }
    }
    for (uint32_t index = 0; static_cast<CAudioPlaceHolder**>(mPriorityList)[index] != nullptr; ++index) {
        static_cast<CAudioPlaceHolder**>(mPriorityList)[index]->PlaySample(now);
    }
}

// 0x1001c170 (120)
void CAudioPlaceHolderList::RemovePlaceHolder(CAudioPlaceHolder* holder) {
    TbSoundCriticalSection lock(static_cast<void**>(mCriticalSection));
    holder->Release();
    MoveToFreeItemList(holder);
}

// 0x1001c23c (148)
TbError CAudioPlaceHolderList::SetBoundingRectangle(void* handle, const TbRect& rect) {
    CAudioPlaceHolder* holder = FindPlaceHolder(handle);
    if (holder == nullptr) return TbError(-1);
    holder->AllocateExtraData();
    holder->mExtra->mBounds = rect;
    holder->mFlags |= CAudioPlaceHolder::kBoundingRectangle;
    return TbError(0);
}

// 0x1001c36c (76)
void* CAudioPlaceHolderList::GetPlaceholderAndSoundHandle(void* handle, CAudioPlaceHolder*& holder) {
    holder = FindPlaceHolder(handle);
    if (holder == nullptr) return nullptr;
    return holder->mHandle;
}

// 0x1001c418 (100)
void CAudioPlaceHolderList::StopAllPlaceHolders(uint32_t) {
    for (CAudioPlaceHolder* holder = mUsed; holder != nullptr; holder = holder->mNext) holder->Stop();
    ProcessPlaceHolders();
}

// 0x1001c4c0 (140): the handle when the placeholder exists and its Update did not fail
uint32_t CAudioPlaceHolderList::UpdatePlaceHolder(void* handle, const TbSoundPosition& position, const TbSoundPosition& velocity) {
    CAudioPlaceHolder* holder = FindPlaceHolder(handle);
    if (holder != nullptr) {
        const TbError error = holder->Update(position, velocity);
        if (error.Succeeded()) return Word(handle);
    }
    return 0;
}

// 0x1001c5b4 (116)
uint32_t CAudioPlaceHolderList::UpdatePlaceHolderParameter(void* handle, uint8_t parameter, uint32_t value) {
    CAudioPlaceHolder* holder = FindPlaceHolder(handle);
    if (holder == nullptr) return 0;
    holder->UpdateParameter(parameter, value);
    return Word(handle);
}

// ---- creation ----------------------------------------------------------------------------------------------------------------

// 0x1001c678 (448). An event of type 1 makes a list sentence. Otherwise the sound's flag half-word (+0x10) chooses: bit 4 clear a
// one-shot; bit 4 set and 0x10 a linear sentence; bit 4 set, 0x10 clear and 2 clear a one-shot sentence (0x400: a branching
// one); bits 4 and 2 set and 0x400 a branching sentence; bits 4 and 2 set, 0x400 clear, a sentence (0x100: the shuffled one).
void* CAudioPlaceHolderList::CreatePlaceHolder(CCatagoryContainer* container, void* sound, const sound::TbAudioEvent& event) {
    TbSoundCriticalSection lock(static_cast<void**>(mCriticalSection));
    void* handle = nullptr;
    CAudioPlaceHolder* slot = NewPlaceHolderMemory(handle);
    if (slot != nullptr) {
        if (event.GetType() == 1) {
            CListSentence::CreateObject(container, slot, sound, event);
        } else {
            const uint16_t flags = static_cast<const CEventData*>(sound)->mFlags;
            if ((flags & 4) == 0) {
                CPlaceHolderOneShot::CreateObject(container, slot, sound, event);
            } else if ((flags & 0x10) == 0) {
                if ((flags & 2) == 0) {
                    if ((flags & 0x400) == 0) {
                        CPlaceHolderOneShotSentence::CreateObject(container, slot, sound, event);
                    } else {
                        CPlaceHolderOneShotBranchingSentence::CreateObject(container, slot, sound, event);
                    }
                } else if ((flags & 0x400) == 0) {
                    if ((flags & 0x100) == 0) {
                        CPlaceHolderSentence::CreateObject(container, slot, sound, event);
                    } else {
                        CPlaceHolderSentenceShuffle::CreateObject(container, slot, sound, event);
                    }
                } else {
                    CPlaceHolderBranchingSentence::CreateObject(container, slot, sound, event);
                }
            } else {
                CPlaceHolderLinearSentence::CreateObject(container, slot, sound, event);
            }
        }
    }
    return handle;
}

// 0x1001c8d8 (152): the slot a handle names, when the slot still holds exactly that handle (the generation matches). NULL handle:
// no lock is taken.
CAudioPlaceHolder* CAudioPlaceHolderList::FindPlaceHolder(void* handle) {
    if (handle == nullptr) return nullptr;
    TbSoundCriticalSection lock(static_cast<void**>(mCriticalSection));
    CAudioPlaceHolder* holder = Handle2Ptr(handle);
    if (holder != nullptr && Word(holder->GetHandle()) == Word(handle)) return holder;
    return nullptr;
}

// AudioPlaceHolder.cpp: CAudioPlaceHolder, the base of every sound the game is playing (unit w79g2, "sound symbols shared", file
// AudioPlaceHolder.cpp of the library). See include/AudioPlaceHolder.h for the layout, the vtable and the flag bits.
//
// Conventions of this file. The sound manager is reached through the class static mpSoundManager (0x1003f360), never tested for
// NULL except where the listing tests it. Every command to the manager is built on the stack and handed to its virtual
// SystemCommand (slot +0x18); the answer is dropped unless the function says otherwise. The file words of Save and Load are the
// Mac's SamsUtilities swaps: the file is little-endian, so the host (little-endian) writes and reads the native words (the helpers of
// include/TbSoundBank.h do nothing here). The random numbers come from bfl::maths::Random and RangedRandomSlow (stand-ins), always
// given a COPY of mRandomSeed. All float arithmetic is single precision in the listing's order; the build has -ffp-contract=off.

void* CAudioPlaceHolder::GetHandle() const {
    return mListHandle;
}

// 0x1001c9ec (176): the head of the free list moves to the used list and gets its handle; with an empty free list a block is added
// first (a failed block: NULL, and the handle stays as it was).
CAudioPlaceHolder* CAudioPlaceHolderList::NewPlaceHolderMemory(void*& handle) {
    CAudioPlaceHolder* holder = mFree;
    if (holder == nullptr) {
        const TbError error = AddNewBlock();
        if (static_cast<int32_t>(error) < 0) return nullptr;
        return NewPlaceHolderMemory(handle);
    }
    MoveToUsedItemList(holder);
    handle = CreateHandle(holder);
    return holder;
}

// 0x1001cae0 (88)
void CAudioPlaceHolderList::MoveToUsedItemList(CAudioPlaceHolder* holder) {
    if (holder->mPrevious == nullptr) {
        mFree = holder->mNext;
    } else {
        holder->mPrevious->mNext = holder->mNext;
    }
    if (holder->mNext != nullptr) holder->mNext->mPrevious = holder->mPrevious;
    AddToUsedItemList(holder);
}

// 0x1001cb8c (88)
CAudioPlaceHolder* CAudioPlaceHolderList::FindFreeItem(void* handle) {
    for (CAudioPlaceHolder* holder = mFree; holder != nullptr; holder = holder->mNext) {
        if (Word(holder->GetHandle()) == Word(handle)) return holder;
    }
    return nullptr;
}

// 0x1001cc20 (96): the new generation (shifted up 16) plus the slot's current handle, in 32 bits
void* CAudioPlaceHolderList::CreateHandle(CAudioPlaceHolder* holder) {
    const uint32_t old = Word(holder->GetHandle());
    const uint32_t generation = NewHandleIndex();
    return holder->SetHandle(HandleOf(generation * 0x10000u + old));
}

void* CAudioPlaceHolder::SetHandle(void* handle) {
    mListHandle = handle;
    return handle;
}

// 0x1001cd10 (20)
uint32_t CAudioPlaceHolderList::NewHandleIndex() {
    return ++mHandleGeneration;
}

// 0x1001cd60 (272). The block goes to the end of the chain; its 100 slots take the numbers mNextSlotIndex.. in order and are pushed
// on the free list one by one, so the LAST slot of the block is the head and the first to be used.
TbError CAudioPlaceHolderList::AddNewBlock() {
    CMemBlock* block = nullptr;
    const TbError error = CMemBlock::CreateObject(&block, kSlotsPerBlock);
    if (static_cast<int32_t>(error) < 0) return TbError(-1);
    if (mBlocks == nullptr) {
        mBlocks = block;
    } else {
        CMemBlock* last = mBlocks;
        while (last->GetNext() != nullptr) last = last->GetNext();
        last->SetNext(block);
    }
    CAudioPlaceHolder* slot = static_cast<CAudioPlaceHolder*>(block->Data());
    for (uint32_t count = kSlotsPerBlock; count != 0; --count) {
        const uint32_t number = mNextSlotIndex++;
        slot->SetHandle(HandleOf(number));
        slot->mFlags = 0;
        AddToFreeItemList(slot);
        slot = reinterpret_cast<CAudioPlaceHolder*>(reinterpret_cast<char*>(slot) + kSlotSize);
    }
    ++mBlockCount;
    return TbError(0);
}

// 0x1001ceac (8)
void* CAudioPlaceHolderList::CMemBlock::Data() {
    return reinterpret_cast<char*>(this) + kBlockHeader;
}

// 0x1001cef4 (8)
void CAudioPlaceHolderList::CMemBlock::SetNext(CMemBlock* next) {
    mNext = next;
}

// 0x1001cf60 (40)
void CAudioPlaceHolderList::AddToFreeItemList(CAudioPlaceHolder* holder) {
    holder->mNext = mFree;
    holder->mPrevious = nullptr;
    if (mFree != nullptr) mFree->mPrevious = holder;
    mFree = holder;
}

// 0x1001cfdc (40)
void CAudioPlaceHolderList::AddToUsedItemList(CAudioPlaceHolder* holder) {
    holder->mNext = mUsed;
    holder->mPrevious = nullptr;
    if (mUsed != nullptr) mUsed->mPrevious = holder;
    mUsed = holder;
}

// 0x1001d058 (68): the next link is cleared before the walk, so a holder already at the end of the list ends up its own tail
void CAudioPlaceHolderList::AddToEndOfUsedItemList(CAudioPlaceHolder* holder) {
    holder->mNext = nullptr;
    CAudioPlaceHolder* tail = mUsed;
    if (tail == nullptr) {
        mUsed = holder;
        holder->mPrevious = nullptr;
        return;
    }
    while (tail->mNext != nullptr) tail = tail->mNext;
    tail->mNext = holder;
    holder->mPrevious = tail;
}

// ---- CMemBlock ---------------------------------------------------------------------------------------------------------------

// 0x1001d0f4 (144)
TbError CAudioPlaceHolderList::CMemBlock::CreateObject(CMemBlock** block, uint32_t count) {
    if (block != nullptr) {
        void* memory = ::operator new[](count * kSlotSize + kBlockHeader, std::nothrow);
        if (memory != nullptr) {
            *block = new (memory) CMemBlock(count * static_cast<uint32_t>(kSlotSize));
            return TbError(0);
        }
    }
    return TbError(-1);
}

// 0x1001d1f0 (24)
CAudioPlaceHolderList::CMemBlock::CMemBlock(uint32_t size) : mSize(size), mNext(nullptr) {}

// 0x1001d270 (84)
void CAudioPlaceHolderList::MoveToFreeItemList(CAudioPlaceHolder* holder) {
    if (holder->mPrevious == nullptr) {
        mUsed = holder->mNext;
    } else {
        holder->mPrevious->mNext = holder->mNext;
    }
    if (holder->mNext != nullptr) holder->mNext->mPrevious = holder->mPrevious;
    AddToFreeItemList(holder);
}

// 0x1001d318 (140): the low 16 bits of the handle are the slot number: the block is number / 100 and the slot number % 100
CAudioPlaceHolder* CAudioPlaceHolderList::Handle2Ptr(void* handle) {
    const uint32_t number = Word(handle) & 0xffff;
    CMemBlock* block = mBlocks;
    for (uint32_t skip = number / kSlotsPerBlock; block != nullptr && skip != 0; --skip) block = block->GetNext();
    if (block == nullptr) return nullptr;
    return reinterpret_cast<CAudioPlaceHolder*>(static_cast<char*>(block->Data()) + (number % kSlotsPerBlock) * kSlotSize);
}

// ---- the priority list -------------------------------------------------------------------------------------------------------

// 0x1001d3e0 (72)
void CAudioPlaceHolderList::InitialisePriorityList() {
    std::memset(static_cast<CAudioPlaceHolder**>(mPriorityList), 0, kPriorityEntries * sizeof(CAudioPlaceHolder*));
    mLowestPriority = 0;
}

// 0x1001d46c (356). Only a placeholder whose priority is above the lowest one in the list is considered (unsigned compares of
// CDistPriority's word, the distance in the high half). It is inserted before the first entry with a lower priority, and the entry
// it displaces is carried on down the list, so the list stays in descending order (equal priorities keep their order). A carried
// placeholder that loops (flag 2) is refused, ending the walk with the list as it is so far, when the entry it meets is not playing
// (flag 1 clear) and plays the same sample (+0xc). Afterwards mLowestPriority is the last entry's priority, or 0 when that is empty.
void CAudioPlaceHolderList::AddToHiPriorityList(CAudioPlaceHolder* holder) {
    if (mLowestPriority >= static_cast<uint32_t>(holder->mPriority)) return;
    CAudioPlaceHolder** list = mPriorityList;
    uint32_t index = 0;
    while (index < mNumberOfChannels && holder != nullptr) {
        CAudioPlaceHolder* entry = list[index];
        if (entry != nullptr && (holder->mFlags & 2) != 0 && (entry->mFlags & 1) == 0 && entry->mSample == holder->mSample) return;
        if (list[index] == nullptr || static_cast<uint32_t>(list[index]->mPriority) < static_cast<uint32_t>(holder->mPriority)) {
            CAudioPlaceHolder* displaced = list[index];
            list[index] = holder;
            holder = displaced;
        }
        ++index;
    }
    if (list[mNumberOfChannels - 1] == nullptr) {
        mLowestPriority = 0;
    } else {
        mLowestPriority = static_cast<uint32_t>(list[mNumberOfChannels - 1]->mPriority);
    }
}

// ---- the save game block -----------------------------------------------------------------------------------------------------

// 0x1001d624 (608). The size word counts from the position of its own first byte (the position before anything was written) to the
// end of the block, so a skipping Load seeks size - 4 after reading it. A finished placeholder flagged 0x400 is not saved. With an
// empty used list the size and count words stay 0 and the file position is left after the block count.
TbError CAudioPlaceHolderList::Save(bfl::FileSystem::CFile* file) {
    TbSoundCriticalSection lock(static_cast<void**>(mCriticalSection));
    uint32_t count = 0;
    const uint32_t now = LbTime_GetClock();
    const int32_t start = file->GetPos();
    WriteWord<uint32_t>(file, 0);
    WriteWord<uint32_t>(file, 0);
    TbMapStreamer::Instance()->SaveCatgoryFixupTable(file);
    WriteWord<uint32_t>(file, mBlockCount);
    if (mUsed != nullptr) {
        CAudioPlaceHolder* holder = mUsed;
        while (holder->mNext != nullptr) holder = holder->mNext;
        for (; holder != nullptr; holder = holder->mPrevious) {
            if ((holder->mFlags & 0x10) == 0 || (holder->mFlags & 0x400) == 0) {
                ++count;
                WriteWord<uint32_t>(file, Word(holder->GetHandle()));
                holder->Save(file, now);
            }
        }
        const int32_t end = file->GetPos();
        file->Seek(static_cast<uint32_t>(start), 0);
        WriteWord<int32_t>(file, end - start);
        WriteWord<uint32_t>(file, count);
        file->Seek(static_cast<uint32_t>(end), 0);
    }
    return TbError(0);
}

// 0x1001da8c (764). `flag` false: the block is skipped (the size word read, then a relative seek of size - 4). Otherwise every used
// placeholder is stopped (virtual +0x20) and removed, the fix-up table is loaded, blocks are added up to the saved count, and for each
// saved record (handle, class id) the FREE slot whose handle is the saved slot number (the low 16 bits) is built by the class's
// CreateObject, loads its own data with the clock and takes the saved handle on the used list. A record whose slot is not free
// or whose class id is outside 1..6 is skipped without reading its data (the stream is not realigned).
TbError CAudioPlaceHolderList::Load(bfl::FileSystem::CFile* file, bool flag) {
    TbSoundCriticalSection lock(static_cast<void**>(mCriticalSection));
    const uint32_t now = LbTime_GetClock();
    uint32_t savedBlocks = 0;
    CAudioPlaceHolder* holder = mUsed;
    while (holder != nullptr) {
        CAudioPlaceHolder* next = holder->mNext;
        holder->Stop();
        RemovePlaceHolder(holder);
        holder = next;
    }
    uint32_t size = 0;
    ReadWord<uint32_t>(file, size);
    if (!flag) {
        file->Seek(size - 4, 1);
        return TbError(0);
    }
    uint32_t count = 0;
    ReadWord<uint32_t>(file, count);
    TbMapStreamer::Instance()->LoadCatgoryFixupTable(file);
    ReadWord<uint32_t>(file, savedBlocks);
    const int32_t missing = LbMax(static_cast<int32_t>(savedBlocks - mBlockCount), 0);
    for (uint32_t added = 0; added < static_cast<uint32_t>(missing); ++added) AddNewBlock();
    while (true) {
        const bool more = count != 0;
        --count;
        if (!more) break;
        uint32_t savedHandle = 0;
        uint32_t classId = 0;
        ReadWord<uint32_t>(file, savedHandle);
        ReadWord<uint32_t>(file, classId);
        CAudioPlaceHolder* slot = FindFreeItem(HandleOf(savedHandle & 0xffff));
        if (slot == nullptr) continue;
        switch (static_cast<int32_t>(classId)) {
            case 1:
                CPlaceHolderBranchingSentence::CreateObject(slot);
                break;
            case 2:
                CPlaceHolderLinearSentence::CreateObject(slot);
                break;
            case 3:
                CPlaceHolderOneShot::CreateObject(slot);
                break;
            case 4:
                CPlaceHolderOneShotSentence::CreateObject(slot);
                break;
            case 5:
                CPlaceHolderSentence::CreateObject(slot);
                break;
            case 6:
                CPlaceHolderOneShotBranchingSentence::CreateObject(slot);
                break;
            default:
                continue;
        }
        slot->Load(file, now);
        slot->SetHandle(HandleOf(savedHandle));
        MoveToUsedItemList(slot);
    }
    TbMapStreamer::Instance()->UnloadCatgoryFixupTable();
    return TbError(0);
}
