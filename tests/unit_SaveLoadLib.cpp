// Test library for the three small bullfrog functions unit w108 added (libtpw_unit_SaveLoadLib.dylib; see
// tests/test_SaveLoadLib.py): src/bullfrog/LbFile.cpp (LbFile_CompareFileStamps) and src/bullfrog/LbZLib.cpp (CZLib::DecompressedSize and
// LbZLib_MemDecompressedSize). Fakes that record through calllog: the platform's NS_MacDoze::CompareFileTime (answers the queue
// "CompareFileTime") and SwapHeader (the host's: it only marks the header, as src/bullfrog/LbZLib.cpp's does on a little endian host).
#include <cstdint>
#include <cstring>

#include "LbFile.h"
#include "bfl_ZLib.h"
#include "calllog.h"
#include "platform/macdoze.h"

using calllog::ev;
using calllog::lab;
using calllog::ret;

int32_t NS_MacDoze::CompareFileTime(const tagFILETIME* a, const tagFILETIME* b) {
    ev("CompareFileTime", {a->dwLowDateTime, a->dwHighDateTime, b->dwLowDateTime, b->dwHighDateTime});
    return static_cast<int32_t>(ret("CompareFileTime"));
}

void SwapHeader(ZLibCompHeader* header) {
    if (header->mSwapped == kZLibSwapMarker) return;
    header->mSwapped = kZLibSwapMarker;
}

extern "C" {

int32_t tpw_slib_stamps(const uint32_t* a, const uint32_t* b) {
    return LbFile_CompareFileStamps(reinterpret_cast<const TbFileStamp*>(a), reinterpret_cast<const TbFileStamp*>(b));
}
uint32_t tpw_slib_decompressed_size(uint8_t* source) {
    static const bfl::compress::CZLib decompressor;
    return decompressor.DecompressedSize(source);
}
uint32_t tpw_slib_memdecompressed_size(void* source) { return LbZLib_MemDecompressedSize(source); }

}  // extern "C"
