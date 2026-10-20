// Unit w216, bullfrog LbMaths.cpp range: packing, buffered unpacking and C callbacks.
#include "bfl_ZLib.h"
#include <new>
#include <zlib.h>

namespace {
struct ZLibArena {
    uint8_t* mNext;
    uint8_t* mEnd;
};
// Copies of the unpacker's arena helpers, 0x10024bcc and 0x10024c2c.
voidpf ZLibAlloc(voidpf opaque, uInt items, uInt size) {
    ZLibArena* arena = static_cast<ZLibArena*>(opaque);
    uint8_t* block = arena->mNext;
    arena->mNext += static_cast<uint32_t>(items * size);
    return arena->mNext > arena->mEnd ? nullptr : block;
}
void ZLibFree(voidpf, voidpf) {}

// Packed headers differ between the memory and buffered compressors on the Mac.
void WriteWord(uint8_t* dest, uint32_t word, bool bigEndian) {
    for (uint32_t i = 0; i < 4; ++i) dest[i] = static_cast<uint8_t>(word >> ((bigEndian ? 3 - i : i) * 8));
}
void WriteHeader(uint8_t* dest, uint32_t length, uint32_t packed, bool bigEndian) {
    WriteWord(dest, kZLibTag, bigEndian);
    WriteWord(dest + 4, length, bigEndian);
    WriteWord(dest + 8, packed, bigEndian);
    WriteWord(dest + 12, 15, bigEndian);
    WriteWord(dest + 16, 9, bigEndian);
    WriteWord(dest + 20, 0, bigEndian);
    WriteWord(dest + 24, 0, bigEndian);
}
} // namespace

namespace bfl::compress {
// 0x10024d0c (428 bytes).
uint32_t CZLib::MemCompress(uint8_t* dest, const uint8_t* source, uint32_t length, uint8_t* workspace) const {
    uint32_t result = 0;
    if (length > 0x1c) {
        const uint32_t size = static_cast<uint32_t>(CompressWorkspaceSize());
        uint8_t* owned = nullptr;
        if (workspace == nullptr) workspace = owned = new (std::nothrow) uint8_t[size];
        if (workspace != nullptr) {
            ZLibArena arena = {workspace, workspace + size};
            z_stream stream = {};
            stream.zalloc = ZLibAlloc;
            stream.zfree = ZLibFree;
            stream.opaque = &arena;
            if (deflateInit2(&stream, -1, 8, 15, 9, 0) == Z_OK) {
                stream.next_in = const_cast<uint8_t*>(source);
                stream.avail_in = length;
                stream.next_out = dest + 0x1c;
                stream.avail_out = length - 0x1c;
                if (deflate(&stream, Z_FINISH) == Z_STREAM_END) {
                    result = static_cast<uint32_t>(stream.total_out) + 0x1c;
                    WriteHeader(dest, length, result, false);
                }
                deflateEnd(&stream);
            }
        }
        if (owned != nullptr) delete[] owned;
    }
    return result;
}

// 0x100251b8 (448 bytes).
uint32_t CZLib::BufferedCompress(const uint8_t* source, uint32_t srcLength, uint8_t* buffer, uint32_t bufferSize,
                                uint8_t* workspace, CBufferCallback& callback) const {
    uint32_t result = 0;
    const uint32_t size = static_cast<uint32_t>(CompressWorkspaceSize());
    uint8_t* owned = nullptr;
    if (workspace == nullptr) workspace = owned = new (std::nothrow) uint8_t[size];
    if (workspace != nullptr) {
        ZLibArena arena = {workspace, workspace + size};
        z_stream stream = {};
        stream.zalloc = ZLibAlloc;
        stream.zfree = ZLibFree;
        stream.opaque = &arena;
        if (deflateInit2(&stream, -1, 8, 15, 9, 0) == Z_OK) {
            WriteHeader(buffer, srcLength, 0, true);
            stream.next_in = const_cast<uint8_t*>(source);
            stream.avail_in = srcLength;
            stream.next_out = buffer + 0x1c;
            stream.avail_out = bufferSize - 0x1c;
            int status;
            while ((status = deflate(&stream, Z_FINISH)) == Z_OK) {
                callback.BufferFull(buffer, bufferSize - stream.avail_out);
                stream.next_out = buffer;
                stream.avail_out = bufferSize;
            }
            if (bufferSize != stream.avail_out) callback.BufferFull(buffer, bufferSize - stream.avail_out);
            if (status == Z_STREAM_END) {
                result = static_cast<uint32_t>(stream.total_out) + 0x1c;
                WriteWord(buffer + 8, result, true);
            }
            deflateEnd(&stream);
        }
    }
    if (owned != nullptr) delete[] owned;
    return result;
}

// 0x100253e4 (392 bytes). The original asks for the compression workspace here too.
uint32_t CZLib::BufferedDecompress(uint8_t* dest, uint8_t* buffer, uint32_t bufferSize, uint8_t* workspace,
                                  CBufferCallback& callback) const {
    uint32_t result = 0;
    if (bufferSize > 0x1d) {
        const uint32_t size = static_cast<uint32_t>(CompressWorkspaceSize());
        uint8_t* owned = nullptr;
        if (workspace == nullptr) workspace = owned = new (std::nothrow) uint8_t[size];
        if (workspace != nullptr) {
            ZLibArena arena = {workspace, workspace + size};
            z_stream stream = {};
            stream.zalloc = ZLibAlloc;
            stream.zfree = ZLibFree;
            stream.opaque = &arena;
            callback.BufferEmpty(buffer, bufferSize);
            auto* header = reinterpret_cast<ZLibCompHeader*>(buffer);
            SwapHeader(header);
            if (header->mTag == kZLibTag && inflateInit2(&stream, 15) == Z_OK) {
                stream.next_in = buffer + 0x1c;
                stream.avail_in = bufferSize - 0x1c;
                stream.next_out = dest;
                stream.avail_out = header->mUnpackedSize;
                int status;
                while ((status = inflate(&stream, Z_SYNC_FLUSH)) == Z_OK) {
                    callback.BufferEmpty(buffer, bufferSize);
                    stream.next_in = buffer;
                    stream.avail_in = bufferSize;
                }
                if (status == Z_STREAM_END) result = static_cast<uint32_t>(stream.total_out);
                inflateEnd(&stream);
            }
        }
        if (owned != nullptr) delete[] owned;
    }
    return result;
}

// 0x100250dc (12 bytes).
int32_t CZLib::DecompressWorkspaceSize() const { return 0x15388; }
// The Mac destructor flags and final frees are supplied by the native ABI.
CBufferedCompressionBase::~CBufferedCompressionBase() {} // 0x1002566c (88 bytes)
Callbacks::~Callbacks() {}                             // 0x10025c00 (88 bytes)
CBufferCallback::~CBufferCallback() {}                  // 0x10025c8c (76 bytes)
// 0x10025e24 (56 bytes).
int32_t Callbacks::BufferFull(uint8_t* buffer, uint32_t size) { return mFull(buffer, size, mContext); }
// 0x10025e98 (56 bytes).
int32_t Callbacks::BufferEmpty(uint8_t* buffer, uint32_t size) { return mEmpty(buffer, size, mContext); }
} // namespace bfl::compress

// Each stateless local stands for the Mac wrappers' one shared lazy object.
uint32_t LbZLib_MemCompress(void* source, void* dest, uint32_t length, void* workspace) { // 0x10025708 (160 bytes)
    static bfl::compress::CZLib compressor;
    return compressor.MemCompress(static_cast<uint8_t*>(dest), static_cast<uint8_t*>(source), length, static_cast<uint8_t*>(workspace));
}
uint32_t LbZLib_BufferedCompress(void* source, void* buffer, uint32_t srcLength, uint32_t bufferSize,
                                bfl::compress::ZLibBufferFunction full, void* context, void* workspace) { // 0x10025ae8 (212 bytes)
    bfl::compress::Callbacks callbacks(full, nullptr, context);
    static bfl::compress::CZLib compressor;
    return compressor.BufferedCompress(static_cast<uint8_t*>(source), srcLength, static_cast<uint8_t*>(buffer), bufferSize,
                                        static_cast<uint8_t*>(workspace), callbacks);
}
uint32_t LbZLib_BufferedDecompress(void* dest, void* buffer, uint32_t bufferSize,
                                  bfl::compress::ZLibBufferFunction empty, void* context, void* workspace) { // 0x10025d14 (204 bytes)
    bfl::compress::Callbacks callbacks(nullptr, empty, context);
    static bfl::compress::CZLib decompressor;
    return decompressor.BufferedDecompress(static_cast<uint8_t*>(dest), static_cast<uint8_t*>(buffer), bufferSize,
                                            static_cast<uint8_t*>(workspace), callbacks);
}
uint32_t LbZLib_CompressWorkspaceSize() { // 0x1002596c (136 bytes)
    static bfl::compress::CZLib compressor;
    return static_cast<uint32_t>(compressor.CompressWorkspaceSize());
}
uint32_t LbZLib_DeCompressWorkspaceSize() { // 0x10025a28 (136 bytes)
    static bfl::compress::CZLib decompressor;
    return static_cast<uint32_t>(decompressor.DecompressWorkspaceSize());
}
