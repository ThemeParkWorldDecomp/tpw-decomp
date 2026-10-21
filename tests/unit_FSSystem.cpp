// The fakes for FS_Registry.cpp, FS_System.cpp, the string helpers and FS_UserInfo.cpp
// (src/game/save/FS_Registry.cpp, src/game/save/FS_System.cpp, src/game/ui/3DFont.cpp, src/game/save/FS_UserInfo.cpp;
// libtpw_unit_FSSystem.dylib, used by test_FSSystem.py and test_FSUserInfo.py; see
// calllog.h) and the test entry points. The bullfrog strings are functional and unrecorded, as on the
// PowerPC side (test_FSSystem.py); the file system, the resources and the Toolbox and
// macdoze calls record events and are functional where values flow back (directory
// listings, file names, Pascal strings, code page conversions, heap blocks).
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include <new>

#include "FS_MasterSave.h"
#include "FS_Registry.h"
#include "FS_System.h"
#include "FS_UserInfo.h"
#include "Game.h"
#include "InterfaceFontWrapper.h"
#include "LbFile.h"
#include "TPDialog.h"
#include "TPGlobals.h"
#include "TPResource.h"
#include "TPString.h"
#include "TbFontRenderParams.h"
#include "TbTextResourceManager.h"
#include "Text.h"
#include "calllog.h"
#include "platform/interfacelib.h"
#include "platform/macdoze.h"
#include "platform/samsutils.h"

using calllog::ev;
using calllog::lab;
using calllog::newlab;
using calllog::ret;

// ------------------------------------------------------------------ bullfrog strings (functional)

#include "fake_strings.h"
#define TPW_RECORDED_ACCESS_IO
#include "fake_file_slots.h"

// ------------------------------------------------------------------ globals of other files

// The game object here is a blank one: the unit sets the fields its code reads.
CGame::CGame() {}
CGame::~CGame() {}
CGame gGame;
// The unit starts from a blank object, not the game's default names.
TPGlobals::TPGlobals() {}
TPGlobals::~TPGlobals() {}
TPGlobals gTPGlobals;
CText gText;
uint32_t gTPDialogLanguageBits;
InterfaceFont::InterfaceFont() {}   // the fake leaves the object as it finds it
InterfaceFont::~InterfaceFont() {}
InterfaceFontWrapper::InterfaceFontWrapper(TbIFont* font) : mFont(font) {}
InterfaceFontWrapper::~InterfaceFontWrapper() {}
TbFontRenderParams::~TbFontRenderParams() {}
TbFontRenderParams::TbFontRenderParams() {}
TbTextResourceManager::TbTextResourceManager() {}
TbTextResourceManager::~TbTextResourceManager() {}

// ------------------------------------------------------------------ callees

namespace {

std::string gProjectDir;              // what LbFile_GetProjectDir returns
std::string gFoundName;               // the entry FindFirstFile finds
CTPBaseResource* gCurrentResource;    // pGetCurrentResource's result
bfl::FileSystem::CFileStorage* gLastStorage;   // pGetLastFoundFileStorage's result

// The directory tree the disc storages list: path -> entries (name, attributes).
std::map<std::string, std::vector<std::pair<std::string, uint32_t>>> gTree;
struct Listing {
    std::string path;
    std::size_t next = 0;
};
std::map<const void*, Listing> gListings;

void FillInfo(bfl::FileSystem::CFileInfo& info, const std::pair<std::string, uint32_t>& entry) {
    info.mAttributes = entry.second;
    std::strcpy(info.mName, entry.first.c_str());
}

std::string Pascal(const unsigned char* text) { return std::string(reinterpret_cast<const char*>(text + 1), text[0]); }

int32_t Lower(int32_t c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }

}  // namespace

void CText::SetLanguageId(uint32_t id) { ev("CText::SetLanguageId", {lab(this), id}); }
const char* LbFile_GetProjectDir() {
    ev("LbFile_GetProjectDir");
    return gProjectDir.c_str();
}
TbError LbFile_DeleteDirectory(const char* name) {
    ev("LbFile_DeleteDirectory", {}, {name});
    return TbError(static_cast<int32_t>(ret("LbFile_DeleteDirectory")));
}
TbError LbFile_CreateDirectory(const char* name) {
    ev("LbFile_CreateDirectory", {}, {name});
    return TbError(static_cast<int32_t>(ret("LbFile_CreateDirectory")));
}
void* LbMem_Alloc(uint32_t size) {
    ev("LbMem_Alloc", {size});
    if (ret("LbMem_Alloc", 1) == 0) return nullptr;
    void* block = std::calloc(1, size + 2);
    newlab(block);
    return block;
}
void LbMem_Free(void* block) { ev("LbMem_Free", {lab(block)}); }

void SamsUtilities::UString::c2pcpy(unsigned char* dest, const char* source) {
    const std::size_t n = std::min<std::size_t>(std::strlen(source), 255);
    dest[0] = static_cast<unsigned char>(n);
    std::memcpy(dest + 1, source, n);
}
bool EqualString(const unsigned char* a, const unsigned char* b, bool caseSensitive, bool diacSensitive) {
    ev("EqualString", {caseSensitive ? 1 : 0, diacSensitive ? 1 : 0}, {Pascal(a).c_str(), Pascal(b).c_str()});
    if (a[0] != b[0]) return false;
    for (int32_t i = 1; i <= a[0]; ++i) {
        const int32_t ca = caseSensitive ? a[i] : Lower(a[i]);
        const int32_t cb = caseSensitive ? b[i] : Lower(b[i]);
        if (ca != cb) return false;
    }
    return true;
}

NS_MacDoze::InternalHandle* NS_MacDoze::FindFirstFile(const char* pattern, _WIN32_FIND_DATAA* data) {
    ev("FindFirstFile", {}, {pattern});
    const int64_t handle = ret("FindFirstFile", 0x5000);
    if (handle != -1) {
        std::strncpy(data->cFileName, gFoundName.c_str(), sizeof(data->cFileName) - 1);
        data->cFileName[sizeof(data->cFileName) - 1] = 0;
    }
    return reinterpret_cast<InternalHandle*>(static_cast<intptr_t>(handle));
}
int32_t NS_MacDoze::FindClose(InternalHandle* handle) {
    ev("FindClose", {static_cast<int32_t>(reinterpret_cast<intptr_t>(handle))});
    return 1;
}
int32_t NS_MacDoze::_stricmp(const char* a, const char* b) {
    ev("_stricmp", {}, {a, b});
    for (;; ++a, ++b) {
        const int32_t ca = Lower(static_cast<unsigned char>(*a));
        const int32_t cb = Lower(static_cast<unsigned char>(*b));
        if (ca != cb) return ca < cb ? -1 : 1;
        if (ca == 0) return 0;
    }
}
int32_t NS_MacDoze::_wcsicmp(const char16_t* a, const char16_t* b) {
    ev("_wcsicmp", {}, {calllog::narrow(a).c_str(), calllog::narrow(b).c_str()});
    for (;; ++a, ++b) {
        const int32_t ca = Lower(*a);
        const int32_t cb = Lower(*b);
        if (ca != cb) return ca < cb ? -1 : 1;
        if (ca == 0) return 0;
    }
}
// Code page conversions: Latin-1 (characters above 0xff become '?'). A queue value
// replaces the result.
int32_t NS_MacDoze::WideCharToMultiByte(uint32_t codePage, uint32_t flags, const char16_t* text, int32_t length,
                                        char* out, int32_t size, const char* defaultChar, int32_t* usedDefault) {
    ev("WideCharToMultiByte", {codePage, flags, length, lab(out), size, lab(defaultChar), lab(usedDefault)},
       {calllog::narrow(text).c_str()});
    const int32_t needed = static_cast<int32_t>(TextLength(text)) + 1;
    if (out != nullptr)
        for (int32_t i = 0; i < needed && i < size; ++i) out[i] = text[i] > 0xff ? '?' : static_cast<char>(text[i]);
    const int32_t written = out == nullptr ? needed : std::min(needed, size);
    return static_cast<int32_t>(ret("WideCharToMultiByte", written));
}
int32_t NS_MacDoze::MultiByteToWideChar(uint32_t codePage, uint32_t flags, const char* text, int32_t length,
                                        char16_t* out, int32_t size) {
    ev("MultiByteToWideChar", {codePage, flags, length, lab(out), size}, {text});
    const int32_t needed = static_cast<int32_t>(std::strlen(text)) + 1;
    if (out != nullptr)
        for (int32_t i = 0; i < needed && i < size; ++i) out[i] = static_cast<unsigned char>(text[i]);
    const int32_t written = out == nullptr ? needed : std::min(needed, size);
    return static_cast<int32_t>(ret("MultiByteToWideChar", written));
}

CTPBaseResource* CTPResourceManager::pGetCurrentResource() const {
    ev("CTPResourceManager::pGetCurrentResource", {lab(this)});
    return gCurrentResource;
}
int32_t CTPResourceManager::OpenFile(bfl::FileSystem::CFile& file, const TbDynamicStringTemplate<char>& name) {
    ev("CTPResourceManager::OpenFile", {lab(this), lab(&file)}, {name.mData});
    return static_cast<int32_t>(ret("CTPResourceManager::OpenFile"));
}
bfl::FileSystem::CFileStorage* CTPBaseResource::pGetLastFoundFileStorage() const {
    ev("CTPBaseResource::pGetLastFoundFileStorage", {lab(this)});
    return gLastStorage;
}
int32_t CTPBaseResource::OpenFile(bfl::FileSystem::CFile& file, const TbDynamicStringTemplate<char>& name, uint32_t mode) {
    ev("CTPBaseResource::OpenFile", {lab(this), lab(&file), mode}, {name.mData});
    return static_cast<int32_t>(ret("CTPBaseResource::OpenFile"));
}
// The resource members this unit does not use (unrecorded), and a resource to construct.
CTPBaseResource::~CTPBaseResource() {}
int32_t CTPBaseResource::OpenFile(bfl::FileSystem::CFile&, const TbDynamicStringTemplate<char>&,
                                  bfl::FileSystem::CFileStorage&, uint32_t) {
    return 0;
}
CTPResourceManager::CTPResourceManager() : mResources(nullptr), mCurrentResource(nullptr), mSharedTextures(nullptr) {}
CTPResourceManager::~CTPResourceManager() {}
namespace {
class TestResource : public CTPBaseResource {
public:
    TestResource() : CTPBaseResource(TbDynamicStringTemplate<char>(), TbDynamicStringTemplate<char>()) {}
};
}  // namespace
// QualifyFilename: `out` = base + "|" + name (the queue gives the result).
int32_t QualifyFilename(TbDynamicStringTemplate<char>& name, TbDynamicStringTemplate<char>& base,
                        TbDynamicStringTemplate<char>& out) {
    ev("QualifyFilename", {lab(&name), lab(&base), lab(&out)}, {name.mData, base.mData});
    const std::string joined = std::string(base.mData) + "|" + name.mData;
    out = joined.c_str();
    return static_cast<int32_t>(ret("QualifyFilename", 1));
}

bfl::FileSystem::CFileStorage::CFileStorage() { ev("CFileStorage", {newlab(this)}); }
bfl::FileSystem::CFileStorage::~CFileStorage() { ev("~CFileStorage", {lab(this)}); }
TbError bfl::FileSystem::CFileStorage::OpenFile(CFile& file, const char* name, uint32_t mode, void* data) {
    ev("CFileStorage::OpenFile", {lab(this), lab(&file), mode, lab(data)}, {name});
    return TbError(static_cast<int32_t>(ret("CFileStorage::OpenFile")));
}
TbError bfl::FileSystem::CFileStorage::FullPath(TbDynamicStringTemplate<char>& path,
                                                     const TbDynamicStringTemplate<char>& name) {
    ev("CFileStorage::FullPath", {lab(this), lab(&path)}, {name.mData});
    const std::string full = std::string("full:") + name.mData;
    path = full.c_str();
    return TbError(static_cast<int32_t>(ret("CFileStorage::FullPath")));
}
TbError bfl::FileSystem::CDiscFileStorage::Open(const char* path) {
    ev("CDiscFileStorage::Open", {lab(this)}, {path});
    gListings[this] = Listing{path, 0};
    return TbError(gTree.count(path) != 0 ? 0 : -1);
}
TbError bfl::FileSystem::CFileStorage::FindFirst(const char* pattern, CFileInfo& info) {
    ev("CFileStorage::FindFirst", {lab(this)}, {pattern});
    Listing& listing = gListings[this];
    const auto& entries = gTree[listing.path];
    if (entries.empty()) return TbError(-1);
    FillInfo(info, entries[0]);
    listing.next = 1;
    return TbError(0);
}
TbError bfl::FileSystem::CFileStorage::FindNext(CFileInfo& info) {
    ev("CFileStorage::FindNext", {lab(this)});
    Listing& listing = gListings[this];
    const auto& entries = gTree[listing.path];
    if (listing.next >= entries.size()) return TbError(-1);
    FillInfo(info, entries[listing.next++]);
    return TbError(0);
}
TbError bfl::FileSystem::CFileStorage::Delete(const char* name) {
    ev("CFileStorage::Delete", {lab(this)}, {name});
    return TbError(0);
}
// The storage members this unit does not use (unrecorded).
void bfl::FileSystem::CFileStorage::Close() {}
TbError bfl::FileSystem::CFileStorage::FullPath(char*, const char*, uint32_t) { return TbError(0); }
const char* bfl::FileSystem::CFileStorage::GetRootPath() { return ""; }
TbError bfl::FileSystem::CFileStorage::SetAttribs(const char*, uint32_t) { return TbError(0); }
TbError bfl::FileSystem::CFileStorage::CreateDir(const char*) { return TbError(0); }
TbError bfl::FileSystem::CFileStorage::Move(const char*, const char*) { return TbError(0); }

bfl::FileSystem::CFile::CFile() : mImplementation(nullptr) { ev("CFile::CFile", {newlab(this)}); }
bfl::FileSystem::CFile::~CFile() { ev("CFile::~CFile", {lab(this)}); }
void bfl::FileSystem::CFile::Close() { ev("CFile::Close", {lab(this)}); }
int32_t bfl::FileSystem::CFile::Seek(uint32_t offset, uint32_t whence) {
    ev("CFile::Seek", {lab(this), offset, whence});
    return static_cast<int32_t>(ret("CFile::Seek"));
}
int32_t bfl::FileSystem::CFile::GetSize() {
    ev("CFile::Size", {lab(this)});
    return static_cast<int32_t>(ret("CFile::Size"));
}
bfl::FileSystem::CAccessItem::~CAccessItem() {}
int32_t bfl::FileSystem::CFileImplementation::Read(void* dest, uint32_t size) {
    ev("CFileImplementation::Read", {lab(this), lab(dest), size});
    return static_cast<int32_t>(ret("CFileImplementation::Read"));
}
int32_t bfl::FileSystem::CFileImplementation::Write(const void* source, uint32_t size) {
    ev("CFileImplementation::Write", {lab(this), lab(source), size});
    return static_cast<int32_t>(ret("CFileImplementation::Write"));
}

// ---- FS_UserInfo.cpp's callees

namespace {
TbDynamicStringTemplate<char> gThemeName;   // what CGame::GetCurrentThemeName returns
}

CFSGlobalMasterSave* gMasterSave;
CFSGlobalMasterSave::CFSGlobalMasterSave() { ev("CFSGlobalMasterSave", {newlab(this)}); }
CFSGlobalMasterSave::~CFSGlobalMasterSave() { ev("~CFSGlobalMasterSave", {lab(this)}); }
int32_t CFSGlobalMasterSave::Load(const char* path) {
    ev("CFSGlobalMasterSave::Load", {lab(this)}, {path});
    if (ret("Load clears") != 0) gMasterSave = nullptr;   // the caller then makes a new one
    return 1;
}
bool CFSGlobalMasterSave::IsEasyModeUser() const {
    ev("CFSGlobalMasterSave::IsEasyModeUser", {lab(this)});
    return ret("CFSGlobalMasterSave::IsEasyModeUser") != 0;
}
Outbox::Outbox() { ev("Outbox", {newlab(this)}); }
Outbox::~Outbox() { ev("~Outbox", {lab(this)}); }
AddressBook::AddressBook() { ev("AddressBook", {newlab(this)}); }
AddressBook::~AddressBook() { ev("~AddressBook", {lab(this)}); }
const TbDynamicStringTemplate<char>& CGame::GetCurrentThemeName() {
    ev("CGame::GetCurrentThemeName", {lab(this)});
    return gThemeName;
}
char16_t* NS_MacDoze::_itow(int32_t value, char16_t* text, int32_t radix) {
    ev("_itow", {value, radix});
    char digits[40];
    std::snprintf(digits, sizeof(digits), "%d", value);
    int32_t i = 0;
    for (; digits[i] != 0; ++i) text[i] = static_cast<unsigned char>(digits[i]);
    text[i] = 0;
    return text;
}

// ------------------------------------------------------------------ test entry points

extern "C" {

void* tpw_fs_addr(const char* name) {
    struct Entry {
        const char* name;
        void* address;
    };
    static const Entry kEntries[] = {
        {"gTPFileInfo", &gTPFileInfo}, {"gGame", &gGame}, {"gTPGlobals", &gTPGlobals}, {"gText", &gText},
        {"gLanguageName", gLanguageName}, {"gTPDialogLanguageBits", &gTPDialogLanguageBits},
        {"gFSRegistryLanguages", gFSRegistryLanguages}, {"gFSRegistryTitle", &gFSRegistryTitle},
        {"gFSRegistryLanguage", &gFSRegistryLanguage},
        {"mLocalPath", &gTPFileInfo.mLocalPath}, {"mRemotePath", &gTPFileInfo.mRemotePath},
        {"mDataPath", &gTPFileInfo.mDataPath}, {"mLevelPath", &gTPFileInfo.mLevelPath},
        {"mLanguagePath", &gTPFileInfo.mLanguagePath},
        {"mSeparator", &gTPGlobals.mSeparator}, {"mAllFiles", &gTPGlobals.mAllFiles},
        {"mWadExtension", &gTPGlobals.mWadExtension}, {"mDataFolder", &gTPGlobals.mDataFolder},
        {"mLanguageFolder", &gTPGlobals.mLanguageFolder}, {"mDefaultLanguagePath", &gTPGlobals.mDefaultLanguagePath},
    };
    for (const Entry& entry : kEntries)
        if (std::strcmp(entry.name, name) == 0) return entry.address;
    return nullptr;
}

// Strings: a new one (wide or not), setting and reading the text (UTF-16 as code units).
void* tpw_fs_new_string(int32_t wide) {
    if (wide != 0) return new TbDynamicStringTemplate<char16_t>();
    return new TbDynamicStringTemplate<char>();
}
void tpw_fs_set_string(void* s, int32_t wide, const char* text) {
    if (wide == 0) {
        *static_cast<TbDynamicStringTemplate<char>*>(s) = text;
        return;
    }
    std::u16string w;
    for (const char* p = text; *p != 0; ++p) w.push_back(static_cast<unsigned char>(*p));
    *static_cast<TbDynamicStringTemplate<char16_t>*>(s) = w.c_str();
}
// Copies the text (code units narrowed to bytes) and returns the stored length.
int32_t tpw_fs_get_string(const void* s, int32_t wide, char* out, int32_t size) {
    if (wide == 0) {
        const auto* t = static_cast<const TbDynamicStringTemplate<char>*>(s);
        std::strncpy(out, t->mData, static_cast<std::size_t>(size - 1));
        out[size - 1] = 0;
        return t->mLength;
    }
    const auto* t = static_cast<const TbDynamicStringTemplate<char16_t>*>(s);
    int32_t i = 0;
    for (; i < size - 1 && t->mData[i] != 0; ++i) out[i] = t->mData[i] > 0xff ? '?' : static_cast<char>(t->mData[i]);
    out[i] = 0;
    return t->mLength;
}
const char16_t* tpw_fs_no_wide_text() { return kNoWideText; }
void tpw_fs_set_wide(void* s, const char16_t* units) { *static_cast<TbDynamicStringTemplate<char16_t>*>(s) = units; }

void tpw_fs_set_project_dir(const char* text) { gProjectDir = text; }
void tpw_fs_set_found_name(const char* text) { gFoundName = text; }

// The resources: one manager, one resource, one storage (built once, while nothing records).
void* tpw_fs_manager() {
    static CTPResourceManager sManager;
    return &sManager;
}
void* tpw_fs_resource() {
    static CTPBaseResource* sResource = new TestResource();
    return sResource;
}
void* tpw_fs_resource_path() { return &static_cast<CTPBaseResource*>(tpw_fs_resource())->mPath; }
void* tpw_fs_storage() {
    static bfl::FileSystem::CFileStorage* sStorage = new bfl::FileSystem::CFileStorage();
    return sStorage;
}
void tpw_fs_set_resources(void* manager, void* resource, void* storage) {
    gGame.mResourceManager = static_cast<CTPResourceManager*>(manager);
    gCurrentResource = static_cast<CTPBaseResource*>(resource);
    gLastStorage = static_cast<bfl::FileSystem::CFileStorage*>(storage);
}

// A file (and its implementation when `implementation` is set).
void* tpw_fs_new_file(int32_t implementation) {
    auto* file = new bfl::FileSystem::CFile();
    if (implementation != 0) file->mImplementation = new bfl::FileSystem::CFileImplementation();
    return file;
}
void* tpw_fs_file_implementation(void* file) { return static_cast<bfl::FileSystem::CFile*>(file)->mImplementation; }

void* tpw_fs_new_info(uint32_t attributes, const char* name) {
    auto* info = new bfl::FileSystem::CFileInfo();
    info->mAttributes = attributes;
    std::strncpy(info->mName, name, sizeof(info->mNameBuffer) - 1);
    return info;
}
void* tpw_fs_new_disc_storage() { return new bfl::FileSystem::CDiscFileStorage(); }

void tpw_fs_tree_clear() {
    gTree.clear();
    gListings.clear();
}
void tpw_fs_tree_add(const char* path, const char* name, uint32_t attributes) {
    if (name == nullptr) {
        gTree[path];
        return;
    }
    gTree[path].push_back({name, attributes});
}

void tpw_fs_sinit_texts(char* out) {
    for (int32_t i = 0; i < 16; ++i) std::strncpy(out + 96 * i, gFSRegistryLanguages[i].mData, 95);
    std::strncpy(out + 96 * 16, gFSRegistryTitle.mData, 95);
    std::strncpy(out + 96 * 17, gFSRegistryLanguage.mData, 95);
    // the paths as __sinit_FS_System_cpp builds them (gTPFileInfo itself has been used since)
    const CSYS_TPFileInfo* fresh = new CSYS_TPFileInfo();
    const TbDynamicStringTemplate<char>* paths[] = {&fresh->mLocalPath, &fresh->mRemotePath, &fresh->mDataPath,
                                                    &fresh->mLevelPath, &fresh->mLanguagePath};
    for (int32_t i = 0; i < 5; ++i) std::strncpy(out + 96 * (18 + i), paths[i]->mData, 95);
}

// ---- FS_UserInfo.cpp
char tpw_fs_result_text[1024];   // the text of a string result

void* tpw_fsu_new() { return new CFSUserInfo(); }
void* tpw_fsu_buffer() { return std::calloc(1, sizeof(CFSUserInfo)); }
void tpw_fsu_set_slot(void* object, int32_t slot, const char16_t* name, int32_t easyMode) {
    auto* info = static_cast<CFSUserInfo*>(object);
    info->mUsers[slot].mName = name;
    info->mUsers[slot].mEasyMode = easyMode;
}
void tpw_fsu_set_user(void* object, int32_t user) { static_cast<CFSUserInfo*>(object)->mGameUser = user; }
void* tpw_fsu_slot_name(void* object, int32_t slot) { return &static_cast<CFSUserInfo*>(object)->mUsers[slot].mName; }
// The slots (name, 1024 bytes each, narrowed) and the logged-in slot.
int32_t tpw_fsu_get(const void* object, char* names, int32_t* easyModes) {
    const auto* info = static_cast<const CFSUserInfo*>(object);
    for (int32_t i = 0; i < 4; ++i) {
        tpw_fs_get_string(&info->mUsers[i].mName, 1, names + 1024 * i, 1024);
        easyModes[i] = info->mUsers[i].mEasyMode;
    }
    return info->mGameUser;
}
void tpw_fsu_members(void* object, void** out) {
    auto* info = static_cast<CFSUserInfo*>(object);
    out[0] = &info->mOutbox;
    out[1] = &info->mAddressBook;
}
// The theme list (gGame.mThemes) from `count` names; out receives the name strings.
void tpw_fsu_set_themes(const char** names, int32_t count, void** out) {
    using Node = TbTList<TbDynamicStringTemplate<char>*, TbDynamicStringTemplate<char>*>::CNode;
    Node* next = nullptr;
    for (int32_t i = count - 1; i >= 0; --i) {
        auto* node = new Node(new TbDynamicStringTemplate<char>(names[i]));
        node->mNext = next;
        next = node;
        out[i] = node->mData;
    }
    gGame.mThemes.mHead = next;
    gGame.mThemes.mCount = count;
}
void tpw_fsu_set_theme_name(const char* text) { gThemeName = text; }
void* tpw_fsu_theme_name() { return &gThemeName; }
void* tpw_fsu_new_master_save() { return new CFSGlobalMasterSave(); }
void tpw_fsu_set_master_save(void* save) { gMasterSave = static_cast<CFSGlobalMasterSave*>(save); }
void* tpw_fsu_master_save() { return gMasterSave; }
void* tpw_fsu_addr(const char* name) {
    if (std::strcmp(name, "gUserInfo") == 0) return &gUserInfo;
    if (std::strcmp(name, "gFSUserInfoUpload") == 0) return &gFSUserInfoUpload;
    if (std::strcmp(name, "gFSUserInfoGmsFile") == 0) return &gFSUserInfoGmsFile;
    if (std::strcmp(name, "mWideSeparator") == 0) return &gTPGlobals.mWideSeparator;
    if (std::strcmp(name, "mSaveFolder") == 0) return &gTPGlobals.mSaveFolder;
    if (std::strcmp(name, "mUsersFolder") == 0) return &gTPGlobals.mUsersFolder;
    if (std::strcmp(name, "mOnlineFolder") == 0) return &gTPGlobals.mOnlineFolder;
    return nullptr;
}

namespace {
void ResultText(const TbDynamicStringTemplate<char16_t>& s) { tpw_fs_get_string(&s, 1, tpw_fs_result_text, 1024); }
}

int32_t tpw_fsu_call(const char* name, const int64_t* a) {
    const std::string n(name);
    auto* info = reinterpret_cast<CFSUserInfo*>(a[0]);
    const auto slot = static_cast<EFSUserInfoGameUser>(a[1]);
    if (n == "CFSUserInfo::CFSUserInfo") new (info) CFSUserInfo();
    else if (n == "CFSUserInfo::~CFSUserInfo") {
        if (info == nullptr) return 0;
        if (static_cast<int16_t>(a[1]) > 0) delete info;
        else info->~CFSUserInfo();
    }
    else if (n == "CFSUserInfo::GameLoginInit") info->GameLoginInit();
    else if (n == "CFSUserInfo::CheckAllThemeDirectoriesExist")
        info->CheckAllThemeDirectoriesExist(*reinterpret_cast<TbDynamicStringTemplate<char16_t>*>(a[1]));
    else if (n == "CFSUserInfo::GetNumValidGameLoginSlots") return info->GetNumValidGameLoginSlots();
    else if (n == "CFSUserInfo::ReturnPathToSaveDirectory") ResultText(info->ReturnPathToSaveDirectory());
    else if (n == "CFSUserInfo::ReturnPathToUsersDirectory") ResultText(info->ReturnPathToUsersDirectory());
    else if (n == "CFSUserInfo::GetGameLoginSlotUserName") return lab(info->GetGameLoginSlotUserName(slot));
    else if (n == "CFSUserInfo::IsGameLoginSlotUserEasyMode") return info->IsGameLoginSlotUserEasyMode(slot);
    else if (n == "CFSUserInfo::ReturnGameLoginSlotUsersDirectoryName")
        ResultText(info->ReturnGameLoginSlotUsersDirectoryName(slot));
    else if (n == "CFSUserInfo::ReturnGameLoginSlotUsersPath") ResultText(info->ReturnGameLoginSlotUsersPath(slot));
    else if (n == "CFSUserInfo::ReturnUsersPath") ResultText(info->ReturnUsersPath());
    else if (n == "CFSUserInfo::ReturnUsersPathWithTheme") ResultText(info->ReturnUsersPathWithTheme());
    else if (n == "CFSUserInfo::ReturnOnlinePath") ResultText(info->ReturnOnlinePath());
    else return -12345;
    return 0;
}

void tpw_fsu_sinit_texts(char* out) {
    tpw_fs_get_string(&gFSUserInfoUpload, 1, out, 96);
    tpw_fs_get_string(&gFSUserInfoGmsFile, 1, out + 96, 96);
}

// Results that are pointers come back as labels.
int32_t tpw_fs_call(const char* name, const int64_t* a) {
    const std::string n(name);
    using TDS = TbDynamicStringTemplate<char>;
    using WDS = TbDynamicStringTemplate<char16_t>;
    auto s = [&](int k) -> TDS& { return *reinterpret_cast<TDS*>(a[k]); };
    auto w = [&](int k) -> WDS& { return *reinterpret_cast<WDS*>(a[k]); };
    auto file = [&](int k) { return reinterpret_cast<bfl::FileSystem::CFile*>(a[k]); };
    auto info = [&](int k) -> bfl::FileSystem::CFileInfo& { return *reinterpret_cast<bfl::FileSystem::CFileInfo*>(a[k]); };
    if (n == "TPConvertTPLangDefs") return static_cast<int32_t>(TPConvertTPLangDefs(static_cast<uint32_t>(a[0])));
    if (n == "FS_ReadRegistry") return FS_ReadRegistry().mValue;
    if (n == "FS_ReadLanguageRegistry") return FS_ReadLanguageRegistry().mValue;
    if (n == "TPFile_Open") return lab(TPFile_Open(reinterpret_cast<const char*>(a[0]), reinterpret_cast<const char*>(a[1])));
    if (n == "TPFile_Read") return static_cast<int32_t>(TPFile_Read(reinterpret_cast<void*>(a[0]), static_cast<uint32_t>(a[1]),
                                                                    static_cast<uint32_t>(a[2]), file(3)));
    if (n == "CFile::Read") return file(0)->bfl::FileSystem::CFile::Read(reinterpret_cast<void*>(a[1]), static_cast<uint32_t>(a[2]));
    if (n == "CFile::Write")
        return file(0)->bfl::FileSystem::CFile::Write(reinterpret_cast<const void*>(a[1]), static_cast<uint32_t>(a[2]));
    if (n == "TPFile_Close") return static_cast<int32_t>(TPFile_Close(file(0)));
    if (n == "TPFile_Seek") return static_cast<int32_t>(TPFile_Seek(file(0), static_cast<uint32_t>(a[1]), static_cast<uint32_t>(a[2])));
    if (n == "TPFile_Size") return TPFile_Size(file(0));
    if (n == "FS_FindFilePath") return FS_FindFilePath(s(0), s(1));
    if (n == "FS_IsSubDirectory") return FS_IsSubDirectory(info(0)) ? 1 : 0;
    if (n == "FS_DelTree") FS_DelTree(s(0));
    else if (n == "CFileInfo::~CFileInfo") {
        auto* p = reinterpret_cast<bfl::FileSystem::CFileInfo*>(a[0]);
        if (p != nullptr) {
            if (static_cast<int16_t>(a[1]) > 0) delete p;
            else p->~CFileInfo();
        }
        return lab(p);
    } else if (n == "CDiscFileStorage::~CDiscFileStorage") {
        auto* p = reinterpret_cast<bfl::FileSystem::CDiscFileStorage*>(a[0]);
        if (p != nullptr) {
            if (static_cast<int16_t>(a[1]) > 0) delete p;
            else p->~CDiscFileStorage();
        }
        return lab(p);
    }
    else if (n == "FS_IsWadFile") return FS_IsWadFile(info(0)) ? 1 : 0;
    else if (n == "FS_AddDirectorySeparator") FS_AddDirectorySeparator(s(0));
    else if (n == "CSYS_TPFileInfo::SetLocalPath") gTPFileInfo.SetLocalPath(s(1));
    else if (n == "CSYS_TPFileInfo::GetLocalPath") return lab(&gTPFileInfo.GetLocalPath());
    else if (n == "CSYS_TPFileInfo::SetRemotePath") gTPFileInfo.SetRemotePath(s(1));
    else if (n == "CSYS_TPFileInfo::GetRemotePath") return lab(&gTPFileInfo.GetRemotePath());
    else if (n == "CSYS_TPFileInfo::SetDataPath") gTPFileInfo.SetDataPath(s(1));
    else if (n == "CSYS_TPFileInfo::GetDataPath") return lab(&gTPFileInfo.GetDataPath());
    else if (n == "CSYS_TPFileInfo::SetLevelPath") gTPFileInfo.SetLevelPath(s(1));
    else if (n == "CSYS_TPFileInfo::GetLevelPath") return lab(&gTPFileInfo.GetLevelPath());
    else if (n == "CSYS_TPFileInfo::SetLanguagePath") gTPFileInfo.SetLanguagePath(s(1));
    else if (n == "TPStrGtr") return TPStrGtr(s(0), s(1)) ? 1 : 0;
    else if (n == "TPStrEq") return TPStrEq(s(0), s(1)) ? 1 : 0;
    else if (n == "TPUniStrEq") return TPUniStrEq(w(0), w(1)) ? 1 : 0;
    else if (n == "RemoveExtension") return lab(&RemoveExtension(s(0), s(1)));
    else if (n == "UniStringToString") return UniStringToString(s(0), w(1));
    else if (n == "StringToUniString") return StringToUniString(w(0), s(1));
    else return -12345;
    return 0;
}

}  // extern "C"
