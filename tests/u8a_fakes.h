// Matching external-engine and platform fakes shared by the two u8a unit libraries.
#include <cstdlib>
#include <cstdarg>
#include <cstring>
#include <new>
#include <unordered_map>
#include <vector>
#include "calllog.h"
#include "tb_loader_dependencies.h"
#include "tb_loaders.h"
#include "tb_drawprimitive.h"
#include "tb_resource_display.h"
#include "tb_resource_system.h"
#include "tb_resource_texture.h"
#include "Wavelet.h"
#include "platform/msl.h"
#include "platform/interfacelib.h"
using namespace calllog;

// Fake CSystem::CSystem for matching external-call events.
CSystem::CSystem() {
}
CSystem gei_System;
// Fake CTextureSettings::CTextureSettings for matching external-call events.
CTextureSettings::CTextureSettings() {
}
CTextureSettings gei_TextureSettings;
CDisplay gei_Display;
CDisplay::CDisplay() {}   // the unit keeps a blank display of its own
CDrawPrimitive* gei_DrawPrimitive;
// Fake CDrawPrimitive::CDrawPrimitive (tb_drawprimitive.cpp, untranslated): nothing to record.
CDrawPrimitive::CDrawPrimitive() {}
// Fake CDrawPrimitive::~CDrawPrimitive for matching external-call events.
CDrawPrimitive::~CDrawPrimitive() {
}
class U8Draw : public CDrawPrimitive {
    // Fake Init for matching external-call events.
    public:
    // Fake Init for matching external-call events.
    int32_t Init() override {
        ev("Draw.Init", {lab(this)});
        return ret("Draw.Init");
    }
    // Fake SetBlendingMode for matching external-call events.
    int32_t SetBlendingMode(int32_t) override {
        return 0;
    }
    // Fake SetTexture for matching external-call events.
    int32_t SetTexture(int32_t, void*) override {
        return 0;
    }
    // Fake Execute for matching external-call events.
    int32_t Execute(SPrimitive*) override {
        return 0;
    }
    // Fake ExecuteShadow for matching external-call events.
    int32_t ExecuteShadow(SPrimitive*) override {
        return 0;
    }
    // Fake SetFogParameters for matching external-call events.
    int32_t SetFogParameters(float, float, uint32_t) override {
        return 0;
    }
    // Fake BeginDraw for matching external-call events.
    int32_t BeginDraw() override {
        return 0;
    }
    // Fake EndDraw for matching external-call events.
    int32_t EndDraw() override {
        return 0;
    }
    // Fake DrawTestPoly for matching external-call events.
    int32_t DrawTestPoly() override {
        return 0;
    }
    // Fake DrawFilledBox for matching external-call events.
    int32_t DrawFilledBox(float, float, float, float, int32_t, CTextureBase*) override {
        return 0;
    }
} u8draw, u8drawGL;

// Fake msl_assertion_failed for matching external-call events.
void msl_assertion_failed(const char* cond, const char* file, int32_t line) {
    ev("assert", {line}, {cond, file});
}
// Fake DebugStr for matching external-call events.
void DebugStr(const unsigned char* text) {
    std::string s(reinterpret_cast<const char*>(text + 1), *text);
    ev("DebugStr", {}, {s.c_str()});
}

extern "C" {
uint8_t tpw_u8_file[65536];
int32_t tpw_u8_file_size = 0;
int32_t tpw_u8_file_pos = 0;
}
std::vector<std::pair<void*, uint32_t>> u8blocks;
// Fake u8malloc for matching external-call events.
void* u8malloc(uint32_t size) {
    ev("Malloc", {size});
    if (!ret("Malloc", 1))
        return nullptr;
    // Assertions return in this harness. Malformed emboss inputs can write beyond the
    // requested size after asserting; guard bytes keep that behavior observable.
    void* p = std::calloc(size + 4096, 1);
    newlab(p);
    u8blocks.push_back({p, size + 4096});
    return p;
}
// Fake u8free for matching external-call events.
void u8free(void*& p) {
    ev("Free", {lab(p)});
    forget(p);
    p = nullptr;
}
// Fake u8open for matching external-call events.
void* u8open(const char* name, const char* mode) {
    ev("Open", {}, {name, mode});
    tpw_u8_file_pos = 0;
    return ret("Open", 1) ? reinterpret_cast<void*>(uintptr_t(1)) : nullptr;
}
// Fake u8close for matching external-call events.
uint32_t u8close(void*) {
    ev("CloseFile");
    return 0;
}
// Fake u8tell for matching external-call events.
int32_t u8tell(void*) {
    ev("Tell");
    return ret("Tell", tpw_u8_file_size);
}
// Fake u8read for matching external-call events.
uint32_t u8read(void* dest, uint32_t size, uint32_t count, void*) {
    ev("Read", {size, count});
    uint32_t n = size * count;
    uint32_t available =
        tpw_u8_file_pos < tpw_u8_file_size ? tpw_u8_file_size - tpw_u8_file_pos : 0;
    uint32_t copy = n < available ? n : available;
    std::memcpy(dest, tpw_u8_file + tpw_u8_file_pos, copy);
    tpw_u8_file_pos += copy;
    return size ? copy / size : 0;
}
// Fake u8seek for matching external-call events.
uint32_t u8seek(void*, uint32_t offset, uint32_t whence) {
    ev("Seek", {offset, whence});
    tpw_u8_file_pos += offset;
    return 0;
}
// Fake u8name for matching external-call events.
char* u8name(const char* name, int32_t flags) {
    ev("Name", {flags}, {name});
    return ret("Name", 1) ? const_cast<char*>(name) : nullptr;
}
TbFileOpenFn gFileOpen = u8open;
TbFileCloseFn gFileClose = u8close;
TbFileReadFn gFileRead = u8read;
TbFileSeekFn gFileSeek = u8seek;
TbFileTellFn gFileTell = u8tell;
TbFileNameFn gFileName = u8name;
TbMallocFn gMalloc = u8malloc;
TbFreeFn gFree = u8free;

namespace SamsUtilities {
ULogManager* gLogger = nullptr;
// Fake ULogManager::ULogManager for matching external-call events.
ULogManager::ULogManager() {
    if (on())
        ev("Logger.ctor", {newlab(this)});
}
// Fake ULogManager::SetContext for matching external-call events.
void ULogManager::SetContext(const char* subsystem, const char* file, int32_t line,
                             uint32_t channel) {
    ev("Context", {lab(this), line, channel}, {subsystem, file});
}
// Fake ULogManager::Log for matching external-call events.
void ULogManager::Log(const char* format, ...) {
    if (std::strstr(format, "%x")) {
        va_list args;
        va_start(args, format);
        int32_t value = va_arg(args, int32_t);
        va_end(args);
        ev("Log", {lab(this), value}, {format});
    } else {
        ev("Log", {lab(this)}, {format});
    }
}
struct PixelState {
    void* data;
    intptr_t format;
    int32_t width, height;
};
std::unordered_map<UPixels*, PixelState> u8pixels;
// Fake UPixels::UPixels for matching external-call events.
UPixels::UPixels(intptr_t format, void* data, int32_t width, int32_t height, int32_t stride,
                 UPixel* palette) {
    // Fake ev for matching external-call events.
    ev("Pixels.ctor",
       {newlab(this),
        format >= 0 && format <= 13 ? format : -100 - lab(reinterpret_cast<void*>(format)),
        lab(data), width, height, stride, lab(palette)});
    u8pixels[this] = {data, format, width, height};
}
// Fake UPixels::~UPixels for matching external-call events.
UPixels::~UPixels() {
    ev("Pixels.dtor", {lab(this)});
    forget(this);
    u8pixels.erase(this);
}
// Fake UPixels::SetAlphaDefault for matching external-call events.
void UPixels::SetAlphaDefault(uint8_t alpha) {
    ev("AlphaDefault", {lab(this), alpha});
}
// Fake UPixels::SetAlphaThreshold for matching external-call events.
void UPixels::SetAlphaThreshold(uint8_t alpha) {
    ev("AlphaThreshold", {lab(this), alpha});
}
// Fake UPixels::Convert for matching external-call events.
void UPixels::Convert(UPixels& source, bool dither) {
    ev("Convert", {lab(this), lab(&source), dither});
    auto d = u8pixels[this], s = u8pixels[&source];
    int32_t bytes = d.format == 4 ? 2 : d.format == 10 ? 4 : 3;
    if (d.width * d.height > 0 && d.width * d.height < 16384) {
        auto* out = static_cast<uint8_t*>(d.data);
        auto* in = static_cast<uint8_t*>(s.data);
        for (int32_t i = 0; i < d.width * d.height * bytes; ++i)
            out[i] = in[0] ^ uint8_t(i) ^ uint8_t(d.format);
    }
}
// Fake UPixels::GetAverages for matching external-call events.
void UPixels::GetAverages(uint8_t* red, uint8_t* green, uint8_t* blue, uint8_t* alpha) {
    ev("Averages", {lab(this)});
    uint32_t v = ret("Averages", 0x12345678);
    *red = v >> 24;
    *green = v >> 16;
    *blue = v >> 8;
    *alpha = v;
}
// Fake UPixels::GetAlphaCount for matching external-call events.
int32_t UPixels::GetAlphaCount() {
    ev("AlphaCount", {lab(this)});
    return ret("AlphaCount");
}
} // namespace SamsUtilities
// Fake CWaveLoad::CWaveLoad for matching external-call events.
CWaveLoad::CWaveLoad() {
    ev("Wave.ctor", {newlab(this)});
}
// Fake CWaveLoad::~CWaveLoad for matching external-call events.
CWaveLoad::~CWaveLoad() {
    ev("Wave.dtor", {lab(this)});
    forget(this);
}
// Fake CWaveLoad::LoadTexture for matching external-call events.
int32_t CWaveLoad::LoadTexture(char* name) {
    ev("Wave.Load", {lab(this)}, {name});
    mWidth = ret("Wave.width", 3);
    mHeight = ret("Wave.height", 4);
    mAlphaData = ret("Wave.alpha") ? reinterpret_cast<uint8_t*>(uintptr_t(1)) : nullptr;
    mAlphaThreshold = ret("Wave.threshold");
    mLevel = ret("Wave.level");
    mData = reinterpret_cast<uint8_t*>(uintptr_t(0x1234));
    return ret("Wave.Load");
}
// The rest of the wave loader's interface: its vtable needs them, nothing here calls them.
CCompBase::~CCompBase() {}
int32_t CCompBase::IsNaturallyFlipped() { return 0; }
int32_t CWaveLoad::Decompress(SUnpackDestination*) { return 0; }
uint32_t CWaveLoad::AverageColor() { return 0; }
int32_t CWaveLoad::Blit(SUnpackDestination*, int32_t) { return 0; }
int32_t CWaveLoad::IsNaturallyFlipped() { return 0; }
// Fake CTextureEntry::CTextureEntry for matching external-call events.
CTextureEntry::CTextureEntry() {
    ev("Entry.ctor", {newlab(this)});
    mFlags = 0;
    mData = nullptr;
    mPitch = 0;
}
// Fake CTextureEntry::~CTextureEntry for matching external-call events.
CTextureEntry::~CTextureEntry() {
    ev("Entry.dtor", {lab(this)});
    forget(this);
}
// Fake CTextureEntry::Create for matching external-call events.
int32_t CTextureEntry::Create(int32_t size, int32_t flags) {
    ev("Entry.Create", {lab(this), size, flags});
    return ret("Entry.Create", 1);
}
// Fake CTextureEntry::Close for matching external-call events.
int32_t CTextureEntry::Close() {
    ev("Entry.Close", {lab(this)});
    return ret("Entry.Close", 1);
}
// Fake CTextureEntry::SetSize for matching external-call events.
void CTextureEntry::SetSize(int32_t width, int32_t height) {
    ev("Entry.SetSize", {lab(this), width, height});
}
// Fake CTextureEntry::GetData for matching external-call events.
void* CTextureEntry::GetData() {
    ev("Entry.GetData", {lab(this)});
    return mData;
}
// Fake CTextureEntry::GetCorrectedData for matching external-call events.
void* CTextureEntry::GetCorrectedData() {
    ev("Entry.Corrected", {lab(this)});
    return mData;
}
// Fake CTextureEntry::Lock for matching external-call events.
void CTextureEntry::Lock() {
    ev("Entry.Lock", {lab(this)});
}

extern "C" {
// Expose u8 setup to the differential harness.
void tpw_u8_setup(int32_t mode, int32_t format, uint32_t flags) {
    for (auto p : u8blocks)
        std::free(p.first);
    u8blocks.clear();
    gei_DrawPrimitive = &u8draw;
    u8draw.mFields[8] = static_cast<uint32_t>(mode);
    gei_TextureSettings.mFormat = format;
    gei_TextureSettings.mFlags = flags;
    tpw_cl_label(1, &gei_System);
    tpw_cl_label(2, &gei_Display);
    tpw_cl_label(3, &gei_TextureSettings);
    tpw_cl_label(4, &u8draw);
    tpw_cl_label(5, &u8drawGL);
}
// Expose u8 label to the differential harness.
int32_t tpw_u8_label(void* p) {
    return lab(p);
}
// Expose u8 block size to the differential harness.
int32_t tpw_u8_block_size(void* p) {
    for (auto b : u8blocks)
        if (b.first == p)
            return b.second;
    return -1;
}
}
