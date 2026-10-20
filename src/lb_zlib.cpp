// The bullfrog library's zlib unpacker (bullfrog symbols shared 0x10024c5c..0x10025040 and 0x100257d8): the wrapper
// around zlib that unpacks the "BILZ" blocks of the version 0x12 wavelet textures. zlib itself is the host's.
#include "bfl_ZLib.h"

#include <new>

#include <zlib.h>

namespace {

// The unlimited input length the original hands zlib (a static it fills in on first use).
constexpr uInt kInputLength = 0xc0000000u;

// The memory zlib allocates from: a bump arena in the workspace (the original's opaque pointer points at two words on
// its stack: the next free byte and the end).
struct ZLibArena {
    uint8_t* mNext;
    uint8_t* mEnd;
};

// 0x10024bcc (48 bytes, @unnamed@::zLibAlloc): the next free bytes of the arena, or NULL when they are not there; the
// next pointer moves on either way. Nothing is aligned.
voidpf ZLibAlloc(voidpf opaque, uInt items, uInt size) {
    ZLibArena* arena = static_cast<ZLibArena*>(opaque);
    uint8_t* block = arena->mNext;
    arena->mNext += static_cast<uint32_t>(items * size);
    return arena->mNext > arena->mEnd ? nullptr : block;
}

// 0x10024c2c (4 bytes, @unnamed@::zLibFree): the arena is dropped as a whole.
void ZLibFree(voidpf, voidpf) {}

// The window sizes zlib 1.1.3's inflateInit2_ takes: 8 to 15 for a zlib stream, and the same negated for a raw one (no header
// or checksum). The host's zlib takes more (0 for the stream's own, 16 to 47 for gzip and automatic detection), which the
// original fails on, so the range is tested first.
bool WindowBitsValid(int32_t bits) { return (bits >= 8 && bits <= 15) || (bits <= -8 && bits >= -15); }

// A word of the file in the host's order.
void SwapWord(uint32_t& word) {
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    word = __builtin_bswap32(word);
#else
    (void)word;
#endif
}

}  // namespace

// 0x10024c5c
void SwapHeader(ZLibCompHeader* header) {
    if (header->mSwapped == kZLibSwapMarker) return;
    SwapWord(header->mTag);
    SwapWord(header->mUnpackedSize);
    SwapWord(header->mPackedSize);
    SwapWord(header->mWindowBits);
    SwapWord(header->mLevel);
    header->mSwapped = kZLibSwapMarker;
}

namespace bfl::compress {

// 0x1002508c
int32_t CZLib::CompressWorkspaceSize() const { return 0x62a39; }

// 0x10024efc
int32_t CZLib::MemDecompress(uint8_t* dest, uint8_t* source, uint8_t* workspace) const {
    uint32_t unpacked = 0;
    const uint32_t arenaSize = static_cast<uint32_t>(CompressWorkspaceSize());
    uint8_t* owned = nullptr;
    if (workspace == nullptr) {
        owned = new (std::nothrow) uint8_t[arenaSize];
        workspace = owned;
    }
    if (workspace != nullptr) {
        ZLibArena arena = {workspace, workspace + arenaSize};
        ZLibCompHeader* header = reinterpret_cast<ZLibCompHeader*>(source);
        z_stream stream = {};
        stream.zalloc = ZLibAlloc;
        stream.zfree = ZLibFree;
        stream.opaque = &arena;
        SwapHeader(header);
        const int32_t windowBits = static_cast<int32_t>(header->mWindowBits);
        if (header->mTag == kZLibTag && WindowBitsValid(windowBits) && inflateInit2(&stream, windowBits) == Z_OK) {
            stream.next_in = source + sizeof(ZLibCompHeader);
            stream.avail_in = kInputLength;
            stream.next_out = dest;
            stream.avail_out = header->mUnpackedSize;
            if (inflate(&stream, Z_FINISH) == Z_STREAM_END) unpacked = static_cast<uint32_t>(stream.total_out);
            inflateEnd(&stream);
        }
    }
    delete[] owned;
    return static_cast<int32_t>(unpacked);
}

}  // namespace bfl::compress

// 0x100257d8
uint32_t LbZLib_MemDecompress(void* source, void* dest, void* workspace) {
    static bfl::compress::CZLib decompressor;   // built on first use, as the original's guard does
    return static_cast<uint32_t>(decompressor.MemDecompress(static_cast<uint8_t*>(dest), static_cast<uint8_t*>(source),
                                                            static_cast<uint8_t*>(workspace)));
}
