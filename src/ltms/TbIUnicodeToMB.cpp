// TbIUnicodeToMB.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of TbIUnicodeToMB code, named after it.

#include "bfl_FileSystem.h"
#include "helpers/TbFontRenderParamsHelpers.h"
#include "platform/ltms.h"
#include "platform/samsutils.h"
#include "TbIResourceIndex.h"
#include "TbIRLE4bitFont.h"
#include "TbITable.h"
#include "TbResource.h"
#include <cstring>

// ltms: the Unicode to multibyte converter (ltms shared, 0x100055fc..0x100059ac). Its
// loader is in TbResourceFactories.cpp.

TbIUnicodeToMB::~TbIUnicodeToMB() {}

uint16_t TbIUnicodeToMB::LocateValue(char16_t value) {
    const auto* node = mData;
    for (;;) {
        if (value < node[0]) {
            if (node[1] == 0) return 0;
            node = reinterpret_cast<const uint16_t*>(reinterpret_cast<const uint8_t*>(node) + node[1]);
        } else if (value > node[2]) {
            if (node[3] == 0) return 0;
            node = reinterpret_cast<const uint16_t*>(reinterpret_cast<const uint8_t*>(node) + node[3]);
        } else {
            return node[4 + (uint16_t(value) - node[0])];
        }
    }
}

// ltms: the resource factories. TbIResourceIndex::OpenResource opens a resource's file and
// hands it to the factory of the resource's type (gLtmsResourceFactories), which reads the
// file and builds the object, usually at the start of one allocation with the file's data
// behind it. File data is little-endian; the Mac swaps it after reading.

// 0x100057f4: the magic word "BFUM", then the range tree to the end of the file.
TbError UnicodeToMBFactory::CreateInterface(bfl::FileSystem::CFile& file, void** out, uint32_t,
                                            TbIResourceIndex*) const {
    uint32_t magic;
    if (file.Read(&magic, 4) != 4) return TbError(-1);
    SamsUtilities::USwapInplace32(magic);
    if (magic != 0x4d554642u) return TbError(-1);
    const uint32_t size = Remaining(file);
    auto* block = new uint8_t[sizeof(TbIUnicodeToMB) + size];
    auto* data = block + sizeof(TbIUnicodeToMB);
    if (static_cast<uint32_t>(file.Read(data, size)) != size) {
        delete[] block;
        return TbError(-1);
    }
    auto* converter = new (block) TbIUnicodeToMB(data);
    SamsUtilities::_USwapBlock16(reinterpret_cast<uint16_t*>(data), static_cast<int32_t>(size >> 1));
    *out = converter;
    return TbError(0);
}

TbError TbIUnicodeToMB::ConvertString(unsigned char* out, const char16_t* source, uint16_t size) {
    uint16_t remaining = static_cast<uint16_t>(size - 2);
    while (*source != 0) {
        const auto before = remaining--;
        if (before == 0) break;
        const auto value = LocateValue(*source++);
        *out++ = static_cast<unsigned char>(value);
        if ((value & 0xff00) != 0 && remaining != 0) {
            *out++ = static_cast<unsigned char>(value >> 8);
            --remaining;
        }
    }
    *out = 0;
    return TbError(remaining == 0 && *source != 0 ? -1 : 0);
}
