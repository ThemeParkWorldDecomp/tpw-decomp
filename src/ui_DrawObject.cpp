// The UI's draw objects and their library (main app of SimTheme Park 2.04b3): the cursor
// base class and the cursor table (0x101cc6a0..0x101ccc8c), the draw object base class and
// DrawObjectLibrary (0x101cccc0..0x101cd42c), the engine mesh objects'
// constructors, destructors and Show (0x10187a28..0x10188c04), and the base's three empty
// members that the compiler put with them (0x1018a1b0..0x1018a234).
// PARTIAL: see ui_DrawObject.h.
#include "ui_DrawObject.h"

#include <cstring>

#include "TPMain.h"

DrawObjectLibrary::Node* gDrawObjectLibrary;
sHeapInfo* gUIHeap;

// ---- InterfaceCursor

InterfaceCursor::InterfaceCursor() {
    mHotX = 0;
    mHotY = 0;
    mVisible = 1;
    mF0C = 0;
}
InterfaceCursor::~InterfaceCursor() {}
uint8_t InterfaceCursor::GetVisible() { return mVisible; }
void InterfaceCursor::SetData(void*, char) {}
void InterfaceCursor::Show() { mVisible = 1; }
void InterfaceCursor::Hide() { mVisible = 0; }
void InterfaceCursor::Use(int16_t) {}
void InterfaceCursor::Draw(int16_t, int16_t) {}
void InterfaceCursor::Animate(int32_t) {}
void InterfaceCursor::SetHot(char x, char y) {
    mHotX = static_cast<int8_t>(x);
    mHotY = static_cast<int8_t>(y);
}

// ---- CursorTable

CursorTable::CursorTable() : mEntries(nullptr), mCount(0) {}
CursorTable::~CursorTable() {}

// Destroys the cursors and frees the table; the pointer to the freed table is left.
void CursorTable::DeAllocate() {
    if (mEntries != nullptr) {
        for (int32_t i = 0; i < mCount; ++i) {
            if (mEntries[i] != nullptr) delete mEntries[i];
        }
        delete[] mEntries;
    }
    mCount = 0;
}

int32_t CursorTable::Allocate(uint16_t count) {
    mEntries = new InterfaceCursor*[count];
    if (mEntries == nullptr) return 0;
    for (int32_t i = 0; i < count; ++i) mEntries[i] = nullptr;
    mCount = count;
    return 1;
}

// Only an empty place takes a cursor.
int32_t CursorTable::SetEntry(uint16_t index, InterfaceCursor* cursor) {
    if (index < mCount && cursor != nullptr && mEntries[index] == nullptr) {
        mEntries[index] = cursor;
        return 1;
    }
    return 0;
}

InterfaceCursor* CursorTable::GetEntry(uint16_t index) { return index < mCount ? mEntries[index] : nullptr; }

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
void InterfaceDrawObject::Refresh() {}
int32_t InterfaceDrawObject::GetWritableDataArea(int32_t*, int32_t*) { return 0; }
InterfaceDrawObject* InterfaceDrawObject::InstanceMe() { return this; }

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

// ---- InterfaceDrawEngineMesh

// Marks the mesh as the interface's (header flag 0x200), takes its node count as the number
// of frames, flags every node of the chain from the header's root (0xc8010, and 0x8000000
// too on a 640-wide screen) and makes the mesh's engine mesh. The matrix is not set.
InterfaceDrawEngineMesh::InterfaceDrawEngineMesh(void* mesh) : InterfaceDrawObject(mesh) {
    tag_sMesh* data = static_cast<tag_sMesh*>(mData);
    if (data != nullptr) {
        data->mHeader->mFlags |= 0x200;
        mFrameCount = static_cast<int16_t>(data->mHeader->mNodeCount);
        for (tag_sNode* node = data->mHeader->mF78; node != nullptr; node = node->mF0C) {
            node->mFlags |= 0xc8010;
            if (gScreenWidth == 0x280) node->mFlags |= 0x8000000;
        }
        CreateEngineMesh(data);
    }
    mNodeFlags = nullptr;
    mLocalised = 0;
}

// An object the library has owns its mesh's engine mesh; any other is an instance.
InterfaceDrawEngineMesh::~InterfaceDrawEngineMesh() {
    tag_sMesh* data = static_cast<tag_sMesh*>(mData);
    if (data != nullptr) {
        if (mInLibrary == 0) DestroyMeshInstance(data, gUIHeap, 0);
        else DestroyEngineMesh(data);
    }
}

void InterfaceDrawEngineMesh::Draw(int16_t, CoolRect*, int16_t) {}
void InterfaceDrawEngineMesh::Draw(int16_t, CoolRect*, int16_t, char16_t*) {}

void InterfaceDrawEngineMesh::Show(int16_t shown) {
    tag_sMesh* data = static_cast<tag_sMesh*>(mData);
    if (data == nullptr || shown == mShown) return;
    if (shown == 0) {
        if (mNodeFlags != nullptr) *mNodeFlags |= 0x8010;
        DisableMeshDraw(data);
    } else {
        if (mNodeFlags != nullptr) *mNodeFlags &= 0xffff7fef;
        EnableMeshDraw(data, &mMatrix, 0);
    }
    mShown = shown;
}

InterfaceDrawEngineResizableFrame::InterfaceDrawEngineResizableFrame(void* mesh) : InterfaceDrawEngineMesh(mesh) {}
InterfaceDrawEngineResizableFrame::~InterfaceDrawEngineResizableFrame() {}
InterfaceDrawEngineResizableSlider::InterfaceDrawEngineResizableSlider(void* mesh) : InterfaceDrawEngineMesh(mesh) {}
InterfaceDrawEngineResizableSlider::~InterfaceDrawEngineResizableSlider() {}
