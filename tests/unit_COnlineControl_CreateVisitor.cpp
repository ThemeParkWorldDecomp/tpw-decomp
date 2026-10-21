// Test library for COnlineControl's avatar moves, chat text and contact lookups (unit v5285; libtpw_unit_COnlineControl_CreateVisitor.dylib,
// tests/test_COnlineControl_CreateVisitor.py), main app:
//   src/game/online/online.cpp
//
// The callees outside the unit are fakes that record what the real one reads and answer from the test's queues. The globals of other files
// (gGameWorld, gMasterSave, gUIInterface) are defined here, zero at start like the Mac's bss.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <string>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_MasterSave.h"
#include "UIInterface.h"
#include "World.h"
#include "analyser.h"
#include "online.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes
CWorld* gGameWorld;
CFSGlobalMasterSave* gMasterSave;
// gUIInterface is a UIInterface on the Mac; only its address is used, and the class's members would pull in code this library does not link,
// so the symbol is raw storage here (a global variable's symbol carries no type).
asm(".comm _gUIInterface,4096,4\n");

namespace {

using ThingTag = tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>;
using MapTag = tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>;

template <class T>
int64_t I(T value) {
    return static_cast<int64_t>(value);
}

// The text as (length, hash); the module computes the same hash from the Mac's string.
int64_t TextLen(const char16_t* text) {
    int64_t n = 0;
    while (text[n] != 0) n++;
    return n;
}

int64_t TextHash(const char16_t* text) {
    uint32_t h = 0;
    for (; *text != 0; ++text) h = h * 31u + static_cast<uint32_t>(*text);
    return h;
}

}  // namespace

// ---- the fakes: the same event names and arguments as the module's PowerPC stubs
ThingTag CThing::GetThingId() const {
    ev("GetThingId", {I(lab(this))});
    ThingTag tag;
    tag.mIndex = static_cast<uint16_t>(ret("GetThingId"));
    return tag;
}

MapTag CMap::GetParkEntrancePos(int32_t which) const {
    ev("GetParkEntrancePos", {I(lab(this)), I(which)});
    MapTag tag;
    tag.mIndex = static_cast<uint16_t>(ret("GetParkEntrancePos"));
    return tag;
}

bool COnlineControl::SetDestination(ThingTag thing, MapTag cell) {
    ev("SetDestination", {I(lab(this)), I(thing.mIndex), I(cell.mIndex)});
    return ret("SetDestination") != 0;
}

void COnlineControl::NoLongerAFK(bool flag) { ev("NoLongerAFK", {I(lab(this)), I(flag ? 1 : 0)}); }


bool CFSGlobalMasterSave::IsSwearFilterOn() const {
    ev("IsSwearFilterOn", {I(lab(this))});
    return ret("IsSwearFilterOn") != 0;
}

bool SwearFilter::Filter(std::u16string& text, bool replace) {
    ev("Filter", {I(lab(this)), I(lab(&text)), I(TextLen(text.c_str())), I(TextHash(text.c_str())), I(replace ? 1 : 0)});
    return ret("Filter") != 0;
}

namespace online_callee {

void* OperatorNew(uint32_t size) {
    ev("OperatorNew", {I(size)});
    return reinterpret_cast<void*>(static_cast<uintptr_t>(ret("new")));
}

// The handle table (defined in src/game/online/online.cpp for the program; this library does not compile that file).
static const void* gHandles[65536];
static uint32_t gHandleCount;

uint32_t PointerToHandle(const void* p) {
    if (p == nullptr) return 0;
    for (uint32_t i = 0; i < gHandleCount; i++)
        if (gHandles[i] == p) return i + 1;
    gHandles[gHandleCount] = p;
    return ++gHandleCount;
}

void* HandleToPointer(uint32_t handle) {
    return (handle != 0 && handle <= gHandleCount) ? const_cast<void*>(gHandles[handle - 1]) : nullptr;
}

void MasterSaveCtor(void* self) { ev("MasterSaveCtor", {I(lab(self))}); }

void ChatMovePlayer(void* chat, int32_t x, int32_t y, int32_t mode, void* context) {
    ev("MovePlayer", {I(lab(chat)), I(x), I(y), I(mode), I(lab(context))});
}

void COnlinePersonCtor(void* self, MapTag cell, uint32_t avatar, void* name, bool flag) {
    ev("PersonCtor", {I(lab(self)), I(cell.mIndex), I(avatar), I(lab(name)), I(flag ? 1 : 0)});
}

bool TeleportCell(COnlineControl* self, ThingTag thing, MapTag cell) {
    ev("Teleport", {I(lab(self)), I(thing.mIndex), I(cell.mIndex)});
    return ret("Teleport") != 0;
}

void WStringCtorFrom(void* self, const char16_t* text) {
    (void)self;
    ev("WStringCtorFrom", {I(TextLen(text)), I(TextHash(text))});
}

void WStringDtor(void* self, int16_t flag) {
    (void)self;
    ev("WStringDtor", {I(flag)});
}

// The only caller that gives it a real string is Feedback (self is a std::u16string there).
void WStringCopy(void* self, const void* other, uint32_t pos, uint32_t count) {
    const std::u16string& source = *static_cast<const std::u16string*>(other);
    ev("WStringCopy", {I(lab(other)), I(pos), I(count), I(TextLen(source.c_str())), I(TextHash(source.c_str()))});
    *static_cast<std::u16string*>(self) = source.substr(pos, count);
}

void NocaseWStringCopy(void* self, const void* other, uint32_t pos, uint32_t count) {
    (void)self;
    ev("NocaseWStringCopy", {I(lab(other)), I(pos), I(count)});
}

void NocaseWStringDtor(void* self, int16_t flag) {
    (void)self;
    ev("NocaseWStringDtor", {I(flag)});
}

bool NocaseWStringEqual(const void* a, const void* b) {
    ev("NocaseEq", {I(lab(a)), I(lab(b))});
    return ret("NocaseEq") != 0;
}

void* BuddyTreeFind(void* tree, const void* key) {
    ev("BuddyFind", {I(lab(tree)), I(lab(key))});
    return reinterpret_cast<void*>(static_cast<uintptr_t>(ret("BuddyFind")));
}

void* NocaseSetFind(void* tree, const void* key) {
    ev("NocaseFind", {I(lab(tree)), I(lab(key))});
    return reinterpret_cast<void*>(static_cast<uintptr_t>(ret("NocaseFind")));
}

void WStringSetInsertOne(void* tree, const std::u16string& key) {
    ev("SetInsert", {I(lab(tree)), I(lab(&key)), I(TextLen(key.c_str())), I(TextHash(key.c_str()))});
}

void UIOnlineReceiveStr(void* ui, const char16_t* text, int32_t colour) {
    ev("OnlineReceiveStr", {I(lab(ui)), I(TextLen(text)), I(TextHash(text)), I(colour)});
}

void UIOnlineAddContact(void* ui, const char16_t* text, int32_t flag) {
    ev("OnlineAddContact", {I(lab(ui)), I(TextLen(text)), I(TextHash(text)), I(flag)});
}

}  // namespace online_callee

extern "C" {

// The host address of a global by the module's name for it (ADDR's keys).
uint64_t tpw_cv_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"world", &gGameWorld},
        {"gms", &gMasterSave},
        {"ui", &gUIInterface},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// A zeroed block the module fills with Mac-offset words (32-bit addressable).
uint64_t tpw_cv_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint32_t tpw_cv_handle(uint64_t p) { return online_callee::PointerToHandle(reinterpret_cast<const void*>(static_cast<uintptr_t>(p))); }

// A std::u16string made from n code units (kept for the whole run).
uint64_t tpw_cv_mkstr(const uint16_t* chars, uint32_t n) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(new std::u16string(reinterpret_cast<const char16_t*>(chars), n)));
}

int32_t tpw_cv_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(COnlineControl));
    return n;
}

// ---- the functions, one wrapper each with C types only
static COnlineControl* Self(uint64_t p) { return reinterpret_cast<COnlineControl*>(static_cast<uintptr_t>(p)); }
static std::u16string& Str(uint64_t p) { return *reinterpret_cast<std::u16string*>(static_cast<uintptr_t>(p)); }
static MapTag Cell(uint32_t v) {
    MapTag tag;
    tag.mIndex = static_cast<uint16_t>(v);
    return tag;
}

uint32_t tpw_cv_set_avatar_destination(uint64_t self, uint32_t cell) { Self(self)->SetAvatarDestination(Cell(cell)); return 0; }
uint32_t tpw_cv_teleport_avatar(uint64_t self, uint32_t cell) { Self(self)->TeleportAvatar(Cell(cell)); return 0; }
uint32_t tpw_cv_is_this_me(uint64_t self, uint64_t text) {
    return Self(self)->IsThisMe(reinterpret_cast<const char16_t*>(static_cast<uintptr_t>(text))) ? 1 : 0;
}
void tpw_cv_receive_text(uint64_t self, uint64_t text, int32_t colour) { Self(self)->ReceiveText(Str(text), colour); }
void tpw_cv_feedback(uint64_t self, uint64_t text) { Self(self)->Feedback(Str(text)); }
uint32_t tpw_cv_get_data_for_user(uint64_t self, uint64_t key) {
    return Self(self)->GetDataForUser(*reinterpret_cast<const nocase_string*>(static_cast<uintptr_t>(key)));
}
void tpw_cv_add_contact(uint64_t self, uint64_t text, int32_t flag) { Self(self)->AddContact(Str(text), flag); }
void tpw_cv_create_visitor(uint64_t self, uint64_t name, int32_t x, int32_t y, int32_t avatar, int32_t unused) {
    Self(self)->CreateVisitor(Str(name), x, y, avatar, unused);
}

}  // extern "C"
