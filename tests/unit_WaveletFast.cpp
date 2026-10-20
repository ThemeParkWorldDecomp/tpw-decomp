// Fast wavelet test library: real transforms, CIgArray, planes and zlib unpacker.
// The object's lifetime and v12 virtual are unused fakes. Platform allocator calls
// record every argument; Clear writes zero bytes as the platform does.
#include <cstdint>
#include <cstddef>
#include <cstring>
#define private public
#include "Wavelet.h"
#undef private
#include "platform/samsutils.h"
#include "calllog.h"

using namespace calllog;

CWavelet::~CWavelet() {}
CWaveletImp::CWaveletImp() : mReferences(1), mBuffer(nullptr) {}
CWaveletImp::~CWaveletImp() {}
int32_t CWaveletImp::AddRef() { return ++mReferences; }
int32_t CWaveletImp::GetRefCount() { return mReferences; }
int32_t CWaveletImp::Release() { return --mReferences; }
int32_t CWaveletImp::Decompress_v12(int32_t, int32_t, int32_t, void*, void*, void*, float, float, float, float) {
    ev("unexpected_v12");
    return 0;
}
uint8_t* gWaveletWorkspace = nullptr;

namespace SamsUtilities {
UAllocator* gMemory = nullptr;
UAllocator::UAllocator() : mOpaque{} {
    tpw_cl_label(7, this);
    ev("UAllocator", {lab(this)});
}
void UAllocator::Clear(void* dest, uint32_t size) {
    ev("Clear", {lab(this), lab(dest), size});
    std::memset(dest, 0, size);
}
}  // namespace SamsUtilities

namespace {
CWaveletImp* Object() {
    static CWaveletImp* object = new CWaveletImp;
    return object;
}
alignas(SamsUtilities::UAllocator) uint8_t allocator[sizeof(SamsUtilities::UAllocator)];
void* arrayBlocks[8];
int32_t arrayCount;

// The work arrays are short-lived; ordinals compare their allocation and release order.
void ObserveAllocation(std::size_t size, bool array, void* block) {
    if (!array || !on()) return;
    arrayBlocks[arrayCount] = block;
    ++arrayCount;
    ev("new[]", {static_cast<int64_t>(size), arrayCount});
}
bool ObserveRelease(void* block, bool array) {
    if (!array || !on() || block == nullptr) return false;
    for (int32_t i = 0; i < arrayCount; ++i) {
        if (arrayBlocks[i] == block) {
            ev("delete[]", {i + 1});
            arrayBlocks[i] = nullptr;
            break;
        }
    }
    return false;
}
}  // namespace

#define TPW_ALLOCATION_OBSERVER ObserveAllocation
#define TPW_RELEASE_OBSERVER ObserveRelease
#include "calllog.cpp"

extern "C" {

// Compatible global accessors let the shared wavelet test helpers drive this library.
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
    if (std::strcmp(name, "allocator") == 0) return allocator;
    return nullptr;
}
void tpw_wd12_set_buffer(void* buffer) { Object()->mBuffer = static_cast<uint8_t*>(buffer); }
// The common helper expects these; the fast test never plants fake unpacker payloads.
void tpw_wd12_payload_reset() {}
void tpw_wd12_payload(const void*, int32_t) {}
void tpw_wd12_heap(int32_t* out) {
    out[0] = tpw_cl_heap[1];
    out[1] = tpw_cl_heap[3];
    out[2] = tpw_cl_heap[2];
    for (int32_t& count : tpw_cl_heap) count = 0;
}
void tpw_wf_allocator(int32_t present) {
    // The old allocator is opaque and never read. Release only the case's own new one.
    if (SamsUtilities::gMemory != nullptr && SamsUtilities::gMemory != reinterpret_cast<SamsUtilities::UAllocator*>(allocator)) {
        delete SamsUtilities::gMemory;
    }
    SamsUtilities::gMemory = present ? reinterpret_cast<SamsUtilities::UAllocator*>(allocator) : nullptr;
    arrayCount = 0;
}
void tpw_wf_mono(uint32_t width, uint32_t height, const uint8_t* source, uint8_t* dest, float scale) {
    Object()->DecompressMonoFast(width, height, source, dest, scale);
}
void tpw_wf_color(uint32_t width, uint32_t height, const uint8_t* source, uint8_t* dest, float y, float cb, float cr) {
    Object()->DecompressColorFast(width, height, source, dest, y, cb, cr);
}
void tpw_wf_reverse(float* source, float* dest, uint32_t stride, uint32_t count) {
    Object()->ReverseChannelFast(source, dest, stride, count);
}
void tpw_wf_convert(uint32_t width, uint32_t height, uint32_t stride, uint8_t* dest) {
    Object()->ConvertAndBlend(width, height, stride, dest);
}

}  // extern "C"
