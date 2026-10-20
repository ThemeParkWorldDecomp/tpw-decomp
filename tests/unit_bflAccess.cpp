// Real access classes and decoder, with the existing disc unit's platform fakes.
#define TPW_REAL_FILE_ACCESS
#include "unit_bflDiscAccess.cpp"
#include "bfl_WadFile.h"
#include "bfl_Refpack.h"
#include <cstdlib>
#include <new>

// Clang's complete destructor for an abstract class traps. Exercise its base variant.
extern "C" void CompressionBaseDtor(bfl::compress::CCompressionBase*)
#ifdef __APPLE__
    asm("__ZN3bfl8compress16CCompressionBaseD2Ev");
#else
    asm("_ZN3bfl8compress16CCompressionBaseD2Ev");
#endif

namespace {
void AccessAllocation(std::size_t size, bool array, void* p);
}
#define TPW_ALLOCATION_OBSERVER AccessAllocation
#include "calllog.cpp"

namespace {
CAccessItem* gBaAccess;
int32_t gBaType;
uint8_t* gBaBuffer;
uint8_t* gBaOutput;
uint8_t gBaPacked[1024];
uint32_t gBaPackedLength;
_LbSemaphore gBaSemaphore;

// Pointer arguments compare as a buffer identity and an offset within it.
std::pair<int32_t, int32_t> Span(const void* p) {
    const auto address = reinterpret_cast<uintptr_t>(p);
    if (address >= reinterpret_cast<uintptr_t>(gBaOutput) && address < reinterpret_cast<uintptr_t>(gBaOutput) + 1024)
        return {4, static_cast<int32_t>(address - reinterpret_cast<uintptr_t>(gBaOutput))};
    uint8_t* buffer = gBaBuffer;
    if (gBaType == 1) buffer = static_cast<_TbBufferAccess*>(gBaAccess)->mBuffer;
    if (gBaType == 5) buffer = static_cast<_TbRefpackFileAccess*>(gBaAccess)->mBuffer;
    if (buffer != nullptr && address >= reinterpret_cast<uintptr_t>(buffer) && address < reinterpret_cast<uintptr_t>(buffer) + 1024)
        return {calllog::lab(buffer), static_cast<int32_t>(address - reinterpret_cast<uintptr_t>(buffer))};
    return {calllog::lab(p), 0};
}

// A queued physical file below the class under test.
class Backend : public CAccessItem {
public:
    uint32_t position;
    void AttachAccessClass(CAccessItem* access) override {
        ev("Backend.AttachAccessClass", {calllog::lab(this), calllog::lab(access)});
        mNext = access;
    }
    int32_t Read(void* dest, uint32_t size) override {
        const auto span = Span(dest);
        ev("Backend.Read", {calllog::lab(this), span.first, span.second, size});
        const int32_t count = ret("Backend.Read", size);
        const uint32_t done = std::min(size, count < 0 ? 0u : static_cast<uint32_t>(count));
        if (gBaType == 5) {
            std::memcpy(dest, gBaPacked, std::min(size, gBaPackedLength));
        } else {
            for (uint32_t i = 0; i < done; ++i) static_cast<uint8_t*>(dest)[i] = (i * 7 + 3 + position) & 0xff;
        }
        position += count;
        return count;
    }
    int32_t Write(const void* source, uint32_t size) override {
        const auto span = Span(source);
        uint32_t checksum = 0;
        for (uint32_t i = 0; i < size; ++i) checksum = checksum * 31 + static_cast<const uint8_t*>(source)[i];
        ev("Backend.Write", {calllog::lab(this), span.first, span.second, size, checksum});
        const int32_t count = ret("Backend.Write", size);
        position += count;
        return count;
    }
    int32_t Seek(int32_t offset, uint32_t whence) override {
        ev("Backend.Seek", {calllog::lab(this), offset, whence});
        position = ret("Backend.Seek", static_cast<uint32_t>(offset));
        return position;
    }
    int32_t GetPos() override {
        ev("Backend.GetPos", {calllog::lab(this)});
        return ret("Backend.GetPos", position);
    }
    int32_t GetSize() override {
        ev("Backend.GetSize", {calllog::lab(this)});
        return ret("Backend.GetSize", gBaType == 5 ? gBaPackedLength : 128);
    }
    const uint8_t* MapFile(int32_t offset, int32_t size) override {
        ev("Backend.MapFile", {calllog::lab(this), offset, size});
        return ret("Backend.MapFile", 1) ? gBaOutput : nullptr;
    }
    void UnmapFile(const uint8_t* p) override {
        ev("Backend.UnmapFile", {calllog::lab(this), calllog::lab(p)});
    }
    int32_t GetMacFileReference() override {
        ev("Backend.GetMacFileReference", {calllog::lab(this)});
        return ret("Backend.GetMacFileReference", 17);
    }
    _LbSemaphore* GetSemaphore() override {
        ev("Backend.GetSemaphore", {calllog::lab(this)});
        return &gBaSemaphore;
    }
};
Backend* gBaBackend;

void AccessAllocation(std::size_t size, bool array, void* p) {
    if (!array || !calllog::on()) return;
    std::memset(p, 0x5a, size);
    ev("new[]", {static_cast<int64_t>(size), calllog::newlab(p)});
}
}

void* operator new[](std::size_t size, const std::nothrow_t&) noexcept {
    void* p = calllog::on() && ret("array failure") ? nullptr : std::malloc(size != 0 ? size : 1);
    if (p != nullptr) AccessAllocation(size, true, p);
    else if (calllog::on()) ev("new[]", {static_cast<int64_t>(size), 0});
    return p;
}

// Lock helpers are outside this unit; the copied words and wait argument are recorded.
int32_t _LbSemaLock::Lock(int32_t wait) {
    ev("SemaLock.Lock", {mState, mWaiting, mLocked, wait});
    return ret("SemaLock.Lock", 1);
}
_LbSemaLock::~_LbSemaLock() { ev("SemaLock.dtor", {mState, mWaiting, mLocked}); }

namespace NS_MacDoze {
void CopyMemory(void* dest, const void* source, uint32_t size) {
    const auto out = Span(dest);
    const auto in = Span(source);
    ev("CopyMemory", {out.first, out.second, in.first, in.second, size});
    std::memcpy(dest, source, size);
}
}
int32_t bfl::compress::CRefpack::MemCompress(uint8_t*, uint8_t*, uint32_t, uint8_t*) const { return -1; }

extern "C" {
void tpw_ba_begin(int32_t type, const uint32_t* fields, const uint8_t* packed, uint32_t length) {
    gBaType = type;
    gBaPackedLength = length;
    std::memcpy(gBaPacked, packed, length);
    gBaBuffer = static_cast<uint8_t*>(std::malloc(1024));
    gBaOutput = static_cast<uint8_t*>(std::malloc(1024));
    for (int i = 0; i < 1024; ++i) gBaBuffer[i] = (i * 11 + 19) & 0xff;
    std::memset(gBaOutput, 0x69, 1024);
    gBaSemaphore.mState = fields[10];
    gBaBackend = new Backend;
    gBaBackend->position = fields[8];
    gBaBackend->mReferences = fields[9];
    if (type == 0) gBaAccess = new CAccessItem;
    if (type == 1) gBaAccess = new _TbBufferAccess;
    if (type == 2) gBaAccess = new _TbQuickPosAccess;
    if (type == 3) gBaAccess = new _TbSharedAccess;
    if (type == 4) gBaAccess = new _TbWadFileAccess;
    if (type == 5) gBaAccess = new _TbRefpackFileAccess;
    gBaAccess->mNext = fields[7] ? nullptr : gBaBackend;
    gBaAccess->mReferences = 3;
    if (type == 1) {
        auto* access = static_cast<_TbBufferAccess*>(gBaAccess);
        access->mBuffer = gBaBuffer;
        access->mCacheBuffer = gBaBuffer + fields[6];
        access->mPosition = fields[0];
        access->mBufferSize = fields[1];
        access->mBaseOffset = fields[2];
        access->mCacheStart = fields[3];
        access->mCacheEnd = fields[4];
    }
    if (type == 2 || type == 3) static_cast<_TbQuickPosAccess*>(gBaAccess)->mPosition = fields[0];
    if (type == 4) {
        auto* access = static_cast<_TbWadFileAccess*>(gBaAccess);
        access->mPosition = fields[0];
        access->mSize = fields[1];
        access->mBaseOffset = fields[2];
        access->mView = fields[5] ? gBaOutput : nullptr;
    }
    if (type == 5) {
        auto* access = static_cast<_TbRefpackFileAccess*>(gBaAccess);
        access->mSize = fields[1];
        access->mBuffer = gBaBuffer;
        access->mCursor = gBaBuffer + fields[0];
    }
    tpw_cl_label(1, gBaAccess);
    tpw_cl_label(2, gBaBackend);
    tpw_cl_label(4, gBaOutput);
    tpw_cl_label(5, gBaBuffer);
    tpw_cl_label(7, &gBaSemaphore);
    tpw_cl_label(103, &gBaAccess->mReferences);
    tpw_cl_label(104, &gBaBackend->mReferences);
}

int32_t tpw_ba_call(int32_t op, int32_t offset, uint32_t amount, uint32_t whence) {
    switch (op) {
    case 0: return gBaAccess->Read(gBaOutput, amount);
    case 1: return gBaAccess->Write(gBaOutput, amount);
    case 2: return gBaAccess->Seek(offset, whence);
    case 3: return gBaAccess->GetSize();
    case 4: return gBaAccess->GetPos();
    case 5: gBaAccess->Close(); return 0;
    case 6: gBaAccess->AttachAccessClass(gBaBackend); return 0;
    case 7: return calllog::lab(gBaAccess->MapFile(offset, amount));
    case 8: gBaAccess->UnmapFile(gBaOutput); return 0;
    case 9: return gBaAccess->Flush();
    case 10: return gBaAccess->GetMacFileReference();
    case 11: return calllog::lab(gBaAccess->GetSemaphore());
    case 12:
        if (static_cast<int16_t>(offset) > 0) {
            delete gBaAccess;
            gBaAccess = nullptr;
        } else gBaAccess->~CAccessItem();
        return 0;
    case 13:
        if (gBaType == 1) return static_cast<_TbBufferAccess*>(gBaAccess)->Open(amount);
        if (gBaType == 4) return static_cast<_TbWadFileAccess*>(gBaAccess)->Open(amount, offset);
        return static_cast<_TbRefpackFileAccess*>(gBaAccess)->Open(amount);
    case 14: static_cast<_TbSharedAccess*>(gBaAccess)->ValidateFilePosition(); return 0;
    case 15: return static_cast<_TbBufferAccess*>(gBaAccess)->BasicRead(gBaOutput, amount);
    case 16: return static_cast<_TbBufferAccess*>(gBaAccess)->ReadDirectToOutputBuffer(gBaOutput, amount);
    case 17: return static_cast<_TbBufferAccess*>(gBaAccess)->CacheBufferToOutputBuffer(gBaOutput, amount);
    case 18: static_cast<_TbBufferAccess*>(gBaAccess)->RefillCacheBuffer(); return 0;
    case 19:
        if (gBaType == 0) new (gBaAccess) CAccessItem;
        if (gBaType == 1) new (gBaAccess) _TbBufferAccess;
        if (gBaType == 2) new (gBaAccess) _TbQuickPosAccess;
        if (gBaType == 3) new (gBaAccess) _TbSharedAccess;
        if (gBaType == 4) new (gBaAccess) _TbWadFileAccess;
        if (gBaType == 5) new (gBaAccess) _TbRefpackFileAccess;
        return 0;
    default: return -999;
    }
}

void tpw_ba_state(uint32_t* fields, uint8_t* output, uint8_t* buffer, int32_t closed) {
    std::memset(fields, 0, 12 * sizeof(uint32_t));
    if (gBaAccess == nullptr) {
        fields[2] = gBaBackend->position;
        fields[3] = gBaBackend->mReferences;
        fields[11] = calllog::lab(gBaBackend->mNext);
        std::memcpy(output, gBaOutput, 256);
        return;
    }
    fields[0] = calllog::lab(gBaAccess->mNext);
    fields[1] = gBaAccess->mReferences;
    fields[2] = gBaBackend->position;
    fields[3] = gBaBackend->mReferences;
    fields[11] = calllog::lab(gBaBackend->mNext);
    uint8_t* data = gBaBuffer;
    if (gBaType == 1) {
        auto* access = static_cast<_TbBufferAccess*>(gBaAccess);
        fields[4] = access->mPosition;
        fields[5] = access->mBufferSize;
        fields[6] = access->mBaseOffset;
        fields[7] = access->mCacheStart;
        fields[8] = access->mCacheEnd;
        fields[9] = calllog::lab(access->mBuffer);
        fields[10] = access->mBuffer == nullptr ? calllog::lab(access->mCacheBuffer) : access->mCacheBuffer - access->mBuffer;
        data = access->mBuffer;
    }
    if (gBaType == 2 || gBaType == 3) fields[4] = static_cast<_TbQuickPosAccess*>(gBaAccess)->mPosition;
    if (gBaType == 4) {
        auto* access = static_cast<_TbWadFileAccess*>(gBaAccess);
        fields[4] = access->mPosition;
        fields[5] = access->mSize;
        fields[6] = access->mBaseOffset;
        fields[9] = calllog::lab(access->mView);
    }
    if (gBaType == 5) {
        auto* access = static_cast<_TbRefpackFileAccess*>(gBaAccess);
        fields[4] = access->mCursor == nullptr ? 0xffffffff : static_cast<uint32_t>(access->mCursor - access->mBuffer);
        fields[5] = access->mSize;
        fields[9] = calllog::lab(access->mBuffer);
        data = access->mBuffer;
    }
    std::memcpy(output, gBaOutput, 256);
    if (!closed && data != nullptr) std::memcpy(buffer, data, std::min(256u, fields[5]));
}
int32_t tpw_refpack_call(int32_t op, uint8_t* output, uint8_t* source, uint8_t* workspace, int32_t flag) {
    auto* decoder = new bfl::compress::CRefpack;
    if (op == 0) return decoder->MemDecompress(output, source, workspace);
    if (op == 1) return decoder->DecompressedSize(source);
    if (op == 2) return decoder->CompressWorkspaceSize();
    if (op == 3) return decoder->DecompressWorkspaceSize();
    tpw_cl_label(1, decoder);
    if (flag > 0) delete decoder;
    else if (op == 4) decoder->~CRefpack();
    else CompressionBaseDtor(decoder);
    return 0;
}
}
