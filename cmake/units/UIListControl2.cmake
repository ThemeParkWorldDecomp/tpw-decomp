# Unit w93: InterfaceListControl2's item management (the item block, UpdateDisplay, the column formats, AddItem, ChangeItem, the
# current entry) and InterfaceWindow::SetTextOffset and GetDims. The window, the draw object, the message pump and the string
# library are fakes in the unit file; CoolRect is real.
set(TPW_UNIT_UIListControl2 src/InterfaceListControl2_items.cpp src/InterfaceWindow_dims.cpp src/UI_Base.cpp)
set(TPW_UNIT_UIListControl2_OWN_CALLLOG ON)
