# The sprite draw object, the scroll bar and the window's show and position leaves (unit w30b). The window's constructor and the rectangles
# are real (they build the test trees); the engine's sprite surfaces, the map, the window's other members and the senders are fakes.
set(TPW_UNIT_UIScrollBar src/ui_DrawObjectSprite.cpp src/InterfaceScrollBar.cpp src/InterfaceWindow_show.cpp src/InterfaceWindow.cpp
    src/UI_Base.cpp)
