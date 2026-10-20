# The default window handlers of the button, the check buttons, the knob, the text box and the vary box, with the window's focus and
# capture leaves and the knob's and the vary box's members (unit w37a). The button, the controls' constructors, the window's
# constructor, GetWindow and the rectangles are real; the senders, the clock, the help text, the draw objects and the window's other
# members are fakes in the unit file.
set(TPW_UNIT_UIHandlers src/ui_handlers_small.cpp src/InterfaceWindow_focus.cpp src/InterfaceKnob.cpp src/InterfaceVaryBox.cpp
    src/InterfaceButton.cpp src/ui_controls_create.cpp src/InterfaceWindow.cpp src/InterfaceWindow_show.cpp src/UI_Base.cpp)
