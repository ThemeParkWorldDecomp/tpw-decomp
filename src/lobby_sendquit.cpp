// lobby.cpp (main app), unit w48: CLBLobby::SendQuit 0x100a643c (32 bytes). It ignores `this` and reads the globals: the game is
// told to restart and the lobby that runs is told its state is 3, which ends the lobby loop.
#include "Game.h"
#include "lobby.h"

void CLBLobby::SendQuit() {
    gGame.mRestart = 1;
    gLobby->mState = 3;
}
