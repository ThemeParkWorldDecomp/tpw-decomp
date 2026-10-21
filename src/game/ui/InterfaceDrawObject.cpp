// InterfaceDrawObject.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of InterfaceDrawObject code, named after it.

#include "TPMain.h"
#include "UI_Base.h"
#include "ui_DrawObject.h"
#include <cstring>

// The UI's draw objects and their library (main app of SimTheme Park 2.04b3): the cursor
// base class and the cursor table (0x101cc6a0..0x101ccc8c), the draw object base class and
// DrawObjectLibrary (0x101cccc0..0x101cd42c), the engine mesh objects'
// constructors, destructors and Show (0x10187a28..0x10188c04), and the base's three empty
// members that the compiler put with them (0x1018a1b0..0x1018a234).
// PARTIAL: see ui_DrawObject.h.

DrawObjectLibrary::Node* gDrawObjectLibrary;
// ---- InterfaceDrawObject

// (The depth at 0x34 is left as the memory had it.)
InterfaceDrawObject::InterfaceDrawObject(void* data) {
    mInLibrary = 0;
    mResizable = 0;
    mData = data;
    mFrameBase = 0;
    mFrame = 0;
    mShown = 0;
    mF32 = 0;
    mF30 = 0;
    mF2E = 0;
    mF2C = 0;
    mF2A = 0;
    mF28 = 0;
    Show(0);   // (a virtual call that reaches this class's own Show: the object is no more than this yet)
}

InterfaceDrawObject::~InterfaceDrawObject() {}

void InterfaceDrawObject::Draw(int16_t, CoolRect*, int16_t) {}
void InterfaceDrawObject::Draw(int16_t, CoolRect*, int16_t, char16_t*) {}

void InterfaceDrawObject::Change(uint32_t flags, DOChange* change) {
    if (change == nullptr || flags == 0) return;
    if ((flags & 1) != 0) SetFrame(change->mFrame);
    if ((flags & 4) != 0) mF2C = change->mF08;
    if ((flags & 0x80000) != 0) SetFrameBase(change->mFrameBase);
    if ((flags & 0x40) != 0) {
        mF30 = change->mF0C;
        mF32 = change->mF0E;
    }
}

intptr_t InterfaceDrawObject::Query(uint32_t, intptr_t) { return 0; }
void InterfaceDrawObject::Show(int16_t shown) { mShown = shown; }
void InterfaceDrawObject::SetFrameBase(int16_t frame) {
    if (mFrameCount <= frame) return;
    mFrameBase = frame;
}

void InterfaceDrawObject::SetFrame(int16_t frame) {
    if (mFrameCount <= frame) return;
    mFrame = frame;
}

// ---- DrawObjectLibrary

// Adds `object` under `name` at the end of the list; 0 when there is no object, the name is
// empty, or a node BEFORE THE LAST has the name's hash (the walk to the end of the list
// compares every node it leaves, so the last node's hash is never looked at).
int32_t DrawObjectLibrary::AddDrawObject(InterfaceDrawObject* object, char* name) {
    if (object == nullptr || std::strlen(name) == 0) return 0;
    const uint32_t hash = UI_hash_cakes(name, 0x2f);
    Node* last = gDrawObjectLibrary;
    bool isNew = true;
    if (last != nullptr) {
        while (last->mNext != nullptr && isNew) {
            if (last->mHash == hash) isNew = false;
            last = last->mNext;
        }
    }
    if (!isNew) return 0;
    Node* node = new Node;
    object->mInLibrary = 1;
    node->mNext = nullptr;
    node->mObject = object;
    std::strncpy(node->mName, name, 0xc);
    node->mHash = hash;
    if (gDrawObjectLibrary == nullptr) gDrawObjectLibrary = node;
    else last->mNext = node;
    return 1;
}

// Frees the nodes, and the objects too when asked. The list's head is left pointing at the
// freed first node.
void DrawObjectLibrary::RemoveAll(int16_t destroyObjects) {
    Node* node = gDrawObjectLibrary;
    while (node != nullptr) {
        Node* next = node->mNext;
        if (destroyObjects != 0 && node->mObject != nullptr) delete node->mObject;
        delete node;
        node = next;
    }
}

// DrawObjectLibrary::FindDrawObject, main 0x101cd2d4 (name, 160 bytes), 0x101cd3b0 (hash, 124); existing bodies, unit w43.

InterfaceDrawObject* DrawObjectLibrary::FindDrawObject(char* name) {
    InterfaceDrawObject* found = nullptr;
    bool done = false;
    Node* node = gDrawObjectLibrary;
    const uint32_t hash = UI_hash_cakes(name, 0x2f);
    while (node != nullptr && !done) {
        if (node->mHash == hash) {
            done = true;
            found = node->mObject;
        }
        node = node->mNext;
    }
    return found != nullptr ? found->InstanceMe() : nullptr;
}

InterfaceDrawObject* DrawObjectLibrary::FindDrawObject(uint32_t hash) {
    InterfaceDrawObject* found = nullptr;
    bool done = false;
    Node* node = gDrawObjectLibrary;
    while (node != nullptr && !done) {
        if (hash == node->mHash) {
            done = true;
            found = node->mObject;
        }
        node = node->mNext;
    }
    return found != nullptr ? found->InstanceMe() : nullptr;
}
