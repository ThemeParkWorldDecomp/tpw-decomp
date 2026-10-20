// The size half of the bullfrog library's zlib unpacker (bullfrog symbols shared 0x1002512c and 0x100258a0; the rest is
// src/lb_zlib.cpp). Unit w108 (the save file reader) asks it how big a packed block unpacks to.
#include "bfl_ZLib.h"

// 0x1002512c
uint32_t bfl::compress::CZLib::DecompressedSize(uint8_t* source) const {
    ZLibCompHeader* header = reinterpret_cast<ZLibCompHeader*>(source);
    SwapHeader(header);
    return header->mTag == kZLibTag ? header->mUnpackedSize : 0;
}

// 0x100258a0
uint32_t LbZLib_MemDecompressedSize(void* source) {
    static bfl::compress::CZLib decompressor;   // the library's one object, built on first use (stateless here)
    return decompressor.DecompressedSize(static_cast<uint8_t*>(source));
}
