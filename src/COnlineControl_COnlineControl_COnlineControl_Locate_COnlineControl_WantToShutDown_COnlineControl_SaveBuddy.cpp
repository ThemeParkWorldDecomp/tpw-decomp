// COnlineControl::SaveBuddy 0x1021c75c and COnlineControl::ProcessVoteResponse 0x1021c3cc (online.cpp).
// SaveBuddy writes the buddy map's names, one swprintf'd line each, to a file whose wide path is the online path + a separator + the
// player's name + a suffix. The narrow name built from the argument (and ":buddy.dat") is never used afterwards: the original builds it
// and drops it. Both stream failure checks throw an ios_base::failure when the exception mask asks for it (the fake __throw returns).
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <string>

#include "online.h"
#include "FS_UserInfo.h"
#include "TPGlobals.h"
#include "Text.h"
#include "TbITable.h"
#include "platform/arena.h"
#include "platform/samsutils.h"

extern char gIosFailureDtorVec[];   // 0x102661d0
extern char gIosFailureVtbl[];      // 0x1029d084

namespace {

// The 32-bit words of the Mac's objects hold pointers as offsets from the arena base (Ptr32's convention).
uint8_t* Deref(uint32_t offset) { return offset != 0 ? reinterpret_cast<uint8_t*>(tpw::gArenaBase + offset) : nullptr; }
uint32_t* Word(uint8_t* address) { return reinterpret_cast<uint32_t*>(address); }

// Text at the TOC slot -0x3960's base (0x1025179c) + 0x40 and + 0x74; see the test for the strings the original holds.
const char16_t kPathSuffix[] = u".bud";      // 0x102517dc
const char16_t kLineFormat[] = u"%ls\n";      // 0x10251810
const char kTypeName[] = "!std::exception!!std::ios_base::failure!!";   // 0x102517e6
const char kFailureMessage[] = "ios_base failure in clear";               // 0x1025148b
const char kBuddyFile[] = ":buddy.dat";                                  // 0x10251480

// The stack object of the original: stream at 0x1f0, filebuf at 0x1f8, ios part at 0x248 (host layout differs, the code never relies on it).
struct BuddyStreamObject {
    ignore_callee::IgnoreStream mStream;
    ignore_callee::IgnoreFilebuf mBuf;
    ignore_callee::IgnoreIos mIos;
};

struct IosFailureObject {
    const void* mVtable;   // 0x00 gIosFailureVtbl
    char* mMessage;        // 0x04 the RefCountedPtr
};

void ThrowFailure() {
    using namespace ignore_callee;
    IosFailureObject object;
    object.mVtable = gIosFailureVtbl;
    uint32_t size = static_cast<uint32_t>(std::strlen(kFailureMessage)) + 1;
    char* block = NewArray(size);
    RefPtrCtor(&object.mMessage, block);
    std::strcpy(object.mMessage, kFailureMessage);
    buddy_callee::Throw(kTypeName, &object, gIosFailureDtorVec);
}

// ios.setstate(failbit) when the open or close failed: the badbit goes on, the failbit too when there is no buffer, and the
// exception mask decides whether the failure is thrown.
void SetFailure(ignore_callee::IgnoreIos* ios) {
    ios->mState |= 4;
    if (ios->mStreamBuf == 0) ios->mState |= 1;
    if ((ios->mState & ios->mExceptions) != 0) ThrowFailure();
}

void Teardown(BuddyStreamObject& o, ignore_callee::IgnorePathString& path, void* name) {
    using namespace ignore_callee;
    PathDtor(&path, -1);
    online_callee::StringDtor(name, -1);
    o.mStream.mSlot = gOfsWVtbl;
    o.mStream.mIos->mVtable = reinterpret_cast<const uint8_t*>(gOfsWVtbl) + 0xc;
    o.mStream.mIos->mBaseOffset = static_cast<int32_t>(reinterpret_cast<intptr_t>(&o.mIos) - reinterpret_cast<intptr_t>(o.mStream.mIos));
    FilebufDtor(&o.mBuf, -1);
    OstreamDtor(&o.mStream, 0);
    IosDtor(&o.mIos, 0);
}

}  // namespace

bool OnlineSaveBuddy(COnlineControl* self, const char* path) {
    using namespace ignore_callee;
    uint8_t* me = reinterpret_cast<uint8_t*>(self);
    BuddyStreamObject o;
    OfstreamCtor(&o.mStream, 1);
    void* name[2];
    online_callee::StringCtor(name);
    if (gUserInfo == nullptr) {
        void* block = online_callee::OperatorNew(0xd4);
        if (block != nullptr) UserInfoCtor(block);
        gUserInfo = static_cast<CFSUserInfo*>(block);
    }
    IgnorePathString temp;
    ReturnOnlinePath(&temp, gUserInfo);
    IgnorePathString full;
    PathCopy(&full, &temp);
    PathDtor(&temp, -1);
    PathAppendString(&full, reinterpret_cast<const uint8_t*>(&gTPGlobals) + 0x44c0);
    uint32_t nameRep = *reinterpret_cast<uint32_t*>(me + 8);
    uint32_t nameText = *Word(Deref(nameRep) + 0xc);
    PathAppendText(&full, reinterpret_cast<const char16_t*>(Deref(nameText)));
    PathAppendText(&full, kPathSuffix);
    CharStringAssign(name, path, static_cast<uint32_t>(std::strlen(path)));
    CharStringAppend(name, kBuddyFile, static_cast<uint32_t>(std::strlen(kBuddyFile)));
    char buffer[260];
    SprintfWide(buffer, "%ls", full.mData);
    if (FilebufOpen(&o.mBuf, buffer, 0x10) == 0) SetFailure(o.mStream.mIos);
    if (o.mBuf.mFile == 0) {
        Teardown(o, full, name);
        return false;
    }
    uint32_t node = *reinterpret_cast<uint32_t*>(me + 0xe40);
    while (node != tpw::ArenaOffset(me + 0xe38)) {
        uint32_t key = *Word(Deref(node) + 0xc);
        uint32_t text = *Word(Deref(key) + 0xc);
        char16_t line[32];
        SwprintfWide(line, 0x20, kLineFormat, reinterpret_cast<const char16_t*>(Deref(text)));
        OstreamWrite(&o.mStream, line);
        TreeIncrement(&node);
    }
    if (FilebufClose(&o.mBuf) == 0) SetFailure(o.mStream.mIos);
    Teardown(o, full, name);
    return true;
}

void COnlineControl::SaveBuddy(const char* path) { OnlineSaveBuddy(this, path); }

// The answer of the vote connection: the string picked by the code is formatted from the UI table into the buffer at 0x220, copied
// into a std::wstring and shown as chat text (colour 7); then the done-voting flag goes on.
void COnlineControl::ProcessVoteResponse(int32_t code) {
    uint32_t id = 0x81;
    if (code == 0) id = 0x80;
    else if (code == -2) id = 0x82;
    else if (code == -0x1e) id = 0x83;
    else if (code == -0x1f) id = 0xb4;
    uint8_t* me = reinterpret_cast<uint8_t*>(this);
    char16_t* buffer = reinterpret_cast<char16_t*>(me + 0x220);
    static_cast<TbITable*>(gText.mResourcePointers[19])->FormatString(buffer, 0x400, id);
    void* first[2];
    online_callee::WStringCtorFrom(first, buffer);
    void* second[2];
    online_callee::WStringCopy(second, first, 0, 0xffffffffu);
    ReceiveText(*reinterpret_cast<std::u16string*>(second), 7);
    online_callee::WStringDtor(second, -1);
    online_callee::WStringDtor(first, -1);
    me[0xe60] = 1;
    SamsUtilities::UDebug("Done voting - setting the mDoneVoting flag\n");
}
