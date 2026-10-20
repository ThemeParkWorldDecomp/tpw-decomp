# Unit w47: the "Create new player" bubble's message function and OK, the quit and delete-player confirmations, the dialog box callback,
# and the small functions they use (GetCurrent, UIMain_EnableAllKeyTables, UI_UnPauseTimerSystem). Everything else they call is a recording
# fake in the unit file; the string class and wcslen are real.
set(TPW_UNIT_PlayerDialogs src/ui_newname_msg.cpp src/ui_dialogbox_callback.cpp src/InterfaceCheckButtons_current.cpp src/TbString.cpp
    src/TbStringBuffer.cpp src/TbUniStringVTag.cpp platform/msl.cpp)
