// LbList.h: bullfrog library, functions at 0x10014c80..0x10014df8. The library names no
// file here; LbList.cpp follows the functions' prefix.
//
// An intrusive doubly linked list with the Amiga exec layout: the header's three words
// are two overlapping sentinel nodes. The head sentinel is the list itself (mNext =
// mHead), the tail sentinel is &mTail (mNext = mTail, always null; mPrev = mTailPrev), so
// the last node's mNext is &mTail and the first node's mPrev is the list.
#pragma once
#include <cstdint>

struct TbListNode {
    TbListNode* mNext;   // 0x00
    TbListNode* mPrev;   // 0x04
};

struct TbList {
    TbListNode* mHead;       // 0x00  first node, or the tail sentinel when empty
    TbListNode* mTail;       // 0x04  always null
    TbListNode* mTailPrev;   // 0x08  last node, or the list itself when empty
};

void LbList_Init(TbList* list);                                            // 0x10014c80
// Links `node` in after `after` (a node or the list itself); `list` is not used.
void LbList_Insert(TbList* list, TbListNode* after, TbListNode* node);     // 0x10014cc4
int32_t LbList_NodeExists(const TbList* list, const TbListNode* node);     // 0x10014d28
TbListNode* LbList_GetFirst(const TbList* list);                           // 0x10014d98
TbListNode* LbList_GetLast(const TbList* list);                            // 0x10014de4
