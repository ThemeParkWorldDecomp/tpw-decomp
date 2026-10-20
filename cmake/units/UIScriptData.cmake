# Unit w36: the island lobby controller's message function, the window classes' WhatAmI and CalcClientArea, the help text handler, the
# timer constructor and the login panel's script, under the real script interpreter, control factory and constructors (so that the
# lobby's three scripts are built here the way the game builds them). The unit file includes calllog.cpp itself (below) for its
# allocation observer.
set(TPW_UNIT_UIScriptData src/UI_IslandLobby_msg.cpp src/ui_whatami.cpp src/ui_helptext.cpp src/ui_loginpanel_data.cpp
    src/InterfaceWindow_script.cpp src/ui_controls_create.cpp src/InterfaceButton.cpp src/ui_controls_small.cpp src/ui_cool_shapes.cpp
    src/InterfaceWindow.cpp src/UI_Base.cpp)
# The unit file includes calllog.cpp itself.
set(TPW_UNIT_UIScriptData_OWN_CALLLOG ON)
