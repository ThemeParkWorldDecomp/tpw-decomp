// Unit v51125: small leaves of the main app (see include/std_exception_what.h).
#include "std_exception_what.h"

#include "std_basic_filebuf.h"
#include "ui_main.h"

// ---- the callees outside the unit: weak defaults (the real calls) that the unit's test library replaces with recording fakes.
#define TPW_WEAK __attribute__((weak))
TPW_WEAK void stdexc_callee::OperatorDelete(void* block) { ::operator delete(block); }
TPW_WEAK void stdexc_callee::UStringDtor(SamsUtilities::UString* string, int16_t) { string->~UString(); }
TPW_WEAK void stdexc_callee::StringDtor(void* string, int16_t flag) {
    msl_char_string::destroy(static_cast<MslCharString*>(string), flag);
}
TPW_WEAK void stdexc_callee::DestroyCityInfos(ccCityInfo* infos) { delete[] infos; }
TPW_WEAK void stdexc_callee::DestroyParkInfos(ccParkInfo* infos) { delete[] infos; }
TPW_WEAK void stdexc_callee::DestroyCityRecord(CFSOnlineCity* city) { gCityRecordDeleter(city); }
TPW_WEAK void stdexc_callee::DestroyParkRecord(CFSOnlinePark* park) { delete park; }
TPW_WEAK void stdexc_callee::DestroyCityList(TbPtrList<CFSOnlineCity>* list) { delete list; }
TPW_WEAK void stdexc_callee::DestroyParkList(TbPtrList<CFSOnlinePark>* list) { delete list; }
TPW_WEAK void stdexc_callee::UshortDestroy(UshortSetTree*, uint16_t*) {}
TPW_WEAK void stdexc_callee::UshortNodeDeallocate(UshortSetTree*, UshortSetTree::node* node, unsigned long) { ::operator delete(node); }

template<class T>
static T RemoveHeadBody(TbTList<T, T>* list) {
    if (list->mHead == nullptr) return nullptr;
    auto* node = list->mHead;
    list->mHead = node->mNext;
    if (list->mHead == nullptr) list->mTail = nullptr;
    else list->mHead->mPrev = nullptr;
    --list->mCount;
    T value = node->mData;
    delete node;
    return value;
}
template<> TPW_WEAK CFSOnlineCity* TbTList<CFSOnlineCity*, CFSOnlineCity*>::RemoveHead() { return RemoveHeadBody(this); }
template<> TPW_WEAK CFSOnlinePark* TbTList<CFSOnlinePark*, CFSOnlinePark*>::RemoveHead() { return RemoveHeadBody(this); }

// ---- std::exception::what 0x100025a8: the address 0x1023bba2 + 0xe6 is the text "exception".
const char* stdexc::ExceptionWhat(const void*) { return "exception"; }

// ---- std::logic_error::what 0x10002570: the counted message's pointer word (+4 on the Mac).
const char* stdexc::LogicErrorWhat(const IME::MslExceptionObject* self) { return self->mRc.mPtr; }

// ---- std::length_error::~length_error 0x100024ac. The vtable word takes three values in turn (0x10278b74, 0x10278bb0, 0x10278bc0); the
// message's counted pointer (+4) is released between the second and the third.
IME::MslExceptionObject* IME::MslLengthErrorDtor(MslExceptionObject* self, int16_t deleteFlag) {
    if (self != nullptr) {
        self->mVtbl = gMslLengthErrorVtbl;
        self->mVtbl = gMslExcVtblB;
        MslRefCountedCharsDtor25e0(&self->mRc, -1);
        self->mVtbl = gMslExcVtblC;
        if (deleteFlag > 0) stdexc_callee::OperatorDelete(self);
    }
    return self;
}

// ---- SamsUtilities::UResourceId::~UResourceId 0x10001ad0: the type string is destroyed (flag -1).
SamsUtilities::UResourceId* stdexc::UResourceIdDestruct(SamsUtilities::UResourceId* self, int16_t deleteFlag) {
    if (self != nullptr) {
        stdexc_callee::UStringDtor(&self->mType, -1);
        if (deleteFlag > 0) stdexc_callee::OperatorDelete(self);
    }
    return self;
}

// ---- CMapLinkBase::~CMapLinkBase 0x10007b04: the Mac stores the vtable word 0x10278c9c, then frees for a flag above 0.
CMapLinkBase* stdexc::CMapLinkBaseDestruct(CMapLinkBase* self, int16_t deleteFlag) {
    if (self != nullptr) {
        if (deleteFlag > 0) stdexc_callee::OperatorDelete(self);
    }
    return self;
}

// ---- TbTimeStamp::TbTimeStamp 0x1013dca8
TbTimeStamp* stdexc::TbTimeStampCtor(TbTimeStamp* self) {
    self->mTicks = 0;
    return self;
}

// ---- the byte swaps 0x1005e5b4, 0x1005e610, 0x1005e658. Their one caller, LoadRideHeightMap, swaps a .hmp's little-endian words to the
// Mac's order: the swaps are real only where the host's order differs from the file's (or when a test asks, TPW_SWAP_FILE_WORDS), like
// platform/sams_swaps.cpp's; a little-endian host's real swaps turned every .hmp's magic and rejected it (unit v5L44).
#if (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__) || defined(TPW_SWAP_FILE_WORDS)
uint32_t USwap32UCharPtr_1005e5b4(uint32_t word) { return __builtin_bswap32(word); }
void USwapInplace16UShort_1005e610(uint16_t& half) { half = __builtin_bswap16(half); }
void USwapInplace32ULong_1005e658(uint32_t& word) { word = __builtin_bswap32(word); }
#else
uint32_t USwap32UCharPtr_1005e5b4(uint32_t word) { return word; }
void USwapInplace16UShort_1005e610(uint16_t&) {}
void USwapInplace32ULong_1005e658(uint32_t&) {}
#endif

// ---- std::__tree<unsigned short>::destroy 0x1016e65c. The compiler inlined two levels of the recursion: a child of `subtree` is
// handled by hand (its children are freed by the recursive call, the value destroyed and the node given back through the allocator),
// then freed with operator delete; `subtree` is freed last.
namespace {
using Tree = UshortSetTree;
void DestroyGrandChild(Tree* tree, Tree::node* node) {
    if (node->mLeft != nullptr) tree->destroy(node->mLeft);
    if (node->mRight != nullptr) tree->destroy(node->mRight);
    stdexc_callee::UshortDestroy(tree, &node->mValue);
    stdexc_callee::UshortNodeDeallocate(tree, node, 1);
}
void DestroyChild(Tree* tree, Tree::node* child) {
    if (child->mLeft != nullptr) DestroyGrandChild(tree, child->mLeft);
    if (child->mRight != nullptr) DestroyGrandChild(tree, child->mRight);
    stdexc_callee::OperatorDelete(child);
}
}  // namespace

void UshortSetTree::destroy(node* subtree) {
    if (subtree->mLeft != nullptr) DestroyChild(this, subtree->mLeft);
    if (subtree->mRight != nullptr) DestroyChild(this, subtree->mRight);
    stdexc_callee::OperatorDelete(subtree);
}

// ---- the three list destructors 0x101701c8, 0x10170834, 0x10170ca8: the stored answer is released (its count drops, at zero the infos
// are destroyed and the answer freed), the typed list is emptied (each record destroyed through its virtual destructor), two words are
// cleared and the list freed; the object itself is freed for a flag above 0.
namespace {
struct CityTraits {
    static void DestroyInfos(ccCityInfo* infos) { stdexc_callee::DestroyCityInfos(infos); }
    static void DestroyRecord(CFSOnlineCity* record) { stdexc_callee::DestroyCityRecord(record); }
    static void DestroyList(TbPtrList<CFSOnlineCity>* list) { stdexc_callee::DestroyCityList(list); }
};
struct ParkTraits {
    static void DestroyInfos(ccParkInfo* infos) { stdexc_callee::DestroyParkInfos(infos); }
    static void DestroyRecord(CFSOnlinePark* record) { stdexc_callee::DestroyParkRecord(record); }
    static void DestroyList(TbPtrList<CFSOnlinePark>* list) { stdexc_callee::DestroyParkList(list); }
};
template<class Traits, class Self>
Self* DestructList(Self* self, int16_t deleteFlag) {
    if (self != nullptr) {
        auto* received = self->mReceived;
        if (received != nullptr) {
            received->mRefCount = received->mRefCount - 1;
            if (received->mRefCount == 0) {
                if (received->mInfos != nullptr) Traits::DestroyInfos(received->mInfos);
                received->mInfos = nullptr;
                stdexc_callee::OperatorDelete(received);
            }
            self->mReceived = nullptr;
        }
        while (self->mList->GetCount() != 0) {
            auto* record = self->mList->RemoveHead();
            if (record != nullptr) Traits::DestroyRecord(record);
        }
        self->mF24 = 0;
        self->mF28 = 0;
        if (self->mList != nullptr) Traits::DestroyList(self->mList);
        if (deleteFlag > 0) stdexc_callee::OperatorDelete(self);
    }
    return self;
}
}  // namespace

CFSOnlineSystemList<TbPtrList<CFSOnlineCity>, ccCityList>* stdexc::CityListDestruct(
    CFSOnlineSystemList<TbPtrList<CFSOnlineCity>, ccCityList>* self, int16_t deleteFlag) {
    return DestructList<CityTraits>(self, deleteFlag);
}
CFSOnlineSystemParkList_InWorldByParkID* stdexc::ParkListByParkIDDestruct(CFSOnlineSystemParkList_InWorldByParkID* self, int16_t deleteFlag) {
    return DestructList<ParkTraits>(self, deleteFlag);
}
CFSOnlineSystemParkList_InCityTop10* stdexc::ParkListTop10Destruct(CFSOnlineSystemParkList_InCityTop10* self, int16_t deleteFlag) {
    return DestructList<ParkTraits>(self, deleteFlag);
}

// ---- std::list<std::string>::clear 0x1017fc04: nothing for a count of 0; else each node's string is destroyed (flag -1) and the node
// freed (its next link read before), then the list is empty.
void IME::MslStrList_clear(MslStrList* self) {
    if (self->mSize != 0) {
        MslStrNode* node = self->mHead.mNext;
        MslStrNode* end = reinterpret_cast<MslStrNode*>(&self->mHead);
        while (node != end) {
            stdexc_callee::StringDtor(&node->mStr, -1);
            MslStrNode* next = node->mNext;
            stdexc_callee::OperatorDelete(node);
            node = next;
        }
        self->mSize = 0;
        self->mHead.mPrev = end;
        self->mHead.mNext = end;
    }
}

// ---- FeatureStatusPanel::DeInitViewWindow 0x1019e314
void FeatureStatusPanel::DeInitViewWindow() { UIMain_DestroyRidePreview(); }
