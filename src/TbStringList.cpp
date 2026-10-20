#include "TbDynamicStringTemplate.h"
#include "TbTList.h"

template<> TbDynamicStringTemplate<char>
TbTList<TbDynamicStringTemplate<char>, TbDynamicStringTemplate<char>&>::RemoveHead() {
    if (mHead == nullptr) return TbDynamicStringTemplate<char>();
    CNode* node = mHead;
    mHead = node->mNext;
    if (mHead == nullptr) mTail = nullptr;
    else mHead->mPrev = nullptr;
    TbDynamicStringTemplate<char> temporary(node->mData);
    --mCount;
    delete node;
    TbDynamicStringTemplate<char> result(temporary);
    return result;
}

// w225: char-string list destructor 0x1000a074, 100 B; RemoveAll 0x1000a138, 132 B.
// The generic bodies in TbTList.h preserve tail/head clearing and next-before-delete order.
template class TbTList<TbDynamicStringTemplate<char>, TbDynamicStringTemplate<char>&>;
