# The main panel (unit w118): CreateMainPanel, InitStaffPanel, the vertical gradient (constructor, destructor, CalcVerts, CalcUVs, Show), the number and
# cash text objects (constructors, destructors), SetOriginalPosition and the three small message functions, with the window scripts. The rectangles
# and the text sprite's constructor are real; the windows, the draw object base classes, the sprite parts, the text formatter, the mesh calls and the
# fonts are fakes in the unit file. Unit w131 adds platform/msl.cpp (swprintf, for GoldenStuffMsg) and the stand-ins its five handlers reach.
set(TPW_UNIT_MainPanel src/ui_mainpanel2_create.cpp src/ui_mainpanel2_msg.cpp src/ui_staffPanel_init.cpp src/ui_mainpanel_data.cpp
    src/ui_DrawObjectGradient.cpp src/ui_DrawObjectNumber.cpp src/ui_DrawObjectText_ctor.cpp src/InterfaceWindow_original.cpp src/UI_Base.cpp
    platform/arena.cpp platform/msl.cpp)
set(TPW_UNIT_MainPanel_OWN_CALLLOG ON)
