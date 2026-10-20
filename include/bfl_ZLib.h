// The bullfrog library's zlib unpacker (src/lb_zlib.cpp; "bullfrog symbols shared", LbMaths.cpp range of the inventory).
// PARTIAL: bfl::compress::CZLib is a class with a vtable on the Mac (the compression interface of bfl_Refpack.h plus the
// buffered calls; vtable 0x100730f4). The packing and unpacking members are translated. The Mac
// builds the one object lazily inside LbZLib_MemDecompress; the port has no need for a virtual call, so the members are
// plain. The inflate itself is the host's zlib, called with the window bits of the block's header.
#pragma once
#include <cstdint>

// The header of a packed block (the original's @unnamed@::_ZLibComp_Header, 0x1c bytes), as the file has it: little-endian
// words. The Mac reads them as big-endian words and swaps them in place (SwapHeader); the port reads them as they are.
struct ZLibCompHeader {
    uint32_t mTag;            // 0x00  "BILZ" in the file: 0x5a4c4942 once swapped
    uint32_t mUnpackedSize;   // 0x04  bytes the stream inflates to
    uint32_t mPackedSize;     // 0x08  bytes of the stream after the header
    uint32_t mWindowBits;     // 0x0c  zlib's window bits (15 in every block of the game)
    uint32_t mLevel;          // 0x10  memLevel (9); the compression level is -1
    uint32_t mF14;            // 0x14  0
    uint32_t mSwapped;        // 0x18  0 in a file; SwapHeader leaves kZLibSwapMarker here
};
static_assert(sizeof(ZLibCompHeader) == 0x1c, "the packed block's header is 0x1c bytes");
constexpr uint32_t kZLibTag = 0x5a4c4942;           // mTag of a packed block
constexpr uint32_t kZLibSwapMarker = 0x53574150;    // "SWAP": mSwapped once the header's words have been swapped

// 0x10024c5c (112 bytes): puts the header's first five words in the host's order, in place, unless mSwapped already
// holds the marker, and then stores the marker. (A little-endian host has the words right already: only the marker is
// written. A block whose file bytes already read as the marker on a big-endian machine would not be swapped by the
// original and would then fail the tag test; the port, which cannot see the difference, does not reproduce that.)
void SwapHeader(ZLibCompHeader* header);

namespace bfl::compress {

class CBufferCallback;
class CZLib {
public:
    // 0x10024efc (332 bytes): `source` is a block that starts with the 0x1c-byte "BILZ" header (tag, unpacked size,
    // packed size, zlib window bits, level, 0, 0) and goes on with the zlib stream; the stream is inflated into `dest`,
    // which has room for the header's unpacked size, using `workspace` as zlib's memory (a block of its own when NULL).
    // SwapHeader runs on the header first. Answers the number of bytes written when the stream ended exactly there, else
    // 0 (a wrong tag, window bits outside 8 to 15 and -8 to -15 (zlib 1.1.3's range: the negative ones are raw streams), a
    // failed or incomplete inflate, no memory); the destination may be written in part then.
    // The input length is not bounded (the original passes 0xc0000000): a truncated stream reads on past the block.
    // The workspace is the one the OTHER size says: the original asks CompressWorkspaceSize (vtable slot 0x14, not the
    // DecompressWorkspaceSize next to it), and that is how long its own block is and where its zlib arena ends.
    int32_t MemDecompress(uint8_t* dest, uint8_t* source, uint8_t* workspace) const;
    // 0x1002508c (12 bytes): 0x62a39.
    int32_t CompressWorkspaceSize() const;
    // 0x1002512c (76 bytes, src/lb_zlib_size.cpp): the unpacked size the block's header gives, 0 when the tag is not "BILZ".
    // SwapHeader runs on the header first.
    uint32_t DecompressedSize(uint8_t* source) const;

    // Unit w216 (src/lb_zlib_compress.cpp).
    uint32_t MemCompress(uint8_t* dest, const uint8_t* source, uint32_t length, uint8_t* workspace) const; // 0x10024d0c
    uint32_t BufferedCompress(const uint8_t* source, uint32_t srcLength, uint8_t* buffer, uint32_t bufferSize,
                              uint8_t* workspace, CBufferCallback& callback) const; // 0x100251b8
    uint32_t BufferedDecompress(uint8_t* dest, uint8_t* buffer, uint32_t bufferSize, uint8_t* workspace,
                                CBufferCallback& callback) const; // 0x100253e4
    int32_t DecompressWorkspaceSize() const; // 0x100250dc
    // 0x100255d8 (100 bytes): the native plain class has no vtable stores.
    ~CZLib() {}
};

// Unit w216: invented typedef name for the original C callback signature.
using ZLibBufferFunction = int32_t (*)(void* buffer, uint32_t size, void* context);

// Plain in the port, independent of CZLib; Mac base vtable 0x1007311c.
class CBufferedCompressionBase {
public:
    ~CBufferedCompressionBase(); // 0x1002566c
};

// Mac vtable 0x10073174, slots +8 destructor, +0xc full, +0x10 empty.
class CBufferCallback {
public:
    virtual ~CBufferCallback(); // 0x10025c8c
    virtual int32_t BufferFull(uint8_t* buffer, uint32_t size) = 0;
    virtual int32_t BufferEmpty(uint8_t* buffer, uint32_t size) = 0;
};

// Original @unnamed@::Callbacks (vtable 0x10073160), exposed here for the wrapper tests.
class Callbacks : public CBufferCallback {
public:
    Callbacks(ZLibBufferFunction full, ZLibBufferFunction empty, void* context)
        : mFull(full), mEmpty(empty), mContext(context) {}
    ~Callbacks() override; // 0x10025c00
    int32_t BufferFull(uint8_t* buffer, uint32_t size) override; // 0x10025e24
    int32_t BufferEmpty(uint8_t* buffer, uint32_t size) override; // 0x10025e98
    ZLibBufferFunction mFull;  // Mac +4
    ZLibBufferFunction mEmpty; // Mac +8
    void* mContext;           // Mac +0xc
};

}  // namespace bfl::compress

// 0x100257d8 (152 bytes): the bullfrog library's export that the engine's version 0x12 wavelet decoder (Decompress_v12)
// calls for each of its two blocks: CZLib::MemDecompress(dest, source, workspace) on the library's one CZLib object.
// Declared with the same signature in include/Wavelet.h (unit w19). The original returns what MemDecompress answers.
uint32_t LbZLib_MemDecompress(void* source, void* dest, void* workspace);

// 0x100258a0 (152 bytes, src/lb_zlib_size.cpp): CZLib::DecompressedSize(source) on the library's one CZLib object. The save file
// reader (CSaveLoad::Load) asks it how big the packed block it has read unpacks to; 0 is a block that is not one.
uint32_t LbZLib_MemDecompressedSize(void* source);

// Unit w216 (src/lb_zlib_compress.cpp).
uint32_t LbZLib_MemCompress(void* source, void* dest, uint32_t length, void* workspace); // 0x10025708
uint32_t LbZLib_BufferedCompress(void* source, void* buffer, uint32_t srcLength, uint32_t bufferSize,
                                bfl::compress::ZLibBufferFunction full, void* context, void* workspace); // 0x10025ae8
uint32_t LbZLib_BufferedDecompress(void* dest, void* buffer, uint32_t bufferSize,
                                  bfl::compress::ZLibBufferFunction empty, void* context, void* workspace); // 0x10025d14
uint32_t LbZLib_CompressWorkspaceSize();   // 0x1002596c
uint32_t LbZLib_DeCompressWorkspaceSize(); // 0x10025a28
