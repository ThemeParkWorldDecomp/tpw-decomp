// Bullfrog registry wrapper, one Mac handle word. Unit w221.
#pragma once
#include "TbError.h"
#include "TbStringMatch.h"
#include "platform/macdoze.h"
#include "platform/indirectx.h"

// Enumerator names are invented; other raw REG numbers also pass through this type.
enum TbRegItemType : int32_t {
    kTbRegItemKey = -2,
    kTbRegItemNone = -1,
    kTbRegItemString = 1,
    kTbRegItemBinary = 3,
    kTbRegItemULong = 4
};

class TbRegistryKey {
public:
    TbRegistryKey(const TbRegistryKey&); // 0x10001e60
    TbRegistryKey& operator=(const TbRegistryKey&); // 0x10001ed0
    TbError OpenBullfrogGameKey(const char*, int32_t); // 0x10001f40
    TbError OpenEAGameKey(const char*, int32_t); // 0x1000207c
    TbError OpenKey(const char*); // 0x100021b4
    TbError CreateKey(const char*); // 0x10002300
    ~TbRegistryKey(); // 0x10002454
    TbError CloseKey(); // 0x100024e0
    void Initialise(); // 0x10002578
    // The original signatures use void* for the 32-bit HKEY.
    static NS_MacDoze::HKEY _OpenSubKey(const char*, NS_MacDoze::HKEY); // 0x100025b4
    static NS_MacDoze::HKEY _CreateSubKey(const char*, NS_MacDoze::HKEY, int32_t); // 0x1000263c
    TbError SetValue(const char*, TbRegItemType, const void*, uint32_t) const; // 0x100026e8
    TbError GetValue(const char*, void*, uint32_t) const; // 0x100027c0
    uint32_t GetValueSize(const char*) const; // 0x1000288c
    TbError SetValueGUID(const char*, const NS_MacDoze::_GUID&) const; // 0x10002928
    TbError GetValueGUID(const char*, NS_MacDoze::_GUID&) const; // 0x10002a2c
    TbError GetValueGUID(const char*, NS_MacDoze::_GUID&, const NS_MacDoze::_GUID&) const; // 0x10002c18
    TbError SetValueString(const char*, const char*) const; // 0x10002d28
    TbError GetValueString(const char*, char*, uint32_t, const char*) const; // 0x10002e18
    TbError SetValueULONG(const char*, uint32_t) const; // 0x10003010
    TbError GetValueULONG(const char*, uint32_t&) const; // 0x100030e0
    TbError GetValueULONG(const char*, uint32_t&, uint32_t) const; // 0x10003268
    TbError OpenSubKey(const char*, TbRegistryKey&) const; // 0x1000344c
    TbError CreateSubKey(const char*, TbRegistryKey&); // 0x100035b0
    TbRegItemType GetItemType(const char*) const; // 0x10003724
    TbRegistryKey(); // 0x100037ec
    TbError ListItems(TbCharStringList&, uint32_t); // 0x10003824
    TbError DeleteItem(const char*); // 0x10003b04

    NS_MacDoze::HKEY mHandle; // 0x00
};
static_assert(sizeof(TbRegistryKey) == 4);
