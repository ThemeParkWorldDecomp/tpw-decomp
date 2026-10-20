// TbTList.h (bullfrog library template): a doubly linked list of values, and TbPtrList, a
// list of pointers. The main app instantiates both; the instances' out-of-line members are
// emitted in each user's range (TPSettings.cpp's: 0x101648b4.. for the file storage list,
// 0x101655d8.. for the resource list). The node walk (GetCount, GetHeadPosition, GetNext) is
// inline.
//
// Layout on the Mac (16 bytes): vtable (the destructor at slot 8), head, tail, count; a node
// (12 bytes) is the value, the next node, the previous node.
#pragma once
#include <cstdint>

typedef void* POSITION;   // a node

template<class TYPE, class ARG_TYPE> class TbTList {
public:
    struct CNode {
        CNode(TYPE value) : mData(value), mNext(nullptr), mPrev(nullptr) {}
        TYPE mData;       // 0x00
        CNode* mNext;     // 0x04
        CNode* mPrev;     // 0x08
    };

    TbTList();
    virtual ~TbTList();

    int32_t GetCount() const { return mCount; }
    POSITION GetHeadPosition() const { return mHead; }
    // The value at `position`, moving `position` on to the next node.
    TYPE& GetNext(POSITION& position) {
        CNode* node = static_cast<CNode*>(position);
        position = node->mNext;
        return node->mData;
    }

    POSITION Find(ARG_TYPE value);   // the first node holding `value`, or NULL
    void RemoveAll();
    TYPE RemoveHead();               // NULL when empty
    TYPE RemoveTail();               // NULL when empty
    TYPE RemoveAt(POSITION& position);
    void AddHead(ARG_TYPE value);
    void AddTail(ARG_TYPE value);
    // A new node in front of `position`; at the tail when `position` is NULL.
    void InsertBefore(ARG_TYPE value, POSITION& position);

    CNode* mHead;      // 0x04
    CNode* mTail;      // 0x08
    int32_t mCount;    // 0x0c
};

// A list of pointers to T. Removing or destroying it frees the nodes only, not the objects.
template<class T> class TbPtrList : public TbTList<T*, T*> {
public:
    TbPtrList() {}
    ~TbPtrList() override {}
};

template<class TYPE, class ARG_TYPE> TbTList<TYPE, ARG_TYPE>::TbTList() : mHead(nullptr), mTail(nullptr), mCount(0) {}

template<class TYPE, class ARG_TYPE> TbTList<TYPE, ARG_TYPE>::~TbTList() { RemoveAll(); }

template<class TYPE, class ARG_TYPE> POSITION TbTList<TYPE, ARG_TYPE>::Find(ARG_TYPE value) {
    for (CNode* node = mHead; node != nullptr; node = node->mNext)
        if (node->mData == value) return node;
    return nullptr;
}

template<class TYPE, class ARG_TYPE> void TbTList<TYPE, ARG_TYPE>::RemoveAll() {
    CNode* node = mHead;
    mTail = nullptr;
    mHead = nullptr;
    while (node != nullptr) {
        CNode* next = node->mNext;
        delete node;
        node = next;
    }
    mCount = 0;
}

template<class TYPE, class ARG_TYPE> TYPE TbTList<TYPE, ARG_TYPE>::RemoveHead() {
    if (mHead == nullptr) return TYPE();
    CNode* node = mHead;
    mHead = node->mNext;
    if (mHead == nullptr) mTail = nullptr;
    else mHead->mPrev = nullptr;
    --mCount;
    TYPE value = node->mData;
    delete node;
    return value;
}

template<class TYPE, class ARG_TYPE> TYPE TbTList<TYPE, ARG_TYPE>::RemoveTail() {
    if (mTail == nullptr) return TYPE();
    CNode* node = mTail;
    mTail = node->mPrev;
    if (mTail == nullptr) mHead = nullptr;
    else mTail->mNext = nullptr;
    TYPE value = node->mData;
    --mCount;
    delete node;
    return value;
}

template<class TYPE, class ARG_TYPE> TYPE TbTList<TYPE, ARG_TYPE>::RemoveAt(POSITION& position) {
    CNode* node = static_cast<CNode*>(position);
    if (node == mHead) mHead = node->mNext;
    else node->mPrev->mNext = node->mNext;
    if (node == mTail) mTail = node->mPrev;
    else node->mNext->mPrev = node->mPrev;
    --mCount;
    TYPE value = node->mData;
    delete node;
    return value;
}

template<class TYPE, class ARG_TYPE> void TbTList<TYPE, ARG_TYPE>::AddHead(ARG_TYPE value) {
    CNode* node = new CNode(value);
    node->mNext = mHead;
    if (mHead == nullptr) mTail = node;
    else mHead->mPrev = node;
    mHead = node;
    ++mCount;
}

template<class TYPE, class ARG_TYPE> void TbTList<TYPE, ARG_TYPE>::AddTail(ARG_TYPE value) {
    CNode* node = new CNode(value);
    node->mPrev = mTail;
    if (mTail == nullptr) mHead = node;
    else mTail->mNext = node;
    mTail = node;
    ++mCount;
}

template<class TYPE, class ARG_TYPE> void TbTList<TYPE, ARG_TYPE>::InsertBefore(ARG_TYPE value, POSITION& position) {
    CNode* before = static_cast<CNode*>(position);
    if (before == nullptr) {
        AddTail(value);
    } else if (before == mHead) {
        AddHead(value);
    } else {
        CNode* node = new CNode(value);
        node->mNext = before;
        node->mPrev = before->mPrev;
        before->mPrev->mNext = node;
        before->mPrev = node;
        ++mCount;
    }
}

// The string instance returns an owning string and makes two copies when removing a node.
template<class Char> class TbDynamicStringTemplate;
template<> TbDynamicStringTemplate<char>
TbTList<TbDynamicStringTemplate<char>, TbDynamicStringTemplate<char>&>::RemoveHead();
