// Test entry points for the version 0x13 wavelet decoder (library unit_WaveletDecode:
// src/engine/EngineUtil.cpp, src/engine/Wavelet.cpp, and src/engine/tb_resource_texture.cpp for CTextureSettings::GetAlphaMask,
// which the decoder calls). tests/test_WaveletDecode.py drives them.
//
// Fakes: the rest of CWaveletImp is src/engine/Wavelet.cpp's (unit EngineStartup) or untranslated;
// its vtable still needs every virtual member, so they are defined here. The only one the
// unit can reach is Decompress_v12 (the dispatch's version 0x12), a recording fake. The
// decoder's workspace pointer belongs to src/engine/EngineUtil.cpp, src/engine/Wavelet.cpp too; the test points it at a buffer.
#include <cstdint>
#include <cstring>
#include <new>

#include "IgArray.h"
#include "Wavelet.h"
#include "calllog.h"

using namespace calllog;

CWavelet::~CWavelet() {}

// Wavelet.cpp's constructor also allocates the work buffer; the decoder never uses it.
CWaveletImp::CWaveletImp() : mReferences(1), mBuffer(nullptr) {}
CWaveletImp::~CWaveletImp() {}
// Not reached from the unit (the vtable needs them).
int32_t CWaveletImp::AddRef() { return ++mReferences; }
int32_t CWaveletImp::GetRefCount() { return mReferences; }
int32_t CWaveletImp::Release() { return --mReferences; }
// The fast path's decoders (untranslated; the dispatcher is src/engine/EngineUtil.cpp, src/engine/Wavelet.cpp's).
void CWaveletImp::DecompressColorFast(uint32_t, uint32_t, const uint8_t*, uint8_t*, float, float, float) {
    ev("DecompressColorFast");
}
void CWaveletImp::DecompressMonoFast(uint32_t, uint32_t, const uint8_t*, uint8_t*, float) {
    ev("DecompressMonoFast");
}

// The version 0x12 decoder: records every argument and answers from the queue "v12".
int32_t CWaveletImp::Decompress_v12(int32_t width, int32_t height, int32_t bytesPerPixel, void* data, void* alpha,
                                    void* dest, float lumaScale, float cbScale, float crScale, float alphaScale) {
    ev("Decompress_v12", {lab(this), width, height, bytesPerPixel, lab(data), lab(alpha), lab(dest), fbits(lumaScale),
                          fbits(cbScale), fbits(crScale), fbits(alphaScale)});
    return static_cast<int32_t>(ret("v12"));
}

uint8_t* gWaveletWorkspace = nullptr;

namespace {
// The decompressor object the entry points call (label 1 in the call log).
CWaveletImp* Object() {
    static CWaveletImp* object = new CWaveletImp;
    return object;
}

// The fill pattern of the array script's "fill" operation (tests/test_WaveletDecode.py makes
// the same): positive finite floats.
uint32_t FillBits(int32_t seed, int32_t i) {
    return (static_cast<uint32_t>(seed) * 2654435761u + static_cast<uint32_t>(i) * 40503u) & 0x3fffffffu;
}
}  // namespace

extern "C" {

// The addresses of the decoder's statics, for the test to set and read.
void* tpw_wd_global(const char* name) {
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

// Blocks handled by operator new[], delete[] and delete since the last call (and clears them).
void tpw_wd_heap(int32_t* out) {
    out[0] = tpw_cl_heap[1];
    out[1] = tpw_cl_heap[3];
    out[2] = tpw_cl_heap[2];
    for (int32_t& count : tpw_cl_heap) count = 0;
}

void tpw_wd_lzss(void* source, void* dest) { LzssDecompress(source, dest); }
uint32_t tpw_wd_read_code(uint32_t bits) { return read_code(bits); }
void tpw_wd_read_reset() { read_reset(); }

void tpw_wd_reverse_vert(const float* source, float* dest, int32_t count, int32_t stride) {
    Object()->ReverseTransformVert(source, dest, count, stride);
}

int32_t tpw_wd_v13(int32_t width, int32_t height, int32_t bytesPerPixel, void* data, void* alpha, void* dest,
                   float s1, float s2, float s3, float s4) {
    return Object()->Decompress_v13(width, height, bytesPerPixel, data, alpha, dest, s1, s2, s3, s4);
}

int32_t tpw_wd_dispatch(int32_t version, int32_t width, int32_t height, int32_t bytesPerPixel, void* data, void* alpha,
                        void* dest, float s1, float s2, float s3, float s4) {
    return Object()->Decompress(version, width, height, bytesPerPixel, data, alpha, dest, s1, s2, s3, s4);
}

int32_t tpw_wd_blit(int32_t version, int32_t width, int32_t height, int32_t destWidth, int32_t destHeight,
                    int32_t pitch, int32_t bytesPerPixel, void* data, void* alpha, void* dest, float s1, float s2,
                    float s3, float s4) {
    return Object()->Decompress(version, width, height, destWidth, destHeight, pitch, bytesPerPixel, data, alpha, dest,
                                s1, s2, s3, s4);
}

// A script over one CIgArray<float, float>, built by its constructor over 0xa5 bytes (out
// gets the four fields and the three words after them). Operations, three words each:
// (0, size, growBy) SetSize; (1, seed, 0) fill the elements; (2, 0, 0) GetData; (3, flag, 0)
// destroy (flag > 0: delete the object too), last. After each: GetData's answer as 1 (the
// elements) or 0 (NULL) for operation 2, else -1; whether the block changed; the size,
// capacity and step; the heap traffic (new[], delete[], delete); then the elements. Returns
// the number of values written.
int32_t tpw_wd_array(const int32_t* script, int32_t count, int64_t* out, int32_t capacity) {
    int32_t n = 0;
    auto put = [&](int64_t value) {
        if (n < capacity) out[n] = value;
        ++n;
    };
    void* storage = ::operator new(sizeof(CIgArray<float, float>));
    std::memset(storage, 0xa5, sizeof(CIgArray<float, float>));
    int32_t heap[3];
    tpw_wd_heap(heap);
    auto* array = new (storage) CIgArray<float, float>;
    put(array->mData == nullptr ? 0 : 1);
    put(array->mSize);
    put(array->mCapacity);
    put(array->mGrowBy);
    for (uint32_t word : array->mF10) put(word);
    for (int32_t i = 0; i < count; ++i) {
        const int32_t* op = script + 3 * i;
        float* before = array->mData;
        int64_t answer = -1;
        bool gone = false;
        switch (op[0]) {
        case 0:
            array->SetSize(op[1], op[2]);
            break;
        case 1:
            for (int32_t k = 0; k < array->mSize; ++k) {
                const uint32_t bits = FillBits(op[1], k);
                std::memcpy(&array->mData[k], &bits, sizeof bits);
            }
            break;
        case 2:
            answer = array->GetData() == nullptr ? 0 : (array->GetData() == array->mData ? 1 : 2);
            break;
        case 3:
            if (op[1] > 0) {
                delete array;
            } else {
                array->~CIgArray();
                ::operator delete(storage);
                tpw_cl_heap[2] = 0;    // the test's own release of the storage
            }
            gone = true;
            break;
        }
        tpw_wd_heap(heap);
        put(answer);
        if (gone) {
            put(heap[0]);
            put(heap[1]);
            put(heap[2]);
            break;
        }
        put(array->mData != before ? 1 : 0);
        put(array->mSize);
        put(array->mCapacity);
        put(array->mGrowBy);
        put(heap[0]);
        put(heap[1]);
        put(heap[2]);
        if (array->mData != nullptr) {
            for (int32_t k = 0; k < array->mSize; ++k) {
                uint32_t bits;
                std::memcpy(&bits, &array->mData[k], sizeof bits);
                put(bits);
            }
        }
    }
    return n;
}

}  // extern "C"
