// Unit v51105: CUI2DMap::Register, GetMapGridCell, the destructor, CUI2DMapSprite::SetPosition, the empty grid cell constructor, and
// small pieces of the Metrowerks library (ctype<wchar_t>, the Argument deque, the nocase wide string).
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "CellBase.h"
#include "Debug.h"
#include "DrawRenderClasses.h"
#include "DrawState.h"
#include "Light.h"
#include "TPCoaster.h"
#include "TPMain.h"
#include "TbGraphicFile.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"
#include "platform/d3d_types.h"
#include "ppc_math.h"
#include "ui_main.h"
#include "Main.h"
#include "TbRaConvert.h"
#include "TbTList.h"
#include "tb_ezsprite.h"
#include "tb_mapzoom.h"
#include "tb_resource_texture.h"
#include "online.h"
#include "std_cdeque.h"
#include "std_ctype_wchar.h"
#include "ui_ime.h"
#include "FS_MasterSave.h"
#include "std_basic_streambuf_wchar.h"
#include "MslWStringTree.h"
#include "std_deque_errorthingy.h"

namespace {
template <class T>
T& At(void* base, size_t offset) {
    return *reinterpret_cast<T*>(static_cast<uint8_t*>(base) + offset);
}
// A Mac pointer word as a host pointer: the host's heap lies above 4 GB, gMslMacAddressHigh is the upper half (0 on the Mac).
void* FromWord(uint32_t word) { return reinterpret_cast<void*>(static_cast<uintptr_t>((gMslMacAddressHigh << 32) | word)); }
uint32_t ToWord(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }
}  // namespace

// 0x102b1f90 (initial word 0x2a): elements per block of a new deque (this constructor's slot).
uint32_t gArgDequeCtorBlockSize = 0x2a;
// 0x102b208c (initial word 0): the character the nocase string's data ends with.
uint16_t gNocaseWStringTerminator = 0;

// ---- CUI2DMap

namespace ui2dmap_callee {
// The list's AddTail (0x102318f0); weak so the test library can fake it.
__attribute__((weak)) void ListAddTail(TbTList<CUI2DMapSprite*, CUI2DMapSprite*>* list, CUI2DMapSprite* sprite) {
    list->AddTail(sprite);
}
}  // namespace ui2dmap_callee

// 0x10230c70 (44 bytes)
void CUI2DMap::Register(CUI2DMapSprite* sprite) {
    ui2dmap_callee::ListAddTail(At<TbTList<CUI2DMapSprite*, CUI2DMapSprite*>*>(this, 0x20080), sprite);
}

// 0x1022ddd0 (64 bytes)
void* CUI2DMap::GetMapGridCell(int32_t x, int32_t y) {
    if (x < 0 || x >= 0x80 || y < 0 || y >= 0x80) return nullptr;
    return reinterpret_cast<uint8_t*>(this) + 0x60 + ((y << 7) + x) * 8;
}

// 0x1022bf04 (76 bytes)
CUI2DMap* CUI2DMapDestruct(CUI2DMap* self, int16_t deleteFlag) {
    if (self != nullptr) {
        At<uint32_t>(self, 0) = ToWord(&gUI2DMapVtableSlot);
        if (deleteFlag > 0) UI2DMapDelete(self);
    }
    return self;
}

// 0x10231014 (64 bytes)
void CUI2DMapSprite::SetPosition(const _D3DVECTOR& position) {
    D3DVECTOR copy = position;
    gei_EZSprite.SetSpritePosition(mSprite, copy);
}

// 0x1022bed0 (4 bytes): blr
void TUI2DMapMapGridCellCtor(void* self) { (void)self; }

// ---- std::ctype<wchar_t>

// 0x10229860 (88 bytes): two vtable stores (the first is overwritten), then the deleting form.
StdCtypeWchar* CtypeWcharDestruct(StdCtypeWchar* self, int16_t deleteFlag) {
    if (self != nullptr) {
        At<uint32_t>(self, 0) = 0x102b20bc;
        At<uint32_t>(self, 0) = 0x1029d2a0;
        if (deleteFlag > 0) online_callee::OperatorDelete(self);
    }
    return self;
}

// 0x10229808 (36 bytes)
char16_t StdCtypeWchar::do_toupper(char16_t c) const {
    if (c < 0x100) return gWUpperMap[c];
    return c;
}

// 0x102297b0 (36 bytes)
char16_t StdCtypeWchar::do_tolower(char16_t c) const {
    if (c < 0x100) return gSwearLowerMap[c];
    return c;
}

// 0x10229760 (28 bytes): a character below 0xff narrows (signed byte), the rest answers the default.
char StdCtypeWchar::do_narrow(char16_t c, char dflt) const {
    if (c < 0xff) return static_cast<char>(c);
    return dflt;
}

// ---- the Argument deque

namespace argdeque_new {
// 0x102295ec (28 bytes): the iterator copy constructor, three words.
void CdequeIterCopy(MslArgumentCursor* dest, const MslArgumentCursor* src) {
    dest->mCdeque = src->mCdeque;
    dest->mIndex = src->mIndex;
    dest->mSlot = src->mSlot;
}

// 0x10228e2c (240 bytes): the same body as make_iterator 0x10228c84.
void MakeIterator(MslArgumentDequeIter* out, const MslArgumentDeque* self, uint32_t index) {
    MslCdequeIter cursor;
    cursor.mDeque = reinterpret_cast<std::__cdeque<const void*, std::allocator<const void*>>*>(
        const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(self)) + 4);
    cursor.mIndex = 0;
    cursor.mPtr = static_cast<uint32_t*>(FromWord(self->mTable + self->mStart * 4));
    uint32_t blockSize = self->mBlockSize;
    uint32_t block;
    uint32_t element;
    if (index < blockSize) {
        block = (self->mCount != 0) ? *cursor.mPtr : 0;
        element = block + index * 12;
    } else {
        cdeque_callee::IterAdd(&cursor, static_cast<int32_t>(index / blockSize));
        block = *cursor.mPtr;
        element = block + (index % blockSize) * 12;
    }
    out->mCursor.mCdeque = ToWord(cursor.mDeque);
    out->mCursor.mIndex = cursor.mIndex;
    out->mCursor.mSlot = ToWord(cursor.mPtr);
    out->mElement = element;
    out->mBlockEnd = block + blockSize * 12;
    out->mIndex = index;
}
}  // namespace argdeque_new

namespace argdestroy_callee {
// 0x1022935c (80 bytes): each vector from first up to last (0xc bytes a vector) is destroyed with the flag -1.
void DestroyRange(uint32_t first, uint32_t last) {
    for (uint32_t p = first; p < last; p += 0xc) argdeque_callee::VectorDtor(FromWord(p), -1);
}
}  // namespace argdestroy_callee

namespace online_callee {
// 0x10225938 (44 bytes)
void DequeCtor(void* self) {
    uint32_t* w = static_cast<uint32_t*>(self);
    w[0] = gArgDequeCtorBlockSize;
    w[1] = 0;
    w[2] = 0;
    w[3] = 0;
    w[4] = 0;
    w[5] = 0;
    w[6] = 0;
}
}  // namespace online_callee

namespace argdeque_callee {
// 0x10225874 (8 bytes): the element word of the iterator.
void* IterDeref(const MslArgumentDequeIter* it) { return FromWord(it->mElement); }
}  // namespace argdeque_callee

// ---- the wide strings (one pointer to a representation: length, capacity, refs, data)

namespace nocase_callee {
// 0x10228770 (56 bytes): the copy constructor is the substring constructor over the whole string.
nocase_string* StringCopy(nocase_string* self, const nocase_string* other) {
    online_callee::NocaseWStringCopy(self, other, 0, 0xffffffffu);
    return self;
}
}  // namespace nocase_callee

namespace online_callee {
// 0x1022705c (264 bytes): allocates the representation and the data, copies the characters, terminates. (The original's catch
// block frees the representation and rethrows; the runtime's operator new never fails, so it is not reachable.)
void NocaseWStringInit(void* self, const char16_t* first, const char16_t* last) {
    IME::MslWideStringRep** slot = static_cast<IME::MslWideStringRep**>(self);
    *slot = static_cast<IME::MslWideStringRep*>(OperatorNew(0x10));
    int32_t bytes = static_cast<int32_t>(ToWord(last) - ToWord(first));
    int32_t n = bytes / 2;
    IME::MslWideStringRep* rep = *slot;
    if (rep != nullptr) {
        rep->mCapacity = static_cast<uint32_t>(n + 3 - (n & 3));
        rep->mRefs = 1;
        rep->mData = static_cast<char16_t*>(OperatorNew((rep->mCapacity + 1) * 2));
    }
    wmemcpy((*slot)->mData, first, static_cast<size_t>(n));
    (*slot)->mLength = static_cast<uint32_t>(n);
    (*slot)->mData[n] = gNocaseWStringTerminator;
}
}  // namespace online_callee

namespace online_lt {
// 0x10226134 (12 bytes)
uint32_t NocaseWStringSize(const void* self) { return (*static_cast<IME::MslWideStringRep* const*>(self))->mLength; }
// 0x102260c8 (12 bytes)
const char16_t* NocaseWStringBegin(const void* self) { return (*static_cast<IME::MslWideStringRep* const*>(self))->mData; }
}  // namespace online_lt

namespace wstring_less_callee {
// 0x1022605c (12 bytes)
uint32_t Size(const MslWideString* s) { return (*reinterpret_cast<IME::MslWideStringRep* const*>(s))->mLength; }
// 0x10225ff0 (12 bytes)
const char16_t* Begin(const MslWideString* s) { return (*reinterpret_cast<IME::MslWideStringRep* const*>(s))->mData; }
}  // namespace wstring_less_callee
