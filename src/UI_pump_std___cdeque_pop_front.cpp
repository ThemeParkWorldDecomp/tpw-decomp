// Unit v51076: small forwarders of UIInterface and the CodeWarrior deque pieces next to them.
#include "InterfaceWindow.h"
#include "TbITable.h"
#include "TPSettings.h"
#include "UIInterface.h"
#include "UI_Base.h"
#include "UI_pump.h"
#include "platform/msl.h"
#include "std_cdeque.h"
#include "ui_Postcard.h"
#include "ui_main.h"

// 0x102b1428, original name unknown: the first word of a new error deque (0x40).
uint32_t gMslErrorDequeMapSize = 0x40;

// ---- std::__cdeque<const void*>
// 0x101f25e8: one element less, the start moves on and wraps at the capacity.
void std::__cdeque<const void*, std::allocator<const void*>>::pop_front() {
    mWord4 = mWord4 - 1;
    mWord8 = mWord8 + 1;
    if (mWord8 >= mWord0) mWord8 = mWord8 - mWord0;
}

// 0x101f492c: the smaller of two constants the Mac stores on its stack, then reads back: 0x3fffffff.
uint32_t std::__cdeque<const void*, std::allocator<const void*>>::max_size() const { return 0x3fffffffu; }

namespace cdeque_callee {
// 0x101f2dd8 iterator::iterator(const iterator&): the three words; the Mac's r3 stays the destination.
MslCdequeIter* IterCopy(MslCdequeIter* dest, const MslCdequeIter* src) {
    dest->mDeque = src->mDeque;
    dest->mIndex = src->mIndex;
    dest->mPtr = src->mPtr;
    return dest;
}
}  // namespace cdeque_callee

// ---- std::__cdeque<errorthingy*>
void ErrorCdequeResize(void* self, uint32_t n, errorthingy* value) {
    static_cast<std::__cdeque<const void*, std::allocator<const void*>>*>(self)->resize(n, value);
}

void ErrorCdequeReserve(void* self, uint32_t n) {
    static_cast<std::__cdeque<const void*, std::allocator<const void*>>*>(self)->reserve(n);
}

// ---- std::deque<errorthingy>::deque(const allocator&) 0x101f5158: seven words (0x1c bytes), the first from the global.
namespace errsinit_callee {
void DequeCtor(void* self, void* allocator) {
    (void)allocator;
    uint32_t* w = static_cast<uint32_t*>(self);
    w[0] = gMslErrorDequeMapSize;
    w[1] = 0;
    w[2] = 0;
    w[3] = 0;
    w[4] = 0;
    w[5] = 0;
    w[6] = 0;
}
}  // namespace errsinit_callee

// ---- UIInterface
void UIInterface::ShowVersionTag() { ShowVersionNumberTag(); }
void UIInterface::Congratulations(CObject* feature) { UI_Congratulations(feature); }
void UIInterface::Bankrupt(CObject* gates) { UI_Bankrupt(gates); }
void UIInterface::ChangeCoasterButtonsEnabled(uint32_t mask) { CoasterButtonsEnabled(mask); }
void UIInterface::ChangeCoasterButtonsDown(uint32_t mask) { CoasterButtonsDown(mask); }
void UIInterface::EndCoasterEdit() { EndCoasterEditMode(); }
void UIInterface::OnlineUpdateBuddy(char16_t* name, int32_t data) { _OnlineUpdateBuddy(name, data); }
void UIInterface::OnlineRemoveBuddy(char16_t* name) { _OnlineRemoveBuddy(name); }
void UIInterface::OnlineAddBuddy(char16_t* name, int32_t data) { _OnlineAddBuddy(name, data); }
void UIInterface::OnlineUpdateContact(char16_t* name, int32_t data) { _OnlineUpdateContact(name, data); }
void UIInterface::OnlineRemoveContact(char16_t* name) { _OnlineRemoveContact(name); }
void UIInterface::OnlineAddContact(char16_t* name, int32_t data) { _OnlineAddContact(name, data); }
void UIInterface::OnlineReceiveStr(char16_t* text, int32_t data) { OnlineReceiveString(text, data); }

void UIInterface::ToggleHelpText() {
    UI_ToggleHelpText();
    gPlayerSettings.mCurrent.mTooltipsOn = UI_HelpTextEnabled();   // stb at +0x36
}

void UIInterface::Screen_Research() { CreateResearchPanel(); }
void UIInterface::Screen_EntryPrice() { CreateEntryPricePanel(); }
void UIInterface::SwitchGameWindow() { ToggleGameWindow(); }
void UIInterface::ShowFirstPersonWindow(int32_t on) { UI_SwitchGameWindow(on == 0); }   // cntlzw, srwi 5
void UIInterface::TogglePause() { TogglePauseUI(); }
void UIInterface::DeInitOnline() { DeInitOnlineUI(); }
