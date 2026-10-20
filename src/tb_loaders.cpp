// Image loaders, Mac engine 0x10014e68..0x10016550 (asserts: tb_loaders.cpp).
// TGA shorts are little endian on disk. Pixel bytes stay in the file's order.
#include "tb_loaders.h"
#include "tb_loader_dependencies.h"
#include "tb_drawprimitive.h"
#include "tb_resource_texture.h"
#include "Wavelet.h"
#include "platform/msl.h"
#include "platform/interfacelib.h"
#include "tpw_probe.h"
#include <cstdio>
#include <cstring>

CTextureLoadManager* gei_TLoadManager = nullptr;
int32_t gTextureExtraPlanes = 0;
uint32_t gTextureAverageColour = 0;
uint32_t gTextureLoadFlags = 0;
void* gTextureWaveData = nullptr;
const int32_t gTextureBytesPerPixel[6] = {2, 2, 2, 4, 2, 4};

namespace {
void readHeader(STGAHeader* header, void* file) {
    gFileRead(header, 18, 1, file);
    auto* bytes = reinterpret_cast<uint8_t*>(header);
    auto word = [bytes](int32_t off) { return static_cast<int16_t>(bytes[off] | (bytes[off+1] << 8)); };
    int16_t x = word(8), y = word(10), w = word(12), h = word(14);
    header->mXOrigin = x;
    header->mYOrigin = y;
    header->mWidth = w;
    header->mHeight = h;
}
void loaderAssert(const char* condition, int32_t line) {
    msl_assertion_failed(condition, "tb_loaders.cpp", line);
}
}

void* STGAHeader::Load(char* name, int32_t extraPlanes) {
    void* file = gFileOpen(name, "rb");
    if (!file) return nullptr;
    int32_t size = gFileTell(file);
    if (size <= 0) loaderAssert("fsize>0", 0x8b);
    readHeader(this, file);
    if (mImageCodeType != 2) loaderAssert("b_ImageCodeType==2", 0x93);
    int32_t bytes = (mWidth * mHeight * mPixelSize) >> 3;
    if (size <= bytes) loaderAssert("fsize>datasize", 0x99);
    void* data = gMalloc(static_cast<uint32_t>(bytes * (extraPlanes + 1)));
    if (data) gFileRead(data, static_cast<uint32_t>(bytes), 1, file);
    gFileClose(file);
    return data;
}

void* STGAHeader::LoadBumpmap(char* name) {
    void* file = gFileOpen(name, "rb");
    if (!file) return nullptr;
    readHeader(this, file);
    if (mImageCodeType != 2) loaderAssert("b_ImageCodeType==2", 0xb9);
    int32_t pixels = mWidth * mHeight;
    gFileSeek(file, static_cast<uint32_t>((pixels * mPixelSize) >> 3), 1);
    void* data = gMalloc(static_cast<uint32_t>(pixels));
    if (data) gFileRead(data, static_cast<uint32_t>(pixels), 1, file);
    gFileClose(file);
    return data;
}

void STGAHeader::ConvertToEmbossBumpmap(uint8_t* data) {
    if (mPixelSize != 32 || mWidth != mHeight)
        loaderAssert("(b_PixelSize==0x20) && (w_ImageWidth==w_ImageHeight)", 0xd6);
    int32_t mode = gei_DrawPrimitive->EmbossMode();
    int32_t plane = mHeight * ((mWidth * mPixelSize) >> 3);
    int32_t pixels = mWidth * mHeight;
    if (mode == 1 || mode == 3) {
        for (int32_t i = 0; i < pixels; ++i) {
            uint8_t alpha = data[i * 4 + 3];
            std::memset(data + plane + i * 4, mode == 1 ? alpha >> 1 : alpha, 4);
            if (mode == 1) std::memset(data + plane * 2 + i * 4, (alpha ^ 255) >> 1, 4);
        }
    } else loaderAssert("0", 0xf2);
}

uint32_t UncompressedConvert(void* dest, void* source, int32_t width, int32_t height,
                             lARGBType* palette, int32_t destBytes, int32_t sourceBytes,
                             int32_t flipped) {
    uint32_t flags = 0;
    int32_t destFormat = destBytes == 2 ? 4 : destBytes == 4 ? 10 : 13;
    // For unsupported source sizes the original accidentally passes the data pointer
    // as the pixel format. Palette and orientation parameters are unused.
    intptr_t sourceFormat = reinterpret_cast<intptr_t>(source);
    if (sourceBytes == 2) sourceFormat = 5;
    else if (sourceBytes == 3) sourceFormat = 8;
    else if (sourceBytes == 4) { sourceFormat = 11; flags = 1; }
    SamsUtilities::UPixels src(sourceFormat, source, width, height, -1, nullptr);
    SamsUtilities::UPixels dst(destFormat, dest, width, height, -1, nullptr);
    src.SetAlphaDefault(255);
    dst.SetAlphaThreshold(240);
    dst.Convert(src, true);
    uint8_t red, green, blue, alpha;
    dst.GetAverages(&red, &green, &blue, &alpha);
    gTextureAverageColour = red | (uint32_t(alpha) << 8) | (uint32_t(blue) << 16) | (uint32_t(green) << 24);
    if (sourceBytes == 4) {
        int32_t count = dst.GetAlphaCount();
        int32_t threshold = static_cast<int32_t>(0.05f * static_cast<float>(width * height));
        if (count >= threshold) flags |= 2;
    }
    return flags;
}

void* TGALoadAndConvert(char* name, int32_t* width, int32_t* height, int32_t flags,
                        lARGBType* palette, int32_t bytesPerPixel) {
    if (!name) return nullptr;
    gTextureLoadFlags = 0;
    STGAHeader header;
    if (flags & 1) {
        void* data = header.LoadBumpmap(name);
        if (!data) return nullptr;
        if (header.mDescriptor & 0x20) {
            for (int32_t i = 0; i < header.mHeight; ++i) {
                const unsigned char message[] = "\x11" "assembler missing";
                DebugStr(message);
            }
        }
        gTextureAverageColour = 0xffffffff;
        gTextureLoadFlags = 0;
        return data;
    }
    char waveName[268];
    std::strcpy(waveName, name);
    char* start = waveName;
    while (*start == '.') ++start;
    char* end = start + std::strlen(start);
    while (*end != '.' && start < end) --end;
    if (*end == '.' && end[1]) {
        ++end;
        const char* ext = "WCT";
        while (*end && *ext) *end++ = *ext++;
    }
    auto* wave = new CWaveLoad;
    if (wave->LoadTexture(waveName)) {
        gTextureLoadFlags = 0x24;
        if (wave->mAlphaData) gTextureLoadFlags |= 1;
        if (wave->mAlphaData && wave->mAlphaThreshold) gTextureLoadFlags |= 2;
        *width = wave->mWidth;
        *height = wave->mHeight;
        gTextureLoadFlags |= uint32_t(wave->mLevel) << 8;
        gTextureWaveData = static_cast<CCompBase*>(wave);
        return wave;
    }
    delete wave;
    void* source = header.Load(name, gTextureExtraPlanes);
    if (!source) return nullptr;
    if (!bytesPerPixel) bytesPerPixel = gTextureBytesPerPixel[gei_TextureSettings.mFormat];
    *width = header.mWidth;
    *height = header.mHeight;
    if (flags & 2) { header.ConvertToEmbossBumpmap(static_cast<uint8_t*>(source)); gTextureLoadFlags |= 8; }
    gTextureLoadFlags |= 0x300;
    int32_t pixels = header.mWidth * header.mHeight;
    int32_t destPlane = bytesPerPixel * pixels;
    int32_t sourcePlane = (header.mPixelSize * pixels) >> 3;
    void* dest = gMalloc(static_cast<uint32_t>(destPlane * (gTextureExtraPlanes + 1) + 4));
    if (dest) {
        for (int32_t i = 0; i <= gTextureExtraPlanes; ++i)
            gTextureLoadFlags |= UncompressedConvert(static_cast<uint8_t*>(dest) + i * destPlane,
                static_cast<uint8_t*>(source) + i * sourcePlane, header.mWidth, header.mHeight,
                palette, bytesPerPixel, header.mPixelSize >> 3, (header.mDescriptor & 0x20) != 0);
    }
    gFree(source);
    return dest;
}

STextureLoad::STextureLoad() : mFolder(nullptr), mName(nullptr), mTexture(nullptr), mReferences(0) {}
CTextureLoadManager::CTextureLoadManager() : mCount(0), mDefault(-1), mLoadFlags(0) {}

int32_t TextureLoadManager_Init() {
    if (gei_TLoadManager) return 0;
    gei_TLoadManager = new CTextureLoadManager;
    return gei_TLoadManager->Init();
}
bool TextureLoadManager_Close() {
    if (!gei_TLoadManager) return false;
    gei_TLoadManager->Close();
    delete gei_TLoadManager;
    gei_TLoadManager = nullptr;
    return true;
}
int32_t CTextureLoadManager::Init() {
    char* path = gFileName("data:Generic:defaulttexture:notfound.tga", 1);
    mLoads[mCount].mFolder = const_cast<char*>("data:Generic:defaulttexture");
    mLoads[mCount].mName = const_cast<char*>("notfound.tga");
    mLoads[mCount].mTexture = LoadFile(path);
    mLoads[mCount].mReferences = 1;
    mDefault = mCount;
    ++mCount;
    return 1;
}
int32_t CTextureLoadManager::Close() {
    for (int32_t i = 0; i < mCount; ++i) mLoads[i].Close();
    mCount = 0;
    return 1;
}
int32_t STextureLoad::Close() {
    if (mTexture) {
        CTextureEntry* last = mTexture;
        while (last->mFlags & 0x800000) ++last;
        mTexture->Close();
        delete[] mTexture;
        mTexture = nullptr;
    }
    mReferences = 0;
    return 1;
}
CTextureEntry* CTextureLoadManager::LoadFile(char* name) {
    if (!name) return nullptr;
    gTextureExtraPlanes = 0;
    uint32_t flags = mLoadFlags;
    mLoadFlags = 0;
    if (flags & 2) {
        switch (gei_DrawPrimitive->EmbossMode()) {
        case 0: flags &= ~2u; break;
        case 1: case 2: gTextureExtraPlanes = 2; break;
        case 3: gTextureExtraPlanes = 1; break;
        default: loaderAssert("0", 0x34d); break;
        }
    }
    auto* entries = new CTextureEntry[gTextureExtraPlanes + 1];
    int32_t width, height;
    auto* data = static_cast<uint8_t*>(TGALoadAndConvert(name, &width, &height, flags, nullptr, 0));
    if (tpw_probe::gLog != nullptr) {
        char line[320];
        std::snprintf(line, sizeof line, "probe: texture %s: %s", name, data ? "loaded" : "NOT LOADED");
        tpw_probe::gLog(line);
    }
    if (!data) { delete[] entries; return nullptr; }
    uint32_t load = gTextureLoadFlags;
    uint32_t entryFlags = (load & 4) ? 0x11000000 : 0x1000000;
    if (load & 8) entryFlags |= 0x30400;
    else if (flags & 4) entryFlags |= 0x8030000;
    else if (load & 2) entryFlags |= 0x200;
    else if (load & 1) entryFlags |= 0x100;
    if (flags & 8) entryFlags |= 0x300;
    if (load & 0x20) entryFlags |= 0x40000;
    entryFlags |= (load & 0x300) >> 2;
    int32_t plane = width * height * gTextureBytesPerPixel[gei_TextureSettings.mFormat];
    for (int32_t i = 0; i <= gTextureExtraPlanes; ++i) {
        entries[i].mFlags = entryFlags | (i < gTextureExtraPlanes ? 0x800000 : 0);
        entries[i].mData = (entryFlags & 0x10000000) ? gTextureWaveData : data + i * plane;
        entries[i].SetSize(width, height);
        entries[i].mPitch = static_cast<int16_t>(width);
    }
    return entries;
}
CTextureEntry* CTextureLoadManager::LoadTexture(char* folder, char* name) {
    for (int32_t i = mCount - 1; i >= 0; --i) {
        auto& load = mLoads[i];
        bool sameFolder = folder == load.mFolder || (folder && load.mFolder && !std::strcmp(folder, load.mFolder));
        if (sameFolder && !std::strcmp(name, load.mName)) {
            ++load.mReferences;
            return load.mTexture;
        }
    }
    if (mCount >= 4096) { loaderAssert("0", 0x429); return nullptr; }
    char path[256];
    if (!folder || !*folder) path[0] = 0;
    else {
        std::strcpy(path, folder);
        size_t len = std::strlen(path);
        if (path[len - 1] != ':') std::strcat(path, ":");
    }
    std::strcat(path, name);
    CTextureEntry* entry = LoadFile(path);
    if (!entry) {
        if (mDefault == -1) return nullptr;
        ++mLoads[mDefault].mReferences;
        return mLoads[mDefault].mTexture;
    }
    auto& load = mLoads[mCount++];
    load.mFolder = folder;
    load.mName = name;
    load.mTexture = entry;
    load.mReferences = 1;
    return entry;
}
