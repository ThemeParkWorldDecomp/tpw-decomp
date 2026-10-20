// Test library for unit v51105 (libtpw_unit_CUI2DMap_Register.dylib, tests/test_CUI2DMap_Register.py), main app:
//   src/CUI2DMap_..._CUI2DMap_UnRegister_CUI2DMap_Register.cpp  CUI2DMap::Register, GetMapGridCell, the destructor, CUI2DMapSprite::SetPosition,
//   the grid cell constructor, std::ctype<wchar_t> pieces, Argument deque pieces and the wide string accessors.
//
// The callees outside the unit are recording fakes (AddTail, SetSpritePosition, operator delete, operator new, wmemcpy, the cdeque
// iterator add, the vector destructor, the substring constructor); the globals of other files are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "TbTList.h"
#include "tb_ezsprite.h"
#include "ui_main.h"
#include "online.h"
#include "std_cdeque.h"
#include "std_ctype_wchar.h"
#include "ui_ime.h"
#include "FS_MasterSave.h"
#include "std_deque_errorthingy.h"
#include "MslWStringTree.h"
#include "std_basic_streambuf_wchar.h"

using namespace calllog;

// ---- globals of other files
uint32_t gUI2DMapVtableSlot;
CEZSprite gei_EZSprite;
uint16_t gWUpperMap[256];
uint16_t gSwearLowerMap[256];
uint64_t gMslMacAddressHigh;

namespace {
int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}
}  // namespace

CEZSprite::CEZSprite() {}
CEZSprite::~CEZSprite() {}

// ---- the fakes
bool CEZSprite::SetSpritePosition(void* instance, D3DVECTOR position) {
    ev("SetSpritePosition", {lab(this), lab(instance), CanonBits(position.x), CanonBits(position.y), CanonBits(position.z)});
    return false;
}

namespace ui2dmap_callee {
void ListAddTail(TbTList<CUI2DMapSprite*, CUI2DMapSprite*>* list, CUI2DMapSprite* sprite) {
    ev("AddTail", {lab(list), lab(sprite)});
}
}  // namespace ui2dmap_callee

void UI2DMapDelete(void* object) { ev("delete", {lab(object)}); }

namespace online_callee {
void OperatorDelete(void* p) { ev("delete", {lab(p)}); }
void* OperatorNew(uint32_t size) {
    ev("new", {static_cast<int64_t>(size)});
    return tpw::ArenaAllocZeroed(size < 32 ? 32 : size);
}
void NocaseWStringCopy(void* self, const void* other, uint32_t pos, uint32_t count) {
    ev("Copy", {lab(self), lab(other), static_cast<int64_t>(pos), static_cast<int64_t>(count)});
}
}  // namespace online_callee

char16_t* wmemcpy(char16_t* dst, const char16_t* src, size_t n) {
    ev("wmemcpy", {lab(dst), lab(src), static_cast<int64_t>(n)});
    std::memcpy(dst, src, n * 2);
    return dst;
}

namespace cdeque_callee {
MslCdequeIter* IterAdd(MslCdequeIter* it, int32_t n) {
    ev("IterAdd", {static_cast<int64_t>(it->mIndex), n});
    int32_t adv = static_cast<int32_t>(ret("adv"));
    it->mPtr += adv;
    it->mIndex = static_cast<uint32_t>(ret("idx"));
    return it;
}
}  // namespace cdeque_callee

namespace argdeque_callee {
void VectorDtor(void* vec, int16_t flag) { ev("VectorDtor", {lab(vec), flag}); }
}  // namespace argdeque_callee

extern "C" {

uint64_t tpw_u2_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"vt", &gUI2DMapVtableSlot},
        {"gei", &gei_EZSprite},
        {"upper", gWUpperMap},
        {"lower", gSwearLowerMap},
        {"block", &gArgDequeCtorBlockSize},
        {"term", &gNocaseWStringTerminator},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_u2_alloc(uint32_t size) {
    uint64_t p = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size)));
    gMslMacAddressHigh = p >> 32;
    return p;
}

// [offset of mSprite in the sprite, sizeof the sprite, sizeof the map, offset of the data pointer in a string representation]
int32_t tpw_u2_layout(int64_t* out) {
    alignas(16) static char buffer[0x100];
    CUI2DMapSprite* sprite = reinterpret_cast<CUI2DMapSprite*>(buffer);
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(reinterpret_cast<char*>(&sprite->mSprite) - buffer);
    out[n++] = static_cast<int64_t>(sizeof(CUI2DMapSprite));
    out[n++] = static_cast<int64_t>(sizeof(CUI2DMap));
    out[n++] = static_cast<int64_t>(offsetof(IME::MslWideStringRep, mData));
    return n;
}

void tpw_u2_register(uint64_t map, uint64_t sprite) {
    reinterpret_cast<CUI2DMap*>(map)->Register(reinterpret_cast<CUI2DMapSprite*>(sprite));
}
uint64_t tpw_u2_gridcell(uint64_t map, uint64_t x, uint64_t y) {
    return reinterpret_cast<uintptr_t>(reinterpret_cast<CUI2DMap*>(map)->GetMapGridCell(static_cast<int32_t>(x), static_cast<int32_t>(y)));
}
uint64_t tpw_u2_dtor(uint64_t map, uint64_t flag) {
    return reinterpret_cast<uintptr_t>(CUI2DMapDestruct(reinterpret_cast<CUI2DMap*>(map), static_cast<int16_t>(flag)));
}
void tpw_u2_setpos(uint64_t sprite, uint64_t vec) {
    reinterpret_cast<CUI2DMapSprite*>(sprite)->SetPosition(*reinterpret_cast<const _D3DVECTOR*>(vec));
}
void tpw_u2_gridctor(uint64_t p) { TUI2DMapMapGridCellCtor(reinterpret_cast<void*>(p)); }
uint64_t tpw_u2_ctype_dtor(uint64_t p, uint64_t flag) {
    return reinterpret_cast<uintptr_t>(CtypeWcharDestruct(reinterpret_cast<StdCtypeWchar*>(p), static_cast<int16_t>(flag)));
}
uint64_t tpw_u2_toupper(uint64_t p, uint64_t c) { return reinterpret_cast<StdCtypeWchar*>(p)->do_toupper(static_cast<char16_t>(c)); }
uint64_t tpw_u2_tolower(uint64_t p, uint64_t c) { return reinterpret_cast<StdCtypeWchar*>(p)->do_tolower(static_cast<char16_t>(c)); }
uint64_t tpw_u2_narrow(uint64_t p, uint64_t c, uint64_t d) {
    return static_cast<uint8_t>(reinterpret_cast<StdCtypeWchar*>(p)->do_narrow(static_cast<char16_t>(c), static_cast<char>(d)));
}
void tpw_u2_cursor_copy(uint64_t d, uint64_t s) {
    argdeque_new::CdequeIterCopy(reinterpret_cast<MslArgumentCursor*>(d), reinterpret_cast<const MslArgumentCursor*>(s));
}
void tpw_u2_destroy(uint64_t first, uint64_t last) {
    argdestroy_callee::DestroyRange(static_cast<uint32_t>(first), static_cast<uint32_t>(last));
}
void tpw_u2_makeit(uint64_t out, uint64_t self, uint64_t index) {
    argdeque_new::MakeIterator(reinterpret_cast<MslArgumentDequeIter*>(out), reinterpret_cast<const MslArgumentDeque*>(self),
                               static_cast<uint32_t>(index));
}
void tpw_u2_dequector(uint64_t p) { online_callee::DequeCtor(reinterpret_cast<void*>(p)); }
uint64_t tpw_u2_deref(uint64_t it) {
    return reinterpret_cast<uintptr_t>(argdeque_callee::IterDeref(reinterpret_cast<const MslArgumentDequeIter*>(it)));
}
uint64_t tpw_u2_nocase_copy(uint64_t self, uint64_t other) {
    return reinterpret_cast<uintptr_t>(
        nocase_callee::StringCopy(reinterpret_cast<nocase_string*>(self), reinterpret_cast<const nocase_string*>(other)));
}
void tpw_u2_init(uint64_t self, uint64_t first, uint64_t last) {
    online_callee::NocaseWStringInit(reinterpret_cast<void*>(self), reinterpret_cast<const char16_t*>(first),
                                     reinterpret_cast<const char16_t*>(last));
}
uint64_t tpw_u2_nsize(uint64_t p) { return online_lt::NocaseWStringSize(reinterpret_cast<const void*>(p)); }
uint64_t tpw_u2_nbegin(uint64_t p) { return reinterpret_cast<uintptr_t>(online_lt::NocaseWStringBegin(reinterpret_cast<const void*>(p))); }
uint64_t tpw_u2_wsize(uint64_t p) { return wstring_less_callee::Size(reinterpret_cast<const MslWideString*>(p)); }
uint64_t tpw_u2_wbegin(uint64_t p) {
    return reinterpret_cast<uintptr_t>(wstring_less_callee::Begin(reinterpret_cast<const MslWideString*>(p)));
}

}  // extern "C"
