// ui_cityLobbyController.cpp range of the main app (SimTheme Park 2.04b3), unit v5L06: InterfaceWindow::FinishWithClientObjectSurface
// (0x101dbd1c, 56 bytes) and InterfaceTimer::SetSlice (0x101d84c8, 8 bytes). Unit v5L16: InterfaceWindow::GetClientObjectSurface
// (0x101dbc98, 64 bytes).
#include "InterfaceWindow.h"
#include "UI_Base.h"
#include "ui_DrawObject.h"

// Refreshes the client draw object after its pixels were written (the virtual at vtable +0x20).
void InterfaceWindow::FinishWithClientObjectSurface() {
    if (mClientDrawObject != nullptr) mClientDrawObject->Refresh();
}

// The client draw object's pixels and size (the virtual at vtable +0x24), 0 without one. The draw objects answer the pixels as a
// 32-bit word (Mac addresses; the host's surfaces are arena memory), which is widened back to a pointer here.
uint8_t* InterfaceWindow::GetClientObjectSurface(int32_t* width, int32_t* height) {
    if (mClientDrawObject == nullptr) return nullptr;
    const uint32_t pixels = static_cast<uint32_t>(mClientDrawObject->GetWritableDataArea(width, height));
    return reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(pixels));
}

void InterfaceTimer::SetSlice(int32_t slice) { mSlice = slice; }
