// List control drawing and teardown (main app, unit w37b).
#include "ui_controls.h"
#include "ui_DrawObject.h"

// 0x1018a934 (104 bytes). The count and array are read again for each item.
void InterfaceListControl2::ClientDraw() {
    if (mItemWindows != nullptr) {
        for (int32_t i = 0; i < mVisibleRows; ++i) mItemWindows[i]->DrawMe();
    }
}

// 0x101d154c (68 bytes).
void InterfaceListControl2::CalcClientArea() {
    mClientRect.mLeft = mRect.mLeft;
    mClientRect.mTop = mRect.mTop;
    mClientRect.mRight = mRect.mRight;
    mClientRect.mBottom = mRect.mBottom;
    mListRect.mLeft = mClientRect.mLeft;
    mListRect.mTop = mClientRect.mTop;
    mListRect.mRight = mClientRect.mRight;
    mListRect.mBottom = mClientRect.mBottom;
}

// 0x101d1768 (380 bytes). The shared draw object is released after the item arrays.
InterfaceListControl2::~InterfaceListControl2() {
    RemoveAllItems();
    delete[] mColumns;
    if (mItemWindows != nullptr) {
        for (int32_t i = 0; i < mVisibleRows; ++i) delete mItemWindows[i];
        delete[] mItemWindows;
    }
    delete[] mItemStorage;
    if (mItemDrawObject != nullptr) {
        InterfaceDrawObject* object = mItemDrawObject;
        object->mResizable = static_cast<int16_t>(object->mResizable - 1);
        object->Show(0);
        if (static_cast<int16_t>(object->mResizable - (1 - object->mInLibrary)) == -1) {
            delete mItemDrawObject;
        }
    }
}
