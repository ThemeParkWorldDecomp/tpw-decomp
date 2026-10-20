# The lobby's small leaves part 4 (unit w62): the online globe's constructor and the download and publish globes' Init, the online system's
# Tick and StopProcessing, the outbox and address book constructors with the thread mutex, CGame::DeInit and CInGame::Init. The strings are
# the real ones (src/Game.cpp needs them); the rest of Game.cpp's callees are quiet fakes in the unit file.
set(TPW_UNIT_LobbyLeaves4 src/lobby_globe.cpp src/FS_Online.cpp src/email.cpp src/Game.cpp src/InGame.cpp src/TbString.cpp
    src/TbStringBuffer.cpp platform/msl.cpp)
