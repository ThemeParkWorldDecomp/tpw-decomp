#include <cstdint>
#include <cstring>
#include "analyser.h"
#include "Debug.h"
#include "ThingReceivers.h"
#include "online.h"
#include "ui_ime.h"

// 0x10212b2c: std::set<wchar_t>'s tree, insert_node_at. The length check throws (the fakes of __throw return, so the code goes on).
WcharSetTree::node* WcharSetTree::insert_node_at(node* parent, bool left, bool leftmost, const char16_t& value) {
    static const char kMessage[] = "tree::insert length error";
    static const char kTypes[] = "!std::exception!!std::logic_error!!std::length_error!!";
    if (mCount > 0xfffffffeu) {
        IME::MslExceptionObject object;
        object.mVtbl = IME::gMslExcVtblC;
        object.mVtbl = IME::gMslExcVtblB;
        size_t length = IME::MslStrlen(kMessage);
        object.mRc.mPtr = static_cast<char*>(IME::MslNewArray(static_cast<uint32_t>(length + 1)));
        object.mRc.mCount = nullptr;
        if (object.mRc.mPtr != nullptr) {
            object.mRc.mCount = static_cast<int32_t*>(IME::MslNew(4));
            if (object.mRc.mCount != nullptr) *object.mRc.mCount = 1;
        }
        IME::MslStrcpy(object.mRc.mPtr, kMessage);
        object.mVtbl = IME::gMslLengthErrorVtbl;
        IME::MslThrow(kTypes, &object, IME::gMslLengthErrorDtorTV);
    }
    node* made = static_cast<node*>(IME::MslNew(0xe));
    if (made != nullptr) made->mValue = value;
    made->mLeft = nullptr;
    made->mRight = nullptr;
    made->mParent = reinterpret_cast<uintptr_t>(parent) | (made->mParent & 1);
    if (left)
        parent->mLeft = made;
    else
        parent->mRight = made;
    mCount = mCount + 1;
    WcharTreeCallee::BalanceInsert(made, mRoot);
    if (leftmost) mBegin = made;
    return made;
}
