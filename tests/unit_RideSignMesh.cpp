// The established CMesh dependency fakes and mesh builder, plus ride-sign operations.
#define TPW_RIDESIGN_TEST
#include "unit_Mesh.cpp"
#include <exception>

namespace {
int32_t gSignCount;
int32_t gConstructed;
bool gSignReset;
bool gSignGenerate;
struct Assertion : std::exception {};
// Allocation failures use the engine's raw Free callback, without destructors.
void SignFree(void*& block) {
    ev("Free", {lab(block)});
    block = nullptr;
}
}

void* operator new[](std::size_t size, const std::nothrow_t&) noexcept {
    if (!gSignReset) return std::malloc(size);
    ev("ArrayOld", {gSignCount});
    if (!ret("NewOld", 1)) return nullptr;
    void* block = std::calloc(1, size ? size : 1);
    newlab(block);
    return block;
}
void* CsTexture::operator new[](size_t size) noexcept {
    ev("ArrayTex", {gSignCount});
    if (!ret("NewTex", 1)) return nullptr;
    void* block = tpw::ArenaAllocZeroed(size);
    newlab(block);
    gConstructed = 0;
    return block;
}
void CsTexture::operator delete[](void* block) {
    ev("delete", {lab(block)});
    forget(block);
    tpw::ArenaFree(block);
}
int32_t CsTexture::ReLoad(CsTexture* other) {
    ev("ReLoad", {lab(this), lab(other)});
    mTexture = other->mTexture;
    mInfo = other->mInfo;
    mEntryCount = 1;
    mEntries = static_cast<CTextureEntry**>(std::calloc(1, sizeof(CTextureEntry*)));
    if (ret("Entry", 1)) {
        mEntries[0] = static_cast<CTextureEntry*>(tpw::ArenaAllocZeroed(sizeof(CTextureEntry)));
        newlab(mEntries[0]);
    }
    return static_cast<int32_t>(ret("ReLoad", 1));
}
void* CTextureEntry::GetData() {
    ev("Data", {lab(this)});
    // Scripted opaque data addresses, normalized by labels in the test.
    const uintptr_t value = ret("Data", 0);
    return value && gSignGenerate ? tpw::ArenaBase() + value : reinterpret_cast<void*>(value);
}
void msl_assertion_failed(const char* condition, const char* file, int32_t line) {
    ev("Assert", {line}, {condition, file});
    throw Assertion();
}

extern "C" {
void tpw_rsm_reset(void* header, uint32_t flags, int32_t count, int32_t planted) {
    gSignReset = false;
    tpw_ms_reset(header);
    new (gMesh) CMesh;
    gMesh->mFlags = flags;
    gMesh->mHeader = static_cast<tag_sM3D2Header*>(header);
    gMesh->mRideSignOld = planted & 1 ? reinterpret_cast<SOldCsTexture*>(0x1234) : nullptr;
    gMesh->mRideSignTextures = planted & 2 ? reinterpret_cast<CsTexture*>(0x5678) : nullptr;
    gMesh->mRideSignCount = 9;
    gSignCount = count;
    gSignReset = true;
    gFree = SignFree;
}
void* tpw_rsm_texture(void* record, void* info, int32_t entry) {
    auto* texture = static_cast<CsTexture*>(tpw::ArenaAllocZeroed(sizeof(CsTexture)));
    texture->mTexture = static_cast<sTexture*>(record);
    texture->mInfo = static_cast<sTextureInfo*>(info);
    texture->mEntries = static_cast<CTextureEntry**>(std::calloc(1, sizeof(CTextureEntry*)));
    if (entry) texture->mEntries[0] = static_cast<CTextureEntry*>(tpw::ArenaAllocZeroed(sizeof(CTextureEntry)));
    return texture;
}
void* tpw_rsm_entry(void* texture) { return static_cast<CsTexture*>(texture)->mEntries[0]; }
void tpw_rsm_null_header() { gMesh->mHeader = nullptr; }
int32_t tpw_rsm_generate(uint32_t* indices) {
    gSignGenerate = true;
    try {
        return gMesh->GenerateRideSign(indices, gSignCount);
    } catch (const Assertion&) {
        return -99;
    }
}
uintptr_t tpw_rsm_get(int32_t index) {
    gSignGenerate = false;
    try {
        return gMesh->GetTexturePointer(index);
    } catch (const Assertion&) {
        return static_cast<uintptr_t>(-99);
    }
}
int32_t tpw_rsm_state(int64_t* out) {
    int32_t n = 0;
    out[n++] = gMesh->mFlags;
    out[n++] = gMesh->mRideSignOld != nullptr;
    out[n++] = gMesh->mRideSignTextures != nullptr;
    out[n++] = gMesh->mRideSignCount;
    for (int32_t i = 0; i < gHeader->mTextureCount; ++i) out[n++] = lab(gHeader->mTextures[i].mTexture.get());
    if (gMesh->mRideSignOld != nullptr && reinterpret_cast<uintptr_t>(gMesh->mRideSignOld) != 0x1234) {
        for (int32_t i = 0; i < gSignCount; ++i) {
            out[n++] = lab(gMesh->mRideSignOld[i].mOld);
            out[n++] = gMesh->mRideSignOld[i].mWhere == nullptr ? -1 :
                (reinterpret_cast<char*>(gMesh->mRideSignOld[i].mWhere) - reinterpret_cast<char*>(gHeader->mTextures.get()) - 4) / 8;
        }
    }
    if (gMesh->mRideSignTextures != nullptr && reinterpret_cast<uintptr_t>(gMesh->mRideSignTextures) != 0x5678) {
        for (int32_t i = 0; i < gSignCount; ++i) {
            const auto& texture = gMesh->mRideSignTextures[i];
            out[n++] = lab(&texture);
            out[n++] = texture.mTexture == nullptr ? -1 : texture.mTexture - gHeader->mTextures.get();
            out[n++] = texture.mInfo == nullptr ? -1 : texture.mInfo - gHeader->mTextureInfos.get();
            out[n++] = texture.mEntryCount;
            out[n++] = texture.mReferences;
            out[n++] = texture.mEntries == nullptr ? 0 : lab(texture.mEntries[0]);
        }
    }
    return n;
}
}
