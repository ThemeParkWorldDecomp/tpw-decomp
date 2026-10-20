// Bullfrog library, LbSCBack.cpp registry range 0x10001e60..0x10003c44.
#include "TbRegistryKey.h"
#include "LbFile.h"
#include <cstdio>
#include <cstring>

using namespace NS_MacDoze;
namespace {
// 0x10071863, 0x16-byte entries including the terminating empty entry.
const char kRootNames[8][0x16] = {
    "HKEY_CLASSES_ROOT", "HKEY_CURRENT_USER", "HKEY_LOCAL_MACHINE", "HKEY_USERS",
    "HKEY_PERFORMANCE_DATA", "HKEY_CURRENT_CONFIG", "HKEY_DYN_DATA", ""
};
char sGuidRead[0x28]; // 0x10073ca4, invented name
char sGuidWrite[0x28]; // 0x10073ccc, invented name and host capacity
}

// 0x10001e60 (52)
TbRegistryKey::TbRegistryKey(const TbRegistryKey&) { Initialise(); }
// 0x10001ed0 (52): the old handle is abandoned without closing it.
TbRegistryKey& TbRegistryKey::operator=(const TbRegistryKey&) {
    Initialise();
    return *this;
}

// 0x10001f40 (256)
TbError TbRegistryKey::OpenBullfrogGameKey(const char* name, int32_t user) {
    if (mHandle != 0) CloseKey();
    HKEY base = _CreateSubKey("software\\Bullfrog Productions Ltd",
                              user != 0 ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE, 0);
    if (base == 0) return TbError(-1);
    if (name != nullptr) {
        mHandle = _CreateSubKey(name, base, 0);
        RegCloseKey(base);
    } else mHandle = base;
    return TbError(mHandle != 0 ? 0 : -1);
}
// 0x1000207c (256)
TbError TbRegistryKey::OpenEAGameKey(const char* name, int32_t user) {
    if (mHandle != 0) CloseKey();
    HKEY base = _CreateSubKey("software\\Electronic Arts",
                              user != 0 ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE, 0);
    if (base == 0) return TbError(-1);
    if (name != nullptr) {
        mHandle = _CreateSubKey(name, base, 0);
        RegCloseKey(base);
    } else mHandle = base;
    return TbError(mHandle != 0 ? 0 : -1);
}
// 0x100021b4 (284)
TbError TbRegistryKey::OpenKey(const char* path) {
    if (mHandle != 0) CloseKey();
    Initialise();
    HKEY root = 0;
    const char* rest = nullptr;
    for (uint32_t i = 0; kRootNames[i][0] != 0; ++i) {
        uint32_t n = std::strlen(kRootNames[i]);
        if (_strnicmp(kRootNames[i], path, n) == 0) {
            root = i | 0x80000000u;
            rest = path + n;
            break;
        }
    }
    if (root == 0) return TbError(-1);
    if (*rest != 0) ++rest; // The skipped character need not be a separator.
    mHandle = _OpenSubKey(rest, root);
    return TbError(mHandle != 0 ? 0 : -1);
}
// 0x10002300 (288)
TbError TbRegistryKey::CreateKey(const char* path) {
    if (mHandle != 0) CloseKey();
    Initialise();
    HKEY root = 0;
    const char* rest = nullptr;
    for (uint32_t i = 0; kRootNames[i][0] != 0; ++i) {
        uint32_t n = std::strlen(kRootNames[i]);
        if (_strnicmp(kRootNames[i], path, n) == 0) {
            root = i | 0x80000000u;
            rest = path + n;
            break;
        }
    }
    if (root == 0) return TbError(-1);
    if (*rest != 0) ++rest;
    mHandle = _CreateSubKey(rest, root, 0);
    return TbError(mHandle != 0 ? 0 : -1);
}
// 0x10002454 (96), native delete supplies the compiler's deleting flag.
TbRegistryKey::~TbRegistryKey() { CloseKey(); }
// 0x100024e0 (104)
TbError TbRegistryKey::CloseKey() {
    if (mHandle != 0 && RegCloseKey(mHandle) != 0) return TbError(-1);
    mHandle = 0;
    return TbError(0);
}
// 0x10002578 (12)
void TbRegistryKey::Initialise() { mHandle = 0; }
// 0x100025b4 (80)
HKEY TbRegistryKey::_OpenSubKey(const char* name, HKEY parent) {
    HKEY handle;
    return RegOpenKeyExA(parent, name, 0, 0, &handle) == 0 ? handle : 0;
}
// 0x1000263c (116)
HKEY TbRegistryKey::_CreateSubKey(const char* name, HKEY parent, int32_t isVolatile) {
    HKEY handle;
    return RegCreateKeyExA(parent, name, 0, nullptr, isVolatile != 0 ? 1 : 0,
                           0xf003f, nullptr, &handle, nullptr) == 0 ? handle : 0;
}
// 0x100026e8 (144)
TbError TbRegistryKey::SetValue(const char* name, TbRegItemType type, const void* data, uint32_t size) const {
    if (data == nullptr || mHandle == 0) return TbError(-1);
    if (name == nullptr && type != kTbRegItemString) return TbError(-1);
    return TbError(RegSetValueExA(mHandle, name, 0, type, data, size) == 0 ? 0 : -1);
}
// 0x100027c0 (148)
TbError TbRegistryKey::GetValue(const char* name, void* data, uint32_t size) const {
    if (name == nullptr || mHandle == 0) return TbError(-1);
    uint32_t type = 0;
    return TbError(RegQueryValueExA(mHandle, name, nullptr, &type, data, &size) == 0 ? 0 : -1);
}
// 0x1000288c (100)
uint32_t TbRegistryKey::GetValueSize(const char* name) const {
    if (name == nullptr || mHandle == 0) return 0;
    uint32_t size;
    return RegQueryValueExA(mHandle, name, nullptr, nullptr, nullptr, &size) == 0 ? size : 0;
}
// 0x10002928 (184), format at 0x1006c157 includes the original stray percent.
TbError TbRegistryKey::SetValueGUID(const char* name, const _GUID& g) const {
    std::sprintf(sGuidWrite, "{%.8X-%.4X-%.4X-%.2X%.2X-%.2X%.2X%.2X%.2X%.2X%.2X%}",
                 (uint32_t(g.bytes[0]) << 24) | (uint32_t(g.bytes[1]) << 16) | (uint32_t(g.bytes[2]) << 8) | g.bytes[3],
                 (unsigned(g.bytes[4]) << 8) | g.bytes[5], (unsigned(g.bytes[6]) << 8) | g.bytes[7],
                 g.bytes[8], g.bytes[9], g.bytes[10], g.bytes[11], g.bytes[12], g.bytes[13], g.bytes[14], g.bytes[15]);
    return SetValueString(name, sGuidWrite);
}
// 0x10002a2c (416), scan format at 0x1006c18b. Data4 is byte-reversed in groups.
TbError TbRegistryKey::GetValueGUID(const char* name, _GUID& g) const {
    if (GetItemType(name) != kTbRegItemString) return TbError(-1);
    if (GetValueSize(name) != 0x27) return TbError(-1);
    if (!GetValueString(name, sGuidRead, 0x28, nullptr).Succeeded()) return TbError(-1);
    unsigned long d1 = (uint32_t(g.bytes[0]) << 24) | (uint32_t(g.bytes[1]) << 16) | (uint32_t(g.bytes[2]) << 8) | g.bytes[3];
    unsigned short d2 = (unsigned(g.bytes[4]) << 8) | g.bytes[5];
    unsigned short d3 = (unsigned(g.bytes[6]) << 8) | g.bytes[7];
    unsigned short a, b;
    unsigned long c;
    std::sscanf(sGuidRead, "{%lx-%hx-%hx-%hx-%4hx%8lx}", &d1, &d2, &d3, &a, &b, &c);
    g.bytes[0] = d1 >> 24;
    g.bytes[1] = d1 >> 16;
    g.bytes[2] = d1 >> 8;
    g.bytes[3] = d1;
    g.bytes[4] = d2 >> 8;
    g.bytes[5] = d2;
    g.bytes[6] = d3 >> 8;
    g.bytes[7] = d3;
    g.bytes[8] = a;
    g.bytes[9] = a >> 8;
    g.bytes[10] = b;
    g.bytes[11] = b >> 8;
    g.bytes[12] = c;
    g.bytes[13] = c >> 8;
    g.bytes[14] = c >> 16;
    g.bytes[15] = c >> 24;
    return TbError(0);
}
// 0x10002c18 (176)
TbError TbRegistryKey::GetValueGUID(const char* name, _GUID& g, const _GUID& fallback) const {
    if (GetValueGUID(name, g).Succeeded()) return TbError(0);
    g = fallback;
    return TbError(-1);
}
// 0x10002d28 (180): strlen precedes the null check in the original.
TbError TbRegistryKey::SetValueString(const char* name, const char* text) const {
    uint32_t size = std::strlen(text);
    if (text == nullptr || mHandle == 0) return TbError(-1);
    return TbError(RegSetValueExA(mHandle, name, 0, 1, text, size) == 0 ? 0 : -1);
}
// 0x10002e18 (440)
TbError TbRegistryKey::GetValueString(const char* name, char* buffer, uint32_t length, const char* fallback) const {
    uint32_t size = length == 0 ? GetValueSize(name) : length;
    TbRegItemType type = GetItemType(name);
    if (type != kTbRegItemNone && uint32_t(type) - 1u < 2u && GetValue(name, buffer, size).Succeeded())
        return TbError(0);
    if (fallback == nullptr) return TbError(-1);
    if (length == 0) std::strcpy(buffer, fallback);
    else std::strncpy(buffer, fallback, length);
    return TbError(-1);
}
// 0x10003010 (152)
TbError TbRegistryKey::SetValueULONG(const char* name, uint32_t value) const {
    if (mHandle == 0) return TbError(-1);
    if (name == nullptr) return TbError(-1);
    return TbError(RegSetValueExA(mHandle, name, 0, 4, &value, 4) == 0 ? 0 : -1);
}
// 0x100030e0 (332)
TbError TbRegistryKey::GetValueULONG(const char* name, uint32_t& value) const {
    uint32_t raw;
    if (GetValue(name, &raw, 4).Succeeded()) {
        TbRegItemType type = GetItemType(name);
        if (type == 6) {
            value = (raw >> 24) | ((raw >> 8) & 0xff00) | ((raw << 8) & 0xff0000) | (raw << 24);
            return TbError(0);
        }
        if (type >= 4 && type < 6) {
            value = raw;
            return TbError(0);
        }
    }
    return TbError(-1);
}
// 0x10003268 (424)
TbError TbRegistryKey::GetValueULONG(const char* name, uint32_t& value, uint32_t fallback) const {
    if (GetValueULONG(name, value).Succeeded()) return TbError(0);
    value = fallback;
    return TbError(-1);
}
// 0x1000344c (288)
TbError TbRegistryKey::OpenSubKey(const char* name, TbRegistryKey& sub) const {
    HKEY handle = _OpenSubKey(name, mHandle);
    if (handle == 0) return TbError(-1);
    if (sub.mHandle != 0 && !sub.CloseKey().Succeeded()) {
        RegCloseKey(handle);
        return TbError(-1);
    }
    sub.mHandle = handle;
    return TbError(0);
}
// 0x100035b0 (304)
TbError TbRegistryKey::CreateSubKey(const char* name, TbRegistryKey& sub) {
    HKEY handle = _CreateSubKey(name, mHandle, 0);
    if (handle == 0) return TbError(-1);
    if (sub.mHandle != 0 && !sub.CloseKey().Succeeded()) {
        RegCloseKey(handle);
        return TbError(-1);
    }
    sub.mHandle = handle;
    return TbError(0);
}
// 0x10003724 (148)
TbRegItemType TbRegistryKey::GetItemType(const char* name) const {
    uint32_t type;
    if (RegQueryValueExA(mHandle, name, nullptr, &type, nullptr, nullptr) == 0)
        return static_cast<TbRegItemType>(type);
    HKEY handle;
    if (RegOpenKeyA(mHandle, name, &handle) == 0) {
        RegCloseKey(handle);
        return kTbRegItemKey;
    }
    return kTbRegItemNone;
}
// 0x100037ec (12)
TbRegistryKey::TbRegistryKey() : mHandle(0) {}
// 0x10003824 (668)
TbError TbRegistryKey::ListItems(TbCharStringList& list, uint32_t flags) {
    uint32_t subKeys, maxSubKeyLen, values, maxValueNameLen;
    if (RegQueryInfoKeyA(mHandle, nullptr, nullptr, nullptr, &subKeys, &maxSubKeyLen,
                        nullptr, &values, &maxValueNameLen, nullptr, nullptr, nullptr) != 0)
        return TbError(-1);
    uint32_t length = (maxValueNameLen < maxSubKeyLen ? maxSubKeyLen : maxValueNameLen) + 1;
    char* buffer = static_cast<char*>(LbMem_Alloc(length));
    if (buffer == nullptr) return TbError(-1);
    if (flags & 8) {
        for (uint32_t i = 0; subKeys-- != 0; ++i) {
            uint32_t n = length;
            if (RegEnumKeyExA(mHandle, i, buffer, &n, nullptr, nullptr, nullptr, nullptr) != 0) {
                LbMem_Free(buffer);
                return TbError(-1);
            }
            TbDynamicStringTemplate<char> text(buffer);
            list.AddTail(text);
        }
    }
    if (flags & ~8u) {
        for (uint32_t i = 0; values-- != 0; ++i) {
            uint32_t n = length, type;
            if (RegEnumValueA(mHandle, i, buffer, &n, nullptr, &type, nullptr, nullptr) != 0) {
                LbMem_Free(buffer);
                return TbError(-1);
            }
            if (((flags & 1) && type == 1) || ((flags & 2) && type >= 4 && type <= 6) ||
                ((flags & 4) && type == 3) || ((flags & 0x10) && (type == 0 || type >= 7))) {
                TbDynamicStringTemplate<char> text(buffer);
                list.AddTail(text);
            }
        }
    }
    LbMem_Free(buffer);
    return TbError(0);
}
// 0x10003b04 (320)
TbError TbRegistryKey::DeleteItem(const char* name) {
    if (name == nullptr) return TbError(-1);
    if (mHandle == 0) return TbError(-1);
    TbRegItemType type = GetItemType(name);
    if (type == kTbRegItemNone) return TbError(-1);
    return TbError((type == kTbRegItemKey ? RegDeleteKeyA(mHandle, name) : RegDeleteValueA(mHandle, name)) == 0 ? 0 : -1);
}
