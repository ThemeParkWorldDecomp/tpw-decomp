# Park interface set-up and teardown (w116), with real rectangles and all external callees faked.
set(TPW_UNIT_GameUIInit src/Mouse.cpp src/ui_main_game.cpp src/ui_StatusPanel.cpp src/ui_entryPricePanel.cpp
    src/ui_staffPanel.cpp src/ui_allKidsPanel.cpp src/InterfaceWindow_cursor.cpp src/InterfaceWindow_unlink.cpp
    src/ui_game_panels_data.cpp src/UI_Base.cpp)
set(TPW_UNIT_GameUIInit_OWN_CALLLOG ON)
