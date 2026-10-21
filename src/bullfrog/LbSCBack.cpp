// LbSCBack.cpp: name proven by __sinit_LbSCBack_cpp in the Bullfrog library.

#include "LbSCBack.h"
#include "LbSemaphore.h"
#include "lobby.h"
#include "platform/macdoze.h"
#include "platform/samsutils.h"
#include "TbGraphicFile.h"
#include "TbRaConvert.h"
#include "WinScrn.h"
#include <cstring>
#include <new>

// bullfrog LbSCBack.cpp, the system event callback object (unit v5461):
//   _LbSystemEventCallback::~_LbSystemEventCallback 0x10014564, EventNotification 0x1001462c, DeleteAllCallbacks 0x100147c8.
// The object holds six lists of callbacks (one per event type, 0x10 bytes each on the Mac) and a critical section at 0x60.

uint8_t kLbSEListDtorVector[8];   // 0x100700c4

// Declared before its first use: the definition is further down.
template <>
_LbSEFunction* TbTList<_LbSEFunction*, _LbSEFunction*>::RemoveHead();

// The list's RemoveHead (0x10014a78); weak so that a test library can fake it.
__attribute__((weak, noinline)) _LbSEFunction* LbSEList_RemoveHead(TbPtrList<_LbSEFunction>* list) { return list->RemoveHead(); }

// operator delete; weak so that a test library can record the calls.
__attribute__((weak, noinline)) void LbSE_OperatorDelete(void* block) { ::operator delete(block); }

// 0x10014564
_LbSystemEventCallback* LbSystemEventCallback_DtorBody(_LbSystemEventCallback* self, int16_t flag) {
    if (self != nullptr) {
        self->DeleteAllCallbacks();
        LbSystemEventCallbackBody* body = reinterpret_cast<LbSystemEventCallbackBody*>(self);
        NS_MacDoze::DeleteCriticalSection(body->mCriticalSection);
        LbSE_OperatorDelete(body->mCriticalSection);
        body->mCriticalSection = nullptr;
        __destroy_arr(self, kLbSEListDtorVector, 0x10, 6);
        if (flag > 0) LbSE_OperatorDelete(self);
    }
    return self;
}

// 0x1001462c
void _LbSystemEventCallback::EventNotification(int32_t type, void* data) {
    LbSystemEventCallbackBody* body = reinterpret_cast<LbSystemEventCallbackBody*>(this);
    void** section = body->mCriticalSection;
    NS_MacDoze::EnterCriticalSection(section);
    if (type != 6) {
        TbPtrList<_LbSEFunction>::CNode* node = body->mLists[type].mHead;
        while (node != nullptr) {
            _LbSEFunction* function = node->mData;
            node = node->mNext;
            if (function->mCallback != nullptr) function->mCallback(type, data, function->mContext);
        }
    }
    if (section != nullptr) NS_MacDoze::LeaveCriticalSection(section);
}

// 0x100147c8
void _LbSystemEventCallback::DeleteAllCallbacks() {
    LbSystemEventCallbackBody* body = reinterpret_cast<LbSystemEventCallbackBody*>(this);
    void** section = body->mCriticalSection;
    NS_MacDoze::EnterCriticalSection(section);
    for (uint32_t i = 0; i < 6; ++i) {
        while (body->mLists[i].mCount != 0) {
            _LbSEFunction* function = LbSEList_RemoveHead(&body->mLists[i]);
            LbSE_OperatorDelete(function);
        }
    }
    if (section != nullptr) NS_MacDoze::LeaveCriticalSection(section);
}

// Unit v51127: small bullfrog functions (LbSCBack.cpp application globals, the _LbSEFunction list instances, CConverter and TbJPEGLoader
// destructors, SamsUtilities swap helpers, TbPoint, _LbSemaLock stand-ins). Mac addresses are in the comments.

// The list instance of this unit: the members below are explicit specialisations (the header's generic bodies would call the plain
// operator delete). Declared before any use.
typedef TbTList<_LbSEFunction*, _LbSEFunction*> LbSEList;
template <> LbSEList::TbTList();
template <> LbSEList::~TbTList();
template <> void LbSEList::RemoveAll();
template <> _LbSEFunction* LbSEList::RemoveHead();

// TbTList<_LbSEFunction*,_LbSEFunction*>::TbTList 0x100148b8 is outside the unit: a weak stand-in (the generic body) that a test replaces.
template <>
__attribute__((weak, noinline)) LbSEList::TbTList() : mHead(nullptr), mTail(nullptr), mCount(0) {}

template <>
TbTList<_LbSEFunction*, _LbSEFunction*>::~TbTList() {
    RemoveAll();
}
// TbTList<_LbSEFunction*,_LbSEFunction*>: the destructor (0x1001491c), RemoveAll (0x100149c8) and RemoveHead (0x10014a78).
template <>
void TbTList<_LbSEFunction*, _LbSEFunction*>::RemoveAll() {
    CNode* node = mHead;
    mTail = nullptr;
    mHead = nullptr;
    while (node != nullptr) {
        CNode* next = node->mNext;
        LbSE_OperatorDelete(node);
        node = next;
    }
    mCount = 0;
}

template <>
_LbSEFunction* TbTList<_LbSEFunction*, _LbSEFunction*>::RemoveHead() {
    CNode* node = mHead;
    if (node == nullptr) return nullptr;
    mHead = node->mNext;
    if (mHead == nullptr)
        mTail = nullptr;
    else
        mHead->mPrev = nullptr;
    int32_t count = mCount;
    _LbSEFunction* data = node->mData;
    mCount = count - 1;
    LbSE_OperatorDelete(node);
    return data;
}

// LbSCBack.cpp of the bullfrog library: window name, screen saver, message pump, screen mode (unit v5284).

LbSCBackGlobals gLbSCBack;              // 0x10073e0c

void LbSEFunctionList_Construct(void*);

void LbSEFunctionList_Destruct(void*);

void LbSystemEventCallback_Destruct(void*);

// 0x10014b4c
void __sinit_LbSCBack_cpp() {
    LbSCBackGlobals* object = &gLbSCBack;
    __construct_array(object, reinterpret_cast<void*>(&LbSEFunctionList_Construct),
                      reinterpret_cast<void*>(&LbSEFunctionList_Destruct), 0x10, 6);
    object = &gLbSCBack;
    object->mCriticalSection = static_cast<void**>(::operator new(4));
    NS_MacDoze::InitializeCriticalSection(object->mCriticalSection);
    __register_global_object(object, reinterpret_cast<void*>(&LbSystemEventCallback_Destruct), object->mNode);
}
