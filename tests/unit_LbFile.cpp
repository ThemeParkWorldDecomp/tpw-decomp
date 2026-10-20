#include "LbFile.h"
#include "LbTime.h"
#include "bfl_FileSystem.h"
#include "platform/macdoze.h"
#include <cstdlib>
#include <cstring>
#include <new>
#define TPW_ACCESS_ONLY
#define TPW_RECORDED_ACCESS_IO
#include "fake_file_slots.h"

extern "C" {
struct FileEvent {
    int32_t kind;
    uint32_t a;
    uint32_t b;
    char text[512];
};
FileEvent tpw_lbf_events[128];
int32_t tpw_lbf_n;
uint32_t tpw_lbf_config[12];
char tpw_lbf_folder[512] = "App:";
}

namespace {
bool gRecording;
void* gFile;
void* gBuffer;
char* gProject;
char gEmpty[] = "";
void event(int32_t kind, uint32_t a = 0, uint32_t b = 0, const char* text = "") {
    if (!gRecording) return;
    auto& out = tpw_lbf_events[tpw_lbf_n++];
    out.kind = kind;
    out.a = a;
    out.b = b;
    std::strncpy(out.text, text != nullptr ? text : "(null)", sizeof(out.text) - 1);
    out.text[sizeof(out.text) - 1] = 0;
}
uint32_t label(const void* p) {
    return p == nullptr ? 0 : p == gFile ? 1 : p == gBuffer ? 4 : 99;
}
class TestImplementation : public bfl::FileSystem::CFileImplementation {
public:
    int32_t Read(void* dest, uint32_t size) override {
        event(18, size, label(dest));
        if (size != 0) static_cast<unsigned char*>(dest)[0] = 0xa5;
        return static_cast<int32_t>(tpw_lbf_config[1]);
    }
    int32_t Write(const void* source, uint32_t size) override {
        event(19, size, label(source));
        return static_cast<int32_t>(tpw_lbf_config[1]);
    }
};
class TestFolder : public SamsUtilities::UFolder {
public:
    void GetPath(SamsUtilities::UString& path) override {
        event(12, 5);
        path.mLength = static_cast<uint32_t>(std::strlen(tpw_lbf_folder));
        path.mText = tpw_lbf_folder;
    }
};
TestFolder gFolder;
TestImplementation gImplementation;
}

void* operator new(std::size_t size) {
    event(1, 8); // CDiscFile is 8 bytes on the Mac; host pointer fields are wider.
    void* p = std::malloc(size);
    if (p == nullptr) throw std::bad_alloc();
    return p;
}
void* operator new[](std::size_t size) {
    event(7, static_cast<uint32_t>(size));
    void* p = std::malloc(size != 0 ? size : 1);
    if (p == nullptr) throw std::bad_alloc();
    return p;
}
void operator delete(void* p) noexcept {
    event(6, label(p));
    std::free(p);
}
void operator delete[](void* p) noexcept {
    event(8, label(p));
    std::free(p);
}
void operator delete(void* p, std::size_t) noexcept { ::operator delete(p); }
void operator delete[](void* p, std::size_t) noexcept { ::operator delete[](p); }

namespace bfl::FileSystem {
CFile::CFile() : mImplementation(&gImplementation) {}
CFile::~CFile() {}
void CFile::Close() { event(4, label(this)); }
int32_t CFile::Seek(uint32_t offset, uint32_t whence) {
    event(20, offset, whence);
    return static_cast<int32_t>(tpw_lbf_config[5]);
}
int32_t CFile::GetSize() {
    event(26, label(this));
    return static_cast<int32_t>(tpw_lbf_config[5]);
}
int32_t CFile::GetPos() { return 0; }
void* CFile::MapFile() { return nullptr; }
TbError CFile::Flush() { return TbError(0); }
int32_t CFile::GetMacFileReference() { return -1; }
CAccessItem::~CAccessItem() {}
int32_t CFileImplementation::Read(void*, uint32_t) { return 0; }
int32_t CFileImplementation::Write(const void*, uint32_t) { return 0; }
CDiscFile::CDiscFile() {
    gFile = this;
    event(2, 1);
}
CDiscFile::~CDiscFile() { event(5, label(this)); }
TbError CDiscFile::Open(const char* name, uint32_t mode) {
    event(3, label(this), mode, name);
    return TbError(static_cast<int32_t>(tpw_lbf_config[0]));
}
}

namespace SamsUtilities {
UString::UString() : mLength(0), mText(gEmpty) { event(10, 5); }
UString::~UString() { event(13, 5); }
UFileItem::~UFileItem() {}
UFileItem* UFolder::GetItem(const UString&, int32_t) { return nullptr; }
UProcess gThisProcess;
UFolder* UProcess::GetFolder() { event(11); return &gFolder; }
uint64_t UTimer::GetAbsolute() {
    event(9);
    return (uint64_t(tpw_lbf_config[6]) << 32) | tpw_lbf_config[7];
}
}

namespace NS_MacDoze {
int32_t CreateDirectory(const char* name, void* security) {
    event(14, label(security), 0, name);
    return static_cast<int32_t>(tpw_lbf_config[8]);
}
uint32_t GetFileAttributes(const char* name) {
    event(17, 0, 0, name);
    return tpw_lbf_config[4];
}
int32_t SetFileAttributes(const char* name, uint32_t attributes) {
    event(21, attributes, 0, name);
    return static_cast<int32_t>(tpw_lbf_config[8]);
}
int32_t DeleteFile(const char* name) {
    event(22, 0, 0, name);
    return static_cast<int32_t>(tpw_lbf_config[8]);
}
int32_t RemoveDirectory(const char* name) {
    event(23, 0, 0, name);
    return static_cast<int32_t>(tpw_lbf_config[8]);
}
int32_t MoveFile(const char* from, const char* to) {
    event(24, 0, 0, from);
    event(25, 0, 0, to);
    return static_cast<int32_t>(tpw_lbf_config[8]);
}
// LbFile_Copy's call (unit w90): the two names (events 27 and 28; the fail-if-exists flag is the first one's second word)
int32_t CopyFile(const char* from, const char* to, int32_t failIfExists) {
    event(27, 0, static_cast<uint32_t>(failIfExists), from);
    event(28, 0, 0, to);
    return static_cast<int32_t>(tpw_lbf_config[8]);
}
InternalHandle* FindFirstFile(const char* name, _WIN32_FIND_DATAA* data) {
    event(15, 0, 0, name);
    data->nFileSizeLow = tpw_lbf_config[2];
    data->nFileSizeHigh = 0x76543210;
    return reinterpret_cast<InternalHandle*>(tpw_lbf_config[3] ? 3 : intptr_t(-1));
}
int32_t FindClose(InternalHandle* handle) {
    event(16, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(handle)));
    return -10;
}
}

extern "C" {
void* tpw_lbf_project() {
    if (gProject == nullptr) gProject = const_cast<char*>(LbFile_GetProjectDir());
    return gProject;
}
// Fixed buffers and outputs keep pointer identity separate from native ABI layout.
int32_t tpw_lbf_call(int32_t op, char* name, void* buffer, uint32_t x, uint32_t y,
                      int32_t optional, uint32_t* outputs) {
    gBuffer = buffer;
    gFile = nullptr;
    bfl::FileSystem::CDiscFile* file = nullptr;
    if (op == 2 || op == 3 || op == 6 || op == 7 || op == 22) file = new bfl::FileSystem::CDiscFile();
    if (op == 14 && optional != 0) gFile = std::malloc(16);
    void* out = reinterpret_cast<void*>(uintptr_t(99));
    uint32_t transferred = 0x11223344;
    int32_t length = 0x11223344;
    tpw_lbf_n = 0;
    gRecording = true;
    int32_t result = 0;
    switch (op) {
    case 0: result = LbFile_CreateDirectory(name); break;
    case 1: result = LbFile_Open(out, name, x); break;
    case 2: result = LbFile_Close(file); break;
    case 3: result = LbFile_Seek(file, static_cast<int32_t>(x), y); break;
    case 4: result = LbFile_GetLength(name); break;
    case 5: result = LbFile_Exists(name); break;
    case 6: result = LbFile_Read(file, buffer, x, optional ? &transferred : nullptr); break;
    case 7: result = LbFile_Write(file, buffer, x, optional ? &transferred : nullptr); break;
    case 8: LbFile_SetProjectDir(name); break;
    case 9: LbFile_GetProjectDir(); break;
    case 10: result = LbFile_ToProjectFilename(static_cast<char*>(buffer), x, name); break;
    case 11: result = LbFile_Load(name, buffer, x, optional ? &length : nullptr); break;
    case 12: LbFile_GetProjectPath(); break;
    case 13: out = LbMem_Alloc(x); result = out != nullptr; break;
    case 14: LbMem_Free(gFile); break;
    case 15: result = static_cast<int32_t>(LbTime_GetClock()); break;
    case 16: result = LbFile_GetAttributes(name, &transferred); break;
    case 17: result = LbFile_SetAttributes(name, x); break;
    case 18: result = LbFile_IsDirectory(name); break;
    case 19: result = LbFile_Delete(name); break;
    case 20: result = LbFile_DeleteDirectory(name); break;
    case 21: result = LbFile_Rename(name, static_cast<const char*>(buffer)); break;
    case 22: result = LbFile_GetSize(file); break;
    case 23: result = LbFile_Copy(name, static_cast<const char*>(buffer)); break;
    }
    outputs[0] = transferred;
    outputs[1] = static_cast<uint32_t>(length);
    outputs[2] = label(out);
    gRecording = false;
    if (op == 1 && gFile != nullptr) delete static_cast<bfl::FileSystem::CDiscFile*>(gFile);
    if (file != nullptr && op != 2) delete file;
    if (op == 13) ::operator delete[](out);
    return result;
}
}
