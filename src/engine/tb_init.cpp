// tb_init.cpp: name proven by __sinit_tb_init_cpp in the engine library.

#include "platform/indirectx.h"
#include "platform/msl.h"
#include "platform/samsutils.h"
#include "platform/upixels.h"
#include "ppc_math.h"
#include "SMachineParameters.h"
#include "tb_capture.h"
#include "tb_ddraw.h"
#include "tb_drawprimitive.h"
#include "tb_error.h"
#include "tb_hfield.h"
#include "tb_init.h"
#include "tb_init_dependencies.h"
#include "tb_loader_dependencies.h"
#include "tb_loaders.h"
#include "tb_matrix.h"
#include "tb_printf_opengl.h"
#include "tb_proctexture.h"
#include "tb_resource_display.h"
#include "tb_resource_system.h"
#include "tb_resource_texture.h"
#include "tb_sky.h"
#include "tb_switch.h"
#include "tb_texturecache.h"
#include "Wavelet.h"
#include <cstdint>
#include <cstdio>
#include <cstring>

// Engine startup, Mac engine 0x10013298..0x10013c04. The version argument is unused.

namespace {
SamsUtilities::ULogManager* logger() {
    if (!SamsUtilities::gLogger) SamsUtilities::gLogger = new SamsUtilities::ULogManager;
    return SamsUtilities::gLogger;
}
template <typename... Args>
void note(int32_t line, const char* text, Args... args) {
    logger()->SetContext("engine", "tb_init.cpp", line, 0x80000000);
    logger()->Log(text, args...);
}
}

int32_t EngineAdd_Init(int32_t textureFormat, int32_t version, char* name) {
    note(0x3b, "Initializing engine internals");
    gei_Display.mFlags &= ~2u;
    CreateSineTable();
    if (!Gamut_Init() || !Matrix_Init()) goto failed;
    if (gei_System.mRenderer == 3) {
        if (!DrawPrimitiveOpenGL_Init()) goto failed;
        gei_DrawPrimitive = gOpenGLDrawPrimitive;
    }
    note(0x7b, " (gamut,matrix,drawprimitive ok)");
    if (!RenderMemory_Init() || !LightManager_Init() || !RenderList_Init()) goto failed;
    note(0x86, " (rmem/lights/render)");
    if (!HField_Init()) goto failed;
    if (name) gei_HField->LoadProcTextures(name, 0);
    if (!MapWho_Init() || !Switch_Init()) goto failed;
    note(0x92, " (hfield,mapwho,switch)");
    if (!Clipping_Init() || !SpriteManager_Init() || !Particle_Init()) goto failed;
    note(0x9d, " (clipping/sprite/particle ok)");
    if (!TCache_Init(textureFormat)) goto failed;
    note(0xa2, " (tcache ok)");
    if (!TextureLoadManager_Init() || !Rain_Init() || !gei_Sky.Init()) goto failed;
    note(0xad, " (tload/rain/sky ok)");
    if (!InitWaveLoad()) goto failed;
    gei_Rain->SetWindParameters(0.0f, 0.3f);
    note(0xb5, " (wavelet/printf ok)");
    gei_TextureSettings.CalculateUVFixup();
    note(0xba, " (uv correction ok)");
    if (gei_System.mFlags & 0x1000000) {
        gei_System.OutputDebugInfo();
        gei_Display.OutputDebugInfo();
        gei_TextureSettings.OutputDebugInfo();
    }
    note(199, "Engine internals initialized succesfully");
    return 1;
failed:
    note(0xcd, " --- FAILED, exiting (error code %x)", gDDResult);
    EngineAdd_Close();
    return 0;
}

int32_t EngineAdd_Close() {
    CloseWaveLoad();
    Particle_Close();
    Rain_Close();
    SpriteManager_Close();
    Clipping_Close();
    Switch_Close();
    TextureLoadManager_Close();
    MapWho_Close();
    HField_Close();
    TCache_Close();
    RenderList_Close();
    LightManager_Close();
    RenderMemory_Close();
    if (gei_System.mRenderer == 3) DrawPrimitiveOpenGL_Close();
    Matrix_Close();
    Gamut_Close();
    return 1;
}

bool EngineAdd_Reinit() { return gei_DrawPrimitive->Init() != 0; }

uint8_t* gProcTextureBuffer = nullptr;

CProcTextureHandler::CProcTextureHandler() : mLandscape(nullptr), mSize(64), mCyclic(nullptr), mCount(0) {
    std::memset(mTiles, 0, sizeof(mTiles));
}
CProcTextureHandler::~CProcTextureHandler() { Close(); }
int32_t CProcTextureHandler::Init(int32_t count, int32_t size) {
    if (!size) size = mSize;
    int32_t bytes = gTextureBytesPerPixel[gei_TextureSettings.mFormat];
    if (static_cast<int32_t>(gei_System.mTotalPhys) < 0x3000000 || gei_TextureSettings.mTextureMemory < bytes * 0x300000)
        size = 16;
    if ((static_cast<int32_t>(gei_System.mTotalPhys) < 0x4000000 || gei_TextureSettings.mTextureMemory < bytes * 0x500000) && size == 64)
        size = 32;
    if (gei_System.mRenderer == 4) return 1;
    if (mCyclic || mLandscape)
        msl_assertion_failed("(!mps_Cyclic) && (!mps_Landscape)", "tb_hfield_proctexture.cpp", 0x37);
    mLandscape = new Landscape;
    std::memset(mTiles, 0, sizeof(mTiles));
    mCyclic = new SProcTexture[count];
    std::memset(mCyclic, 0, sizeof(SProcTexture) * count);
    mCount = count;
    mSize = size;
    for (int32_t i = 0; i < count; ++i) {
        mCyclic[i].mPrevious = &mCyclic[i == 0 ? count - 1 : i - 1];
        mCyclic[i].mNext = &mCyclic[i == count - 1 ? 0 : i + 1];
        auto* entry = new CTextureEntry[1];
        if (!entry->Create(size, 0)) {
            // The failed entry is never stored in the list, so the original leaks it.
            gei_ErrorHandler.Push(2, kErrOutOfMemory);
            Close();
            return 0;
        }
        mCyclic[i].mTexture = entry;
    }
    mCurrent = mCyclic + (count >> 1);
    return 1;
}
int32_t CProcTextureHandler::Close() {
    gei_TextureSettings.mFlags &= ~4u;
    if (mCount) {
        for (int32_t i = 0; i < mCount; ++i) if (mCyclic[i].mTexture) delete[] mCyclic[i].mTexture;
        delete[] mCyclic;
        mCount = 0;
    }
    if (mLandscape) { delete mLandscape; mLandscape = nullptr; }
    return 1;
}
bool CProcTextureHandler::Load(char* name) {
    if (!mLandscape) msl_assertion_failed("mps_Landscape", "tb_hfield_proctexture.cpp", 0x91);
    mLandscape->set_tilesize(mSize);
    uint8_t result = static_cast<uint8_t>(mLandscape->load_landscape(name));
    if (result) gei_TextureSettings.mFlags |= 4;
    else {
        msl_assertion_failed("0", "tb_hfield_proctexture.cpp", 0x96);
        gei_ErrorHandler.Push(2, static_cast<int32_t>(0xb0754001));
    }
    return result != 0;
}
int32_t CProcTextureHandler::PrepareList(PrepareEntry* entries, sTileOverlay* overlays, int32_t count) {
    SProcTexture* cursor = mCurrent;
    for (int32_t i = 0; i < count; ++i) {
        uint32_t tile = entries[i].mTile;
        uint32_t x = tile & 255;
        int32_t y = static_cast<int32_t>(tile) >> 8;
        auto& slot = mTiles[y * 128 + x];
        SProcTexture* node = slot;
        CTextureEntry* entry;
        uint32_t regenerate;
        if (!node) {
            mCurrent = mCurrent->mPrevious;
            entry = mCurrent->mTexture;
            entries[i].mTexture = entry;
            if (mCurrent->mSlot) *mCurrent->mSlot = nullptr;
            slot = mCurrent;
            mCurrent->mSlot = &slot;
            regenerate = 1;
        } else {
            entry = node->mTexture;
            entries[i].mTexture = entry;
            if (node == cursor) cursor = cursor->mNext;
            else {
                node->mPrevious->mNext = node->mNext;
                node->mNext->mPrevious = node->mPrevious;
                cursor->mPrevious->mNext = node;
                node->mPrevious = cursor->mPrevious;
                node->mNext = cursor;
                cursor->mPrevious = node;
                if (cursor == mCurrent) mCurrent = mCurrent->mPrevious;
            }
            regenerate = overlays[i].mFlags & 0x100;
        }
        if (regenerate) {
            if (overlays[i].mData) overlays[i].mData = static_cast<CTextureEntry*>(overlays[i].mData)->GetCorrectedData();
            if (gTextureBytesPerPixel[gei_TextureSettings.mFormat] == 2) {
                mLandscape->generate_tile(gProcTextureBuffer, x, y, &overlays[i]);
                ConvertSquareFrom32(entry->GetData(), gProcTextureBuffer, mSize);
            } else {
                mLandscape->generate_tile(static_cast<uint8_t*>(entry->GetData()), x, y, &overlays[i]);
                uint32_t pixels = static_cast<uint32_t>(mSize * mSize);
                auto* data = static_cast<uint8_t*>(entry->GetData());
                // generate_tile writes ARGB bytes. Keep the Mac's byte rotation,
                // including its signed right shift when the alpha high bit is set.
                for (uint32_t j = 0; j < pixels; ++j) {
                    uint32_t word = (uint32_t(data[0]) << 24) | (uint32_t(data[1]) << 16) |
                                    (uint32_t(data[2]) << 8) | data[3];
                    word = (word << 8) | static_cast<uint32_t>(static_cast<int32_t>(word) >> 24);
                    data[0] = word >> 24; data[1] = word >> 16; data[2] = word >> 8; data[3] = word;
                    data += 4;
                }
            }
            gei_TCache->Remove(entry);
        }
        entry->Lock();
        gei_TCache->Prepare(entry, 0.0f);
    }
    return 1;
}
int32_t CProcTextureHandler::CreateProcTexture(CTextureEntry* entry, int32_t x, int32_t y, sTileOverlay* overlay) {
    if (!entry->Create(mSize, 0)) return 0;
    if (gTextureBytesPerPixel[gei_TextureSettings.mFormat] == 2) {
        mLandscape->generate_tile(gProcTextureBuffer, x, y, overlay);
        ConvertSquareFrom32(entry->GetData(), gProcTextureBuffer, mSize);
    } else {
        auto* data = static_cast<uint8_t*>(entry->GetData());
        mLandscape->generate_tile(data, x, y, overlay);
        for (int32_t i = 0; i < mSize * mSize; ++i) {
            uint8_t a = data[0], b = data[1];
            data[0] = data[3]; data[3] = a;
            data[1] = data[2]; data[2] = b;
            data += 4;
        }
    }
    return 1;
}

// Compares the record's limits (flag bits 0..5 of its word at 4) against the measured values; the record pointer never
// advances in the original, so every pass reads the same record. Answers the record's word 0 (0 when count <= 0).
uint32_t SMachineParameters::Return(SCfgRecord* record, int32_t mode) {
    const uint8_t* b = record->mBytes;
    auto word = [&](int off) {
        uint32_t v;
        memcpy(&v, b + off, 4);
        return v;
    };
    auto flt = [&](int off) {
        float v;
        memcpy(&v, b + off, 4);
        return v;
    };
    uint32_t result = 0;
    for (int32_t n = mode; n > 0; n--) {
        uint32_t flags = word(4);
        if ((flags & 1) && static_cast<int32_t>(word(8)) > mVRAMSize) break;
        if ((flags & 2) && flt(0xc) > mVRAMSpeed) break;
        if ((flags & 4) && static_cast<int32_t>(word(0x10)) > mRAMSize) break;
        if ((flags & 8) && static_cast<int32_t>(word(0x14)) > mCPUFreq) break;
        if (flags & 0x10) {
            float v = static_cast<float>(static_cast<double>(static_cast<int32_t>(word(0x18))));
            if (v > mMathSpeed) break;
        }
        if ((flags & 0x20) && flt(0x1c) > mFillRate) break;
        result = word(0);
    }
    return result;
}

// SMachineParameters::Output (0x10013d3c) and ::Suggest (0x10013eec), the engine's machine detection.

uint8_t gSMachineCfgData[0x400];     // the records at r2 - 0x5c74 .. (see the header)
int32_t gSMachineOutputCount;        // engine data 0x100af124

void SMachineParameters::Output() {
    char path[200];
    strcpy(path, "detect.txt");
    FILE* file;
    if (gSMachineOutputCount != 0) {
        file = fopen(path, "a");
    } else {
        gSMachineOutputCount = gSMachineOutputCount + 1;
        file = fopen(path, "w+b");
    }
    if (file == nullptr) return;
    // CDirectDraw + 0x21c: mDeviceInfo (0x1c) + the 512 bytes of szDriver = szDescription
    fprintf(file, "Graphics card description: %s\n", reinterpret_cast<const char*>(gei_DDraw) + 0x21c);
    fprintf(file, "VRAM Size: %d\n", mVRAMSize);
    fprintf(file, "VRAM Speed: %f\n", static_cast<double>(mVRAMSpeed));
    fprintf(file, "RAM Size: %d\n", mRAMSize);
    fprintf(file, "CPU Freq: %d\n", mCPUFreq);
    fprintf(file, "Math Speed (%d): %f\n", gei_System.mF20, static_cast<double>(mMathSpeed));
    fprintf(file, "Pixel Fill Rate (%d): %f\n\n", gei_System.mRenderer, static_cast<double>(mFillRate));
    fflush(file);
    fclose(file);
}

int32_t SMachineParameters::Suggest() {
    mVRAMSize = gei_System.mRenderSystem->GetVRAMSize() >> 10;
    mVRAMSpeed = static_cast<float>(gei_TCache->TestUploadSpeed(0x100, 0x10));
    mRAMSize = static_cast<int32_t>(gei_System.mTotalPhys) >> 10;
    mCPUFreq = gei_System.mClocksPerMs / 1000;
    mMathSpeed = gei_MathUnit->TestMathSpeed();
    mFillRate = gei_DrawPrimitive->GetPixelFillRate();
    Output();
    mFlags = 0;
    // offset in gSMachineCfgData (the record's address minus r2 + 0x5c74), mode
    static const int32_t kRecords[16][2] = {
        {0x000, 2}, {0x040, 1}, {0x060, 1}, {0x080, 1}, {0x0a0, 1}, {0x0c0, 1}, {0x0e0, 1}, {0x100, 3},
        {0x160, 1}, {0x180, 3}, {0x1e0, 3}, {0x240, 3}, {0x2a0, 1}, {0x2c0, 3}, {0x340, 2}, {0x3a0, 2},
    };
    for (int i = 0; i < 16; ++i) {
        const uint32_t flags = Return(reinterpret_cast<SCfgRecord*>(gSMachineCfgData + kRecords[i][0]), kRecords[i][1]);
        mFlags = mFlags | flags;
    }
    return 1;
}

uint32_t gei_ParameterSettings[4];   // 0x100af128
// 0x100141bc
void __sinit_tb_init_cpp() {
    gei_ParameterSettings[0] = 0;
    gei_ParameterSettings[1] = 0;
    gei_ParameterSettings[2] = 0;
    gei_ParameterSettings[3] = 0;
}
