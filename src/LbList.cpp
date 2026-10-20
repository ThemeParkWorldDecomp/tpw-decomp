// LbList.cpp (decompiled from the bullfrog library of SimTheme Park 2.04b3, functions at
// 0x10014c80..0x10014df8; original file name unknown, see LbList.h).
#include "LbList.h"

namespace {
// The sentinels overlap the header's words (see LbList.h).
TbListNode* HeadSentinel(const TbList* list) {
    return reinterpret_cast<TbListNode*>(const_cast<TbList*>(list));
}
TbListNode* TailSentinel(const TbList* list) {
    return reinterpret_cast<TbListNode*>(const_cast<TbListNode**>(&list->mTail));
}
}  // namespace

void LbList_Init(TbList* list) {
    list->mHead = TailSentinel(list);
    list->mTail = nullptr;
    list->mTailPrev = HeadSentinel(list);
}

void LbList_Insert(TbList* list, TbListNode* after, TbListNode* node) {
    node->mNext = after->mNext;
    node->mPrev = after;
    after->mNext->mPrev = node;
    after->mNext = node;
}

// Walks every node up to the tail sentinel (the one whose mNext is null).
int32_t LbList_NodeExists(const TbList* list, const TbListNode* node) {
    for (const TbListNode* n = list->mHead; n->mNext != nullptr; n = n->mNext)
        if (n == node) return 1;
    return 0;
}

TbListNode* LbList_GetFirst(const TbList* list) {
    return list->mHead != TailSentinel(list) ? list->mHead : nullptr;
}

TbListNode* LbList_GetLast(const TbList* list) {
    return list->mTailPrev != HeadSentinel(list) ? list->mTailPrev : nullptr;
}
