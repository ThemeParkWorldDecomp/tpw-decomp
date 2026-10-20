// AudioPlaceHolderList.h: CAudioPlaceHolderList, the owner of every CAudioPlaceHolder (unit w79g1, "sound symbols shared", Code at
// 0x10000000, TOC r2 = 0x1003abf0; the library's file is CSprocketsModule.cpp). The functions are in src/AudioPlaceHolderList.cpp
// with their addresses. This header REPLACES unit w79f's placeholder of the same name and keeps every name w79f uses (the
// constructor, the virtual destructor, FindPlaceHolder, CreatePlaceHolder, UpdatePlaceHolder and UpdatePlaceHolderParameter).
//
// What the list is: a pool of placeholder slots (blocks of 100 slots, chained), a free list and a used list threaded through the
// placeholders' mNext / mPrevious (Mac +0x40 / +0x44), a generation counter for the handles it gives out, and the per-tick
// priority list of at most mNumberOfChannels placeholders (31 entries, the channel count clamped to 30).
//
// Mac layout, 0x2c bytes (the host members are native, so the host object is larger; the order is the Mac's):
//   +0 vptr (the deleting destructor at +8 is the only virtual)  +4 mCriticalSection (one word)  +8 mBlocks  +0xc mFree
//   +0x10 mUsed  +0x14 mBlockCount  +0x18 mNextSlotIndex  +0x1c mHandleGeneration  +0x20 mPriorityList  +0x24 mLowestPriority
//   +0x28 mNumberOfChannels
//
// The pool. A Mac block is 0xc header bytes (vptr, size in bytes, next) and 100 slots of 0x60 bytes. A slot is the memory of one
// placeholder: the subclasses of unit w79g3 are placement-constructed into it (the CreateObject statics). The host's slots are
// sizeof(CAudioPlaceHolder) bytes (the host base class is larger than the Mac's 0x60) and the block header is padded to the
// slot's alignment; a subclass must add no data members. A handle is `generation * 0x10000 + slot number`: the low 16 bits are
// the slot's number (counted from the first slot of the first block, so block = number / 100 and slot = number % 100), the high
// 16 bits come from NewHandleIndex. CAudioPlaceHolder's destructor clears the high half. The handle words are 32 bits wide on the
// Mac and are void* on the host: every handle the list makes is zero-extended and every compare uses the low 32 bits.
//
// The list takes the sound lock (TbSoundCriticalSection on mCriticalSection) in RemovePlaceHolder, CreatePlaceHolder,
// FindPlaceHolder, Save and Load only.
#pragma once
#include <cstdint>

#include "TbAudioSystem.h"
#include "TbError.h"
#include "TbGraphicFile.h"
#include "TbMapStreamer.h"
#include "snd_containers.h"
#include "snd_handles.h"

class CAudioPlaceHolder;

class CAudioPlaceHolderList {
public:
    // A block of slots. Mac size 0xc: +0 vptr (the only virtual is the deleting destructor 0x1001e694, +8), +4 the slots' size in
    // bytes (count * 0x60), +8 the next block; the slots follow at +0xc.
    class CMemBlock {
    public:
        // 0x1001d0f4 (144): `count` slots in one new[] block (header and slots); -1 for a NULL out pointer (nothing is allocated)
        // or a failed allocation, else the new block through *block and 0. The Mac allocator answers NULL; the host asks for
        // the nothrow form.
        static TbError CreateObject(CMemBlock** block, uint32_t count);
        explicit CMemBlock(uint32_t size);   // 0x1001d1f0 (24): the size, no next block
        virtual ~CMemBlock();                // 0x1001e694 (76): the deleting form frees the block when the flag is positive
        CMemBlock* GetNext() const;          // 0x1001bf2c (8)
        void SetNext(CMemBlock* next);       // 0x1001cef4 (8)
        void* Data();                        // 0x1001ceac (8): the first slot, 0xc bytes after the block's start on the Mac

        uint32_t mSize;      // Mac +4, in bytes
        CMemBlock* mNext;    // Mac +8
    };

    explicit CAudioPlaceHolderList(uint32_t channels);   // 0x1001bb40 (140): the channel count is clamped to 30 (0x1003cc6c)
    virtual ~CAudioPlaceHolderList();                    // 0x1001be30 (200), slot +8: releases every used placeholder, frees the blocks

    // 0x1001bd68 (64): clamped to 30 (0x1003cc70); answers the clamped count (in r3: the clamp's result stays there; unit w79h found it)
    uint32_t SetNumberOfChannels(uint32_t channels);
    uint32_t GetNumberOfChannels() const;                // 0x10028950 (8)

    // 0x1001bf78 (340): one tick. Finished placeholders (flag 0x10, or FinishedPlaying) are removed, those flagged 0x100 are
    // stopped (virtual +0x20), the others that are not flagged 0x40 and whose volume control is on and whose position and
    // priority are in range go into the priority list; every placeholder of the priority list then plays (virtual +0x1c).
    void ProcessPlaceHolders();
    void RemovePlaceHolder(CAudioPlaceHolder* holder);   // 0x1001c170 (120): Release (virtual +0x14) and back to the free list
    // 0x1001c23c (148): -1 for an unknown handle, else the rectangle goes into the extra data and flag 0x1000 is set
    TbError SetBoundingRectangle(void* handle, const TbRect& rect);
    // 0x1001c36c (76): the placeholder of the handle through `holder` (NULL for an unknown one) and its sound handle (+0x30)
    void* GetPlaceholderAndSoundHandle(void* handle, CAudioPlaceHolder*& holder);
    void StopAllPlaceHolders(uint32_t unused);           // 0x1001c418 (100): Stop (virtual +0x20) on every used one, then a tick
    // 0x1001c4c0 (140) and 0x1001c5b4 (116): the answers are the handle words the game keeps (0 for an unknown handle, and for
    // UpdatePlaceHolder an Update that failed)
    uint32_t UpdatePlaceHolder(void* handle, const TbSoundPosition& position, const TbSoundPosition& velocity);
    uint32_t UpdatePlaceHolderParameter(void* handle, uint8_t parameter, uint32_t value);
    // 0x1001c678 (448): makes a placeholder for the event (the catagory container, the sound data the mapper chose) and answers its
    // handle (0 when no slot could be made). The subclass is chosen from the event's type (1: a list sentence) and the sound's
    // flags; see the .cpp.
    void* CreatePlaceHolder(CCatagoryContainer* container, void* sound, const sound::TbAudioEvent& event);
    CAudioPlaceHolder* FindPlaceHolder(void* handle);    // 0x1001c8d8 (152): NULL for a stale or unknown handle
    CAudioPlaceHolder* NewPlaceHolderMemory(void*& handle);   // 0x1001c9ec (176): a slot moved to the used list, with its new handle
    void MoveToUsedItemList(CAudioPlaceHolder* holder);  // 0x1001cae0 (88)
    CAudioPlaceHolder* FindFreeItem(void* handle);       // 0x1001cb8c (88): the free slot whose handle is exactly `handle`
    void* CreateHandle(CAudioPlaceHolder* holder);       // 0x1001cc20 (96): generation * 0x10000 + the slot's handle; answers it
    uint32_t NewHandleIndex();                           // 0x1001cd10 (20): the generation counter, incremented first
    TbError AddNewBlock();                               // 0x1001cd60 (272): 100 slots on the free list; -1 when the block fails
    void AddToFreeItemList(CAudioPlaceHolder* holder);   // 0x1001cf60 (40): at the head
    void AddToUsedItemList(CAudioPlaceHolder* holder);   // 0x1001cfdc (40): at the head
    void AddToEndOfUsedItemList(CAudioPlaceHolder* holder);   // 0x1001d058 (68)
    void MoveToFreeItemList(CAudioPlaceHolder* holder);  // 0x1001d270 (84)
    CAudioPlaceHolder* Handle2Ptr(void* handle);         // 0x1001d318 (140): the slot a handle's low 16 bits name, NULL past the last block
    void InitialisePriorityList();                       // 0x1001d3e0 (72)
    void AddToHiPriorityList(CAudioPlaceHolder* holder); // 0x1001d46c (356)
    // 0x1001d624 (608) and 0x1001da8c (764): the save game block. `file` is a bfl CFile whose virtual Read (+0x10), Write (+0x14),
    // Seek (+0x18) and GetPos (+0x20) are used; the words are written as SamsUtilities::USwap32 does (little-endian in the file).
    // Save writes {size, count, the catagory fix-up table, the block count, then handle and placeholder from the END of the
    // used list backwards} and patches the size and count. Load with `flag` false only skips the block; otherwise it stops and
    // removes every used placeholder first, adds blocks up to the saved count and recreates each saved placeholder by class id
    // (1 branching sentence, 2 linear sentence, 3 one-shot, 4 one-shot sentence, 5 sentence, 6 one-shot branching sentence) in
    // the free slot of the saved number.
    TbError Save(bfl::FileSystem::CFile* file);
    TbError Load(bfl::FileSystem::CFile* file, bool flag);

    CAutoCriticalSection mCriticalSection;   // +4
    CMemBlock* mBlocks;                      // +8 the first block
    CAudioPlaceHolder* mFree;                // +0xc the head of the free list
    CAudioPlaceHolder* mUsed;                // +0x10 the head of the used list
    uint32_t mBlockCount;                    // +0x14
    uint32_t mNextSlotIndex;                 // +0x18 the number the next slot of a new block gets
    uint32_t mHandleGeneration;              // +0x1c
    CAutoArrayPtr<CAudioPlaceHolder*> mPriorityList;   // +0x20 31 entries, cleared (0x7c bytes) by InitialisePriorityList
    uint32_t mLowestPriority;                // +0x24 the priority of the last entry when the list is full, else 0
    uint32_t mNumberOfChannels;              // +0x28
};
