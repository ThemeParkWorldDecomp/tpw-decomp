# Unit w37c: the edit box, its handler, timer start and client mouse routing.
# Rectangles, window text and the default handler are real. UI boundaries record in the test library.
set(TPW_UNIT_UIEditBox src/InterfaceEditBox.cpp src/ui_editbox_handler.cpp src/InterfaceTimer_start.cpp
    src/InterfaceWindow_mouse.cpp src/InterfaceWindow_font.cpp src/InterfaceWindow_draw.cpp src/UI_Base.cpp platform/msl.cpp)
set(TPW_UNIT_UIEditBox_OWN_CALLLOG ON)
