# The island lobby's controller, the global master save and the ride names (unit w32). The strings, the 16-bit C library and the
# rectangles are real; the window toolkit, the balance table and the string table are fakes in the unit file.
set(TPW_UNIT_IslandLobby src/RideDB_balance.cpp src/UI_IslandLobby.cpp src/FS_MasterSave_global.cpp src/FS_MasterSave_global_ctor.cpp src/RideDB_names.cpp src/InterfaceWindow_frame.cpp
    src/TbString.cpp src/TbStringBuffer.cpp src/UI_Base.cpp platform/msl.cpp)
