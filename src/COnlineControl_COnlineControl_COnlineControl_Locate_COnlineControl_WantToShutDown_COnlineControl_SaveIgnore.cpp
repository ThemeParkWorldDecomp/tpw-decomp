// Unit v5235: COnlineControl::SaveIgnore 0x1021d74c and COnlineControl::LoadIgnore 0x1021db98 (online.cpp).
// Both build the file name "<online path><separator><player name><suffix>" in a wide string, print it with sprintf("%ls") and open an
// (o|i)fstream on it. The narrow string built from the argument and ":ignore.dat" is never used (kept so the calls run as on the Mac).
// Save writes one line per name of the ignore tree (this + 0xe48); Load reads lines and ignores each name.
// The iostream failure path (clear() with a mask hit) builds an ios_base::failure object and calls __throw, which returns here.
#include <cstdint>
#include <cstring>
#include <cstdio>

#include "FS_UserInfo.h"
#include "TPGlobals.h"
#include "online.h"
#include "std_basic_ifstream.h"
#include "std_basic_istream.h"

using namespace ignore_callee;

namespace {

const char kFormatPath[] = "%ls";                                          // 0x1025135d
const char kIgnoreDat[] = ":ignore.dat";                                   // 0x102514a5
const char kIosFailure[] = "ios_base failure in clear";                    // 0x1025148b
const char kExceptionTypes[] = "!std::exception!!std::ios_base::failure!!";   // 0x102517e6
const char16_t kPathSuffix[] = u".ign";                                 // 0x1025181a
const char16_t kFormatEntry[] = u"%ls\n";                                  // 0x10251810

uint32_t Word(const void* base, uint32_t offset) {
    uint32_t value;
    std::memcpy(&value, static_cast<const uint8_t*>(base) + offset, 4);
    return value;
}

// basic_ios::clear(state | badbit): sets badbit, failbit too when there is no stream buffer, and throws when the mask matches.
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

// The start both functions share (the compiler emitted it twice): the file name in `buffer`.
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
    CharStringAppend(text, kIgnoreDat, static_cast<uint32_t>(std::strlen(kIgnoreDat)));
    SprintfWide(buffer, kFormatPath, filePath->mData);
}

}  // namespace

// 0x1021d74c
bool OnlineSaveIgnore(COnlineControl* self, const char* path) {
    IgnoreStream stream{};
    IgnoreFilebuf filebuf{};
    alignas(8) uint8_t iosPart[0x50] = {};
    uint64_t text = 0;   // the narrow std::string (one word on the Mac)
    IgnorePathString filePath{};
    char buffer[260];

    OfstreamCtor(&stream, 1);
    online_callee::StringCtor(&text);
    BuildPath(self, path, &text, &filePath, buffer);
    if (FilebufOpen(&filebuf, buffer, 0x10) == 0) SetFailed(stream.mIos);

    bool answer = false;
    if (filebuf.mFile != 0) {
        uint32_t node = Word(self, 0xe4c);
        const uint32_t end = online_callee::PointerToHandle(reinterpret_cast<uint8_t*>(self) + 0xe48);
        while (node != end) {
            char16_t line[32];
            const uint8_t* entry = static_cast<const uint8_t*>(online_callee::HandleToPointer(node));
            const uint8_t* rep = static_cast<const uint8_t*>(online_callee::HandleToPointer(Word(entry, 0xc)));
            const char16_t* name;
            std::memcpy(&name, rep + 0x10, sizeof(name));
            SwprintfWide(line, 0x20, kFormatEntry, name);
            OstreamWrite(&stream, line);
            TreeIncrement(&node);
        }
        if (FilebufClose(&filebuf) == 0) SetFailed(stream.mIos);
        answer = true;
    }
    PathDtor(&filePath, -1);
    online_callee::StringDtor(&text, -1);
    stream.mSlot = gOfsWVtbl;
    stream.mIos->mVtable = reinterpret_cast<const uint8_t*>(gOfsWVtbl) + 0xc;
    stream.mIos->mBaseOffset = static_cast<int32_t>(iosPart - reinterpret_cast<uint8_t*>(stream.mIos));
    FilebufDtor(&filebuf, -1);
    OstreamDtor(&stream, 0);
    IosDtor(iosPart, 0);
    return answer;
}

void COnlineControl::SaveIgnore(const char* path) { OnlineSaveIgnore(this, path); }

// 0x1021db98
bool COnlineControl::LoadIgnore(const char* path) {
    if (Word(this, 0) == 0) return false;
    IgnoreStream stream{};
    IgnoreFilebuf filebuf{};
    alignas(8) uint8_t iosPart[0x50] = {};
    uint64_t text = 0;
    IgnorePathString filePath{};
    char buffer[260];
    char16_t line[32];

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
        IgnoreWString first, name, second;
        online_callee::WStringCtorFrom(&first, line);
        online_callee::WStringCopy(&name, &first, 0, 0xffffffffu);
        online_callee::WStringDtor(&first, -1);
        IgnoreOn(*reinterpret_cast<std::u16string*>(&name));
        online_callee::WStringCtorFrom(&second, line);
        ChatIgnore(Word(this, 0), &second, this);
        online_callee::WStringDtor(&second, -1);
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
