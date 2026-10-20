// The theme map's red-black tree (std::__tree of the CFSGlobalMasterSave theme map): find, clear and insert_one.
#include "FS_MasterSave.h"

uint8_t gParkTreeInserted = 1;      // 0x102a7238
uint8_t gParkTreeNotInserted = 0;   // 0x102a7239

// 0x1016cf68: the lower bound of the key; end() when that node's key is greater than the key.
ParkThemeTree::Iterator ParkThemeTree::find(const Key& key) {
    node* last = end();
    node* cur = mRoot;
    while (cur != nullptr) {
        if (!(cur->mValue.first < key)) {
            last = cur;
            cur = cur->mLeft;
        } else {
            cur = cur->mRight;
        }
    }
    if (last != end() && !(key < last->mValue.first)) return Iterator{last};
    return Iterator{end()};
}

// 0x1016cd78
void ParkThemeTree::clear() {
    if (mRoot != nullptr) {
        destroy(mRoot);
        mCount = 0;
        mRoot = nullptr;
        mBegin = end();
    }
}

// 0x1016e0b8
ParkThemeTree::InsertResult ParkThemeTree::insert_one(const Pair& value) {
    const Key& key = value.first;
    bool left = true;
    bool leftmost = true;
    node* parent = end();
    node* last = nullptr;
    node* cur = mRoot;
    while (cur != nullptr) {
        parent = cur;
        if (key < cur->mValue.first) {
            cur = cur->mLeft;
            left = true;
        } else {
            last = cur;
            cur = cur->mRight;
            left = false;
            leftmost = false;
        }
    }
    if (last == nullptr || last->mValue.first < key) {
        node* made = insert_node_at(parent, left, leftmost, value);
        return InsertResult{made, gParkTreeInserted};
    }
    return InsertResult{last, gParkTreeNotInserted};
}
