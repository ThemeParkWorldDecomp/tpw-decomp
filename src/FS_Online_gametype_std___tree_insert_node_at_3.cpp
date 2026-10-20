// Unit v5917: std::__tree<unsigned short> find 0x1016d1f8, insert_one 0x1016e9c4, insert_node_at 0x1016f164; IPSystem::Startup 0x1016f3c8
// and Shutdown 0x1016f448; ccParkInfo::~ccParkInfo 0x1016ff48.
#include "Debug.h"
#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "TPMain.h"
#include "ui_ime.h"

uint8_t gTreeInsertedTrue = 1;    // 0x102a7230
uint8_t gTreeInsertedFalse = 0;   // 0x102a7231

// 0x1016d1f8: the first node whose value is not below the key, or the end node when the key is below that node's value (or none).
UshortSetTree::Iterator UshortSetTree::find(const uint16_t& key) {
    node* candidate = end();
    node* current = mRoot;
    while (current != nullptr) {
        if (current->mValue < key) {
            current = current->mRight;
        } else {
            candidate = current;
            current = current->mLeft;
        }
    }
    Iterator result;
    if (candidate != end() && key >= candidate->mValue)
        result.mNode = candidate;
    else
        result.mNode = end();
    return result;
}

// 0x1016e9c4: walks down remembering the last node whose value is not above the key (the candidate) and the last node visited with the side
// taken; inserts when there is no candidate or its value is below the key.
UshortSetTree::InsertResult UshortSetTree::insert_one(const uint16_t& key) {
    bool left = true;
    bool leftmost = true;
    node* candidate = nullptr;
    node* parent = end();
    node* current = mRoot;
    while (current != nullptr) {
        parent = current;
        if (key < current->mValue) {
            current = current->mLeft;
            left = true;
        } else {
            candidate = current;
            current = current->mRight;
            left = false;
            leftmost = false;
        }
    }
    InsertResult result;
    if (candidate == nullptr || candidate->mValue < key) {
        result.mNode = insert_node_at(parent, left, leftmost, key);
        result.mInserted = gTreeInsertedTrue;
    } else {
        result.mNode = candidate;
        result.mInserted = gTreeInsertedFalse;
    }
    return result;
}

// 0x1016f164. A count of 0xffffffff builds the length_error and throws it (the host's throw returns, like the test fakes; the original
// would not); the node is made either way.
UshortSetTree::node* UshortSetTree::insert_node_at(node* parent, bool left, bool leftmost, const uint16_t& value) {
    static const char kMessage[] = "tree::insert length error";
    static const char kTypes[] = "!std::exception!!std::logic_error!!std::length_error!!";
    if (mCount > 0xfffffffeu) {
        IME::MslExceptionObject object;
        object.mVtbl = IME::gMslExcVtblC;
        object.mVtbl = IME::gMslExcVtblB;
        size_t length = IME::MslStrlen(kMessage);
        char* text = static_cast<char*>(IME::MslNewArray(static_cast<uint32_t>(length + 1)));
        IME::MslRefCountedCharsCtor(&object.mRc, text);
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
    v5917_callee::BalanceInsert(made, mRoot);
    if (leftmost) mBegin = made;
    return made;
}

// 0x1016f3c8
bool IPSystem::Startup() {
    uint8_t data[404];
    int32_t error = v5917_callee::WSAStartup(0x101, data);
    if (error != 0) {
        ShowError();
        return true;
    }
    return false;
}

// 0x1016f448
bool IPSystem::Shutdown() {
    int32_t error = v5917_callee::WSACleanup();
    if (error != 0) {
        ShowError();
        return true;
    }
    return false;
}

// 0x1016ff48. The five blocks are 32-bit words of the library's record (offsets 4, 0xc, 8, 0x44, 0x4c), freed when set and cleared.
ccParkInfo* ccParkInfo::Destruct(ccParkInfo* self, int16_t deleteFlag) {
    if (self != nullptr) {
        static const uint32_t kSlots[5] = {0x4, 0xc, 0x8, 0x44, 0x4c};
        for (uint32_t slot : kSlots) {
            Ptr32<void>* word = reinterpret_cast<Ptr32<void>*>(self->mF00 + slot);
            if (word->mOffset != 0) v5917_callee::Free(word->get());
            word->mOffset = 0;
        }
        if (deleteFlag > 0) v5917_callee::OperatorDelete(self);
    }
    return self;
}

ccParkInfo::~ccParkInfo() { Destruct(this, 0); }
