// Unit v5975: InterfaceTextBox::CalcTextPos 0x101d7dac, InterfaceTextBox::SetText 0x101d7f50, InterfaceStringHeap::GetString 0x101d79ec,
// InterfaceStringHeap::GetAndCopy 0x101d7b60, InterfaceTimer::OperatorDelete 0x101d822c (the class's operator delete).
#include "ui_controls.h"

#include <cstddef>
#include <new>

#include "InterfaceFont.h"
#include "UI_Base.h"
#include "platform/msl.h"

// The heap's tables (src/lobby_start_leaves.cpp; main app globals 0x10395c34..0x10395c48).
extern uint8_t* gStringHeapTableB;   // 0x10395c34
extern uint8_t* gStringHeapTableA;   // 0x10395c38
extern int32_t gStringHeapRowsB;     // 0x10395c3c
extern int32_t gStringHeapWidthB;    // 0x10395c40
extern int32_t gStringHeapRowsA;     // 0x10395c44
extern int32_t gStringHeapWidthA;    // 0x10395c48

__attribute__((weak)) void* textbox_callee::NewArray(uint32_t size) { return ::operator new[](static_cast<std::size_t>(size)); }
__attribute__((weak)) void textbox_callee::Delete(void* block) { ::operator delete(block); }

void InterfaceTextBox::CalcTextPos() {
    if (mFont == nullptr || mText == nullptr) return;
    if (mHorizontal == 1) {
        int32_t length = static_cast<int32_t>(wcslen(mText));
        int32_t width = mFont->GetWidth(mText, 0, static_cast<int16_t>(length - 1));
        mTextX = static_cast<int16_t>(mClientRect.mRight - width);
    } else if (mHorizontal == 2) {
        int32_t left = mClientRect.mLeft;
        int32_t length = static_cast<int32_t>(wcslen(mText));
        int32_t width = static_cast<int16_t>(mFont->GetWidth(mText, 0, static_cast<int16_t>(length - 1)));
        mTextX = static_cast<int16_t>(left + ((mClientRect.mRight - left - width) >> 1));
    } else {
        mTextX = mClientRect.mLeft;
    }
    if (mVertical == 1) {
        int32_t height = mFont->GetHeight();
        mTextY = static_cast<int16_t>(mClientRect.mBottom - height);
    } else if (mVertical == 2) {
        int32_t top = mClientRect.mTop;
        int32_t height = static_cast<int16_t>(mFont->GetHeight());
        mTextY = static_cast<int16_t>(top + ((mClientRect.mBottom - top - height) >> 1));
    } else {
        mTextY = mClientRect.mTop;
    }
}

int32_t InterfaceTextBox::SetText(char16_t* text) {
    int32_t length = static_cast<int32_t>(wcslen(text));
    if (length > mTextLength) {
        if (mText != nullptr) textbox_callee::Delete(mText);
        mText = static_cast<char16_t*>(textbox_callee::NewArray(static_cast<uint32_t>(length + 1) << 1));
        mTextLength = length;
    }
    if (mText == nullptr) return 0;
    wcscpy(mText, text);
    CalcTextPos();
    return 1;
}

char16_t* InterfaceStringHeap::GetString(int32_t length) {
    char16_t* found = nullptr;
    uint8_t* rowA = gStringHeapTableA;
    if (rowA == nullptr) return nullptr;
    uint8_t* rowB = gStringHeapTableB;
    if (rowB == nullptr) return nullptr;
    if (length < gStringHeapWidthB) {
        for (int32_t i = 0; i < gStringHeapRowsB && found == nullptr; ++i) {
            if (*reinterpret_cast<int32_t*>(rowB) == -1) found = reinterpret_cast<char16_t*>(rowB);
            rowB += static_cast<int32_t>(static_cast<uint32_t>(gStringHeapWidthB) << 1);
        }
    }
    if (length < gStringHeapWidthA && found == nullptr) {
        for (int32_t i = 0; i < gStringHeapRowsA; ++i) {   // no early exit: the last free row wins
            if (*reinterpret_cast<int32_t*>(rowA) == -1) found = reinterpret_cast<char16_t*>(rowA);
            rowA += static_cast<int32_t>(static_cast<uint32_t>(gStringHeapWidthA) << 1);
        }
    }
    if (found != nullptr) *found = 0;
    return found;
}

char16_t* InterfaceStringHeap::GetAndCopy(const char16_t* text) {
    int32_t length = static_cast<int32_t>(wcslen(text));
    char16_t* string = GetString(length);
    if (string != nullptr) wcscpy(string, text);
    return string;
}

void InterfaceTimer::OperatorDelete(void* object) {
    InterfaceTimer* slot = static_cast<InterfaceTimer*>(object);
    bool inside = false;
    if (slot != nullptr && gUITimers != nullptr && slot >= gUITimers && slot < gUITimers + gUITimerCount) inside = true;
    if (!inside)
        msl_assertion_failed("p_mem && pUITimerFreeList && (p_mem >= pUITimerFreeList) && (p_mem < pUITimerFreeList+UITimerFreeListSize)",
                             "ui_timers.cpp", 0x48);
    slot->mFree = 1;
}
