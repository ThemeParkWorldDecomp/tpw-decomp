# The login panel (unit w39): CreateLoginPanel, FillLoginNames, the window's font setters and the key tables' disabling, under the real script
# interpreter, control factory, constructors and WhatAmI, so that the panel's windows are built here the way the game builds them. The
# user information, the advisor, the draw objects, the font table and the string table are fakes in the unit file, which includes
# calllog.cpp itself (below) for its allocation observer.
set(TPW_UNIT_LoginPanel src/ui_loginpanel.cpp src/InterfaceWindow_font.cpp src/keytables_disable.cpp src/ui_loginpanel_data.cpp
    src/InterfaceWindow_script.cpp src/ui_controls_create.cpp src/InterfaceButton.cpp src/ui_controls_small.cpp src/ui_cool_shapes.cpp
    src/ui_whatami.cpp src/InterfaceWindow.cpp src/UI_Base.cpp)
# The unit file includes calllog.cpp itself.
set(TPW_UNIT_LoginPanel_OWN_CALLLOG ON)
