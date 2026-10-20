# Unit w131: the park's UI panel message functions (MainPanelButtonsMsg, CashPanelMsg, StaffPanelMsg, HappyBarMsg, ControlPanelMsg, StaffLocMsg,
# CashWindowCallback, BobbleCallback, ModifiedCheckButtonsCallback, GoldenStuffMsg), UIInterface::CheckMouseContext and CIntrface::Process. The
# rectangles (src/UI_Base.cpp) and the 16-bit C library (swprintf) are real; the windows, the master save, the world, the analyser and every panel the handlers open are fakes in the
# unit file.
set(TPW_UNIT_UIPanels src/ui_mainpanel2_msg.cpp src/ui_entryPricePanel_msg.cpp src/ui_staffPanel_msg.cpp src/UIInterface_context.cpp
    src/intrface_process.cpp src/UI_Base.cpp platform/msl.cpp)
set(TPW_UNIT_UIPanels_OWN_CALLLOG ON)
