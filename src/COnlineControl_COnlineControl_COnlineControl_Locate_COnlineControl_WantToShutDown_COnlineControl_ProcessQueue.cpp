// COnlineControl::ProcessQueue 0x10214c34, AddBuddy 0x1021baf8 and LoadBuddy 0x1021d004 (main app, online.cpp; unit v5695).
// ProcessQueue hands the queued answers of the chat server (vectors of Argument in a deque at 0xe68) to the vote, normal or init handler.
// AddBuddy puts a name into the buddy map (0xe34) or updates its two flag bytes, then tells the UI. LoadBuddy reads the buddy file
// line by line into AddBuddy; like LoadIgnore it builds a narrow ":buddy.dat" string that is never used (the file name comes from the
// wide path string).
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

using namespace ignore_callee;

namespace {

inline uint8_t* Bytes(COnlineControl* self) { return reinterpret_cast<uint8_t*>(self); }
inline uint32_t& Word(COnlineControl* self, std::size_t offset) { return *reinterpret_cast<uint32_t*>(Bytes(self) + offset); }
inline uint32_t Word32(const void* address) {
    uint32_t value;
    std::memcpy(&value, address, sizeof(value));
    return value;
}

const char kBuddyDat[] = ":buddy.dat";                                      // 0x10251480
const char kFormatPath[] = "%ls";                                           // 0x1025135d
const char kIosFailure[] = "ios_base failure in clear";                     // 0x1025148b
const char kExceptionTypes[] = "!std::exception!!std::ios_base::failure!!"; // 0x102517e6
const char16_t kPathSuffix[] = u".bud";                                      // 0x102517dc

// One look at the deque's first record through the block table (the original calls operator[] again for every use).
uint8_t* FirstRecord(COnlineControl* self) {
    void* slot = errdeque_callee::CdequeIndex(Bytes(self) + 0xe6c, Word(self, 0xe7c) / Word(self, 0xe68));
    const uint32_t first = Word(self, 0xe7c);
    const uint32_t blockSize = Word(self, 0xe68);
    uint8_t* block = Ptr32<uint8_t>{Word32(slot)}.get();
    return block + (first - first / blockSize * blockSize) * 0xc;
}

void SetFailed(IgnoreIos* ios) {
    ios->mState |= 4;
    if (ios->mStreamBuf == 0) ios->mState |= 1;
    if ((ios->mState & ios->mExceptions) != 0) {
        MslThrowObject object;
        object.mVtbl = gIosFailureVtbl;
        char* block = NewArray(static_cast<uint32_t>(std::strlen(kIosFailure)) + 1);
        RefPtrCtor(&object.mMessage, block);
        StrCpy(object.mMessage, kIosFailure);
        MslThrow(kExceptionTypes, &object, gIosFailureDtorVec);
    }
}

void BuildPath(COnlineControl* self, const char* path, void* text, IgnorePathString* filePath, char* buffer) {
    if (gUserInfo == nullptr) {
        void* block = online_callee::OperatorNew(0xd4);
        if (block != nullptr) UserInfoCtor(block);
        gUserInfo = static_cast<CFSUserInfo*>(block);
    }
    IgnorePathString online;
    ReturnOnlinePath(&online, gUserInfo);
    PathCopy(filePath, &online);
    PathDtor(&online, -1);
    PathAppendString(filePath, reinterpret_cast<const uint8_t*>(&gTPGlobals) + 0x44c0);
    // this + 8 is the player name (a std::wstring: one word, the representation's data pointer at +0xc, native pointer at +0x10 here)
    const uint8_t* rep = static_cast<const uint8_t*>(online_callee::HandleToPointer(Word(self, 8)));
    const char16_t* name;
    std::memcpy(&name, rep + 0x10, sizeof(name));
    PathAppendText(filePath, name);
    PathAppendText(filePath, kPathSuffix);
    CharStringAssign(text, path, static_cast<uint32_t>(std::strlen(path)));
    CharStringAppend(text, kBuddyDat, static_cast<uint32_t>(std::strlen(kBuddyDat)));
    SprintfWide(buffer, kFormatPath, filePath->mData);
}

}  // namespace

// 0x10214c34
void COnlineControl::ProcessQueue() {
    reinterpret_cast<bthread_mutex*>(Bytes(this) + 0x18)->lock();
    while (Word(this, 0xe80) != 0) {
        uint8_t* record = FirstRecord(this);
        bool vote = false;
        if (Word32(record + 4) != 0) {
            record = FirstRecord(this);
            const uint8_t* arguments = Ptr32<uint8_t>{Word32(record + 8)}.get();
            vote = Word32(arguments + 8) == 0x2711;
        }
        if (vote) {
            if (gOnlineControl == nullptr) {
                void* block = online_callee::OperatorNew(0xe8c);
                if (block != nullptr) new (block) COnlineControl();
                gOnlineControl = static_cast<COnlineControl*>(block);
            }
            COnlineControl* target = gOnlineControl;
            record = FirstRecord(this);
            const uint8_t* arguments = Ptr32<uint8_t>{Word32(record + 8)}.get();
            target->ProcessVoteResponse(static_cast<int32_t>(Word32(arguments + 0xc + 8)));
        } else if (Bytes(this)[0x10] != 0) {
            record = FirstRecord(this);
            ProcessResponse(record);
        } else {
            record = FirstRecord(this);
            ProcessInitResponse(record);
        }
        online_callee::ArgumentDequePopFront(Bytes(this) + 0xe68);
    }
    reinterpret_cast<bthread_mutex*>(Bytes(this) + 0x18)->unlock();
}

// 0x1021baf8
bool COnlineControl::AddBuddy(std::u16string& name, int32_t flags) {
    uint8_t* self = Bytes(this);
    const uint8_t direct = (flags & 2) != 0 ? 1 : 0;
    const uint8_t second = (flags & 4) != 0 ? 1 : 0;
    uint64_t key = 0;
    online_callee::NocaseWStringCopy(&key, &name, 0, 0xffffffffu);
    uint8_t* node = static_cast<uint8_t*>(online_callee::BuddyTreeFind(self + 0xe34, &key));
    online_callee::NocaseWStringDtor(&key, 0);
    bool added;
    if (node != self + 0xe38) {
        node[0x10] = direct;
        node[0x11] = second;
        added = false;
    } else {
        uint64_t copy = 0;
        BuddyEntry entry{};
        online_callee::NocaseWStringCopy(&copy, &name, 0, 0xffffffffu);
        online_callee::NocaseWStringCopy(&entry.mKey, &copy, 0, 0xffffffffu);
        entry.mDirect = direct;
        entry.mFlag = second;
        online_callee::BuddyTreeInsert(self + 0xe34, &entry);
        online_callee::NocaseWStringDtor(&entry.mKey, 0);
        online_callee::NocaseWStringDtor(&copy, 0);
        added = true;
    }
    uint64_t lookup = 0;
    online_callee::NocaseWStringCopy(&lookup, &name, 0, 0xffffffffu);
    const uint32_t data = GetDataForUser(*reinterpret_cast<const nocase_string*>(&lookup));
    online_callee::NocaseWStringDtor(&lookup, 0);
    // the name is a pointer to the representation, whose data pointer is at +0xc (native pointer at +0x10 here)
    const char16_t* text;
    const uint8_t* rep = *reinterpret_cast<const uint8_t* const*>(&name);
    std::memcpy(&text, rep + 0x10, sizeof(text));
    online_callee::UIOnlineAddBuddy(&gUIInterface, text, static_cast<int32_t>(data));
    rep = *reinterpret_cast<const uint8_t* const*>(&name);
    std::memcpy(&text, rep + 0x10, sizeof(text));
    online_callee::UIOnlineUpdateContact(&gUIInterface, text, static_cast<int32_t>(data));
    return added;
}

// 0x1021d004
bool COnlineControl::LoadBuddy(const char* path) {
    if (Word(this, 0) == 0) return false;
    IgnoreStream stream{};
    IgnoreFilebuf filebuf{};
    alignas(8) uint8_t iosPart[0x50] = {};
    IgnorePathString filePath{};
    char buffer[260];
    char16_t line[32];
    uint64_t text = 0;

    IfstreamCtor(&stream, 1);
    online_callee::StringCtor(&text);
    BuildPath(this, path, &text, &filePath, buffer);
    if (FilebufOpen(&filebuf, buffer, 8) == 0) SetFailed(stream.mIos);
    for (;;) {
        IgnoreLocale locale;
        IosGetloc(&locale, stream.mIos);
        void* facet = UseFacetCtype(&locale);
        const char16_t delimiter = CtypeWiden(facet, 10);
        LocaleDtor(&locale, -1);
        IgnoreStream* result = IstreamGetline(&stream, line, 0x20, delimiter);
        IgnoreIos* ios = result != nullptr ? result->mIos : nullptr;
        uint32_t more = 0;
        if ((ios->mState & 5) == 0) more = ios->mStreamBuf;
        if (more == 0) break;
        IgnoreWString first, name;
        online_callee::WStringCtorFrom(&first, line);
        online_callee::WStringCopy(&name, &first, 0, 0xffffffffu);
        online_callee::WStringDtor(&first, -1);
        AddBuddy(*reinterpret_cast<std::u16string*>(&name), 0);
        online_callee::WStringDtor(&name, -1);
    }
    if (FilebufClose(&filebuf) == 0) SetFailed(stream.mIos);
    PathDtor(&filePath, -1);
    online_callee::StringDtor(&text, -1);
    stream.mSlot = gIfsWVtblB;
    stream.mIos->mVtable = reinterpret_cast<const uint8_t*>(gIfsWVtblB) + 0xc;
    stream.mIos->mBaseOffset = static_cast<int32_t>(iosPart - reinterpret_cast<uint8_t*>(stream.mIos));
    FilebufDtor(&filebuf, -1);
    IstreamDtor(&stream, 0);
    IosDtor(iosPart, 0);
    return true;
}
