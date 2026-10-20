// ltms: the resource factories. TbIResourceIndex::OpenResource opens a resource's file and
// hands it to the factory of the resource's type (gLtmsResourceFactories), which reads the
// file and builds the object, usually at the start of one allocation with the file's data
// behind it. File data is little-endian; the Mac swaps it after reading.
#include "TbIRLE4bitFont.h"
#include "TbIResourceIndex.h"
#include "TbITable.h"
#include "TbResource.h"
#include "bfl_FileSystem.h"
#include "platform/ltms.h"
#include "platform/samsutils.h"

#include <cstring>

namespace {
// The header both character-set files start with (little-endian in the file).
struct MBHeader {
    uint32_t mMagic;      // 'BFMB' for the plain description, 'BFMU' or 'BFMH' with a table
    uint16_t mLead;       // low byte: 0xff minus the first lead byte
    uint16_t mCounts;     // see the factories
};

void SwapHeader(MBHeader& header) {
    SamsUtilities::USwapInplace32(header.mMagic);
    SamsUtilities::USwapInplace16(header.mLead);
    SamsUtilities::USwapInplace16(header.mCounts);
}
// What is left of the file after the header.
uint32_t Remaining(bfl::FileSystem::CFile& file) {
    return static_cast<uint32_t>(file.GetSize()) - static_cast<uint32_t>(file.GetPos());
}
}  // namespace

// 0x10002d34: the 8-byte header alone. A bad file is closed here (the other factories
// leave that to their caller).
TbError MBInfoFactory::CreateInterface(bfl::FileSystem::CFile& file, void** out, uint32_t, TbIResourceIndex*) const {
    MBHeader header;
    if (file.Read(&header, 8) != 8) {
        file.Close();
        return TbError(-1);
    }
    SwapHeader(header);
    if (header.mMagic != 0x424d4642u) {   // "BFMB"
        file.Close();
        return TbError(-1);
    }
    *out = new TbIMBInfo(static_cast<uint8_t>(-1 - static_cast<int8_t>(header.mLead & 0xff)), header.mCounts);
    return TbError(0);
}

// 0x10002fbc: the header, then the table of 16-bit values to the end of the file. The
// 'BFMH' variant builds the same object and still reports failure.
TbError MBToUnicodeFactory::CreateInterface(bfl::FileSystem::CFile& file, void** out, uint32_t,
                                            TbIResourceIndex*) const {
    MBHeader header;
    if (file.Read(&header, 8) != 8) return TbError(-1);
    SwapHeader(header);
    if (header.mMagic != 0x554d4642u && header.mMagic != 0x484d4642u) return TbError(-1);   // "BFMU", "BFMH"
    const uint16_t firstLead = static_cast<uint16_t>(0xff - (header.mLead & 0xff));
    const uint32_t lastLead = header.mCounts & 0xff;
    const uint32_t lastTrail = (header.mCounts >> 8) & 0xff;
    const uint32_t size = Remaining(file);
    auto* block = new uint8_t[sizeof(TbIMBToUnicode) + size];
    auto* table = block + sizeof(TbIMBToUnicode);
    if (static_cast<uint32_t>(file.Read(table, size)) != size) {
        delete[] block;
        return TbError(-1);
    }
    SamsUtilities::_USwapBlock16(reinterpret_cast<uint16_t*>(table), static_cast<int32_t>(size >> 1));
    const uint32_t count = (((lastLead - 1) & 0xff) * 0xff + (firstLead & 0xff) * static_cast<uint32_t>(-0xfe) + lastTrail) - 1;
    *out = new (block) TbIMBToUnicode(static_cast<uint8_t>(firstLead), static_cast<uint16_t>(count), table);
    return TbError(header.mMagic == 0x484d4642u ? -1 : 0);
}

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

// 0x100053fc: {u32 "BFST", u32 id, u32 string count}, then the offset table and the
// strings to the end of the file. The id in the file must be the one asked for.
TbError TableFactory::CreateInterface(bfl::FileSystem::CFile& file, void** out, uint32_t id,
                                      TbIResourceIndex* index) const {
    uint32_t header[3];
    if (file.Read(header, 12) != 12) return TbError(-1);
    SamsUtilities::USwapInplace32(header[0]);
    SamsUtilities::USwapInplace32(header[1]);
    SamsUtilities::USwapInplace32(header[2]);
    if (header[0] != 0x54534642u) return TbError(-1);
    if (header[1] != id) return TbError(-1);
    const uint32_t size = Remaining(file);
    auto* block = new uint8_t[sizeof(TbITable) + size];
    auto* data = block + sizeof(TbITable);
    if (static_cast<uint32_t>(file.Read(data, size)) != size) {
        delete[] block;
        return TbError(-1);
    }
    *out = new (block) TbITable(header[2], data, index);
    return TbError(0);
}

// 0x1000476c: the whole file (from wherever it stands: the size is not reduced by the
// position) behind the object; the magic word "F4FB" is checked after reading.
TbError RLE4bitFontFactory::CreateInterface(bfl::FileSystem::CFile& file, void** out, uint32_t,
                                            TbIResourceIndex* index) const {
    const uint32_t size = static_cast<uint32_t>(file.GetSize());
    auto* block = new uint8_t[sizeof(TbIRLE4bitFont) + size];
    auto* data = block + sizeof(TbIRLE4bitFont);
    if (static_cast<uint32_t>(file.Read(data, size)) != size) {
        delete[] block;
        return TbError(-1);
    }
    uint32_t magic;
    std::memcpy(&magic, data, 4);
    SamsUtilities::USwapInplace32(magic);
    if (magic != 0x42463446u) {
        delete[] block;
        return TbError(-1);
    }
    *out = new (block) TbIRLE4bitFont(data, index);
    return TbError(0);
}
