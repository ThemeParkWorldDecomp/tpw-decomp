# The tooltip window (unit w56): CUIHelpWindow, its static object, InitHelpText, HelpTextMsg, SetHelpTextString and MoveHelpText
# (src/ui_tooltip.cpp), with the window's GetOriginalPosition, GetPosition, SetPosition and ShiftPosition (src/InterfaceWindow_draw.cpp), the
# help text handler it derives from (src/ui_helptext.cpp), the window's constructor and the rectangles, all real. Every other window member,
# the font table, the draw objects, the string table and the message senders are fakes in the unit file.
set(TPW_UNIT_UITooltip src/ui_tooltip.cpp src/ui_helptext.cpp src/InterfaceWindow_draw.cpp src/InterfaceWindow.cpp src/UI_Base.cpp)
