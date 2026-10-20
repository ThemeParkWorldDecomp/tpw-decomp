// Test library for the hoardings balance file writers and the static objects of LLCellSystem.cpp (unit w208; libtpw_unit_HoardingWrite.dylib,
// tests/test_HoardingWrite.py), main app:
//   src/LLCellSystem_hoardwrite.cpp   LLCC_WriteHoardingsLine, LLCC_WriteHoardingsToBalanceFile, LLCC_WriteCompleteHoardingsToBalanceFile, and
//                                     the definitions that stand for __sinit_LLCellSystem_cpp (the file, seven strings, nine offsets, two words)
//
// The callees outside the unit are recording fakes with the same event names and arguments as the module's PowerPC stubs: the string members,
// get_theme_path, FS_AddDirectorySeparator, CTRACE, the file's Open and Close, the click stack builder, _itoa and the access object's Write. The
// globals of other files the unit reads (the click stack and its copy, gLLCC4818, the hoarding extents, gTPGlobals) are defined here, zero at start
// like the Mac's bss. The library's own statics are built at load, before calllog is ready, so the fakes record only once the first case has begun.
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "calllog.h"
#include "platform/arena.h"
#include "Debug.h"
#include "FS_System.h"
#include "LLCellSystem.h"
#include "Main.h"
#include "TPGlobals.h"
#include "TbDynamicStringTemplate.h"
#include "TbError.h"
#include "bfl_FileSystem.h"

using namespace calllog;

namespace {

// The fakes record only after the first case has begun (see the top), and not in a quiet case (a walk of 0x400 clicks makes more events than
// the log holds, so the case compares the state alone).
bool gRecording = false;
bool gQuiet = false;
bool Rec() { return gRecording && !gQuiet; }

char* dup(const char* text) {
    const std::size_t n = std::strlen(text) + 1;
    char* out = static_cast<char*>(std::malloc(n));
    std::memcpy(out, text, n);
    return out;
}

// A string's text set directly, not through the recording members (the test's way to give the unit's strings their text).
void SetString(TbDynamicStringTemplate<char>& string, const char* text) {
    string.mData = dup(text);
    string.mLength = static_cast<int32_t>(std::strlen(text));
}

}  // namespace

// ------------------------------------------------------------------ bullfrog strings (functional, recorded)

template<> TbDynamicStringTemplate<char>::TbDynamicStringTemplate()
    : mGrowBy(0) {
    mCapacity = 0;
    mData = dup("");
    mLength = 0;
    if (Rec()) ev("TbString()", {newlab(this)});
}
template<> TbDynamicStringTemplate<char>::TbDynamicStringTemplate(const char* text)
    : mGrowBy(0) {
    mCapacity = 0;
    mData = dup(text);
    mLength = static_cast<int32_t>(std::strlen(text));
    if (Rec()) ev("TbString(char*)", {newlab(this)}, {text});
}
template<> TbDynamicStringTemplate<char>::~TbDynamicStringTemplate() {
    if (Rec()) ev("~TbString", {lab(this)});
}
template<> TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator=(const char* text) {
    if (Rec()) ev("TbString=", {lab(this)}, {text});
    SetString(*this, text);
    return *this;
}
template<> TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator+=(const TbStringBase<char>& text) {
    const auto& other = static_cast<const TbDynamicStringTemplate<char>&>(text);
    if (Rec()) ev("TbString+=", {lab(this)}, {other.mData});
    const std::string joined = std::string(mData) + other.mData;
    SetString(*this, joined.c_str());
    return *this;
}

// The string members the unit never calls: the vtables of both instances need a definition each (unrecorded). The wide instance is only
// constructed and destroyed, inside TPGlobals.
template<class Char> void TbDynamicStringTemplate<Char>::Format(const Char*, ...) {}
template<class Char> void TbDynamicStringTemplate<Char>::ResizeTo(uint32_t) {}
template<class Char> void TbDynamicStringTemplate<Char>::AllocBuffer(uint32_t) {}
template<class Char> TbDynamicStringTemplate<Char>& TbDynamicStringTemplate<Char>::operator=(const TbStringBase<Char>&) { return *this; }
template<class Char> TbDynamicStringTemplate<Char>& TbDynamicStringTemplate<Char>::operator=(const TbDynamicStringTemplate&) { return *this; }
template<class Char> TbDynamicStringTemplate<Char>& TbDynamicStringTemplate<Char>::operator=(const Char&) { return *this; }
template<class Char> TbDynamicStringTemplate<Char>& TbDynamicStringTemplate<Char>::operator+=(const Char&) { return *this; }
template<class Char> TbDynamicStringTemplate<Char>& TbDynamicStringTemplate<Char>::operator+=(const Char*) { return *this; }
template<> TbDynamicStringTemplate<char16_t>::TbDynamicStringTemplate()
    : mGrowBy(0) {
    mCapacity = 0;
    mData = nullptr;
    mLength = 0;
}
template<> TbDynamicStringTemplate<char16_t>::~TbDynamicStringTemplate() {}
template<> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator=(const char16_t*) { return *this; }
template<> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator+=(const TbStringBase<char16_t>&) { return *this; }

// ------------------------------------------------------------------ globals of other files (the program's definitions are in files this
// library does not compile)

int32_t gLLCCClickStackCount;                // 0x102e47c0
LLCCClick gLLCCClickStack[0x400];            // 0x102e27c0
LLCCClick gLLCCClickStackCopy[0x400];        // 0x102e07b8
int32_t gLLCCClickCopyIndex;                 // 0x102e27b8
int32_t gLLCCClickCopyCount;                 // 0x102e27bc
LLCCClick gLLCCBacktrackClick;               // 0x102e07b0
uint8_t gLLCC4818;                           // 0x102e4818
int32_t gHoardingRight;                      // 0x1029e03c
int32_t gHoardingBottom;                     // 0x1029e040
// The unit starts from a blank object, not the game's default names; the test gives the two file names it reads.
TPGlobals::TPGlobals() {}
TPGlobals::~TPGlobals() {}
TPGlobals gTPGlobals;                        // 0x103b74c8

namespace {

// What the test arranges before a call: the answer of get_theme_path, the key the line function is handed, the clicks and the count the
// click stack builder leaves behind.
char gThemePath[200];
TbDynamicStringTemplate<char> gTestKey;
LLCCClick gPlan[0x400];
int32_t gPlanned;
int32_t gPlanCount;

// The access object behind the file: every Write records its size and bytes and answers from the queue (the writer ignores the answer).
class FakeAccess : public bfl::FileSystem::CAccessItem {
public:
    int32_t Write(const void* source, uint32_t size) override {
        const std::string text(static_cast<const char*>(source), size < 159 ? size : 159);
        if (Rec()) ev("Access::Write", {size}, {text.c_str()});
        return static_cast<int32_t>(ret("Access::Write", 0));
    }
};
FakeAccess gAccess;

}  // namespace

// ------------------------------------------------------------------ the callees

// 0x1015139c: the case's text.
char* get_theme_path() {
    if (Rec()) ev("get_theme_path");
    return gThemePath;
}
// 0x10004bc0: records the path it was handed, then ends it with ":" (set directly, so no string member records).
void FS_AddDirectorySeparator(TbDynamicStringTemplate<char>& path) {
    if (Rec()) ev("FS_AddDirectorySeparator", {}, {path.mData});
    const std::string joined = std::string(path.mData) + ":";
    SetString(path, joined.c_str());
}
// 0x1006fcbc: the level and the format (the writers pass no argument after it).
void CTRACE(uint32_t level, const char* format, ...) {
    if (Rec()) ev("CTRACE", {level}, {format});
}
bfl::FileSystem::CDiscFile::CDiscFile() {
    if (Rec()) ev("CDiscFile", {newlab(this)});
}
bfl::FileSystem::CDiscFile::~CDiscFile() {
    if (Rec()) ev("~CDiscFile", {lab(this)});
}
void bfl::FileSystem::CFile::Close() {
    if (Rec()) ev("CFile::Close", {lab(this)});
}
// 0x102396a0: the file, the mode and the name; the answer is the queue's (a negative one is a failure).
TbError bfl::FileSystem::CDiscFile::Open(const char* name, uint32_t mode) {
    if (Rec()) ev("CDiscFile::Open", {lab(this), mode}, {name});
    return TbError(static_cast<int32_t>(ret("CDiscFile::Open", -1)));
}
// 0x100936f4: records the byte gLLCC4818 holds as it runs, then leaves the click stack the case planned (the clicks, then the count).
void LLCC_SystemConstructHoardingClickStack() {
    if (Rec()) ev("ConstructHoardingClickStack", {gLLCC4818});
    for (int32_t k = 0; k < gPlanned; ++k) gLLCCClickStack[k] = gPlan[k];
    gLLCCClickStackCount = gPlanCount;
}
// 0x1023b6e0: the value and the radix, then the signed decimal digits in the buffer.
char* NS_MacDoze::_itoa(int32_t value, char* buffer, int32_t radix) {
    if (Rec()) ev("_itoa", {value, radix});
    std::snprintf(buffer, 40, "%d", value);
    return buffer;
}

// ------------------------------------------------------------------ the file layer's members this unit does not call (unrecorded)

namespace bfl {
namespace FileSystem {

CAccessItem::CAccessItem() : mNext(nullptr), mReferences(0) {}
CAccessItem::~CAccessItem() {}
int32_t CAccessItem::Read(void*, uint32_t) { return -1; }
int32_t CAccessItem::Write(const void*, uint32_t) { return -1; }
int32_t CAccessItem::Seek(int32_t, uint32_t) { return -1; }
int32_t CAccessItem::GetSize() { return -1; }
int32_t CAccessItem::GetPos() { return -1; }
const uint8_t* CAccessItem::MapFile(int32_t, int32_t) { return nullptr; }
void CAccessItem::UnmapFile(const uint8_t*) {}
TbError CAccessItem::Flush() { return TbError(-1); }
int32_t CAccessItem::GetMacFileReference() { return -1; }
void CAccessItem::Close() {}
void CAccessItem::AttachAccessClass(CAccessItem*) {}
_LbSemaphore* CAccessItem::GetSemaphore() { return nullptr; }

CFile::CFile() : mImplementation(nullptr) {}
CFile::~CFile() {}
int32_t CFile::Seek(uint32_t, uint32_t) { return 0; }
int32_t CFile::GetSize() { return 0; }
int32_t CFile::GetPos() { return 0; }
void* CFile::MapFile() { return nullptr; }
TbError CFile::Flush() { return TbError(0); }
int32_t CFile::GetMacFileReference() { return 0; }

}  // namespace FileSystem
}  // namespace bfl

extern "C" {

// The host address of a global by the module's name for it (ADDR's keys).
uint64_t tpw_hwr_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"file", &gLLCCHoardingsFile},
        {"right", &gHoardingRight},
        {"bottom", &gHoardingBottom},
        {"temp", &gLLCCBacktrackClick},
        {"copy", gLLCCClickStackCopy},
        {"copy_index", &gLLCCClickCopyIndex},
        {"copy_count", &gLLCCClickCopyCount},
        {"stack", gLLCCClickStack},
        {"stack_count", &gLLCCClickStackCount},
        {"g4818", &gLLCC4818},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// A zeroed block the module fills with Mac-offset words (32-bit addressable, like every Ptr32 the headers hold).
uint64_t tpw_hwr_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The offsets and sizes the module relies on (HOST_LAYOUT): a click is two words, and the stack is 0x400 of them.
int32_t tpw_hwr_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(LLCCClick));
    out[n++] = static_cast<int64_t>(offsetof(LLCCClick, mY));
    out[n++] = static_cast<int64_t>(sizeof(gLLCCClickStack));
    return n;
}

// Every case starts here: the globals the unit touches back to zero, the strings back to the texts the Mac's initialiser gives them, no access
// object, no planned clicks. Recording starts with the first call, and stops again before the statics are destroyed at exit.
void tpw_hwr_reset() {
    static bool registered = false;
    if (!registered) {
        registered = true;
        std::atexit([] { gRecording = false; });
    }
    gRecording = true;
    gQuiet = false;
    gLLCCClickStackCount = 0;
    std::memset(gLLCCClickStack, 0, sizeof(gLLCCClickStack));
    std::memset(gLLCCClickStackCopy, 0, sizeof(gLLCCClickStackCopy));
    gLLCCClickCopyIndex = 0;
    gLLCCClickCopyCount = 0;
    gLLCCBacktrackClick.mX = 0;
    gLLCCBacktrackClick.mY = 0;
    gLLCC4818 = 0;
    gHoardingRight = 0;
    gHoardingBottom = 0;
    gLLCCHoardingsFile.mImplementation = nullptr;
    SetString(gLLCCHoardingKeyPrefix, "HoardingClicks[");
    SetString(gLLCCHoardingKeyX, "].X");
    SetString(gLLCCHoardingKeyY, "].Y");
    SetString(gLLCCHoardingEOL, "\n");
    SetString(gLLCCHoardingSep, "\t");
    SetString(gTPGlobals.mF40EC, "hoardings.sam");
    SetString(gTPGlobals.mF4100, "endhoardings.sam");
    SetString(gTestKey, "");
    gThemePath[0] = '\0';
    gPlanned = 0;
    gPlanCount = 0;
}

// A string of the case: 0 prefix, 1 key X, 2 key Y, 3 line end, 4 separator, 5 and 6 the two file names of gTPGlobals, 7 the theme path
// get_theme_path answers, 8 the key the line function is handed.
void tpw_hwr_set_text(int32_t which, const char* text) {
    switch (which) {
        case 0: SetString(gLLCCHoardingKeyPrefix, text); break;
        case 1: SetString(gLLCCHoardingKeyX, text); break;
        case 2: SetString(gLLCCHoardingKeyY, text); break;
        case 3: SetString(gLLCCHoardingEOL, text); break;
        case 4: SetString(gLLCCHoardingSep, text); break;
        case 5: SetString(gTPGlobals.mF40EC, text); break;
        case 6: SetString(gTPGlobals.mF4100, text); break;
        case 7: std::strncpy(gThemePath, text, sizeof(gThemePath) - 1); break;
        case 8: SetString(gTestKey, text); break;
        default: break;
    }
}

// A quiet case records nothing (the module's PowerPC side records nothing either).
void tpw_hwr_quiet(int32_t on) { gQuiet = on != 0; }

// The file's access object: the fake one, or none (a file that failed to open).
void tpw_hwr_set_access(int32_t on) { gLLCCHoardingsFile.mImplementation = on != 0 ? &gAccess : nullptr; }

// What the click stack builder will leave behind: `clicks` clicks (x then y words) and the count word `count`.
void tpw_hwr_plan(int32_t count, int32_t clicks, const int32_t* words) {
    gPlanCount = count;
    gPlanned = clicks;
    for (int32_t k = 0; k < clicks; ++k) {
        gPlan[k].mX = words[2 * k];
        gPlan[k].mY = words[2 * k + 1];
    }
}

// The state the Mac's initialiser leaves: the seven strings joined with "|" (each followed by one), the nine offsets (x then y) and the two words.
void tpw_hwr_sinit_state(char* texts, int32_t* words) {
    const TbDynamicStringTemplate<char>* strings[] = {
        &gLLCCHoardingFileHeader, &gLLCCHoardingFileNote, &gLLCCHoardingKeyPrefix, &gLLCCHoardingKeyX,
        &gLLCCHoardingKeyY,       &gLLCCHoardingEOL,      &gLLCCHoardingSep,
    };
    std::string joined;
    for (const TbDynamicStringTemplate<char>* string : strings) {
        joined += string->mData;
        joined += '|';
    }
    std::memcpy(texts, joined.c_str(), joined.size() + 1);
    int32_t n = 0;
    for (const MapDelta& offset : gLLCCPointedOffsets) {
        words[n++] = offset.mX;
        words[n++] = offset.mY;
    }
    words[n++] = gLLCC481C;
    words[n++] = gLLCC4820;
}

// ---- the functions, one wrapper each with C types only (the line function gets the key the case set)
uint32_t tpw_hwr_line(int32_t index, int32_t value) { return static_cast<uint32_t>(LLCC_WriteHoardingsLine(gTestKey, index, value)); }
uint32_t tpw_hwr_write() { return static_cast<uint32_t>(LLCC_WriteHoardingsToBalanceFile()); }
uint32_t tpw_hwr_write_complete() { return static_cast<uint32_t>(LLCC_WriteCompleteHoardingsToBalanceFile()); }

}  // extern "C"
