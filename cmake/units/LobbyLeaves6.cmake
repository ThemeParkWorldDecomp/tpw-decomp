# The lobby's small leaves part 6 (unit w75): CGame::ExitLevel, CInGame::Start, CursorLibrary::RemoveAll and FuckOffUIProgressBar, the image
# control's destructor, DestroyEngineMesh and DestroyMeshInstance, StripRideWrapping. Everything they call is a fake in the unit file.
set(TPW_UNIT_LobbyLeaves6 src/Game_exit.cpp src/InGame_start.cpp src/ui_progress_cursor.cpp src/ui_image_dtor.cpp src/MeshDestroy.cpp
    src/RideWrapping.cpp src/UI_Base.cpp platform/arena.cpp)
