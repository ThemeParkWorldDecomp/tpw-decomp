// online.cpp (main app): COnlineControl, the avatar moves, the chat text and the contact lookups (unit v5285).
//   CreateVisitor 0x102182c0, SetAvatarDestination 0x10218140, TeleportAvatar 0x1021802c, IsThisMe 0x10217f34, ReceiveText 0x10217dbc,
//   Feedback 0x1021b784, GetDataForUser 0x1021b854, AddContact 0x1021b950.
// Fields of the object are read raw at their Mac offsets (32-bit pointer words); the library objects (strings, trees, the chat, the UI) are
// reached through online_callee, which the tests fake.
#include <cstdint>
#include <cstring>
#include <new>
#include <string>

#include "FS_MasterSave.h"
#include "UIInterface.h"
#include "World.h"
#include "analyser.h"
#include "online.h"

namespace {

using ThingTag = tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>;
using MapTag = tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>;

uint32_t Word32(const void* base, size_t offset) {
    uint32_t value;
    std::memcpy(&value, static_cast<const uint8_t*>(base) + offset, 4);
    return value;
}

template <class T>
T* ToPtr(uint32_t word) {
    // The object's pointer words (the Chat at 0, the peep at 4) are handles on the host (online_callee::HandleToPointer, as the constructor stores them).
    return static_cast<T*>(online_callee::HandleToPointer(word));
}

// The global master save, made on first use (the original does it inline at both places).
CFSGlobalMasterSave* MasterSave() {
    if (gMasterSave == nullptr) {
        void* block = online_callee::OperatorNew(0x4c);
        online_callee::MasterSaveCtor(block);
        gMasterSave = static_cast<CFSGlobalMasterSave*>(block);
    }
    return gMasterSave;
}

}  // namespace

// 0x102182c0: makes the visitor's COnlinePerson (the fourth int is not read). y == -1: the park entrance 1; else the cell x/256 + (y/256 << 7) + 1.
void COnlineControl::CreateVisitor(std::u16string& name, int32_t x, int32_t y, int32_t avatar, int32_t unused) {
    (void)unused;
    MapTag cell;
    if (y == -1) {
        CMap* map = reinterpret_cast<CMap*>(reinterpret_cast<uint8_t*>(gGameWorld) + 0x2d8);
        cell = map->GetParkEntrancePos(1);
    } else {
        uint32_t sum = static_cast<uint32_t>(x / 256) + (static_cast<uint32_t>(y / 256) << 7) + 1;
        cell.mIndex = static_cast<uint16_t>(sum);
    }
    void* person = online_callee::OperatorNew(0x3a8);
    online_callee::COnlinePersonCtor(person, cell, static_cast<uint32_t>(avatar), &name, false);
}

// 0x10218140
void COnlineControl::SetAvatarDestination(MapTag cell) {
    NoLongerAFK(true);
    ThingTag thing = ToPtr<CThing>(Word32(this, 4))->GetThingId();
    if (!SetDestination(thing, cell)) return;
    uint32_t v = static_cast<uint32_t>(cell.mIndex) - 1;
    online_callee::ChatMovePlayer(ToPtr<void>(Word32(this, 0)), static_cast<int32_t>((v << 8) & 0x7f00),
                                  static_cast<int32_t>((v << 1) & 0xffffff00u), 0, this);
}

// 0x1021802c
void COnlineControl::TeleportAvatar(MapTag cell) {
    ThingTag thing = ToPtr<CThing>(Word32(this, 4))->GetThingId();
    if (!online_callee::TeleportCell(this, thing, cell)) return;
    uint32_t v = static_cast<uint32_t>(cell.mIndex) - 1;
    int16_t x = static_cast<int16_t>(((v & 0x7f) << 8) | 0x80);
    int16_t y = static_cast<int16_t>((((v >> 7) & 0xff) << 8) | 0x80);
    online_callee::ChatMovePlayer(ToPtr<void>(Word32(this, 0)), x, y, 1, this);
    NoLongerAFK(true);
}

// 0x10217f34: the name against the player's own (field 8), both as case-insensitive copies.
bool COnlineControl::IsThisMe(const char16_t* name) {
    alignas(8) uint8_t plain[16] = {};
    alignas(8) uint8_t first[16] = {};
    alignas(8) uint8_t second[16] = {};
    online_callee::WStringCtorFrom(plain, name);
    online_callee::NocaseWStringCopy(first, plain, 0, 0xffffffffu);
    online_callee::WStringDtor(plain, -1);
    online_callee::NocaseWStringCopy(second, reinterpret_cast<uint8_t*>(this) + 8, 0, 0xffffffffu);
    bool same = online_callee::NocaseWStringEqual(first, second);
    online_callee::NocaseWStringDtor(second, 0);
    online_callee::NocaseWStringDtor(first, 0);
    return same;
}

// 0x10217dbc: filters the text when the swear filter is on (the master save is handed to Filter as the filter, as the original does), then shows it.
void COnlineControl::ReceiveText(std::u16string& text, int32_t colour) {
    if (MasterSave()->IsSwearFilterOn()) {
        reinterpret_cast<SwearFilter*>(MasterSave())->Filter(text, true);
    }
    online_callee::UIOnlineReceiveStr(&gUIInterface, text.c_str(), colour);
}

// 0x1021b784: the text in the feedback colour 7.
void COnlineControl::Feedback(std::u16string& text) {
    std::u16string copy;
    online_callee::WStringCopy(&copy, &text, 0, 0xffffffffu);
    ReceiveText(copy, 7);
    online_callee::WStringDtor(&copy, -1);
}

// 0x1021b854
uint32_t COnlineControl::GetDataForUser(const nocase_string& key) const {
    const uint8_t* base = reinterpret_cast<const uint8_t*>(this);
    uint32_t result = 0;
    const uint8_t* node = static_cast<const uint8_t*>(online_callee::BuddyTreeFind(const_cast<uint8_t*>(base) + 0xe34, &key));
    if (node != base + 0xe38) {
        result |= 1;
        if (node[0x10] != 0) result |= 2;
        if (node[0x11] != 0) result |= 4;
    }
    node = static_cast<const uint8_t*>(online_callee::NocaseSetFind(const_cast<uint8_t*>(base) + 0xe44, &key));
    if (node != base + 0xe48) result |= 8;
    return result;
}

// 0x1021b950: the flag is not read (the UI is always given 0).
void COnlineControl::AddContact(std::u16string& name, int32_t flag) {
    (void)flag;
    online_callee::UIOnlineAddContact(&gUIInterface, name.c_str(), 0);
    online_callee::WStringSetInsertOne(reinterpret_cast<uint8_t*>(this) + 0xe28, name);
}
