// 0x1021e664: std::__tree<wstring, less<wstring>, allocator<wstring>>::erase_one<wstring>(const wstring&)
#include "MslWStringTree.h"

static const MslWideString* ValueOf(WStringTree::node_base* node) {
    return reinterpret_cast<const MslWideString*>(reinterpret_cast<const uint8_t*>(node) + sizeof(WStringTree::node_base));
}

bool WStringTree::erase_one(const MslWideString& key) {
    node_base* found = end();
    node_base* current = mRoot;
    while (current != nullptr) {
        if (!WideStringLess(mLess, ValueOf(current), &key)) {
            found = current;
            current = current->mLeft;
        } else {
            current = current->mRight;
        }
    }
    if (found == end() || WideStringLess(mLess, &key, ValueOf(found))) found = end();
    if (found != end()) erase(found);
    return found != end();
}
