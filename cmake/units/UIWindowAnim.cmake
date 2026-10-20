# The window's animation starters (setPivot, startAnim, startAnimTimer) and the check buttons' destructor (unit w91). SetPosition (which
# startAnim calls), the window constructor, the timer's Start and the rectangles are real; the message senders, the clock, the draw
# objects, the window's destructor and the check buttons' constructor are fakes in the unit file.
set(TPW_UNIT_UIWindowAnim src/InterfaceWindow_anim.cpp src/InterfaceCheckButtons_dtor.cpp src/InterfaceTimer_start.cpp
    src/InterfaceWindow_draw.cpp src/InterfaceWindow.cpp src/UI_Base.cpp)
