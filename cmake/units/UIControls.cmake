# Unit w31b: the control factory, the controls' constructors and the button. The window's constructor is real (it builds the test
# trees); the message poster, the timers, the scroll bar, the untranslated virtual members and the draw objects are fakes in the
# unit file, which includes calllog.cpp itself (below) for its allocation observer.
set(TPW_UNIT_UIControls src/ui_controls_create.cpp src/InterfaceButton.cpp src/InterfaceWindow.cpp src/UI_Base.cpp)
# The unit file includes calllog.cpp itself.
set(TPW_UNIT_UIControls_OWN_CALLLOG ON)
