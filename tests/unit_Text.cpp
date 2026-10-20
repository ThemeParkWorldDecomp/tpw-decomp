// Call-log fakes for Text.cpp. No ltms implementation is linked into this unit.
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <new>

#include "Debug.h"
#include "ExitErr.h"
#include "FS_Registry.h"
#include "ResolutionBalance.h"
#include "TPSettings.h"
#include "TbITable.h"
#include "TbResource.h"
#include "Text.h"
#include "calllog.h"
#include "platform/samsutils.h"

#include "fake_strings.h"

using calllog::ev;
using calllog::lab;
using calllog::ret;

// Globals of other files, with unrecorded balance/settings lifetime fakes.
char gLanguageName[0x400];
CExitError gExitError;
CResolutionBalance gResolutionBalance;
CPlayerSettings gPlayerSettings;

// The balance constructor needs no loading in this unit.
void CBalanceTable::Init() {}

// Player settings are supplied by each case.
void CPlayerSettings::Init() {}

// The player settings fake owns no resources.
CPlayerSettings::~CPlayerSettings() {}

// The resolution balance fake owns no resources.
CResolutionBalance::~CResolutionBalance() {}

// No balance descriptions are needed by Text.cpp.
const BalanceDescription* CResolutionBalance::GetBalanceDescription(int32_t) {
    return nullptr;
}

// Expose the fake resolution value slots.
BalanceValue* CResolutionBalance::GetBalanceValuesStart() {
    return mValues;
}

// The base font fake owns no resources.
InterfaceFont::InterfaceFont() {}   // the fake leaves the object as it finds it
InterfaceFont::~InterfaceFont() {}

// Record wrapper construction and retain its supplied font.
InterfaceFontWrapper::InterfaceFontWrapper(TbIFont* font) : mFont(font) {
    ev("FontWrapper", {lab(this), lab(font)});
}

// Cleanup is tested through UnInit rather than wrapper destruction.
InterfaceFontWrapper::~InterfaceFontWrapper() {}

// The manager fake owns no resources.
TbTextResourceManager::~TbTextResourceManager() {}

// The render-parameter fake owns no resources.
TbFontRenderParams::~TbFontRenderParams() {}

// Unused resource slots added by the complete ltms base declaration.
int32_t TbResource::Release() { return 0; }
void TbResource::AddRef() {}
int32_t TbResource::ManagerRelease() { return 0; }
void TbResource::ManagerAddRef() {}

// Record the two CText words already initialised before member construction.
TbTextResourceManager::TbTextResourceManager() {
    ev("ResourceManager", {lab(this), gText.mInitialised, gText.mResolutionGroup});
}

// Record render-parameter construction without changing seeded fields.
TbFontRenderParams::TbFontRenderParams() {
    ev("RenderParams", {lab(this)});
}

TbIFont::~TbIFont() {}

namespace {

class Resource : public TbIFont {
public:
    // Record each resource release and return its queued result.
    int32_t Release() override {
        ev("Release", {lab(this)});
        return static_cast<int32_t>(ret("Release"));
    }
    // The font interface's own slots: nothing in Text.cpp calls them.
    uint32_t GetCharIndex(const unsigned char*&) override { return 0; }
    uint16_t GetCharCount() override { return 0; }
    TbFontCharSize GetCharSize(uint16_t) override { return {0, 0}; }
    uint8_t GetVerticalSpacing() override { return 0; }
    uint32_t GetCaps() override { return 0; }
    uint32_t GetType() override { return 0; }
    TbIMBToUnicode* GetMBToUnicode() override { return nullptr; }
};
Resource resources[512];

// Convert a logical resource label into a stable host pointer.
void* ptr(uint32_t label) {
    return label == 0 ? nullptr : &resources[label];
}

// Apply the queued manager state change and return the operation's status.
TbError Result(const char* name, TbTextResourceManager* p) {
    p->SetState(0, static_cast<uint32_t>(ret("ManagerState", 0x1234)));
    return TbError(static_cast<int32_t>(ret(name)));
}

// Record a resource request; its queued output is independent of its status.
template<class R>
TbError Get(TbTextResourceManager* p, R** out, uint32_t id, const char* name) {
    ev(name, {lab(p), id, lab(*out)});
    const auto output = ret("ResourceOut", id < 1000 ? 180 + id : id - 800);
    if (output >= 0) {
        *out = static_cast<R*>(ptr(static_cast<uint32_t>(output)));
    }
    return Result("GetResource", p);
}

// Format a diagnostic into the same call-log text shape as the PowerPC fake.
void Message(const char* name, const char* format, va_list ap) {
    char text[1024];
    std::vsnprintf(text, sizeof(text), format, ap);
    ev(name, {}, {text});
}

}  // namespace

// Record the resource root and return its queued open status.
TbError TbTextResourceManager::Open(const char* path) {
    ev("Open", {lab(this)}, {path});
    return Result("Open", this);
}

// Record the selected language and return its queued status.
TbError TbTextResourceManager::SetLanguage(const char* name) {
    ev("SetLanguage", {lab(this)}, {name});
    return Result("SetLanguage", this);
}

// Record manager closure and its queued state change.
TbError TbTextResourceManager::Close() {
    ev("Close", {lab(this)});
    return Result("Close", this);
}

// String tables share the resource request queues.
TbError TbTextResourceManager::GetResource(TbITable** out, uint32_t id) {
    return Get(this, out, id, "GetTable");
}

// Fonts share the resource request queues.
TbError TbTextResourceManager::GetResource(TbIFont** out, uint32_t id) {
    return Get(this, out, id, "GetFont");
}

// MB-to-Unicode converters share the resource request queues.
TbError TbTextResourceManager::GetResource(TbIMBToUnicode** out, uint32_t id) {
    return Get(this, out, id, "GetMBToUnicode");
}

// Unicode-to-MB converters share the resource request queues.
TbError TbTextResourceManager::GetResource(TbIUnicodeToMB** out, uint32_t id) {
    return Get(this, out, id, "GetUnicodeToMB");
}

// Record alpha-write selection and apply the queued render-parameter state.
TbError TbFontRenderParams::UseAlphaWriteMode() {
    ev("UseAlphaWriteMode", {lab(this)});
    mState[0] = static_cast<uint32_t>(ret("AlphaState", 0x4321));
    return TbError(static_cast<int32_t>(ret("UseAlphaWriteMode")));
}

// Record the error and mark the fake error object as set.
void CExitError::SetError(ErrorType type, uint32_t value, const char* text) {
    ev("SetError", {lab(this), type, value}, {text});
    mError = 1;
}

// Record the formatted trace message.
void TRACE(const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    Message("TRACE", format, ap);
    va_end(ap);
}

// Record the formatted debug message.
void SamsUtilities::UDebug(const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    Message("UDebug", format, ap);
    va_end(ap);
}

extern "C" {

// Expose the global CText for fixed object labels.
void* tpw_text_object() {
    return &gText;
}

// Expose a labelled fake resource.
void* tpw_text_resource(uint32_t label) {
    return ptr(label);
}

// Expose the manager or render parameters for fixed object labels.
void* tpw_text_subobject(int32_t which) {
    return which == 0 ? static_cast<void*>(&gText.mResources) : &gText.mRenderParams;
}

// Expose the fake error object.
void* tpw_text_error() {
    return &gExitError;
}

// Expose one of the eighteen global strings in construction order.
void* tpw_text_string(int32_t i) {
    return i < 16 ? &gTextLanguages[i] : i == 16 ? &gTextTitle : &gTextLanguage;
}

// Expose a wrapper for static-initializer call labels.
void* tpw_text_font(int32_t i) {
    return &gText.mFonts[i];
}

// Reconstruct the globals in the original static initializer's order.
void tpw_text_sinit() {
    static const char* names[] = {
        "English", "French", "German", "Spanish", "Swedish", "Italian",
        "Dutch", "Polish", "Danish", "Japanese", "TraditionalChinese",
        "SimplifiedChinese", "Thai", "Korean", "Hebrew", "American",
        "Theme Park World", "Language"
    };
    for (int i = 0; i < 18; ++i) {
        new (tpw_text_string(i)) TbDynamicStringTemplate<char>(names[i]);
    }
    new (&gText) CText;
}

// Seed every logical field from the case's values and resource labels.
void tpw_text_seed(const uint32_t* w, int32_t resolution, int32_t override,
                   const char* language, int32_t error) {
    gText.mInitialised = w[0];
    gText.mResolutionGroup = w[1];
    gText.mLanguageId = w[2];
    for (int i = 0; i < 3; ++i) {
        gText.mResources.SetState(i, w[3 + i]);
    }
    for (int i = 0; i < 52; ++i) {
        auto& f = gText.mFonts[i];
        const auto* v = w + 6 + i * 5;
        f.mF00 = v[0] >> 16;
        f.mF02 = v[0];
        f.mF04 = v[1] >> 16;
        f.mHeight = v[1];
        f.mBitmap = reinterpret_cast<uint8_t*>(uintptr_t(v[2]));
        f.mID = v[3];
        f.mFont = static_cast<TbIFont*>(ptr(v[4]));
    }
    for (int i = 0; i < 19; ++i) {
        gText.mRenderParams.mState[i] = w[266 + i];
    }
    for (int i = 0; i < 23; ++i) {
        gText.mResourcePointers[i] = ptr(w[285 + i]);
    }
    gPlayerSettings.mCurrent.mResolution = static_cast<CPlayerSettings::eRESOLUTION>(resolution);
    gResolutionBalance.mValues[1].mInteger = override;
    std::strcpy(gLanguageName, language);
    gExitError.mError = error;
}

// Read all logical fields, converting resource pointers back to labels.
void tpw_text_read(uint32_t* w) {
    w[0] = gText.mInitialised;
    w[1] = gText.mResolutionGroup;
    w[2] = gText.mLanguageId;
    for (int i = 0; i < 3; ++i) {
        w[3 + i] = gText.mResources.State(i);
    }
    for (int i = 0; i < 52; ++i) {
        const auto& f = gText.mFonts[i];
        auto* v = w + 6 + i * 5;
        v[0] = (uint32_t(f.mF00) << 16) | f.mF02;
        v[1] = (uint32_t(f.mF04) << 16) | uint16_t(f.mHeight);
        v[2] = uint32_t(reinterpret_cast<uintptr_t>(f.mBitmap));
        v[3] = f.mID;
        v[4] = lab(f.mFont);
    }
    for (int i = 0; i < 19; ++i) {
        w[266 + i] = gText.mRenderParams.mState[i];
    }
    for (int i = 0; i < 23; ++i) {
        w[285 + i] = lab(gText.mResourcePointers[i]);
    }
    w[308] = gExitError.mError;
}

// Dispatch a test entry while normalising void and pointer results.
uint32_t tpw_text_call(int32_t which, uint32_t arg) {
    switch (which) {
    case 0:
        return gText.Init();
    case 1:
        gText.SetRez(static_cast<int32_t>(arg));
        break;
    case 2:
        return gText.GetLanguageName() == gLanguageName;
    case 3:
        return gText.GetLanguageId();
    case 4:
        gText.SetLanguageId(arg);
        break;
    case 5:
        return gText.IsHebrew();
    case 6:
        gText.UnInit();
        break;
    case 7:
        gText.InitThemeSpecific();
        break;
    case 8:
        gText.DeInitThemeSpecific();
        break;
    }
    return 0;
}

}  // extern "C"
