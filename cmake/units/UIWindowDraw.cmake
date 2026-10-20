# The window drawing and the default window handler (unit w30). The window's constructor is real (it builds the test trees); the
# message senders, the clock and the draw objects are fakes in the unit file.
set(TPW_UNIT_UIWindowDraw src/InterfaceWindow_draw.cpp src/InterfaceWindow.cpp src/UI_Base.cpp)
