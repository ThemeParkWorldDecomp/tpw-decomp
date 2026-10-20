// Test entry points for the version 0x12 wavelet decoder (library unit_WaveletDecodeV12:
// src/Wavelet_decode_v12.cpp, src/Wavelet_decode.cpp for ReverseTransformVert, CIgArray and
// the version dispatch, and src/tb_resource_texture.cpp for CTextureSettings::GetAlphaMask,
// which the decoder calls). tests/test_WaveletDecodeV12.py drives them.
//
// Fakes: LbZLib_MemDecompress (the bullfrog library's zlib unpacker, untranslated) records
// its three pointers and writes the bytes the test planted for that call into its
// destination. The rest of CWaveletImp is src/Wavelet.cpp's (unit EngineStartup) or
// untranslated; its vtable still needs every virtual member, so they are defined here (none
// is reached). The decoder's workspace pointer belongs to src/Wavelet.cpp too; the test points
// it at a buffer.
#include <cstdint>
#include <cstring>
#include <vector>

#include "Wavelet.h"
#include "calllog.h"

using namespace calllog;

CWavelet::~CWavelet() {}

// Wavelet.cpp's constructor also allocates the work buffer (mBuffer); the test sets it.
CWaveletImp::CWaveletImp() : mReferences(1), mBuffer(nullptr) {}
CWaveletImp::~CWaveletImp() {}
// Not reached from the unit (the vtable needs them).
int32_t CWaveletImp::AddRef() { return ++mReferences; }
int32_t CWaveletImp::GetRefCount() { return mReferences; }
int32_t CWaveletImp::Release() { return --mReferences; }
// The fast path's decoders (untranslated; the dispatcher is src/Wavelet_decode.cpp's).
void CWaveletImp::DecompressColorFast(uint32_t, uint32_t, const uint8_t*, uint8_t*, float, float, float) {
    ev("DecompressColorFast");
}
void CWaveletImp::DecompressMonoFast(uint32_t, uint32_t, const uint8_t*, uint8_t*, float) {
    ev("DecompressMonoFast");
}

uint8_t* gWaveletWorkspace = nullptr;

namespace {
// What the unpacker fake writes, one entry per call in order; an entry that is not `present`
// writes nothing (a block without the "BILZ" tag).
struct Payload {
    bool present;
    std::vector<uint8_t> bytes;
};
std::vector<Payload> gPayloads;
size_t gPayloadNext = 0;

// The decompressor object the entry points call (label 1 in the call log).
CWaveletImp* Object() {
    static CWaveletImp* object = new CWaveletImp;
    return object;
}
}  // namespace

// The unpacker: records source, destination and workspace, writes the next planted payload
// and answers from the queue "zlib".
uint32_t LbZLib_MemDecompress(void* source, void* dest, void* workspace) {
    ev("LbZLib_MemDecompress", {lab(source), lab(dest), lab(workspace)});
    if (gPayloadNext < gPayloads.size()) {
        const Payload& payload = gPayloads[gPayloadNext];
        if (payload.present && !payload.bytes.empty()) std::memcpy(dest, payload.bytes.data(), payload.bytes.size());
    }
    ++gPayloadNext;
    return static_cast<uint32_t>(ret("zlib"));
}

extern "C" {

// The addresses of the decoder's statics, for the test to set and read.
void* tpw_wd12_global(const char* name) {
    if (std::strcmp(name, "planes") == 0) return gWaveletPlanes;
    if (std::strcmp(name, "h0") == 0) return &gWaveletH0;
    if (std::strcmp(name, "h1") == 0) return &gWaveletH1;
    if (std::strcmp(name, "h2") == 0) return &gWaveletH2;
    if (std::strcmp(name, "h3") == 0) return &gWaveletH3;
    if (std::strcmp(name, "count") == 0) return &gLzssBitCount;
    if (std::strcmp(name, "buffer") == 0) return &gLzssBitBuffer;
    if (std::strcmp(name, "unused") == 0) return &gLzssUnused;
    if (std::strcmp(name, "pos") == 0) return &gLzssSourcePos;
    if (std::strcmp(name, "dest") == 0) return &gLzssDest;
    if (std::strcmp(name, "source") == 0) return &gLzssSource;
    if (std::strcmp(name, "workspace") == 0) return &gWaveletWorkspace;
    if (std::strcmp(name, "object") == 0) return Object();
    return nullptr;
}

// Points the decompressor's mBuffer (zlib's memory, which only the unpacker sees).
void tpw_wd12_set_buffer(void* buffer) { Object()->mBuffer = static_cast<uint8_t*>(buffer); }

// Forgets the planted payloads.
void tpw_wd12_payload_reset() {
    gPayloads.clear();
    gPayloadNext = 0;
}

// Plants the next payload: `size` bytes, or none at all for a negative size.
void tpw_wd12_payload(const void* bytes, int32_t size) {
    Payload payload{size >= 0, {}};
    if (size > 0) {
        const uint8_t* from = static_cast<const uint8_t*>(bytes);
        payload.bytes.assign(from, from + size);
    }
    gPayloads.push_back(payload);
}

// Blocks handled by operator new[], delete[] and delete since the last call (and clears them).
void tpw_wd12_heap(int32_t* out) {
    out[0] = tpw_cl_heap[1];
    out[1] = tpw_cl_heap[3];
    out[2] = tpw_cl_heap[2];
    for (int32_t& count : tpw_cl_heap) count = 0;
}

int32_t tpw_wd12_v12(int32_t width, int32_t height, int32_t bytesPerPixel, void* data, void* alpha, void* dest,
                     float s1, float s2, float s3, float s4) {
    return Object()->Decompress_v12(width, height, bytesPerPixel, data, alpha, dest, s1, s2, s3, s4);
}

int32_t tpw_wd12_dispatch(int32_t version, int32_t width, int32_t height, int32_t bytesPerPixel, void* data,
                          void* alpha, void* dest, float s1, float s2, float s3, float s4) {
    return Object()->Decompress(version, width, height, bytesPerPixel, data, alpha, dest, s1, s2, s3, s4);
}

}  // extern "C"
