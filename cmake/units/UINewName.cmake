# Unit w43b: the name entry dialog of the login panel (NewName), the Dialog window maker, InterfaceWindow::setZ and GetClientRect, and the check
# buttons' SetCurrent. Everything they call is a fake that records in the unit file; the rectangles are real.
set(TPW_UNIT_UINewName src/ui_newname.cpp src/ui_newname_data.cpp src/ui_dialog.cpp src/InterfaceWindow_z.cpp
    src/InterfaceCheckButtons_current.cpp src/UI_Base.cpp)
