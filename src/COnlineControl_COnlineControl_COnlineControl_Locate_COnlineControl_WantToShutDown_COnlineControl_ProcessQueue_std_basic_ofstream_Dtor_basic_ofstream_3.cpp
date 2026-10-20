// Unit v5903: the buddy / ignore list helpers of COnlineControl, GotoFailed, GotoStoredPerson, the pair<const nocase_string, BuddyInfo>
// destructor 0x1021bd24 and the basic_ofstream<wchar_t> destructor 0x1021ce74.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <string>
#include "online.h"
#include "UI_pump.h"
#include "analyser.h"
#include "ThingReceivers.h"
#include "platform/samsutils.h"
#include "FS_UserInfo.h"
#include "TPGlobals.h"
#include "UIInterface.h"
#include "Ptr32.h"
#include "std_basic_istream.h"
#include "std_basic_ifstream.h"
#include "std_deque_errorthingy.h"
#include "TbITable.h"
#include "Text.h"

// 0x102b1f08 (TOC -0x5eb4): the vtable the ofstream destructor stores last (initial word 0x102b1f00).
uint32_t gOfsWVtblB[4];

namespace {
inline uint32_t RdW(const void* base, uint32_t off) {
    return *reinterpret_cast<const uint32_t*>(reinterpret_cast<const uint8_t*>(base) + off);
}
// The object's pointer words (node links, the person, the key reps) are arena offsets (Ptr32); 0 is null.
inline void* P32(uint32_t v) {
    Ptr32<void> p;
    p.mOffset = v;
    return p.get();
}
inline uint32_t A32(const void* p) { return tpw::ArenaOffset(p); }
}  // namespace

// 0x1021c330: Chat::Goto with the name of the stored person (the text pointer at person + 0xc).
void COnlineControl::GotoStoredPerson() {
    uint8_t* self = reinterpret_cast<uint8_t*>(this);
    ignore_callee::IgnoreWString name;
    uint32_t person = RdW(self, 0xe88);
    online_callee::WStringCtorFrom(&name, static_cast<const char16_t*>(P32(RdW(P32(person), 0xc))));
    docommand_callee::ChatGoto(RdW(self, 0), &name, this);
    online_callee::WStringDtor(&name, -1);
}

// 0x1021c254: the text 0x9d of the UI string table goes to the feedback buffer and to the chat panel; the goto flag is cleared.
void COnlineControl::GotoFailed() {
    uint8_t* self = reinterpret_cast<uint8_t*>(this);
    char16_t* buffer = reinterpret_cast<char16_t*>(self + 0x220);
    const TbITable* table = static_cast<const TbITable*>(gText.mResourcePointers[19]);   // Mac 0x598
    table->FormatString(buffer, 0x400, 0x9d);
    ignore_callee::IgnoreWString first, second;
    online_callee::WStringCtorFrom(&first, buffer);
    online_callee::WStringCopy(&second, &first, 0, 0xffffffffu);
    ReceiveText(*reinterpret_cast<std::u16string*>(&second), 7);
    online_callee::WStringDtor(&second, -1);
    self[0xe84] = 0;
    online_callee::WStringDtor(&first, -1);
}

// 0x1021c190: every name of the ignore set (end node 0xe48, first node word 0xe4c; the key is at node + 0xc).
void COnlineControl::UpdateIgnoreListToServer() {
    const uint8_t* self = reinterpret_cast<const uint8_t*>(this);
    uint32_t it = RdW(self, 0xe4c);
    while (it != A32(self) + 0xe48) {
        ignore_callee::ChatIgnore(RdW(self, 0), static_cast<const ignore_callee::IgnoreWString*>(P32(it + 0xc)), this);
        ignore_callee::TreeIncrement(&it);
    }
}

// 0x1021c0ec: every name of the buddy map (end node 0xe38, first node word 0xe40).
void COnlineControl::UpdateBuddyListToServer() {
    const uint8_t* self = reinterpret_cast<const uint8_t*>(this);
    uint32_t it = RdW(self, 0xe40);
    while (it != A32(self) + 0xe38) {
        docommand_callee::ChatAddBuddy(RdW(self, 0), P32(it + 0xc), this);
        ignore_callee::TreeIncrement(&it);
    }
}

// 0x1021c02c: every buddy to the UI, with its data word.
void COnlineControl::UpdateBuddyList() const {
    const uint8_t* self = reinterpret_cast<const uint8_t*>(this);
    uint32_t it = RdW(self, 0xe40);
    while (it != A32(self) + 0xe38) {
        uint32_t data = GetDataForUser(*static_cast<const nocase_string*>(P32(it + 0xc)));
        uint32_t rep = RdW(P32(it + 0xc), 0);
        online_callee::UIOnlineAddBuddy(&gUIInterface, static_cast<const char16_t*>(P32(RdW(P32(rep), 0xc))),
                                        static_cast<int32_t>(data));
        ignore_callee::TreeIncrement(&it);
    }
}

// 0x1021bd24: std::pair<const nocase_string, BuddyInfo>::~pair
BuddyEntry* BuddyEntryDestruct(BuddyEntry* self, int16_t flag) {
    if (self == nullptr) return self;
    online_callee::NocaseWStringDtor(self, 0);
    if (flag > 0) online_callee::OperatorDelete(self);
    return self;
}

// 0x1021ce74: std::basic_ofstream<wchar_t>::~basic_ofstream. The original tests this + 8, this + 0x24 and this + 0x58 against 0 (never true).
BasicOfstreamWchar* BasicOfstreamWchar::destroy(int16_t flag) {
    if (this == nullptr) return this;
    void* filebuf = &mFilebufVt;   // Mac this + 8
    void* locale = mLocale;        // Mac this + 0x24
    void* ios = &mIosVtable;       // Mac this + 0x58
    const uint8_t* files = gIfsWFiles;

    mVtable = gOfsWVtbl;
    mVbase->mFirst = &gOfsWVtbl[3];
    mVbase->mBaseOffset = reinterpret_cast<const uint8_t*>(ios) - reinterpret_cast<const uint8_t*>(mVbase);
    mFilebufVt = gIfsWVtblD;
    if (mFile != files && mFile != files + 0x54 && mFile != files + 0xa8)
        ignore_callee::FilebufClose(static_cast<ignore_callee::IgnoreFilebuf*>(filebuf));
    mFilebufVt = gIfsWVtblC;
    ifstream_callee::LocaleDtor(locale, -1);

    mVtable = gOfsWVtblB;
    mVbase->mFirst = &gOfsWVtblB[3];
    mVbase->mBaseOffset = reinterpret_cast<const uint8_t*>(filebuf) - reinterpret_cast<const uint8_t*>(mVbase);
    if (flag != 0) {
        mIosVtable = gIfsWVtblE;
        ifstream_callee::IosBaseDtor(ios, 0);
    }
    if (flag > 0) online_callee::OperatorDelete(this);
    return this;
}
