// Wavelet.cpp (engine library of SimTheme Park 2.04b3).
// PARTIAL: the callbacks so far: SetFileCallbacks (0x10038964), translated with
// landscape.cpp, whose loaders read through them; DefaultMalloc (0x100388c0), DefaultFree
// (0x10038914) and SetEngineCallbacks (0x100389f8), translated with tb_capture.cpp;
// BitsSet, ShiftFactor and CreateSineTable (0x10038a78..0x10038bf8), translated with
// engine start-up. The rest of the file is still to do.
#include "Wavelet.h"

#include <cmath>
#include <new>

#include "tb_rendermemory.h"

#include "platform/interfacelib.h"

TbFileOpenFn gFileOpen;
TbFileCloseFn gFileClose;
TbFileReadFn gFileRead;
TbFileSeekFn gFileSeek;
TbFileTellFn gFileTell;
TbFileNameFn gFileName;

void SetFileCallbacks(TbFileOpenFn open, TbFileCloseFn close, TbFileReadFn read, TbFileSeekFn seek,
                      TbFileTellFn tell, TbFileNameFn name) {
    gFileOpen = open;
    gFileClose = close;
    gFileRead = read;
    gFileSeek = seek;
    gFileTell = tell;
    gFileName = name;
}

TbIsFirstPersonFn gIsFirstPerson;
TbUseSmallTexturesFn gUseSmallTextures;
TbReadClockFn gReadClock;
TbUpdateMouseFn gUpdateMouse;
TbMallocFn gMalloc = DefaultMalloc;
TbFreeFn gFree = DefaultFree;

void* DefaultMalloc(uint32_t size) {
    DebugStr(reinterpret_cast<const unsigned char*>("\x17" "malloc callback not set"));
    return nullptr;
}

void DefaultFree(void*& block) {
    DebugStr(reinterpret_cast<const unsigned char*>("\x15" "free callback not set"));
}

void SetEngineCallbacks(TbIsFirstPersonFn isFirstPerson, TbUseSmallTexturesFn useSmallTextures,
                        TbReadClockFn readClock, TbUpdateMouseFn updateMouse, TbMallocFn malloc,
                        TbFreeFn free) {
    gIsFirstPerson = isFirstPerson;
    gReadClock = readClock;
    gUseSmallTextures = useSmallTextures;
    gUpdateMouse = updateMouse;
    gMalloc = malloc;
    gFree = free;
}

int32_t BitsSet(uint32_t mask) {
    int32_t count = 0;
    for (; mask != 0; mask >>= 1) {
        if ((mask & 1) != 0) ++count;
    }
    return count;
}

int32_t ShiftFactor(uint32_t mask) {
    if (mask == 0) return 0;
    int32_t shift = 0;
    for (; (mask & 1) == 0; mask >>= 1) ++shift;
    return shift;
}

float gSineTable[256];

void CreateSineTable() {
    for (int32_t i = 0; i < 256; ++i) {
        gSineTable[i] = static_cast<float>(std::sin(static_cast<double>(3.1415927f * (2.0f * static_cast<float>(i) * 0.00390625f))));
    }
}

// ---- The wavelet decompressor's lifetime --------------------------------------------------
CWavelet* gWaveletInstance = nullptr;
uint8_t* gWaveletWorkspace = nullptr;

CWavelet::~CWavelet() {}

CWaveletImp::CWaveletImp() {
    mReferences = 1;
    mBuffer = nullptr;
    if (gWaveletWorkspace == nullptr) gWaveletWorkspace = gei_RenderMemory->LoadInfo(CRenderMemory::kDynamic)->mData;
    mBuffer = new uint8_t[kBufferSize];
}

CWaveletImp::~CWaveletImp() {
    if (mBuffer != nullptr) delete[] mBuffer;
}

int32_t CWaveletImp::AddRef() { return ++mReferences; }

int32_t CWaveletImp::GetRefCount() { return mReferences; }

int32_t CWaveletImp::Release() {
    const int32_t references = --mReferences;
    if (references < 1) delete this;
    return references;
}

CWavelet* CWavelet::Instance() {
    if (gWaveletInstance == nullptr) gWaveletInstance = new (std::nothrow) CWaveletImp;
    if (gWaveletInstance != nullptr) gWaveletInstance->AddRef();
    return gWaveletInstance;
}
