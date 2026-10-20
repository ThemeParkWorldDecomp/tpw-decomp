// Unit w221: registry and file wrappers. Outside callees are recording fakes.
#include "TbRegistryKey.h"
#include "LbFile.h"
#include "bfl_FileSystem.h"
#include "platform/interfacelib.h"
#include "platform/msl.h"
#include "fake_strings.h"
#include <initializer_list>
#include <map>
#include <vector>
#include <cstdlib>
#include <new>
#include <string>

// A fixed recorder avoids logging the functional string library's native allocations.
namespace calllog {
struct Event {
    char name[24];
    uint32_t ni, nt;
    int64_t ints[12];
    char texts[3][160];
};
Event events[1024];
uint32_t count;
std::map<std::string, std::vector<int64_t>> queues;
std::map<std::string, uint32_t> indices;
std::map<const void*, int64_t> labels;
int64_t lab(const void* p) {
    if (p == nullptr) return 0;
    auto it = labels.find(p);
    return it == labels.end() ? -1 : it->second;
}
int64_t ret(const char* name) {
    auto& q = queues[name];
    if (q.empty()) return 0;
    uint32_t i = indices[name]++;
    return q[i < q.size() ? i : q.size() - 1];
}
void ev(const char* name, std::initializer_list<int64_t> ints = {}, std::initializer_list<const char*> texts = {}) {
    auto& e = events[count++];
    std::strcpy(e.name, name);
    e.ni = ints.size();
    e.nt = texts.size();
    uint32_t i = 0;
    for (int64_t n : ints) e.ints[i++] = n;
    i = 0;
    for (const char* t : texts) {
        std::strncpy(e.texts[i], t, 159);
        e.texts[i++][159] = 0;
    }
}
}
using namespace calllog;
char gProcessPath[261];
extern "C" {
uint32_t tpw_rgf_config[32];
unsigned char tpw_rgf_payload[128];
char tpw_rgf_items[512];
}
namespace {
unsigned char allocation[512];
void* deleting;
const char* text(const char* p) { return p ? p : "(null)"; }
int64_t present(const void* p) { return p != nullptr; }
std::string hex(const void* p, uint32_t n) {
    static const char digits[] = "0123456789abcdef";
    std::string out;
    for (uint32_t i = 0; i < n; ++i) {
        unsigned char b = static_cast<const unsigned char*>(p)[i];
        out += digits[b >> 4];
        out += digits[b & 15];
    }
    return out;
}
}
void operator delete(void* p) noexcept {
    if (p == deleting) ev("Delete", {lab(p)});
    else std::free(p);
}
void operator delete(void* p, std::size_t) noexcept { ::operator delete(p); }

namespace NS_MacDoze {
// A key has a child key plus a configurable value; forced errors exercise each caller.
int32_t RegCloseKey(HKEY h) {
    ev("Close", {h});
    return ret("Close");
}
int32_t RegOpenKeyExA(HKEY h, const char* n, uint32_t options, uint32_t access, HKEY* out) {
    ev("OpenEx", {h, options, access, present(out)}, {text(n)});
    int32_t r = ret("OpenEx");
    if (r == 0) *out = tpw_rgf_config[0];
    return r;
}
int32_t RegOpenKeyA(HKEY h, const char* n, HKEY* out) {
    ev("Open", {h, present(out)}, {text(n)});
    int32_t r = ret("Open");
    if (r == 0) *out = tpw_rgf_config[0];
    return r;
}
int32_t RegCreateKeyExA(HKEY h, const char* n, uint32_t reserved, char* cls, uint32_t options,
                       uint32_t access, void* security, HKEY* out, uint32_t* disposition) {
    ev("Create", {h, reserved, present(cls), options, access, present(security), present(out), present(disposition)}, {text(n)});
    int32_t r = ret("Create");
    if (r == 0) *out = tpw_rgf_config[0];
    return r;
}
int32_t RegSetValueExA(HKEY h, const char* n, uint32_t reserved, uint32_t type, const void* data, uint32_t size) {
    std::string bytes;
    if (type >= 4 && type <= 6 && size == 4) {
        uint32_t v;
        std::memcpy(&v, data, 4);
        unsigned char b[] = {uint8_t(v >> 24), uint8_t(v >> 16), uint8_t(v >> 8), uint8_t(v)};
        bytes = hex(b, 4);
    } else bytes = hex(data, size);
    ev("Set", {h, reserved, type, size, present(data)}, {text(n), bytes.c_str()});
    return ret("Set");
}
int32_t RegQueryValueExA(HKEY h, const char* n, uint32_t* reserved, uint32_t* type, void* data, uint32_t* size) {
    ev("Query", {h, present(reserved), present(type), present(data), present(size), data && size ? *size : 0}, {text(n)});
    int32_t r = ret("Query");
    if (r != 0) return r;
    if (type) *type = tpw_rgf_config[1];
    uint32_t needed = tpw_rgf_config[2];
    uint32_t available = data && size ? *size : 0;
    if (size) *size = needed;
    if (data && available < needed) return 234;
    if (data) {
        if (tpw_rgf_config[1] >= 4 && tpw_rgf_config[1] <= 6 && needed == 4)
            std::memcpy(data, &tpw_rgf_config[3], 4);
        else std::memcpy(data, tpw_rgf_payload, needed);
    }
    return 0;
}
int32_t RegQueryInfoKeyA(HKEY h, char* cls, uint32_t* clsLen, uint32_t* reserved, uint32_t* keys,
                         uint32_t* maxKey, uint32_t* maxClass, uint32_t* values, uint32_t* maxValue,
                         uint32_t* maxData, uint32_t* security, tagFILETIME* time) {
    ev("Info", {h, present(cls), present(clsLen), present(reserved), present(keys), present(maxKey),
                present(maxClass), present(values), present(maxValue), present(maxData), present(security), present(time)});
    int32_t r = ret("Info");
    if (r == 0) {
        *keys = tpw_rgf_config[4];
        *maxKey = tpw_rgf_config[5];
        *values = tpw_rgf_config[6];
        *maxValue = tpw_rgf_config[7];
    }
    return r;
}
int32_t RegEnumKeyExA(HKEY h, uint32_t i, char* name, uint32_t* n, uint32_t* reserved,
                      char* cls, uint32_t* clsLen, tagFILETIME* time) {
    ev("EnumKey", {h, i, *n, present(reserved), present(cls), present(clsLen), present(time)});
    int32_t r = ret("EnumKey");
    if (r == 0) {
        std::strcpy(name, i == 0 ? "child" : "long-key");
        *n = std::strlen(name);
    }
    return r;
}
int32_t RegEnumValueA(HKEY h, uint32_t i, char* name, uint32_t* n, uint32_t* reserved,
                      uint32_t* type, void* data, uint32_t* size) {
    ev("EnumValue", {h, i, *n, present(reserved), present(type), present(data), present(size)});
    int32_t r = ret("EnumValue");
    if (r == 0) {
        std::sprintf(name, "value%u", i);
        *n = std::strlen(name);
        *type = i < 8 ? i : 0xffffffffu;
    }
    return r;
}
int32_t RegDeleteValueA(HKEY h, const char* n) {
    ev("DeleteValue", {h}, {text(n)});
    return ret("DeleteValue");
}
int32_t RegDeleteKeyA(HKEY h, const char* n) {
    ev("DeleteKey", {h}, {text(n)});
    return ret("DeleteKey");
}
int32_t _strnicmp(const char* a, const char* b, int32_t n) {
    ev("Compare", {n}, {a, b});
    return strncasecmp(a, b, n);
}
int32_t GetDiskFreeSpace(const char* root, uint32_t* sectors, uint32_t* bytes, uint32_t* free, uint32_t* total) {
    ev("Disk", {present(sectors), present(bytes), present(free), present(total)}, {root});
    *sectors = tpw_rgf_config[8];
    *bytes = tpw_rgf_config[9];
    *free = tpw_rgf_config[10];
    *total = tpw_rgf_config[11];
    return ret("Disk");
}
InternalHandle* FindFirstFile(const char* pattern, _WIN32_FIND_DATAA* d) {
    ev("FindFirst", {present(d)}, {pattern});
    d->dwFileAttributes = tpw_rgf_config[12];
    std::strcpy(d->cFileName, "first.dat");
    return reinterpret_cast<InternalHandle*>(intptr_t(ret("FindFirst")));
}
int32_t FindNextFile(InternalHandle* h, _WIN32_FIND_DATAA* d) {
    ev("FindNext", {uint32_t(reinterpret_cast<uintptr_t>(h)), present(d)});
    int32_t r = ret("FindNext");
    if (r) {
        d->dwFileAttributes = tpw_rgf_config[13];
        std::strcpy(d->cFileName, "next.bin");
    }
    return r;
}
int32_t FindClose(InternalHandle* h) {
    ev("FindClose", {uint32_t(reinterpret_cast<uintptr_t>(h))});
    return ret("FindClose");
}
}
// Toolbox fakes never dereference the bogus length-byte pointers.
OSErr HGetVol(unsigned char* name, int16_t* ref, int32_t* dir) {
    ev("Volume", {present(name), present(ref), present(dir)});
    name[0] = tpw_rgf_config[14];
    return ret("Volume");
}
void UpperString(unsigned char* p, bool sensitive) {
    ev("Upper", {int32_t(reinterpret_cast<intptr_t>(p)), sensitive});
}
char* p2cstr(unsigned char* p) {
    ev("Pascal", {int32_t(reinterpret_cast<intptr_t>(p))});
    return reinterpret_cast<char*>(uintptr_t(tpw_rgf_config[15]));
}

// File and heap callees use stable labels, with data writes still observable.
void* LbMem_Alloc(uint32_t n) {
    ev("Alloc", {n});
    return ret("Alloc") ? allocation : nullptr;
}
void LbMem_Free(void* p) { ev("Free", {lab(p)}); }
TbError LbFile_ToProjectFilename(char* out, uint32_t size, const char* name) {
    ev("Project", {lab(out), size}, {text(name)});
    std::strcpy(out, "App:");
    std::strcat(out, name ? name : "default");
    return TbError(ret("Project"));
}
uint8_t LbFile_Exists(const char* name) {
    ev("Exists", {}, {name});
    if (tpw_rgf_config[16]) return std::strcmp(name, "Disk") == 0 || std::strcmp(name, "Disk:") == 0 || std::strcmp(name, "Disk:/") == 0;
    return ret("Exists");
}
int32_t LbFile_GetLength(const char* name) {
    ev("Length", {}, {name});
    return ret("Length");
}
TbError LbFile_CreateDirectory(const char* name) {
    ev("Directory", {}, {name});
    return TbError(ret("Directory"));
}
TbError LbFile_Open(void*& file, const char* name, uint32_t mode) {
    ev("FileOpen", {mode}, {text(name)});
    int32_t r = ret("FileOpen");
    if (r >= 0) file = reinterpret_cast<void*>(uintptr_t(0x6000));
    return TbError(r);
}
TbError LbFile_Read(void* file, void* data, uint32_t size, uint32_t* count) {
    ev("Read", {uint32_t(reinterpret_cast<uintptr_t>(file)), lab(data), size, present(count)});
    if (size != 0) static_cast<unsigned char*>(data)[0] = 0x7b;
    return TbError(ret("Read"));
}
TbError LbFile_Write(void* file, const void* data, uint32_t size, uint32_t* count) {
    ev("Write", {uint32_t(reinterpret_cast<uintptr_t>(file)), lab(data), size, present(count)}, {hex(data, size).c_str()});
    return TbError(ret("Write"));
}
TbError LbFile_Close(void* file) {
    ev("FileClose", {uint32_t(reinterpret_cast<uintptr_t>(file))});
    return TbError(ret("FileClose"));
}
namespace bfl::FileSystem {
CFile::CFile() : mImplementation(nullptr) {}
CFile::~CFile() {}
void CFile::Close() {}
int32_t CFile::Seek(uint32_t, uint32_t) { return 0; }
int32_t CFile::GetSize() { return 0; }
int32_t CFile::GetPos() {
    ev("Position", {lab(this)});
    return ret("Position");
}
void* CFile::MapFile() { return nullptr; }
TbError CFile::Flush() {
    ev("Flush", {lab(this)});
    return TbError(ret("Flush"));
}
int32_t CFile::GetMacFileReference() { return 0; }
}
TbCharStringList::~TbCharStringList() {}
namespace {
bfl::FileSystem::CFile fileObject;
}
extern "C" {
void tpw_rgf_begin() {
    count = 0;
    queues.clear();
    indices.clear();
    labels.clear();
}
void tpw_rgf_label(void* p, int64_t n) { labels[p] = n; }
void tpw_rgf_queue(const char* name, const int64_t* p, uint32_t n) { queues[name] = std::vector<int64_t>(p, p + n); }
const Event* tpw_rgf_events(uint32_t* n) { *n = count; return events; }
uint64_t tpw_rgf_addr(int32_t which) {
    const void* addresses[] = {tpw_rgf_config, tpw_rgf_payload, gProcessPath, allocation, &fileObject, tpw_rgf_items};
    return reinterpret_cast<uintptr_t>(addresses[which]);
}
int32_t tpw_rgf_layout(uint32_t* out) {
    out[0] = sizeof(TbFileFind);
    out[1] = offsetof(TbFileFind, mFlags);
    out[2] = offsetof(TbFileFind, mPath);
    out[3] = offsetof(TbFileFind, mName);
    return 4;
}
// Generic ABI wrapper keeps each original's argument order explicit in its case.
int64_t tpw_rgf_call(int32_t op, TbRegistryKey* key, TbRegistryKey* sub, const char* name,
                      void* data, uint32_t x, uint32_t y, void* aux) {
    switch (op) {
    case 0: new (key) TbRegistryKey(*sub); return lab(key);
    case 1: return lab(&(*key = *sub));
    case 2: return key->OpenBullfrogGameKey(name, x);
    case 3: return key->OpenEAGameKey(name, x);
    case 4: return key->OpenKey(name);
    case 5: return key->CreateKey(name);
    case 6:
        if (key) {
            deleting = key;
            if (int16_t(x) > 0) delete key;
            else key->~TbRegistryKey();
            deleting = nullptr;
        }
        return lab(key);
    case 7: return key->CloseKey();
    case 8: key->Initialise(); return 0;
    case 9: return TbRegistryKey::_OpenSubKey(name, x);
    case 10: return TbRegistryKey::_CreateSubKey(name, x, y);
    case 11: return key->SetValue(name, static_cast<TbRegItemType>(x), data, y);
    case 12: return key->GetValue(name, data, x);
    case 13: return key->GetValueSize(name);
    case 14: return key->SetValueGUID(name, *static_cast<NS_MacDoze::_GUID*>(data));
    case 15: return key->GetValueGUID(name, *static_cast<NS_MacDoze::_GUID*>(data));
    case 16: return key->GetValueGUID(name, *static_cast<NS_MacDoze::_GUID*>(data), *static_cast<NS_MacDoze::_GUID*>(aux));
    case 17: return key->SetValueString(name, static_cast<char*>(data));
    case 18: return key->GetValueString(name, static_cast<char*>(data), x, static_cast<char*>(aux));
    case 19: return key->SetValueULONG(name, x);
    case 20: return key->GetValueULONG(name, *static_cast<uint32_t*>(data));
    case 21: return key->GetValueULONG(name, *static_cast<uint32_t*>(data), x);
    case 22: return key->OpenSubKey(name, *sub);
    case 23: return key->CreateSubKey(name, *sub);
    case 24: return key->GetItemType(name);
    case 25: new (key) TbRegistryKey(); return lab(key);
    case 26: {
        TbCharStringList list;
        int32_t r = key->ListItems(list, x);
        tpw_rgf_items[0] = 0;
        POSITION p = list.GetHeadPosition();
        while (p) {
            std::strcat(tpw_rgf_items, list.GetNext(p).mData);
            std::strcat(tpw_rgf_items, "\n");
        }
        return r;
    }
    case 27: return key->DeleteItem(name);
    case 28: return LbFile_GetCurrentDrive();
    case 29: return LbFile_GetFreeDriveSpace(char(x), static_cast<TbDriveInfo*>(data));
    case 30: return LbFile_Flush(&fileObject);
    case 31: return LbFile_GetPosition(&fileObject);
    case 32: return LbFile_GetFileStamp(key, static_cast<TbFileStamp*>(data), static_cast<TbFileStamp*>(aux), nullptr);
    case 33: return LbFile_SetFileStamp(key, static_cast<TbFileStamp*>(data), static_cast<TbFileStamp*>(aux), nullptr);
    case 34: return LbFile_FindFirst(name, static_cast<TbFileFind*>(data), x);
    case 35: return LbFile_FindNext(static_cast<TbFileFind*>(data));
    case 36: return LbFile_FindEnd(static_cast<TbFileFind*>(data));
    case 37: return LbFile_MakePath(name);
    case 38: return lab(LbFile_GetProjectFilename(name));
    case 39: return LbFile_AllocLoad(name, data, static_cast<int32_t*>(aux));
    case 40: return LbFile_Save(name, data, x);
    case 41: return LbFile_Open(static_cast<void**>(data), name, x);
    case 42: return lab(LbMem_ReAlloc(data, x, y));
    case 43: {
        char* a = reinterpret_cast<char*>(uintptr_t(x));
        char* b = reinterpret_cast<char*>(uintptr_t(y));
        return &LbMaxPointer(a, b) == &a ? 0 : 1;
    }
    case 44: new (data) TbError(*static_cast<TbError*>(aux)); return lab(data);
    case 45: return int32_t(*static_cast<TbError*>(data));
    case 46: new (data) TbError(int32_t(x)); return lab(data);
    }
    return 0;
}
}
