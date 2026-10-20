// Test library for four COnlineControl members and the small library functions around them (unit v51062;
// libtpw_unit_COnlineControl_NoLongerAFK.dylib, tests/test_COnlineControl_NoLongerAFK.py), main app:
//   src/COnlineControl_..._NoLongerAFK.cpp   NoLongerAFK, DestroyVisitor, UpdateAvatarPos, HideAvatar, Argument::StrVal / BStrVal,
//   nocase_string::~nocase_string, the wide ios / ostream / streambuf functions, do_widen, the buddy map's erase_one, deallocate.
//
// The callees outside the unit are recording fakes (the same event names and ints as the module's PowerPC stubs). A pointer is logged as
// PtrId: 0 for NULL, (object index + 1) * 0x10000 + offset for a pointer into a block the test registered, -1 otherwise; the host's
// offsets that differ from the Mac's (native pointers) are mapped back with tpw_v51062_map.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

#include "calllog.h"
#include "platform/arena.h"
#include "online.h"
#include "analyser.h"
#include "ThingReceivers.h"
#include "std_basic_filebuf.h"
#include "std_basic_ifstream.h"
#include "InterfaceWindow.h"

using namespace calllog;

// ---- globals of other files (the vtable tables), defined here
uint32_t gIfsWVtblC[4];
uint32_t gIfsWVtblE[4];
uint32_t gOfsWVtblB[4];

namespace {

struct ObjEntry {
    uint64_t base;
    uint32_t size;
    int mapCount;
    uint32_t hostOff[8];
    uint32_t macOff[8];
};
ObjEntry gObjs[16];
int gObjCount = 0;

int64_t PtrId(const void* p) {
    uint64_t v = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(p));
    if (v == 0) return 0;
    for (int i = 0; i < gObjCount; i++) {
        const ObjEntry& o = gObjs[i];
        if (v >= o.base && v < o.base + o.size) {
            uint32_t off = static_cast<uint32_t>(v - o.base);
            for (int k = 0; k < o.mapCount; k++) {
                if (o.hostOff[k] == off) {
                    off = o.macOff[k];
                    break;
                }
            }
            return static_cast<int64_t>(i + 1) * 0x10000 + off;
        }
    }
    return -1;
}

}  // namespace

// ---- the fakes
void docommand_callee::ChatNotAFK(uint32_t chat, void* context) {
    ev("NotAFK", {static_cast<int64_t>(chat), PtrId(context)});
}

void online_callee::ChatMovePlayer(void* chat, int32_t x, int32_t y, int32_t mode, void* context) {
    ev("MovePlayer", {static_cast<int64_t>(reinterpret_cast<uintptr_t>(chat) & 0xffffffffu), static_cast<int64_t>(x), static_cast<int64_t>(y),
                      static_cast<int64_t>(mode), PtrId(context)});
}

COnlinePerson* COnlineControl::GetPerson(std::u16string& name) {
    ev("GetPerson", {PtrId(this), PtrId(&name)});
    return reinterpret_cast<COnlinePerson*>(static_cast<uintptr_t>(ret("GetPerson")));
}

void CThing::KillMe() {
    ev("KillMe", {static_cast<int64_t>(reinterpret_cast<uintptr_t>(this) & 0xffffffffu)});
}

MslCharString* msl_char_string::construct(MslCharString* self, const MslCharString* src, uint32_t pos, uint32_t n) {
    ev("StringCopy", {PtrId(self), PtrId(src), static_cast<int64_t>(pos), static_cast<int64_t>(n)});
    return self;
}

void online_callee::WStringCopy(void* self, const void* other, uint32_t pos, uint32_t count) {
    ev("WStringCopy", {PtrId(self), PtrId(other), static_cast<int64_t>(pos), static_cast<int64_t>(count)});
}

void online_callee::NocaseWStringDtor(void* self, int16_t flag) {
    ev("NocaseWStringDtor", {PtrId(self), static_cast<int64_t>(flag)});
}

void online_callee::OperatorDelete(void* p) { ev("delete", {PtrId(p)}); }

void ifstream_callee::Delete(void* p) { ev("delete", {PtrId(p)}); }

void ifstream_callee::IosBaseDtor(void* ios, int32_t flag) {
    ev("IosBaseDtor", {PtrId(ios), static_cast<int64_t>(flag)});
}

LocaleRefPtr* LocaleRefPtrDestroy(LocaleRefPtr* self, int16_t flag) {
    ev("LocaleDtor", {PtrId(self), static_cast<int64_t>(flag)});
    return self;
}

LocaleRefPtr& LocaleRefPtr::operator=(const LocaleRefPtr& other) {
    ev("LocaleAssign", {PtrId(this), PtrId(&other)});
    return *this;
}

bool buddy_erase_callee::ValueCompareNodeKey(void* compare, const void* pair, const void* key) {
    ev("CmpNK", {PtrId(compare), PtrId(pair), PtrId(key)});
    return ret("CmpNK") != 0;
}

bool buddy_erase_callee::ValueCompareKeyNode(void* compare, const void* key, const void* pair) {
    ev("CmpKN", {PtrId(compare), PtrId(key), PtrId(pair)});
    return ret("CmpKN") != 0;
}

void BuddyTreeErase(BuddyTree* self, BuddyTreeNode* position) {
    ev("TreeErase", {PtrId(self), PtrId(position)});
}

extern "C" {

uint64_t tpw_v51062_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gIfsWVtblC", gIfsWVtblC},
        {"gIfsWVtblE", gIfsWVtblE},
        {"gOfsWVtblB", gOfsWVtblB},
        {"arenaBase", tpw::gArenaBase},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_v51062_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The object table of PtrId: cleared per case, then one entry per block and the offset maps (host offset -> Mac offset).
void tpw_v51062_clear() { gObjCount = 0; }

int32_t tpw_v51062_obj(uint64_t base, uint32_t size) {
    ObjEntry& o = gObjs[gObjCount];
    o.base = base;
    o.size = size;
    o.mapCount = 0;
    return gObjCount++;
}

void tpw_v51062_map(int32_t index, uint32_t hostOff, uint32_t macOff) {
    ObjEntry& o = gObjs[index];
    o.hostOff[o.mapCount] = hostOff;
    o.macOff[o.mapCount] = macOff;
    o.mapCount++;
}

int32_t tpw_v51062_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(COnlineControl));
    out[n++] = static_cast<int64_t>(offsetof(BasicStreambufWchar, mLocale));
    out[n++] = static_cast<int64_t>(offsetof(ignore_callee::IgnoreStream, mSlot));
    out[n++] = static_cast<int64_t>(offsetof(ignore_callee::IgnoreStream, mRest));
    out[n++] = static_cast<int64_t>(offsetof(ignore_callee::IgnoreIos, mBaseOffset));
    out[n++] = static_cast<int64_t>(offsetof(BuddyTree, mRoot));
    out[n++] = static_cast<int64_t>(offsetof(BuddyTree, mField8));
    out[n++] = static_cast<int64_t>(offsetof(BuddyTreeNode, mRight));
    out[n++] = static_cast<int64_t>(offsetof(BuddyTreeNode, mPair));
    return n;
}

// ---- the functions: every argument a 64-bit word (the module passes the Mac's 32-bit register values), the answer a word
uint64_t tpw_v51062_afk(uint64_t a, uint64_t b, uint64_t) {
    reinterpret_cast<COnlineControl*>(a)->NoLongerAFK((b & 0xff) != 0);
    return 0;
}

uint64_t tpw_v51062_destroy(uint64_t a, uint64_t b, uint64_t) {
    reinterpret_cast<COnlineControl*>(a)->DestroyVisitor(*reinterpret_cast<std::u16string*>(b));
    return 0;
}

uint64_t tpw_v51062_updpos(uint64_t a, uint64_t b, uint64_t c) {
    MapPos x{static_cast<uint8_t>(b >> 24), static_cast<uint8_t>(b >> 16)};
    MapPos y{static_cast<uint8_t>(c >> 24), static_cast<uint8_t>(c >> 16)};
    reinterpret_cast<COnlineControl*>(a)->UpdateAvatarPos(x, y);
    return 0;
}

uint64_t tpw_v51062_hide(uint64_t a, uint64_t, uint64_t) {
    reinterpret_cast<COnlineControl*>(a)->HideAvatar();
    return 0;
}

uint64_t tpw_v51062_strval(uint64_t a, uint64_t b, uint64_t) {
    chat_satellite_callee::ArgumentStrVal(reinterpret_cast<MslCharString*>(a), reinterpret_cast<const Argument*>(b));
    return 0;
}

uint64_t tpw_v51062_bstrval(uint64_t a, uint64_t b, uint64_t) {
    docommand_callee::ArgumentBStrVal(reinterpret_cast<void*>(a), reinterpret_cast<const void*>(b));
    return 0;
}

uint64_t tpw_v51062_nocase(uint64_t a, uint64_t b, uint64_t) {
    nocase_callee::StringDtor(reinterpret_cast<nocase_string*>(a), static_cast<int16_t>(b));
    return 0;
}

uint64_t tpw_v51062_dealloc(uint64_t a, uint64_t b, uint64_t c) {
    nocase_callee::Deallocate(reinterpret_cast<void*>(a), reinterpret_cast<void*>(b), static_cast<unsigned long>(c));
    return 0;
}

uint64_t tpw_v51062_ios(uint64_t a, uint64_t b, uint64_t) {
    ignore_callee::IosDtor(reinterpret_cast<void*>(a), static_cast<int16_t>(b));
    return 0;
}

uint64_t tpw_v51062_ostream(uint64_t a, uint64_t b, uint64_t) {
    ignore_callee::OstreamDtor(reinterpret_cast<ignore_callee::IgnoreStream*>(a), static_cast<int16_t>(b));
    return 0;
}

uint64_t tpw_v51062_sbdtor(uint64_t a, uint64_t b, uint64_t) {
    reinterpret_cast<BasicStreambufWchar*>(a)->destroy(static_cast<int16_t>(b));
    return 0;
}

uint64_t tpw_v51062_widen(uint64_t a, uint64_t b, uint64_t) {
    return static_cast<uint64_t>(CtypeWcharDoWiden(reinterpret_cast<const void*>(a), static_cast<char>(b)));
}

uint64_t tpw_v51062_imbue(uint64_t a, uint64_t b, uint64_t) {
    reinterpret_cast<BasicStreambufWchar*>(a)->imbue(reinterpret_cast<const void*>(b));
    return 0;
}

uint64_t tpw_v51062_setbuf(uint64_t a, uint64_t b, uint64_t c) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(
        reinterpret_cast<BasicStreambufWchar*>(a)->setbuf(reinterpret_cast<char16_t*>(b), static_cast<int32_t>(c))));
}

uint64_t tpw_v51062_sync(uint64_t a, uint64_t, uint64_t) {
    return static_cast<uint32_t>(reinterpret_cast<BasicStreambufWchar*>(a)->sync());
}

uint64_t tpw_v51062_showmanyc(uint64_t a, uint64_t, uint64_t) {
    return static_cast<uint32_t>(reinterpret_cast<BasicStreambufWchar*>(a)->showmanyc());
}

uint64_t tpw_v51062_underflow(uint64_t a, uint64_t, uint64_t) {
    return static_cast<uint32_t>(reinterpret_cast<BasicStreambufWchar*>(a)->underflow());
}

uint64_t tpw_v51062_pbackfail(uint64_t a, uint64_t b, uint64_t) {
    return static_cast<uint32_t>(reinterpret_cast<BasicStreambufWchar*>(a)->pbackfail(static_cast<int32_t>(b)));
}

uint64_t tpw_v51062_overflow(uint64_t a, uint64_t b, uint64_t) {
    return static_cast<uint32_t>(reinterpret_cast<BasicStreambufWchar*>(a)->overflow(static_cast<int32_t>(b)));
}

uint64_t tpw_v51062_erase(uint64_t a, uint64_t b, uint64_t) {
    return BuddyTreeEraseOneCount(reinterpret_cast<BuddyTree*>(a), reinterpret_cast<const void*>(b));
}

}  // extern "C"
