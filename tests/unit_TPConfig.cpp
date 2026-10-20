// The fakes for TPConfig.cpp, the quickload indexes (Quickload.cpp) and BuildGeometryTables
// (rsse.cpp) (libtpw_unit_TPConfig.dylib; see calllog.h) and the test entry points. Each
// fake records the event its PowerPC twin in test_TPConfig.py records. They are functional
// where a value flows back: strings, heap blocks, the files TPFile_* serves, the text
// reader (SkipWhitespace, IsWhiteSpace, GetString) and _stricmp.
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "CBalanceTable.h"
#include "Debug.h"
#include "FS_System.h"
#include "Game.h"
#include "Quickload.h"
#include "ResolutionBalance.h"
#include "TPAdvisor.h"
#include "TPConfig.h"
#include "TPDialog.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "TPMain.h"
#include "TPSettings.h"
#include "TbDynamicStringTemplate.h"
#include "calllog.h"
#include "platform/macdoze.h"
#include "rsse.h"
#include "ui_ErrorDisplay.h"

using calllog::ev;
using calllog::lab;
using calllog::newlab;
using calllog::ret;

namespace {

char* dup(const char* text) {
    const std::size_t n = std::strlen(text) + 1;
    char* out = static_cast<char*>(std::malloc(n));
    std::memcpy(out, text, n);
    return out;
}

// The files TPFile_Open serves (test_TPConfig.py FILES).
struct FakeFile {
    std::string name;
    std::string data;
    bool used = false;
};
FakeFile gFiles[8];
char gFileHandles[8][16];

int32_t FileIndex(const bfl::FileSystem::CFile* file) {
    for (int32_t k = 0; k < 8; ++k)
        if (reinterpret_cast<const char*>(file) == gFileHandles[k]) return k;
    return -1;
}

// The text reader's character classes (signed characters, as the original reads them).
bool IsLetter(signed char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); }
bool IsDigit(signed char c) { return c >= '0' && c <= '9'; }
bool IsPunctuation(signed char c) {
    return (c >= 0x21 && c <= 0x2f) || (c >= 0x3a && c <= 0x40) || (c >= 0x5b && c <= 0x60) || (c >= 0x7b && c <= 0x7e);
}
bool IsComment(signed char c) { return c == '#' || c == ';'; }
bool IsTokenChar(signed char c) { return IsLetter(c) || IsDigit(c) || IsPunctuation(c); }

void Advance(sFileParse* parse) {
    ++parse->mCursor;
    ++parse->mPos;
}

}  // namespace

// ------------------------------------------------------------------ bullfrog strings (functional)

template<> TbDynamicStringTemplate<char>::TbDynamicStringTemplate()
    : mGrowBy(0) {
    mCapacity = 0;
    mData = dup("");
    mLength = 0;}
template<> TbDynamicStringTemplate<char>::TbDynamicStringTemplate(const char* text)
    : mGrowBy(0) {
    mCapacity = 0;
    mData = dup(text);
    mLength = static_cast<int32_t>(std::strlen(text));}
template<> TbDynamicStringTemplate<char>::~TbDynamicStringTemplate() {}
template<> TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator=(
    const TbDynamicStringTemplate<char>& other) {
    mData = dup(other.mData);
    mLength = other.mLength;
    return *this;
}
template<> TbDynamicStringTemplate<char16_t>::TbDynamicStringTemplate()
    : mGrowBy(0) {
    mCapacity = 0;
    mData = nullptr;
    mLength = 0;}
template<> TbDynamicStringTemplate<char16_t>::~TbDynamicStringTemplate() {}
template<> int32_t TbStringBase<char>::Length() const {
    const auto& text = static_cast<const TbDynamicStringTemplate<char>&>(*this);
    ev("TbStringBase::Length", {lab(&text)}, {text.mData});
    return text.mLength;
}

// ------------------------------------------------------------------ globals of other files

uint32_t gTPWFlags;
uint32_t gGameOptions;
uint32_t gTPDialogOptions;
uint32_t gTPDialogTextureMegabytes;
int32_t gScreenWidth;
int32_t gScreenHeight;
float gViewCentreX;
float gViewCentreY;
float gScreenCentreX;
float gScreenCentreY;
CPlayerSettings gPlayerSettings;
// TPSettings.cpp's own members this unit does not test (unrecorded).
void CPlayerSettings::Init() {}
CPlayerSettings::~CPlayerSettings() {}
CResolutionBalance gResolutionBalance;
// The unit starts from a blank object, not the game's default names.
TPGlobals::TPGlobals() {}
TPGlobals::~TPGlobals() {}
TPGlobals gTPGlobals;
char gThemeTextureFolder[16];

void CBalanceTable::Init() {}
CResolutionBalance::~CResolutionBalance() {}
const BalanceDescription* CResolutionBalance::GetBalanceDescription(int32_t) { return nullptr; }
BalanceValue* CResolutionBalance::GetBalanceValuesStart() { return mValues; }

// ------------------------------------------------------------------ callees

void* MReleaseAllocF(uint32_t size, uint32_t flags) {
    ev("MReleaseAllocF", {size, flags});
    const bool ok = ret("MReleaseAllocF", 1) != 0;
    if (size == 0 || !ok) return nullptr;
    void* block = std::calloc(1, size);
    newlab(block);
    return block;
}
void* MReleaseFreeF(void* block) {
    ev("MReleaseFreeF", {lab(block)});
    return nullptr;
}
void ResetWorldBoundingArea() { ev("ResetWorldBoundingArea"); }
void ConverttoCurrentTheme(int32_t theme) { ev("ConverttoCurrentTheme", {theme}); }
int32_t GetFileAccessIndex() {
    ev("GetFileAccessIndex");
    return static_cast<int32_t>(ret("GetFileAccessIndex"));
}
void SetFileAccessIndex(int32_t index) { ev("SetFileAccessIndex", {index}); }
int32_t CheckFileExists(char* name) {
    ev("CheckFileExists", {}, {name});
    return static_cast<int32_t>(ret("CheckFileExists"));
}
void SetNewMeshPaths(char* root, char* folder, char* textureFolder, char* sharedTextureFolder) {
    ev("SetNewMeshPaths", {lab(root), lab(folder), lab(sharedTextureFolder)}, {root, folder, textureFolder});
}
void CTRACE(uint32_t level, const char* format, ...) {
    char text[1024];
    va_list args;
    va_start(args, format);
    std::vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    ev("CTRACE", {level}, {text});
}
int32_t NS_MacDoze::_stricmp(const char* a, const char* b) {
    ev("_stricmp", {}, {a, b});
    for (;; ++a, ++b) {
        const int ca = (*a >= 'A' && *a <= 'Z') ? *a + 32 : static_cast<unsigned char>(*a);
        const int cb = (*b >= 'A' && *b <= 'Z') ? *b + 32 : static_cast<unsigned char>(*b);
        if (ca != cb) return ca < cb ? -1 : 1;
        if (ca == 0) return 0;
    }
}

bfl::FileSystem::CFile* TPFile_Open(const char* name, const char* mode) {
    ev("TPFile_Open", {}, {name, mode});
    for (int32_t k = 0; k < 8; ++k)
        if (gFiles[k].used && gFiles[k].name == name) return reinterpret_cast<bfl::FileSystem::CFile*>(gFileHandles[k]);
    return nullptr;
}
int32_t TPFile_Size(bfl::FileSystem::CFile* file) {
    ev("TPFile_Size", {lab(file)});
    const int32_t k = FileIndex(file);
    return k >= 0 ? static_cast<int32_t>(gFiles[k].data.size()) : 0;
}
uint32_t TPFile_Read(void* dest, uint32_t size, uint32_t count, bfl::FileSystem::CFile* file) {
    ev("TPFile_Read", {lab(dest), size, count, lab(file)});
    const int32_t k = FileIndex(file);
    if (k < 0) return 0;
    const std::size_t want = static_cast<std::size_t>(size) * count;
    std::memcpy(dest, gFiles[k].data.data(), want < gFiles[k].data.size() ? want : gFiles[k].data.size());
    return count;
}
uint32_t TPFile_Close(bfl::FileSystem::CFile* file) {
    ev("TPFile_Close", {lab(file)});
    return 0;
}

// The text reader: the same rules as test_TPConfig.py's twins (close to the originals', but
// GetString never reads past the end).
int32_t SkipWhitespace(sFileParse* parse) {
    ev("SkipWhitespace", {parse->mPos});
    for (;;) {
        if (parse->mPos >= parse->mSize) return 1;
        if (IsTokenChar(static_cast<signed char>(parse->mCursor[0]))) return 0;
        Advance(parse);
    }
}
int32_t IsWhiteSpace(sFileParse* parse) {
    ev("IsWhiteSpace", {parse->mPos});
    if (ret("IsWhiteSpace") != 0) return 1;
    return IsTokenChar(static_cast<signed char>(parse->mCursor[0])) ? 0 : 1;
}
int32_t GetString(sFileParse* parse, char* text, int32_t size, int32_t letters, int32_t digits, int32_t punctuation,
                  int32_t comments) {
    ev("GetString", {parse->mPos, size, letters, digits, punctuation, comments});
    int32_t n = 0;
    text[0] = '\0';
    if (ret("GetString") != 0) return 0;   // scripted: an empty name, nothing read
    if (parse->mPos >= parse->mSize) return 1;
    while (n < size - 1) {
        if (parse->mPos >= parse->mSize) return 1;
        const signed char c = static_cast<signed char>(parse->mCursor[0]);
        const bool accept = (letters != 0 && IsLetter(c)) || (digits != 0 && IsDigit(c)) ||
                            (punctuation != 0 && IsPunctuation(c)) || (comments != 0 && IsComment(c));
        if (!accept) return 0;
        text[n++] = static_cast<char>(c);
        text[n] = '\0';
        Advance(parse);
    }
    return 0;
}

// ------------------------------------------------------------------ test entry points

extern "C" {

void* tpw_cfg_addr(const char* name) {
    struct Entry {
        const char* name;
        void* address;
    };
    static const Entry kEntries[] = {
        {"gTPWFlags", &gTPWFlags}, {"gGameOptions", &gGameOptions}, {"gTPDialogOptions", &gTPDialogOptions},
        {"gTPDialogTextureMegabytes", &gTPDialogTextureMegabytes}, {"gScreenWidth", &gScreenWidth},
        {"gScreenHeight", &gScreenHeight}, {"gViewCentreX", &gViewCentreX}, {"gViewCentreY", &gViewCentreY},
        {"gScreenCentreX", &gScreenCentreX}, {"gScreenCentreY", &gScreenCentreY},
        {"gPlayerSettings", &gPlayerSettings}, {"gResolutionBalance", &gResolutionBalance},
        {"gTPGlobals", &gTPGlobals}, {"gThemeTextureFolder", gThemeTextureFolder},
        {"gFilePathBuffer", gFilePathBuffer}, {"gPathString", &gPathString}, {"gPaths", &gPaths},
        {"gPathLength", &gPathLength}, {"gQuickloadTextureIndexes", &gQuickloadTextureIndexes},
        {"gQuickloadIndexes", &gQuickloadIndexes}, {"gQuickloadInitialised", &gQuickloadInitialised},
        {"gQuickloadEnabled", &gQuickloadEnabled}, {"gQuickloadPathCount", &gQuickloadPathCount},
    };
    for (const Entry& entry : kEntries)
        if (std::strcmp(entry.name, name) == 0) return entry.address;
    return nullptr;
}

int32_t tpw_cfg_index_size() { return static_cast<int32_t>(sizeof(sQuickloadIndex)); }
void* tpw_cfg_new(int32_t size) { return std::calloc(1, static_cast<std::size_t>(size)); }

// The settings fields the unit reads, in test_TPConfig.py SETTINGS order.
void tpw_cfg_set_settings(const int32_t* v) {
    gPlayerSettings.mCurrent.mResolution = CPlayerSettings::eRESOLUTION(v[0]);
    gPlayerSettings.mGameOptions = static_cast<uint32_t>(v[1]);
    gPlayerSettings.mExtraEngineOptions = static_cast<uint32_t>(v[2]);
    gPlayerSettings.mCurrentTheme = v[3];
    gPlayerSettings.mTextureCacheSize = static_cast<uint32_t>(v[4]);
    gPlayerSettings.mCustomOptions = v[5];
}
void tpw_cfg_set_resolution(int32_t value) { gResolutionBalance.mValues[1].mInteger = value; }
void tpw_cfg_set_search_path(const char* text) { gTPGlobals.mSearchPath = TbDynamicStringTemplate<char>(text); }
void* tpw_cfg_search_path() { return &gTPGlobals.mSearchPath; }
void* tpw_cfg_sin_header() { return &gTPGlobals.mSinTableSize; }
void* tpw_cfg_sin_table() { return gTPGlobals.mSinTable; }

void tpw_cfg_clear_files() {
    for (FakeFile& file : gFiles) file = FakeFile();
}
void tpw_cfg_set_file(int32_t k, const char* name, const char* data, int32_t size) {
    gFiles[k].name = name;
    gFiles[k].data.assign(data, static_cast<std::size_t>(size));
    gFiles[k].used = true;
}
void* tpw_cfg_file_handle(int32_t k) { return gFileHandles[k]; }

// An index entry holding `count` names, packed (NUL-terminated) in `packed` (`bytes`
// long); blocks[0], blocks[1] receive the names array and the text block (NULL for none).
void tpw_cfg_fill_index(sQuickloadIndex* entry, const char* packed, int32_t bytes, int32_t count, void** blocks) {
    entry->mCount = static_cast<uint32_t>(count);
    entry->mText = nullptr;
    entry->mNames = nullptr;
    if (count > 0) {
        entry->mText = static_cast<char*>(std::malloc(static_cast<std::size_t>(bytes)));
        std::memcpy(entry->mText, packed, static_cast<std::size_t>(bytes));
        entry->mNames = static_cast<char**>(std::malloc(sizeof(char*) * static_cast<std::size_t>(count)));
        char* p = entry->mText;
        for (int32_t i = 0; i < count; ++i) {
            entry->mNames[i] = p;
            p += std::strlen(p) + 1;
        }
    }
    blocks[0] = entry->mNames;
    blocks[1] = entry->mText;
}

void tpw_cfg_sinit_texts(char* out) {
    for (int32_t i = 0; i < 16; ++i) std::strncpy(out + 96 * i, gTPConfigLanguages[i].mData, 95);
    std::strncpy(out + 96 * 16, gTPConfigTitle.mData, 95);
    std::strncpy(out + 96 * 17, gTPConfigLanguage.mData, 95);
}

int32_t tpw_cfg_call(const char* name, const int64_t* a) {
    const std::string n(name);
    auto text = [&](int k) { return reinterpret_cast<char*>(a[k]); };
    if (n == "InitialiseGameStage1") InitialiseGameStage1();
    else if (n == "InitialiseGameStage2") InitialiseGameStage2();
    else if (n == "InitialiseGameStage3") InitialiseGameStage3();
    else if (n == "ClosedownGame") ClosedownGame();
    else if (n == "DoesFileExist") return DoesFileExist(text(0), static_cast<uint32_t>(a[1]));
    else if (n == "FindFilePath")
        return lab(FindFilePath(text(0), static_cast<uint32_t>(a[1]), reinterpret_cast<int32_t*>(a[2])));
    else if (n == "TPSaveRegistry") TPSaveRegistry();
    else if (n == "BuildGeometryTables") BuildGeometryTables(static_cast<uint32_t>(a[0]));
    else if (n == "InitialiseQuickloading") InitialiseQuickloading();
    else if (n == "LoadQuickloadIndex")
        LoadQuickloadIndex(reinterpret_cast<sQuickloadIndex*>(a[0]), reinterpret_cast<void*>(a[1]),
                           static_cast<uint32_t>(a[2]));
    else if (n == "LoadQuickloadIndexes") LoadQuickloadIndexes(text(0), static_cast<int32_t>(a[1]));
    else if (n == "FreeQuickloadIndexes") FreeQuickloadIndexes();
    else if (n == "CloseQuickloading") CloseQuickloading();
    else if (n == "DoesQuickloadFilenameExist")
        return DoesQuickloadFilenameExist(text(0), static_cast<int32_t>(a[1]), static_cast<int32_t>(a[2]));
    else if (n == "FindQuickloadFilename") return FindQuickloadFilename(text(0), static_cast<int32_t>(a[1]));
    else if (n == "CheckIndexedFileExists")
        return CheckIndexedFileExists(text(0), static_cast<int32_t>(a[1]), static_cast<int32_t>(a[2]));
    else if (n == "SetupDirectoryForMeshLoad") return SetupDirectoryForMeshLoad(text(0), text(1), text(2));
    else return -12345;
    return 0;
}

}  // extern "C"

#include "fake_strings.h"
