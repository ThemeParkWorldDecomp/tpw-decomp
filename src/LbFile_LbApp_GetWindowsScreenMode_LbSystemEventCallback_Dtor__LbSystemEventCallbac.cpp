// bullfrog LbSCBack.cpp, the system event callback object (unit v5461):
//   _LbSystemEventCallback::~_LbSystemEventCallback 0x10014564, EventNotification 0x1001462c, DeleteAllCallbacks 0x100147c8.
// The object holds six lists of callbacks (one per event type, 0x10 bytes each on the Mac) and a critical section at 0x60.
#include "LbSCBack.h"
#include "WinScrn.h"
#include "platform/macdoze.h"

uint8_t kLbSEListDtorVector[8];   // 0x100700c4

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
