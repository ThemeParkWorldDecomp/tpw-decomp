// Main ride-sign fakes and a set/mesh builder. Engine tests use unit_RideSignMesh.
#include <cstdarg>
#include <cstring>
#include <strings.h>
#include "TPEngine.h"
#include "SignRenderer.h"
#include "FS_System.h"
#include "TPMain.h"
#include "TPSettings.h"
#include "Text.h"
#include "calllog.h"
#include "fake_strings.h"
#include "platform/msl.h"
#include "tb_mesh.h"
#include "tb_resource_texture.h"
#include "ui_ErrorDisplay.h"

using namespace calllog;
namespace {
tag_sRideAnimSet gSet;
tag_sMesh gMesh;
alignas(CMesh) unsigned char gEngine[2][sizeof(CMesh)];
char gLanguage[64];
SignRenderer* gRenderer;
void* gPixels[2];
void* gNewBlock;
}

// Globals of the main functions, with inert construction for unrelated classes.
CPlayerSettings gPlayerSettings;
void CPlayerSettings::Init() {}
CPlayerSettings::~CPlayerSettings() {}
CSYS_TPFileInfo gTPFileInfo;
tag_sRideAnimSet* gRideAnimSets[kMaxRideInstances];
GrafPort* gMainWindow;
CTextureSettings gei_TextureSettings;
CTextureSettings::CTextureSettings() {}
// CText is only called through this member, so its unused global body is raw storage.
alignas(CText) unsigned char gTextStorage[sizeof(CText)] asm("_gText");
char* CText::GetLanguageName() {
    ev("Language", {lab(this)});
    return gLanguage;
}
const TbDynamicStringTemplate<char>& CSYS_TPFileInfo::GetLocalPath() const {
    ev("LocalPath", {lab(this)});
    return mLocalPath;
}

// Renderer fakes record every argument, including full UTF-16 buffers in chunks.
SignRenderer::SignRenderer() {
    gRenderer = this;
    tpw_cl_label(90, this);
    ev("Renderer.ctor", {lab(this)});
}
SignRenderer::~SignRenderer() { ev("Renderer.dtor", {lab(this)}); }
TrueTypeRender::TrueTypeRender() {}
TrueTypeRender::~TrueTypeRender() {}
ShadowGlow::~ShadowGlow() {}
PlasticShader::~PlasticShader() {}
void Bitmap::clear() {}
int32_t SignRenderer::sign_load_settings(const char* path) { ev("Settings", {lab(this)}, {path}); return 1; }
int32_t SignRenderer::sign_install_fonts(const char* path) { ev("Fonts", {lab(this)}, {path}); return 1; }
int32_t SignRenderer::sign_render(NS_MacDoze::InternalHandle* dc, sign_renderer_options& options) {
    ev("Render", {lab(this), lab(dc), lab(options.mTexture1), lab(options.mTexture2),
                    options.mWidth, options.mHeight, options.mQuality});
    for (const char16_t* text : {options.mText1, options.mText2}) {
        int32_t length = 0;
        while (text[length]) ++length;
        ev("TextLength", {length});
        for (int32_t i = 0; i < length; i += 12) {
            int64_t words[12] = {};
            const int32_t count = length - i < 12 ? length - i : 12;
            for (int32_t j = 0; j < count; ++j) words[j] = text[i + j];
            ev("Text", {words[0], words[1], words[2], words[3], words[4], words[5],
                        words[6], words[7], words[8], words[9], words[10], words[11]});
        }
    }
    return 1;
}
void StripRideWrapping(tag_sRideAnimSet* set, tag_sNode* node, float amount, void* data, s3DPoint* point) {
    ev("Strip", {lab(set), lab(node), fbits(amount), lab(data), lab(point)});
}
void ERRORBox(const char* format, ...) {
    va_list args;
    va_start(args, format);
    const char* name = va_arg(args, const char*);
    va_end(args);
    ev("ERRORBox", {}, {format, name});
}
int32_t CheckFileExists(char* path) {
    ev("Exists", {}, {path});
    return static_cast<int32_t>(ret("Exists", 1));
}
void* MReleaseAllocF(uint32_t size, uint32_t flags) {
    ev("Alloc", {size, flags});
    void* block = tpw::ArenaAllocZeroed(size);
    gNewBlock = block;
    newlab(block);
    newlab(static_cast<char*>(block) + 0x16);
    return block;
}
void* MReleaseFreeF(void* block) {
    ev("Free", {lab(block)});
    // Keep freed bytes available to the dump, as on the emulator side.
    return nullptr;
}
int32_t NS_MacDoze::_stricmp(const char* a, const char* b) {
    ev("Compare", {}, {a, b});
    return ::strcasecmp(a, b);
}
NS_MacDoze::InternalHandle* NS_MacDoze::GetDC(GrafPort* window) {
    ev("GetDC", {lab(window)});
    return reinterpret_cast<InternalHandle*>(0x7777);
}
int32_t NS_MacDoze::ReleaseDC(GrafPort* window, InternalHandle* dc) {
    ev("ReleaseDC", {lab(window), lab(dc)});
    return 1;
}
int32_t CMesh::GenerateRideSign(uint32_t* indices, int32_t count) {
    ev("MeshSign", {lab(this), count, indices[0], indices[1]});
    const int32_t result = static_cast<int32_t>(ret("MeshSign", 1));
    // Write both out parameters even on failure, to test the caller's ordering.
    indices[0] = tpw::ArenaOffset(gPixels[0]);
    indices[1] = tpw::ArenaOffset(gPixels[1]);
    return result;
}
uint32_t wcslen(const char16_t* text) {
    uint32_t n = 0;
    while (text[n]) ++n;
    return n;
}
char16_t* wcscpy(char16_t* dst, const char16_t* src) {
    auto* result = dst;
    while ((*dst++ = *src++)) {}
    return result;
}
char16_t* wcsncpy(char16_t* dst, const char16_t* src, uint32_t count) {
    auto* result = dst;
    for (uint32_t i = 0; i < count; ++i) {
        dst[i] = *src;
        if (*src) ++src;
    }
    return result;
}

extern "C" {
void* tpw_rs_base() { return tpw::ArenaBase(); }
void* tpw_rs_alloc(uint32_t size) { return tpw::ArenaAllocZeroed(size); }
void tpw_rs_reset(void* header, uint32_t flags, int32_t engine, int32_t format,
                   const char* language, const char* path, int32_t software, int32_t present) {
    std::memset(&gSet, 0xa5, sizeof gSet);
    std::memset(&gMesh, 0xa5, sizeof gMesh);
    gSet.mFlags = flags;
    gSet.mHeads = &gMesh;
    gNewBlock = nullptr;
    gMesh.mHeader = static_cast<tag_sM3D2Header*>(header);
    gMesh.mEngineMesh = engine ? reinterpret_cast<CMesh*>(gEngine[0]) : nullptr;
    gMesh.mTypeEngineMesh = reinterpret_cast<CMesh*>(gEngine[1]);
    gei_TextureSettings.mFormat = format;
    std::strcpy(gLanguage, language);
    gTPFileInfo.mLocalPath.mData = const_cast<char*>(path);
    gPlayerSettings.mCurrent.mRenderer = static_cast<CPlayerSettings::eRENDERER>(software);
    gRideAnimSets[1] = present ? &gSet : nullptr;
    gMainWindow = reinterpret_cast<GrafPort*>(0x8888);
    tpw_cl_label(10, &gSet);
    tpw_cl_label(20, &gMesh);
    tpw_cl_label(60, gEngine[0]);
    tpw_cl_label(61, gEngine[1]);
    tpw_cl_label(4, &gText);
    tpw_cl_label(5, &gTPFileInfo);
    tpw_cl_label(6, gMainWindow);
    tpw_cl_label(7, reinterpret_cast<void*>(0x7777));
    gPixels[0] = tpw::ArenaAllocZeroed(4);
    gPixels[1] = tpw::ArenaAllocZeroed(4);
    tpw_cl_label(70, gPixels[0]);
    tpw_cl_label(71, gPixels[1]);
}
uint32_t tpw_rs_pixel(int32_t index) { return tpw::ArenaOffset(gPixels[index]); }
int32_t tpw_rs_call(int32_t op, uint32_t instance, char* folder, char* name, char* font,
                     const char16_t* text1, const char16_t* text2) {
    if (op == 0) return GenerateRideSign(&gSet, folder, name, font, text1, text2);
    if (op == 1) {
        DestroyRideSign(&gSet);
        return 0;
    }
    return RegenerateRideSign(instance, folder, name, text1, text2);
}
uint32_t tpw_rs_flags() { return gSet.mFlags; }
void* tpw_rs_newblock() { return gNewBlock; }
}
