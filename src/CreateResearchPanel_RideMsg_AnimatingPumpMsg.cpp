// AnimatingPumpMsg 0x101b49d0: the window handler of an animated pump. On message 0x1e it steps a 16.16 frame counter
// (extra param 0) by twice the speed (extra param 2) in the mode of extra param 3, wraps it per mode, shows the frame on
// the child window 0xd2d06 and stores the counter and the tick (extra param 1). Then the default handler runs.
#include <cstdint>

#include "InterfaceWindow.h"
#include "UI_Base.h"

int32_t AnimatingPumpMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);

static int32_t Word(void* p) { return static_cast<int32_t>(reinterpret_cast<intptr_t>(p)); }
static void* Ptr(int32_t v) { return reinterpret_cast<void*>(static_cast<intptr_t>(v)); }

int32_t AnimatingPumpMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x1e) {
        int32_t counter = Word(window->getExtraParam(0));
        window->getExtraParam(1);
        int32_t speed = Word(window->getExtraParam(2));
        int32_t mode = Word(window->getExtraParam(3));
        if (mode != 0) {
            int32_t frame = 0;  // the original leaves it unset for a mode above 3
            if (mode == 1) {
                counter = static_cast<int32_t>(static_cast<uint32_t>(counter) + (static_cast<uint32_t>(speed) << 1));
                frame = counter >> 16;
                if (frame > 0x1f) {
                    frame = 0;
                    counter = 0;
                }
            } else if (mode == 2) {
                counter = static_cast<int32_t>(static_cast<uint32_t>(counter) + (static_cast<uint32_t>(speed) << 1));
                frame = counter >> 16;
                if (frame > 0x2f) {
                    window->setExtraParam(3, Ptr(0));
                    frame = 0x30;
                    counter = 0x300000;
                }
            } else if (mode == 3) {
                counter = static_cast<int32_t>(static_cast<uint32_t>(counter) + (static_cast<uint32_t>(speed) << 1));
                frame = counter >> 16;
                if (frame > 0x3f) {
                    window->setExtraParam(3, Ptr(1));
                    frame = 0;
                    counter = 0;
                }
            }
            window->GetWindow(0xd2d06)->SetFrame(frame);
            window->setExtraParam(0, Ptr(counter));
            window->setExtraParam(1, Ptr(static_cast<int32_t>(IveGotTicks())));
        }
    }
    return window->CallDefaultWindowHandler(message, a, b);
}
