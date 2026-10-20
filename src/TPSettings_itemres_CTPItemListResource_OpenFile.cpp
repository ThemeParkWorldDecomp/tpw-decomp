// TPSettings.cpp (main app), the item list resource's file opening and destructor:
//   CTPItemListResource::OpenFile 0x10163b90 and 0x101639f0, ~CTPItemListResource 0x101621f4.
// The destructor's body here is the part that is the class's own (the registered items and their list);
// the rest of the original's code is the inlined base destructors.
#include "TPGlobals.h"
#include "TPResource.h"

// 0x10163b90: tries the folder of each registered item in turn.
int32_t CTPItemListResource::OpenFile(bfl::FileSystem::CFile& file, const TbDynamicStringTemplate<char>& name, uint32_t mode) {
    if (mItems != nullptr) {
        TbTList<CTPItemResource*, CTPItemResource*>::CNode* node = mItems->mHead;
        while (node != nullptr) {
            TbDynamicStringTemplate<char> out;
            TbDynamicStringTemplate<char> base(gTPGlobals.mSearchPath);
            TbDynamicStringTemplate<char> qualified(name);
            CTPItemResource* item = node->mData;
            node = node->mNext;
            bfl::FileSystem::CFileStorage* storage = &item->mStorage;
            base = item->mPath;
            if (QualifyFilename(qualified, base, out) == 1) {
                if (itemlist_callee::StorageOpen(storage, file, out.mData, 0x80000001u, nullptr) >= 0) {
                    mLastFoundFileStorage = storage;
                    return 1;
                }
            }
        }
    }
    mF48 = nullptr;
    mLastFoundFileStorage = nullptr;
    return 0;
}

// 0x101639f0: `storage` and `mode` are not used.
int32_t CTPItemListResource::OpenFile(bfl::FileSystem::CFile& file, const TbDynamicStringTemplate<char>& name,
                                      bfl::FileSystem::CFileStorage& storage, uint32_t mode) {
    TbDynamicStringTemplate<char> out;
    TbDynamicStringTemplate<char> base;
    TbDynamicStringTemplate<char> qualified(name);
    base = mPath;
    if (QualifyFilename(qualified, base, out) == 1) return OpenQualifiedFile(file, out);
    return 0;
}

// 0x101621f4: the items are deleted and the list too; the base destructors do the rest.
CTPItemListResource::~CTPItemListResource() {
    if (mItems != nullptr) {
        while (mItems->mCount != 0) {
            CTPItemResource* item = mItems->RemoveHead();
            if (item != nullptr) itemlist_callee::ItemDelete(item, 1);
        }
        if (mItems != nullptr) itemlist_callee::ItemListDelete(mItems, 1);
        mItems = nullptr;
        mF48 = nullptr;
    }
}
