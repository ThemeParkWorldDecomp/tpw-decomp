# Unit w43: the login handlers, removal, game login and dialog, with the real panel/toolkit path.
# Selected existing constructor and dispatcher bodies have their own source files so this library
# can link them without unrelated functions. The unit file includes calllog.cpp for heap observation.
set(TPW_UNIT_LoginMsg src/ui_loginpanel.cpp src/InterfaceWindow_font.cpp src/keytables_disable.cpp src/ui_loginpanel_data.cpp
    src/InterfaceWindow_script.cpp src/ui_controls_create.cpp src/InterfaceButton.cpp src/ui_controls_small.cpp src/ui_cool_shapes.cpp
    src/ui_whatami.cpp src/InterfaceWindow.cpp src/UI_Base.cpp src/UI_send.cpp src/UI_post.cpp src/InterfaceWindow_show.cpp src/ui_helptext.cpp
    src/ui_loginpanel_msg.cpp src/keytables_enable.cpp src/FS_login.cpp src/FS_UserInfo_ctor.cpp src/FS_MasterSave_global_ctor.cpp src/ui_fonttable.cpp src/ui_DrawObjectText_ctor.cpp src/ui_dialogbox.cpp src/ui_DrawObject_find.cpp src/lobby_islands_login.cpp src/ui_dialogbox_data.cpp
    src/FS_Online_gametype.cpp src/Game_pause.cpp src/TbString.cpp src/TbStringBuffer.cpp src/TbUniStringVTag.cpp src/InterfaceWindow_focus.cpp platform/msl.cpp)
# The unit file includes calllog.cpp itself.
set(TPW_UNIT_LoginMsg_OWN_CALLLOG ON)
