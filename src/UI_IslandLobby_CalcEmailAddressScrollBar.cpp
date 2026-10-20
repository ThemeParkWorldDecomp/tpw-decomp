// The online outbox panel's helpers (the inventory files them under ui_onlineOutbox.cpp; unit v5017): the address list's scroll bar,
// the current e-mail's address text, the outbox list and the picture preview.
//
// The outbox cursor functions (Outbox::First and Next) are untranslated, and the Mac's outbox is read by its Mac offsets (the words are
// 32-bit pointers there): a word of the outbox or of an entry is read with Word() below, from the object's address plus the Mac offset.
#include <cstdint>

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

InterfaceWindow* gOutboxWindow;      // 0x10395e3c
uint8_t* gOutboxCursor;              // 0x10395e38
uint32_t gOutboxPreview[9];          // 0x10395e10
const char16_t gEmailSeparators[3] = {0, 0x000a, 0};   // 0x1024eeca

namespace {

// The word of a Mac object at Mac offset `offset`. On the host a Mac word (a pointer or a number) is a slot of 8 bytes, so the slot
// sits at twice the Mac offset; the value is the slot's full width (pointers are 64 bits here).
uintptr_t Word(const void* object, uint32_t offset) {
    return *reinterpret_cast<const uintptr_t*>(reinterpret_cast<const uint8_t*>(object) + 2 * offset);
}
const void* Ptr(const void* object, uint32_t offset) { return reinterpret_cast<const void*>(Word(object, offset)); }

uint8_t* OutboxOf(CFSUserInfo* info) { return reinterpret_cast<uint8_t*>(&info->mOutbox); }

// The user info is made on first use (the same test as UpdateOutboxMailIcon).
CFSUserInfo* UserInfo() {
    if (gUserInfo == nullptr) gUserInfo = new CFSUserInfo;
    return gUserInfo;
}

}  // namespace

// 0x101e378c: one list row for each mail of the outbox: the address text (into gUIMainConvertBuffer), and the three numbers of the current
// mail formatted by string 0x1c0 of the interface table (variable tags 0x10, 0x11, 0x12); the row's data word is its number | 0x1000000.
void FillOutboxList() {
    Outbox* outbox = &UserInfo()->mOutbox;
    uint8_t* cursor = reinterpret_cast<uint8_t*>(outbox) + 2 * 4;   // the Mac's outbox + 4
    InterfaceListControl2* list = static_cast<InterfaceListControl2*>(gOutboxWindow->GetWindow(0xee8));
    outbox->First();
    uint32_t index = 0;
    do {
        const char* text = static_cast<const char*>(Ptr(Ptr(cursor, 4), 0xc));
        NS_MacDoze::MultiByteToWideChar(0, 0, text, -1, gUIMainConvertBuffer, 0x3fff);
        char16_t formatted[32];
        TbIntegerVTag first(0x10, static_cast<int32_t>(Word(cursor, 0x18)));
        TbIntegerVTag second(0x11, static_cast<int32_t>(Word(cursor, 0x1c)));
        TbIntegerVTag third(0x12, static_cast<int32_t>(Word(cursor, 0x20)));
        static_cast<TbITable*>(gText.mResourcePointers[6])->FormatString(formatted, 0x1f, 0x1c0, &first, &second, &third);
        UI_LCAdd item[4];
        item[0].mRaw = 0;
        item[1].mRaw = 0;
        item[2].mRaw = 0;
        item[3].mRaw = 0;
        item[0].mText = gUIMainConvertBuffer;
        item[1].mText = formatted;
        item[2].mInt = 1;
        item[3].mInt = 0;
        list->AddItem(item, static_cast<int32_t>(index | 0x1000000u), -1, 0);
        ++index;
    } while (outbox->Next());
    list->UpdateDisplay();
}

// 0x101e3960: the outbox goes to mail number `number` (the two top bits of the 6 bits at 0x03000000 are cleared, so the count
// is the low 24 bits and bits 26 up), the cursor is kept, and gPostcardText5da5 becomes the addresses of that mail, each followed by a newline.
void SetCurrentEmailInfo(int32_t number) {
    Outbox* outbox = &UserInfo()->mOutbox;
    gOutboxCursor = reinterpret_cast<uint8_t*>(outbox) + 2 * 4;   // the Mac's outbox + 4
    outbox->First();
    for (uint32_t count = static_cast<uint32_t>(number) & 0xfcffffffu; count != 0; --count) outbox->Next();
    uintptr_t node = Word(gOutboxCursor, 0x10);
    wcscpy(gPostcardText5da5, gEmailSeparators);
    while (node != reinterpret_cast<uintptr_t>(gOutboxCursor) + 2 * 0xc) {
        const char* text = static_cast<const char*>(Ptr(Ptr(reinterpret_cast<const void*>(node), 8), 0xc));
        NS_MacDoze::MultiByteToWideChar(0, 0, text, -1, gUIMainConvertBuffer, 0x3fff);
        wcscat(gPostcardText5da5, gUIMainConvertBuffer);
        wcscat(gPostcardText5da5, gEmailSeparators + 1);
        node = Word(reinterpret_cast<const void*>(node), 4);
    }
}

// 0x101e3aac: the address text is measured in a box as wide as the client rectangle and as high as it, in the interface's units (the
// real screen's pixels over 2048 and over 1536); when the text is higher than the client area the scroll bar is shown with the excess
// as its range, else it is hidden. A bar that does not move (SetValue(0) answers 0) leaves the window to take the text again.
void CalcEmailAddressScrollBar() {
    InterfaceWindow* window = gOutboxWindow->GetWindow(0xee2);
    InterfaceScrollBar* bar = static_cast<InterfaceScrollBar*>(window->GetWindow(0xee3));
    CoolRect client;
    window->GetClientRect(&client);
    const int32_t high = (client.mBottom - client.mTop) * static_cast<int32_t>(gUIRealScreenHeight);
    int32_t scaled = static_cast<int32_t>((static_cast<int64_t>(0x2aaaaaab) * high) >> 32) >> 8;   // mulhw, srawi 8
    scaled = scaled + static_cast<int32_t>(static_cast<uint32_t>(scaled) >> 31);
    const int32_t wide = (client.mRight - client.mLeft) * static_cast<int32_t>(gUIRealScreenWidth);
    TbRect limit;
    limit.left = 0;
    limit.top = 0;
    limit.right = wide / 2048;
    limit.bottom = scaled;
    TbRect size;
    size.left = 0;
    size.top = 0;
    size.right = 0;
    size.bottom = 0;
    UI_MeasureText(gPostcardText5da5, limit, 6, 0, 0, 1, &size);
    const int32_t needed = size.bottom * 0x600 / static_cast<int32_t>(gUIRealScreenHeight);
    const int32_t shown = client.mBottom - client.mTop;
    if (needed > shown) {
        bar->Show(1);
        bar->SetMinMax(needed - shown, 0);
    } else {
        bar->Show(0);
    }
    if (static_cast<int16_t>(bar->SetValue(0)) == 0) window->SetText(gPostcardText5da5);
}

// 0x101e3c94: the current mail's picture (a JPEG file named by the entry the cursor holds) is read into a bitmap and scaled into the
// client surface of the preview window (filter 2 for texture formats 2 and 4, 1 for 3 and 5, none for the others).
void UpdatePreview() {
    TbJPEGLoader loader;
    TbRenderArea area;
    InterfaceWindow* window = gOutboxWindow->GetWindow(0xee4);
    int32_t width;
    int32_t height;
    uint8_t* surface = window->GetClientObjectSurface(&width, &height);
    if (surface == nullptr) return;
    PostcardBitmap bitmap(gOutboxPreview[7], gOutboxPreview[8], 4);
    typedef decltype(SamsUtilities::kPixels32ARGB) Format;   // the format numbers 7 and 9 of the listing
    SamsUtilities::UPixels source(static_cast<Format>(7), gOutboxPreview[7], gOutboxPreview[8], nullptr);
    SamsUtilities::UPixels dest(static_cast<Format>(9), bitmap.mPixels, gOutboxPreview[7], gOutboxPreview[8], -1, nullptr);
    char* path = const_cast<char*>(static_cast<const char*>(Ptr(Ptr(gOutboxCursor, 0x14), 0xc)));
    read_JPEG_file(path, reinterpret_cast<uint8_t*>(source.GetData()));
    dest.Convert(source, false);
    switch (gei_TextureSettings.mFormat) {
        case 2:
        case 4:
            bitmap.resize_into_buffer(surface, width, height, 2);
            break;
        case 3:
        case 5:
            bitmap.resize_into_buffer(surface, width, height, 1);
            break;
        default:
            break;
    }
    window->FinishWithClientObjectSurface();
}
