// Test library for the online outbox panel's helpers (unit v5017; libtpw_unit_CalcEmailAddressScrollBar.dylib,
// tests/test_CalcEmailAddressScrollBar.py), main app:
//   src/UI_IslandLobby_CalcEmailAddressScrollBar.cpp   FillOutboxList, SetCurrentEmailInfo, CalcEmailAddressScrollBar, UpdatePreview
//
// The callees outside the unit are recording fakes (the same event names and arguments as the module's PowerPC stubs). The windows are
// five fixed blocks whose first word is a hand made vtable (SetValue and SetText are virtual calls); operator new answers from the arena
// (so that the words of an object made by `new` hold 32-bit addresses) and counts its calls. The case's numbers come in gCtl.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <type_traits>
#include <utility>

#include "calllog.h"
#include "platform/arena.h"

#include "FS_UserInfo.h"
#include "InterfaceScrollBar.h"
#include "InterfaceWindow.h"
#include "Postcard.h"
#include "TbGraphicFile.h"
#include "TbITable.h"
#include "TbVTag.h"
#include "Text.h"
#include "UI_Base.h"
#include "email.h"
#include "lobby.h"
#include "platform/macdoze.h"
#include "platform/msl.h"
#include "platform/upixels.h"
#include "tb_resource_texture.h"
#include "ui_Postcard.h"
#include "ui_controls.h"
#include "ui_main.h"

using namespace calllog;
typedef int64_t I;

// ---- globals of other files, defined here
CFSUserInfo* gUserInfo;
char16_t gPostcardText5da5[0x200];
char16_t gUIMainConvertBuffer[0x4000];
// Objects with constructors the library does not compile: raw storage under the program's symbol names.
uint8_t gTextStorage[sizeof(CText)] __asm__("_gText");
uint8_t gTextureStorage[sizeof(CTextureSettings)] __asm__("_gei_TextureSettings");

namespace {

int32_t gCtl[32];             // 0..3 client rect l,t,r,b; 4..6 surface pointer, width, height; 7 bitmap pixels; 8 GetData answer;
                              // 9..12 measured size l,t,r,b; 13 SetValue answer; 14 E1, 15 E2, 16..18 numbers (the new user info)
int32_t gNewCount;
alignas(16) uint8_t gWin[5][0x200];
void* gVtable[64];
void* gUPixels[8];
int gUPixelsCount;

I TextHash(const char16_t* text) {
    uint32_t h = 17;
    for (const char16_t* p = text; *p; ++p) h = h * 31 + *p;
    return h;
}
I TextLen(const char16_t* text) {
    I n = 0;
    while (text[n]) ++n;
    return n;
}
I CHash(const char* text) {
    uint32_t h = 17;
    for (const char* p = text; *p; ++p) h = h * 31 + static_cast<uint8_t>(*p);
    return h;
}
int Ordinal(const void* p) {
    for (int i = 0; i < gUPixelsCount; ++i)
        if (gUPixels[i] == p) return i;
    return -1;
}
bool IsOutbox(const void* p) {
    return gUserInfo != nullptr && p == static_cast<const void*>(&gUserInfo->mOutbox);
}

size_t SlotOf(const void* pointer, size_t size) {
    uintptr_t words[2] = {0, 0};
    std::memcpy(words, pointer, size);
    // Itanium: the pointer is the vtable offset + 1; the ARM variant: the offset itself, with the low bit of the adjustment set.
    if (words[1] & 1) return words[0] / sizeof(void*);
    return (words[0] - 1) / sizeof(void*);
}

int32_t FakeSetValue(void* self, int32_t value) {
    ev("SetValue", {I(lab(self)), I(value)});
    return gCtl[13];
}
int32_t FakeSetText(void* self, char16_t* text) {
    ev("SetText", {I(lab(self)), I(text == gPostcardText5da5), TextLen(text), TextHash(text)});
    return 1;
}

struct Init {
    Init() {
        auto setValue = &InterfaceWindow::SetValue;
        auto setText = &InterfaceWindow::SetText;
        gVtable[SlotOf(&setValue, sizeof setValue)] = reinterpret_cast<void*>(&FakeSetValue);
        gVtable[SlotOf(&setText, sizeof setText)] = reinterpret_cast<void*>(&FakeSetText);
        for (auto& w : gWin) *reinterpret_cast<void**>(w) = gVtable;
    }
} gInit;

}  // namespace


// ---- the fakes
// The constructor is defined under its symbol name so that the members' constructors (other files) are not needed.
extern "C" void FakeUserInfoCtor(CFSUserInfo* self) __asm__("__ZN11CFSUserInfoC1Ev");
extern "C" void FakeUserInfoCtor(CFSUserInfo* self) {
    ev("CFSUserInfo", {});
    uint8_t* outbox = reinterpret_cast<uint8_t*>(&self->mOutbox);
    // A Mac word is an 8-byte slot at twice the Mac offset; the entries are pointers of the arena (64 bits on the host).
    uint64_t* w = reinterpret_cast<uint64_t*>(outbox);
    w[2] = static_cast<uint32_t>(gCtl[14]) | (static_cast<uint64_t>(gCtl[19]) << 32);
    w[5] = reinterpret_cast<uintptr_t>(outbox + 2 * 0x10);
    w[6] = static_cast<uint32_t>(gCtl[15]) | (static_cast<uint64_t>(gCtl[20]) << 32);
    w[7] = static_cast<uint32_t>(gCtl[16]);
    w[8] = static_cast<uint32_t>(gCtl[17]);
    w[9] = static_cast<uint32_t>(gCtl[18]);
}

void Outbox::First() { ev("OutboxFirst", {I(IsOutbox(this))}); }
bool Outbox::Next() {
    ev("OutboxNext", {I(IsOutbox(this))});
    return ret("Next") != 0;
}

InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {I(lab(this)), I(id)});
    switch (id) {
        case 0xee2: return reinterpret_cast<InterfaceWindow*>(gWin[1]);
        case 0xee3: return reinterpret_cast<InterfaceWindow*>(gWin[2]);
        case 0xee4: return reinterpret_cast<InterfaceWindow*>(gWin[3]);
        case 0xee8: return reinterpret_cast<InterfaceWindow*>(gWin[4]);
    }
    return nullptr;
}
int32_t InterfaceWindow::GetClientRect(CoolRect* rect) {
    ev("GetClientRect", {I(lab(this))});
    rect->mLeft = static_cast<int16_t>(gCtl[0]);
    rect->mTop = static_cast<int16_t>(gCtl[1]);
    rect->mRight = static_cast<int16_t>(gCtl[2]);
    rect->mBottom = static_cast<int16_t>(gCtl[3]);
    return 0;
}
int32_t InterfaceWindow::Show(int16_t on) {
    ev("Show", {I(lab(this)), I(on)});
    return 0;
}
uint8_t* InterfaceWindow::GetClientObjectSurface(int32_t* width, int32_t* height) {
    ev("GetClientObjectSurface", {I(lab(this))});
    *width = gCtl[5];
    *height = gCtl[6];
    return reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(static_cast<uint32_t>(gCtl[4])));
}
void InterfaceWindow::FinishWithClientObjectSurface() { ev("FinishWithClientObjectSurface", {I(lab(this))}); }
void InterfaceScrollBar::SetMinMax(int32_t min, int32_t max) { ev("SetMinMax", {I(lab(this)), I(min), I(max)}); }

int32_t InterfaceListControl2::AddItem(UI_LCAdd* item, int32_t data, int32_t position, int32_t update) {
    ev("AddItem", {I(lab(this)), I(item[0].mText == gUIMainConvertBuffer), TextLen(item[0].mText), TextHash(item[0].mText),
                   TextLen(item[1].mText), TextHash(item[1].mText), I(item[2].mInt), I(item[3].mInt), I(data), I(position), I(update)});
    return 0;
}
void InterfaceListControl2::UpdateDisplay() { ev("UpdateDisplay", {I(lab(this))}); }

void UI_MeasureText(const char16_t* text, const TbRect& rect, int8_t font, int8_t horizontal, int8_t vertical, int32_t wrap,
                    TbRect* extent) {
    ev("UI_MeasureText", {I(text == gPostcardText5da5), TextLen(text), TextHash(text), I(rect.left), I(rect.top), I(rect.right),
                          I(rect.bottom), I(font), I(horizontal), I(vertical), I(wrap), I(extent != nullptr)});
    if (extent != nullptr) {
        extent->left = gCtl[9];
        extent->top = gCtl[10];
        extent->right = gCtl[11];
        extent->bottom = gCtl[12];
    }
}

int32_t TbITable::FormatString(char16_t* out, uint32_t size, uint32_t id, ...) const {
    va_list args;
    va_start(args, id);
    TbIntegerVTag* tags[3];
    for (auto& tag : tags) tag = static_cast<TbIntegerVTag*>(va_arg(args, TbVTag*));
    va_end(args);
    ev("FormatString", {I(this == gText.mResourcePointers[6]), I(size), I(id), I(tags[0]->mMagic), I(tags[0]->mId), I(tags[0]->mValue),
                        I(tags[1]->mMagic), I(tags[1]->mId), I(tags[1]->mValue), I(tags[2]->mMagic), I(tags[2]->mId),
                        I(tags[2]->mValue)});
    out[0] = u'A' + (tags[0]->mValue & 15);
    out[1] = u'A' + (tags[1]->mValue & 15);
    out[2] = u'A' + (tags[2]->mValue & 15);
    out[3] = 0;
    return 0;
}
int32_t TbIntegerVTag::Format(CStaticBuffer&, TbVarFormat&) { return 0; }

namespace NS_MacDoze {
int32_t MultiByteToWideChar(uint32_t codePage, uint32_t flags, const char* source, int32_t length, char16_t* dest, int32_t size) {
    ev("MultiByteToWideChar", {I(codePage), I(flags), CHash(source), I(std::strlen(source)), I(length), I(dest == gUIMainConvertBuffer),
                               I(size)});
    int32_t n = 0;
    while (source[n] && n < size - 1) {
        dest[n] = static_cast<uint8_t>(source[n]);
        ++n;
    }
    dest[n] = 0;
    return n + 1;
}
}  // namespace NS_MacDoze

char16_t* wcscpy(char16_t* dest, const char16_t* source) {
    ev("wcscpy", {I(dest == gPostcardText5da5), TextLen(source), TextHash(source)});
    char16_t* d = dest;
    while ((*d++ = *source++) != 0) {}
    return dest;
}
char16_t* wcscat(char16_t* dest, const char16_t* source) {
    ev("wcscat", {I(dest == gPostcardText5da5), TextLen(source), TextHash(source)});
    char16_t* d = dest;
    while (*d) ++d;
    while ((*d++ = *source++) != 0) {}
    return dest;
}

PostcardBitmap::PostcardBitmap(uint32_t width, uint32_t height, uint32_t bytesPerPixel)
    : mWidth(width), mHeight(height), mBytesPerPixel(bytesPerPixel),
      mPixels(reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(static_cast<uint32_t>(gCtl[7])))) {
    ev("PostcardBitmap", {I(width), I(height), I(bytesPerPixel)});
}
PostcardBitmap::~PostcardBitmap() { ev("~PostcardBitmap", {}); }
int32_t PostcardBitmap::resize_into_buffer(uint8_t* out, int32_t width, int32_t height, int32_t format) {
    ev("resize_into_buffer", {I(reinterpret_cast<uintptr_t>(out) == static_cast<uint32_t>(gCtl[4])), I(width), I(height), I(format),
                              I(mPixels != nullptr)});
    return 1;
}

namespace SamsUtilities {
typedef void* DataType;
UPixels::UPixels(long format, int width, int height, UPixel* data) {
    gUPixels[gUPixelsCount++] = this;
    ev("UPixels4", {I(format), I(width), I(height), I(data != nullptr)});
}
UPixels::UPixels(long format, void* pixels, int width, int height, int stride, UPixel* data) {
    gUPixels[gUPixelsCount++] = this;
    ev("UPixels6", {I(format), I(reinterpret_cast<uintptr_t>(pixels) == static_cast<uint32_t>(gCtl[7])), I(width), I(height),
                    I(stride), I(data != nullptr)});
}
UPixels::~UPixels() { ev("~UPixels", {I(Ordinal(this))}); }
DataType UPixels::GetData() {
    ev("GetData", {I(Ordinal(this))});
    return reinterpret_cast<DataType>(static_cast<uintptr_t>(static_cast<uint32_t>(gCtl[8])));
}
void UPixels::Convert(UPixels& source, bool flag) { ev("Convert", {I(Ordinal(this)), I(Ordinal(&source)), I(flag)}); }
}  // namespace SamsUtilities

TbJPEGLoader::TbJPEGLoader() { ev("TbJPEGLoader", {}); }
TbJPEGLoader::~TbJPEGLoader() { ev("~TbJPEGLoader", {}); }
TbRenderArea::TbRenderArea() { ev("TbRenderArea", {}); }

void read_JPEG_file(char* path, uint8_t* pixels) {
    ev("read_JPEG_file", {CHash(path), I(reinterpret_cast<uintptr_t>(pixels) == static_cast<uint32_t>(gCtl[8]))});
}

extern "C" {

uint64_t tpw_ob_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gUserInfo", &gUserInfo},
        {"gText", gTextStorage},
        {"gei", gTextureStorage},
        {"gOutboxWindow", &gOutboxWindow},
        {"gOutboxCursor", &gOutboxCursor},
        {"gOutboxPreview", gOutboxPreview},
        {"post", gPostcardText5da5},
        {"conv", gUIMainConvertBuffer},
        {"height", &gUIRealScreenHeight},
        {"width", &gUIRealScreenWidth},
        {"ctl", gCtl},
        {"win0", gWin[0]},
        {"win1", gWin[1]},
        {"win2", gWin[2]},
        {"win3", gWin[3]},
        {"win4", gWin[4]},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_ob_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The state a case starts from (the fakes' counters).
void tpw_ob_reset() {
    gNewCount = 0;
    gUPixelsCount = 0;
}
int32_t tpw_ob_news() { return gNewCount; }

int32_t tpw_ob_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CFSUserInfo, mOutbox));
    out[n++] = static_cast<int64_t>(sizeof(CFSUserInfo));
    out[n++] = static_cast<int64_t>(offsetof(CText, mResourcePointers) + 6 * sizeof(void*));
    out[n++] = static_cast<int64_t>(offsetof(CTextureSettings, mFormat));
    return n;
}

void tpw_ob_fill() { FillOutboxList(); }
void tpw_ob_setcur(int32_t number) { SetCurrentEmailInfo(number); }
void tpw_ob_calc() { CalcEmailAddressScrollBar(); }
void tpw_ob_preview() { UpdatePreview(); }

}  // extern "C"
