// Test library for COnlineControl::ProcessResponse (unit v5638; libtpw_unit_COnlineControl_ProcessResponse.dylib,
// tests/test_COnlineControl_ProcessResponse.py), main app:
//   src/COnlineControl_COnlineControl_COnlineControl_Locate_COnlineControl_WantToShutDown_COnlineControl_ProcessResponse.cpp
//
// Every callee outside the unit is a fake that records what the real one reads (strings by their text, pointers as labels) and answers
// from the test's queues. The Arguments' strings are indices into the case's text tables (tpw_pr_set_text). The pointer words of the
// object (+4) are handles into a small table (tpw_pr_set_ptr), which also holds the nodes the find fakes answer. The globals of other
// files the unit reads (gText, gUIInterface, gOnlineSystem) are defined here.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

#include "calllog.h"
#include "platform/arena.h"
#include "online.h"
#include "UI_pump.h"
#include "analyser.h"
#include "ThingReceivers.h"
#include "platform/samsutils.h"
#include "email.h"
#include "FS_Online.h"
#include "Text.h"
#include "TbITable.h"
#include "TbVTag.h"
#include "UIInterface.h"
#include "Debug.h"
#include "std_basic_filebuf.h"
#include "InterfaceWindow.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes
// gText as zeroed storage under its symbol (its constructor needs the font code; the unit reads only the table pointer at Mac +0x598).
alignas(CText) unsigned char gTextStorage[sizeof(CText)] __asm__("_gText");
UIInterface gUIInterface;
CFSOnlineSystem* gOnlineSystem;

namespace {

std::u16string gWide[16];
std::string gChars[16];
void* gPtr[16];   // 1 person, 2 another person, 3 buddy node, 4 ignore node, 5 contact node, 6 park info

const std::u16string& Str(const void* s) { return *static_cast<const std::u16string*>(s); }
// The log's texts are C strings: a ring of copies keeps each alive until the event is recorded.
const char* Keep(std::string s) {
    static std::string ring[8];
    static int next = 0;
    next = (next + 1) & 7;
    ring[next] = std::move(s);
    return ring[next].c_str();
}
const char* NN(const char16_t* s) { return Keep(std::string(narrow(s))); }
const char* Text(const void* s) { return NN(Str(s).c_str()); }

// The tag count of each text id the function formats.
int32_t TagCount(uint32_t id) {
    switch (id) {
    case 0x58: case 0x5d: case 0x59: case 0x5b: case 0x5a: case 0x5c: return 2;
    case 0x98: return 3;
    case 0x68: case 0x60: case 0x61: case 0x79: case 0x7a: case 0x5f: return 0;
    default: return 1;
    }
}

// A find fake's answer: queue value 1 is the node, anything else the end node (the tree + 4).
void* FindAnswer(const char* key, void* tree, int slot) {
    if (ret(key) == 1) return gPtr[slot];
    return static_cast<uint8_t*>(tree) + 4;
}

TbUniStringVTag gTagModel(0, nullptr);   // its vptr is the label of the vtable

}  // namespace

// ---- the fakes
namespace online_callee {
void* HandleToPointer(uint32_t handle) { return handle < 16 ? gPtr[handle] : nullptr; }
void* OperatorNew(uint32_t size) {
    size_t n = size;
    if (n < sizeof(CFSOnlineSystem)) n = sizeof(CFSOnlineSystem);
    void* p = tpw::ArenaAllocZeroed(static_cast<uint32_t>(n));
    ev("new", {static_cast<int64_t>(size), newlab(p)});
    return p;
}
void WStringCtorFrom(void* self, const char16_t* text) {
    *static_cast<std::u16string*>(self) = text;
    ev("WCtor", {newlab(self), lab(text)}, {NN(text)});
}
void WStringDtor(void* self, int16_t flag) { ev("WDtor", {lab(self), flag}, {Text(self)}); }
void StringDtor(void* self, int16_t flag) { ev("SDtor", {lab(self), flag}); }
void WStringCopy(void* self, const void* other, uint32_t pos, uint32_t count) {
    *static_cast<std::u16string*>(self) = Str(other);
    ev("WCopy", {newlab(self), lab(other), pos, count}, {Text(other)});
}
void NocaseWStringCopy(void* self, const void* other, uint32_t pos, uint32_t count) {
    *static_cast<std::u16string*>(self) = Str(other);
    ev("NCopy", {newlab(self), lab(other), pos, count}, {Text(other)});
}
void NocaseWStringDtor(void* self, int16_t flag) { ev("NDtor", {lab(self), flag}, {Text(self)}); }
void* BuddyTreeFind(void* tree, const void* key) {
    ev("BuddyFind", {lab(tree), lab(key)}, {Text(key)});
    return FindAnswer("BuddyFind", tree, 3);
}
void* NocaseSetFind(void* tree, const void* key) {
    ev("NSetFind", {lab(tree), lab(key)}, {Text(key)});
    return FindAnswer("NSetFind", tree, 4);
}
}  // namespace online_callee

namespace online_parse_callee {
void WStringAssignRange(void* self, const void* src, uint32_t pos, uint32_t count) {
    ev("WAssign", {lab(self), lab(src), pos, count}, {Text(src)});
}
}  // namespace online_parse_callee

namespace response_callee {
void ArgumentBStrVal(std::u16string* result, const Argument* argument) {
    const uint32_t index = argument->mWString;
    *result = index < 16 ? gWide[index] : std::u16string();
    ev("BStrVal", {newlab(result), lab(argument)}, {NN(result->c_str())});
}
bool WStringEqual(const std::u16string& a, const void* b) {
    ev("WEqual", {lab(&a), lab(b)}, {NN(a.c_str())});
    return (ret("WEqual") & 0xff) != 0;
}
void* WStringSetFind(void* tree, const void* key) {
    ev("WSetFind", {lab(tree), lab(key)}, {Text(key)});
    return FindAnswer("WSetFind", tree, 5);
}
void NocaseSetInsertOne(void* tree, const void* key) { ev("NInsert", {lab(tree), lab(key)}, {Text(key)}); }
void NocaseSetErase(void* tree, void* node) { ev("NErase", {lab(tree), lab(node)}); }
void UIOnlineUpdateBuddy(void* ui, const char16_t* name, int32_t flags) { ev("UIBuddy", {lab(ui), flags}, {NN(name)}); }
void UIOnlineUpdateContact(void* ui, const char16_t* name, int32_t flags) { ev("UIContact", {lab(ui), flags}, {NN(name)}); }
}  // namespace response_callee

namespace chat_satellite_callee {
void ArgumentStrVal(MslCharString* result, const Argument* argument) {
    const uint32_t index = argument->mString;
    const std::string text = index < 16 ? gChars[index] : std::string();
    MslCharRep* rep = static_cast<MslCharRep*>(tpw::ArenaAllocZeroed(sizeof(MslCharRep)));
    char* data = static_cast<char*>(tpw::ArenaAllocZeroed(static_cast<uint32_t>(text.size() + 1)));
    std::memcpy(data, text.c_str(), text.size() + 1);
    rep->len = static_cast<uint32_t>(text.size());
    rep->cap = static_cast<uint32_t>(text.size());
    rep->refs = 0;
    rep->data = data;
    result->mRep = rep;
    ev("StrVal", {newlab(result), lab(argument)}, {text.c_str()});
}
}  // namespace chat_satellite_callee

void EarmuffsButtonDown(bool down) { ev("Earmuffs", {down ? 1 : 0}); }
void GotoCallbackFn(int32_t status, const void* data) { ev("GotoCallbackFn", {status, lab(data)}); }

void CTRACE(uint32_t level, const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    const int32_t a = va_arg(ap, int32_t);
    const int32_t b = va_arg(ap, int32_t);
    va_end(ap);
    ev("CTRACE", {level, a, b}, {format});
}

namespace SamsUtilities {
void UDebug(const char* format, ...) { ev("UDebug", {}, {format}); }
}  // namespace SamsUtilities

void SCR_SetFloatVariable(int32_t handle, uint32_t index, float value) { ev("SCRSetF", {handle, index, fbits(value)}); }

tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CThing::GetThingId() const {
    ev("GetThingId", {lab(this)});
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id;
    id.mIndex = static_cast<uint16_t>(ret("ThingId"));
    return id;
}
void CPeep::HideMySprite() { ev("Hide", {lab(this)}); }
void CPeep::DrawMySpriteAt(float x, float y, float z, int32_t frame, CDelayedScript::ScriptIdx script) {
    ev("Draw", {lab(this), fbits(x), fbits(y), fbits(z), frame, static_cast<int32_t>(script)});
}

int32_t TbUniStringVTag::Format(CStaticBuffer&, TbVarFormat&) { return 0; }

int32_t TbITable::FormatString(char16_t* out, uint32_t size, uint32_t id, ...) const {
    ev("Format", {lab(this), lab(out), size, id});
    va_list ap;
    va_start(ap, id);
    const int32_t count = TagCount(id);
    for (int32_t i = 0; i < count; ++i) {
        TbUniStringVTag* tag = va_arg(ap, TbUniStringVTag*);
        void* vt;
        std::memcpy(&vt, tag, sizeof vt);
        ev("Tag", {tag->mMagic, lab(vt), tag->mId}, {NN(tag->mText)});
    }
    va_end(ap);
    static const char kHex[] = "0123456789abcdef";
    char digits[12];
    int n = 0;
    uint32_t v = id;
    do {
        digits[n++] = kHex[v & 0xf];
        v >>= 4;
    } while (v != 0);
    int k = 0;
    out[k++] = u'F';
    while (n > 0) out[k++] = static_cast<char16_t>(digits[--n]);
    out[k] = 0;
    return 0;
}

CFSOnlineSystem::CFSOnlineSystem() { ev("SysCtor", {lab(this)}); }
CFSFTPParkInfo* CFSOnlineSystem::GetFTPDownloadParkInfo() {
    ev("ParkInfo", {lab(this)});
    return static_cast<CFSFTPParkInfo*>(gPtr[6]);
}
void CFSOnlineSystem::GotoPlayerRequestFTPDownloadByID(int32_t parkID, void (*callback)(int32_t, const void*), const void* context) {
    ev("GotoReq", {lab(this), parkID, lab(reinterpret_cast<const void*>(callback)), lab(context)});
}

COnlinePerson* COnlineControl::GetPerson(std::u16string& name) {
    ev("GetPerson", {lab(this), lab(&name)}, {NN(name.c_str())});
    const int64_t k = ret("GetPerson");
    if (k == 1) return static_cast<COnlinePerson*>(gPtr[1]);
    if (k == 2) return static_cast<COnlinePerson*>(gPtr[2]);
    return nullptr;
}
bool COnlineControl::SetDestination(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thing, MapPos x, MapPos y) {
    ev("SetDest", {lab(this), thing.mIndex, (x.mCell << 8) | x.mOffset, (y.mCell << 8) | y.mOffset});
    return false;
}
void COnlineControl::Teleport(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thing, MapPos x, MapPos y) {
    ev("Teleport", {lab(this), thing.mIndex, (x.mCell << 8) | x.mOffset, (y.mCell << 8) | y.mOffset});
}
void COnlineControl::TeleportAvatar(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell) {
    ev("TeleportAvatar", {lab(this), cell.mIndex});
}
void COnlineControl::CreateVisitor(std::u16string& name, int32_t x, int32_t y, int32_t avatar, int32_t unused) {
    ev("CreateVisitor", {lab(this), lab(&name), x, y, avatar, unused}, {NN(name.c_str())});
}
void COnlineControl::AddContact(std::u16string& name, int32_t flag) {
    ev("AddContact", {lab(this), lab(&name), flag}, {NN(name.c_str())});
}
void COnlineControl::Feedback(std::u16string& text) { ev("Feedback", {lab(this), lab(&text)}, {NN(text.c_str())}); }
void COnlineControl::ReceiveText(std::u16string& text, int32_t colour) {
    ev("ReceiveText", {lab(this), lab(&text), colour}, {NN(text.c_str())});
}
uint32_t COnlineControl::GetDataForUser(const nocase_string& key) const {
    ev("GetData", {lab(this), lab(&key)}, {Text(&key)});
    return static_cast<uint32_t>(ret("GetData"));
}
void COnlineControl::UpdateBuddyListToServer() { ev("UpdBuddyList", {lab(this)}); }
void COnlineControl::UpdateIgnoreListToServer() { ev("UpdIgnoreList", {lab(this)}); }
void COnlineControl::DestroyVisitor(std::u16string& name) { ev("DestroyVisitor", {lab(this), lab(&name)}, {NN(name.c_str())}); }
void COnlineControl::RemoveContact(std::u16string& name) { ev("RemoveContact", {lab(this), lab(&name)}, {NN(name.c_str())}); }
bool COnlineControl::AddBuddy(std::u16string& name, int32_t flags) {
    ev("AddBuddy", {lab(this), lab(&name), flags}, {NN(name.c_str())});
    return (ret("AddBuddy") & 0xff) != 0;
}
void COnlineControl::RemoveBuddy(std::u16string& name) { ev("RemoveBuddy", {lab(this), lab(&name)}, {NN(name.c_str())}); }

extern "C" {

uint64_t tpw_pr_addr(const char* name) {
    if (std::strcmp(name, "ui") == 0) return reinterpret_cast<uintptr_t>(&gUIInterface);
    if (std::strcmp(name, "callback") == 0) return reinterpret_cast<uintptr_t>(reinterpret_cast<const void*>(&GotoCallbackFn));
    if (std::strcmp(name, "vt") == 0) {
        void* vt;
        std::memcpy(&vt, &gTagModel, sizeof vt);
        return reinterpret_cast<uintptr_t>(vt);
    }
    return 0;
}

uint64_t tpw_pr_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_pr_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(COnlineControl));
    out[n++] = static_cast<int64_t>(sizeof(Argument));
    out[n++] = static_cast<int64_t>(offsetof(Argument, mWString));
    out[n++] = static_cast<int64_t>(offsetof(Argument, mCommand));
    out[n++] = static_cast<int64_t>(offsetof(ArgumentVector, mSize));
    out[n++] = static_cast<int64_t>(offsetof(ArgumentVector, mData));
    return n;
}

// The case's texts: kind 0 the wide strings (ASCII), 1 the char strings.
void tpw_pr_set_text(int32_t kind, int32_t index, const char* text) {
    if (index < 0 || index >= 16) return;
    if (kind == 0) {
        std::u16string w;
        for (const char* p = text; *p != 0; ++p) w.push_back(static_cast<char16_t>(static_cast<unsigned char>(*p)));
        gWide[index] = w;
    } else {
        gChars[index] = text;
    }
}

void tpw_pr_set_ptr(int32_t index, uint64_t address) {
    if (index >= 0 && index < 16) gPtr[index] = reinterpret_cast<void*>(static_cast<uintptr_t>(address));
}

// The case's world: the string table of gText and the online system pointer.
void tpw_pr_set_world(uint64_t table, uint64_t system) {
    gText.mResourcePointers[19] = reinterpret_cast<void*>(static_cast<uintptr_t>(table));
    gOnlineSystem = reinterpret_cast<CFSOnlineSystem*>(static_cast<uintptr_t>(system));
}

int64_t tpw_pr_system_label() { return lab(gOnlineSystem); }

void tpw_pr_process(uint64_t self, uint32_t capacity, uint32_t size, uint64_t data) {
    uint8_t* block = static_cast<uint8_t*>(tpw::ArenaAllocZeroed(16));
    ArgumentVector* vector = reinterpret_cast<ArgumentVector*>(block);
    vector->mCapacity = capacity;
    vector->mSize = size;
    vector->mData = reinterpret_cast<Argument*>(static_cast<uintptr_t>(data));
    reinterpret_cast<COnlineControl*>(static_cast<uintptr_t>(self))->ProcessResponse(vector);
}

}  // extern "C"
