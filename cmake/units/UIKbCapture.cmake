# The keyboard decision (unit w49): UIMain_DecideKbCapture, MainMenuUp, UIMenu::IsShown and the coaster panel's two small functions,
# with the real key table Enable and the window's CaptureKb (the capture global and the two posted messages). The message poster, the
# window script's callees that CaptureKb's file needs to link, and GetAttr (one line of the message pump's file) are in the unit file.
set(TPW_UNIT_UIKbCapture src/ui_main_kbcapture.cpp src/ui_coasteredit_kb.cpp src/keytables_enable.cpp src/InterfaceWindow_script.cpp
    src/UI_Base.cpp src/ui_cool_shapes.cpp)
