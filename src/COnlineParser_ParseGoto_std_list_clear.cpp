// std::list<CToken>::clear 0x102109fc (unit v5132).
#include "online.h"

void CTokenListClear(CTokenList* list) {
    if (list->mSize == 0) return;
    CTokenListNode* sentinel = reinterpret_cast<CTokenListNode*>(&list->mPrev);
    CTokenListNode* node = list->mNext.get();
    while (node != sentinel) {
        Ptr32<CTokenHandle>* slot = &node->mData;
        CTokenHandle* h = slot->get();
        if (h->mCount == 0) h->mCount = 1;
        h->mCount = h->mCount - 1;
        if (h->mCount != 0) h = nullptr;
        slot->set(h);
        if (slot->get() != nullptr) {
            slc_callee::OperatorDelete(slot->get()->mBlock.get());
            slc_callee::OperatorDelete(slot->get());
        }
        CTokenListNode* next = node->mNext.get();
        slc_callee::OperatorDelete(node);
        node = next;
    }
    list->mSize = 0;
    list->mNext.set(sentinel);
    list->mPrev.set(sentinel);
}
