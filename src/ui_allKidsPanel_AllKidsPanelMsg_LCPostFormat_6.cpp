// Unit v5560: the all-items list's row attach function (LCPostFormat of ui_allKidsPanel.cpp, 0x101966ec; named AllItemsListLCPostFormat
// because LCPostFormat exists at other addresses). The row gets a text draw object, then the columns that the list type needs.
#include <cstddef>
#include <cstdint>
#include <new>

#include "InterfaceFont.h"
#include "InterfaceWindow.h"
#include "MainWindowCallback.h"
#include "ui_DrawObject.h"
#include "ui_main.h"

namespace {

using namespace allitems_post_callee;

// The number of bytes the Mac allocates for a text object (0x19c) and for a gradient (0x124).
void* NewText() { return OperatorNew(0x19c); }

void SetFontOf(InterfaceWindow* window) { window->SetFont(GetFontTableEntry(gAllItemsFontIndex)); }

// A number column: number text, the font, the value cleared, centred.
void NumberColumn(InterfaceWindow* window) {
    void* memory = NewText();
    NumberCtor(memory);
    window->SetClientDrawObject(static_cast<InterfaceDrawObject*>(static_cast<InterfaceNumberTextObject*>(memory)));
    SetFontOf(window);
    WindowSetValue(window, 0);
    window->SetFontJust(1, 1);
}

// A cash column: cash text, right-aligned, the font, the value cleared.
void CashColumn(InterfaceWindow* window) {
    void* memory = NewText();
    CashCtor(memory);
    window->SetClientDrawObject(static_cast<InterfaceDrawObject*>(static_cast<InterfaceCashTextObject*>(memory)));
    window->SetFontJust(2, 1);
    SetFontOf(window);
    WindowSetValue(window, 0);
}

// A bar column: indented by 10, a horizontal gradient of the status bar's mesh, the value cleared.
void BarColumn(InterfaceWindow* window) {
    UIMain_IndentClient(window, 0, 10);
    void* memory = OperatorNew(0x124);
    GradientCtor(memory, "ridestatbar.wct");
    window->SetClientDrawObject(static_cast<InterfaceDrawObject*>(static_cast<InterfaceDrawEngineHGradient*>(memory)));
    WindowSetValue(window, 0);
}

}  // namespace

// The callees the host cannot call as the Mac does. The test library defines TPW_LCPOST_FAKES (its cmake file) and brings recording fakes.
#ifndef TPW_LCPOST_FAKES
namespace allitems_post_callee {
void* OperatorNew(uint32_t size) { return ::operator new(size); }
void SpriteCtor(void* mem) { new (mem) InterfaceTextObjectSprite(nullptr); }
void NumberCtor(void* mem) { new (mem) InterfaceNumberTextObject(nullptr); }
void CashCtor(void* mem) { new (mem) InterfaceCashTextObject(nullptr); }
void GradientCtor(void* mem, const char* mesh) { new (mem) InterfaceDrawEngineHGradient(const_cast<char*>(mesh)); }
void WindowSetValue(InterfaceWindow* window, int32_t value) { window->SetValue(value); }
}  // namespace allitems_post_callee
#endif

void AllItemsListLCPostFormat(InterfaceWindow* window) {
    void* memory = NewText();
    SpriteCtor(memory);
    window->SetClientDrawObject(static_cast<InterfaceDrawObject*>(static_cast<InterfaceTextObjectSprite*>(memory)));
    SetFontOf(window);
    window->SetClientDrawText(nullptr);
    InterfaceWindow* child = window->GetChild(0);
    switch (gAllItemsListType) {
    case 2:
        NumberColumn(child);
        child = child->GetNext();
        BarColumn(child);
        child = child->GetNext();
        CashColumn(child);
        child = child->GetNext();
        CashColumn(child);
        child = child->GetNext();
        BarColumn(child);
        break;
    case 0:
        NumberColumn(child);
        child = child->GetNext();
        BarColumn(child);
        child = child->GetNext();
        BarColumn(child);
        child = child->GetNext();
        BarColumn(child);
        break;
    case 1:
        NumberColumn(child);
        child = child->GetNext();
        CashColumn(child);
        child = child->GetNext();
        CashColumn(child);
        child = child->GetNext();
        BarColumn(child);
        break;
    case 3: {
        void* number = NewText();
        NumberCtor(number);
        child->SetClientDrawObject(static_cast<InterfaceDrawObject*>(static_cast<InterfaceNumberTextObject*>(number)));
        SetFontOf(child);
        WindowSetValue(child, 0);
        child->SetFontJust(1, 1);
        break;
    }
    default:
        break;
    }
}
