// Unit v5907: online.cpp's static initialiser (0x1022a06c), CheckShifts (0x1022a3b4) and the deque assign
// std::__cdeque<const void*>::do_assign<__cdeque<vector<Argument>*>::iterator> (0x10229ae4).
#include <cstdint>
#include <cstdlib>
#include <new>
#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "InterfaceWindow.h"
#include "TbDynamicStringTemplate.h"
#include "email.h"
#include "lobby.h"
#include "online.h"
#include "platform/samsutils.h"
#include "std_basic_filebuf.h"
#include "std_cdeque.h"
#include "std_deque_errorthingy.h"
#include "ui_ime.h"

extern "C" void __register_global_object(void* object, void* destructor, void* node);   // runtime glue 0x10239940
void UIMain_DestroyString(void* object);    // 0x103bcd38, the narrow string destructor glue
void UIMain_DestroyWString(void* object);   // 0x103bcbb8, the wide string destructor glue
namespace NS_MacDoze { int16_t GetKeyState(int32_t key); }   // glue 0x10239280

alignas(16) uint8_t gOnlineSinitNarrow[18][64];
alignas(16) uint8_t gOnlineSinitWide[2][64];
uint8_t gOnlineSinitNodes[20][16];
int32_t gOnlineSinitZero;
int32_t gOnlineSinitMask;

static_assert(sizeof(TbDynamicStringTemplate<char>) <= 64, "slot too small");
static_assert(sizeof(TbDynamicStringTemplate<char16_t>) <= 64, "slot too small");

// 0x1022a06c (720): the language names, the application name and the registry key, then the two wide strings; each is made and its
// destructor registered for the exit.
void __sinit_online_cpp() {
    static const char* const kNames[18] = {"English", "French", "German",  "Spanish",           "Swedish",           "Italian",
                                           "Dutch",   "Polish", "Danish",  "Japanese",          "TraditionalChinese", "SimplifiedChinese",
                                           "Thai",    "Korean", "Hebrew",  "American",          "Theme Park World",  "Language"};
    void* destroy = reinterpret_cast<void*>(&UIMain_DestroyString);
    void* destroyWide = reinterpret_cast<void*>(&UIMain_DestroyWString);
    gOnlineSinitZero = 0;
    gOnlineSinitMask = 0x10000;
    for (int i = 0; i < 18; i++) {
        auto* object = new (gOnlineSinitNarrow[i]) TbDynamicStringTemplate<char>(kNames[i]);
        __register_global_object(object, destroy, gOnlineSinitNodes[i]);
    }
    auto* wideA = new (gOnlineSinitWide[0]) TbDynamicStringTemplate<char16_t>(u"upload");
    __register_global_object(wideA, destroyWide, gOnlineSinitNodes[18]);
    auto* wideB = new (gOnlineSinitWide[1]) TbDynamicStringTemplate<char16_t>(u"gms.dat");
    __register_global_object(wideB, destroyWide, gOnlineSinitNodes[19]);
}

// 0x1022a3b4 (116): the Shift (0x10), Control (0x11) and Alt (0x12) keys held, as two bits each.
int16_t CheckShifts() {
    uint32_t shifts = 0;
    if (NS_MacDoze::GetKeyState(0x10) < 0) shifts = 3;
    if (NS_MacDoze::GetKeyState(0x11) < 0) shifts |= 0xc;
    if (NS_MacDoze::GetKeyState(0x12) < 0) shifts |= 0x30;
    return static_cast<int16_t>(shifts);
}

// 0x10229ae4 (836): assigns the elements first..last (an iterator of a deque of vector pointers, the pointers read as words) to this
// deque: word0 capacity, word4 size, word8 start, wordC the block. A count above 0x3fffffff throws length_error (the fakes return,
// and the original goes on).
void CdequeDoAssignArgumentIter(std::__cdeque<const void*, std::allocator<const void*>>* self, MslCdequeIter first, MslCdequeIter last) {
    using namespace IME;
    // The words hold 32-bit addresses; the host's heap lies above 4 GB (gMslMacAddressHigh, 0 on the Mac).
    auto block = [](uint32_t word) { return reinterpret_cast<uint32_t*>((gMslMacAddressHigh << 32) | word); };
    uint32_t count = last.mIndex - first.mIndex;
    if (count > 0x3fffffffu) {
        MslExceptionObject object;
        object.mVtbl = gMslExcVtblC;
        object.mVtbl = gMslExcVtblB;
        size_t length = MslStrlen("cdeque::assign length error");
        object.mRc.mPtr = static_cast<char*>(MslNewArray(static_cast<uint32_t>(length + 1)));
        object.mRc.mCount = nullptr;
        if (object.mRc.mPtr != nullptr) {
            int32_t* counter = static_cast<int32_t*>(MslNew(4));
            if (counter != nullptr) *counter = 1;
            object.mRc.mCount = counter;
        }
        MslStrcpy(object.mRc.mPtr, "cdeque::assign length error");
        object.mVtbl = gMslLengthErrorVtbl;
        MslThrow("!std::exception!!std::logic_error!!std::length_error!!", &object, gMslLengthErrorDtorTV);
    }
    uint32_t index = first.mIndex;
    uint32_t* slot = first.mPtr;
    auto* ring = first.mDeque;
    if (count <= self->mWord0) {
        uint32_t* destination = block(self->mWordC);
        while (index != last.mIndex) {
            *destination = *slot;
            index++;
            slot++;
            if (slot == block(ring->mWordC) + ring->mWord0) slot = block(ring->mWordC);
            destination++;
        }
        self->mWord8 = 0;
        self->mWord4 = count;
    } else {
        uint32_t oldBlock = self->mWordC;
        self->mWordC = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(MslNew(count * 4)));
        uint32_t* destination = block(self->mWordC);
        while (index != last.mIndex) {
            *destination = *slot;
            index++;
            slot++;
            if (slot == block(ring->mWordC) + ring->mWord0) slot = block(ring->mWordC);
            destination++;
        }
        if (oldBlock != 0) MslDelete(block(oldBlock));
        self->mWord0 = count;
        self->mWord4 = count;
        self->mWord8 = 0;
    }
}
