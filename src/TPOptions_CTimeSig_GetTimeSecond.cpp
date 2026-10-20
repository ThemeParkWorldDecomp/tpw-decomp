// Unit v51118: small stand-ins of the main app (CTimeSig's three members are inline in include/TimeSig.h and are tested through it;
// CSkyObjects, CSaveLoad, InterfaceFontWrapper, the ride database and balance destructors, two messages, and the std containers'
// destructors, constructors and allocator functions of the master save).
#include "TPOptions.h"
#include "OptionsBalance.h"
#include "TPDialog.h"

#include <cstdint>
#include <new>

#include "CBalanceTable.h"
#include "Clock.h"
#include "FS_MasterSave.h"
#include "InterfaceFontWrapper.h"
#include "MsgCheatMode.h"
#include "RideDB.h"
#include "SaveLoad.h"
#include "SkyObjects.h"
#include "TPEngine.h"
#include "TbResource.h"
#include "ThingReceivers.h"

// The words the constructors of std::list<std::wstring> (0x1016d63c) and of the park theme tree (0x1016e37c) read from their globals.
uint32_t gWStringListCtorWord = 0;   // 0x102a723c
uint32_t gParkTreeCtorWord = 0;      // 0x102a7234

namespace v51118 {

// Runtime glue the destructors call; weak defaults here, recording fakes in the test library.
__attribute__((weak)) void OperatorDelete(void* block) { ::operator delete(block); }   // 0x10238218
__attribute__((weak)) void KeyStringDtor(void* key, int16_t flag) {                    // 0x102382f0
    static_cast<TbDynamicStringTemplate<char>*>(key)->~TbDynamicStringTemplate();
    (void)flag;
}

namespace {
uint32_t WordOf(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }
}  // namespace

// 0x1015b61c: nothing to destroy; the hidden flag decides the delete. Answers self.
CSkyObjects* SkyObjectsDtor(CSkyObjects* self, int16_t deleteFlag) {
    if (self != nullptr) {
        if (deleteFlag > 0) OperatorDelete(self);
    }
    return self;
}

// 0x101696f8: the tree is destroyed from its root when there is one.
MacU16Set* U16SetDtor(MacU16Set* self, int16_t deleteFlag) {
    if (self != nullptr) {
        if (self->mRoot != 0) SetTreeDestroy(self, self->mRoot);
        if (deleteFlag > 0) OperatorDelete(self);
    }
    return self;
}

// 0x1016985c
MacWStringList* WStringListDtor(MacWStringList* self, int16_t deleteFlag) {
    if (self != nullptr) {
        WStringListClear(self);
        if (deleteFlag > 0) OperatorDelete(self);
    }
    return self;
}

// 0x10154660
CRideDatabase* RideDatabaseDtor(CRideDatabase* self, int16_t deleteFlag) {
    if (self != nullptr) {
        self->ClearDatabase();
        if (deleteFlag > 0) OperatorDelete(self);
    }
    return self;
}

// 0x101544ec: the Mac first stores the base class's vtable (0x102a5d2c) at +4; the host's vptr is native, so that store has no host form.
CRideBalance* RideBalanceDtor(CRideBalance* self, int16_t deleteFlag) {
    if (self != nullptr) {
        self->UnInit();
        if (deleteFlag > 0) OperatorDelete(self);
    }
    return self;
}

// 0x1016d63c: the count from a global, the sentinel (at +4) linked to itself. The two words the Mac copies from its stack are overwritten.
void WStringListCtor(MacWStringList* list) {
    uint32_t sentinel = WordOf(list) + 4;
    list->mCount = gWStringListCtorWord;
    list->mSentinelPrev = sentinel;
    list->mFirst = sentinel;
}

// 0x1016e37c: the count from a global, no root, the first byte of the compare object kept in the third word, begin = end().
void ParkThemeTreeCtor(ParkThemeTree* tree, const uint8_t* compare) {
    tree->mCount = gParkTreeCtorWord;
    tree->mRoot = nullptr;
    *reinterpret_cast<uint8_t*>(&tree->mSlot8) = *compare;
    tree->mBegin = nullptr;
    tree->mBegin = tree->end();
}

}  // namespace v51118

namespace ParkThemeTreeCallee {
// 0x1016dfd0: destroys the key of the pair (the flag is -1) unless the pointer is NULL.
void PairDestroy(ParkThemeTree* tree, ParkThemeTree::Pair* value) {
    (void)tree;
    if (value != nullptr) v51118::KeyStringDtor(reinterpret_cast<void*>(value), -1);
}

// 0x1016dc80: frees the node (the count is not used).
void NodeDeallocate(ParkThemeTree* tree, ParkThemeTree::node* node, unsigned long count) {
    (void)tree;
    (void)count;
    v51118::OperatorDelete(node);
}

// 0x1016b168
void PairDtor(ParkThemeTree::Pair* pair, int16_t flag) {
    if (pair != nullptr) {
        v51118::KeyStringDtor(reinterpret_cast<void*>(pair), -1);
        if (flag > 0) v51118::OperatorDelete(pair);
    }
}
}  // namespace ParkThemeTreeCallee

// 0x1015b61c (the delete is the hidden flag's: SkyObjectsDtor)
CSkyObjects::~CSkyObjects() {}

// 0x1015b428
void CSaveLoad::PreSaveOperations() { gClock.PreSave(); }
// 0x1015b3dc, 0x1015b370, 0x1015b3a4: the debug log is gone; the Mac answers 0, 0 and 1 in r3 (the members are declared void).
void CSaveLoad::WriteSerialiseLog(int32_t, const char*, LogfileState) {}
void CSaveLoad::OpenLogfile(int32_t) {}
void CSaveLoad::CloseLogfile() {}

// 0x10168c04: 0 without a font, else the font's virtual at +0x28.
__attribute__((weak)) uint8_t fontwrapper_callee::GetVerticalSpacing(TbIFont* font) { return font->GetVerticalSpacing(); }
int32_t InterfaceFontWrapper::GetHeight() {
    if (mFont == nullptr) return 0;
    return fontwrapper_callee::GetVerticalSpacing(mFont);
}

// 0x10151ec8, 0x10151d94: the base's flags word cleared, then the payload.
CMsgCheatMode::CMsgCheatMode(bool cheating) {
    mFlags = 0;
    mCheating = cheating;
}
CMsgResearchCategoryUpdated::CMsgResearchCategoryUpdated(ResearchCat category) {
    mFlags = 0;
    mCategory = category;
}
