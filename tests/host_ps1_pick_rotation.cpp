// host_ps1_pick_rotation.cpp: the ps1-rides mod's starting turn of its rides picked from Buy Ride (TPW_MOD_HOOK
// ps1_pick_rotation, include/Ps1PickRotation.h). The rotation a pick starts with, for a PC ride and one of the mod's,
// with no ride held and with one held at each rotation; then the player's own steps as PlaceRide and the "turn" keys
// make them: a PlayStation ride picked, turned by the player, picked again (no second half turn), a PC ride picked
// while it is held (the player's turn kept, the hook's taken away), a ride placed and the next one picked.
// Run by ctest ("ps1_pick_rotation") in a build with the port's fixes, which alone has the hook. Exits nonzero on a failure.
#include <cstdio>
#include <string>

#include "Ps1PickRotation.h"

namespace {

int gFailures = 0;

void Check(bool ok, const std::string& what) {
    std::printf("%s: %s\n", ok ? "ok" : "FAILED", what.c_str());
    if (!ok) ++gFailures;
}

// The cell system's rotation and mode and the hook's last turn, as the game keeps them.
struct Game {
    int32_t rotation = 0;
    bool held = false;
    int32_t lastTurn = 0;

    void Pick(bool ps1) {   // PlaceRide: the original keeps the rotation; the hook adds its turn
        int32_t turn = 0;
        rotation = Ps1PickRotation(rotation, held, lastTurn, ps1, turn);
        lastTurn = turn;
        held = true;
    }
    void TurnClockwise() { rotation = (rotation + 90) % 360; }   // keyboard.cpp's turn_object_clockwise
    void Place() {   // LLCellMouse.cpp: a placed ride (no shift) puts the rotation back to 0 and leaves mode 4
        rotation = 0;
        held = false;
    }
};

}  // namespace

int main() {
    int32_t turn = -1;
    Check(kPs1PickTurn == 180, "the turn is half a turn");
    Check(Ps1PickRotation(0, false, 0, false, turn) == 0 && turn == 0, "a PC ride, nothing held: 0");
    Check(Ps1PickRotation(0, false, 0, true, turn) == 180 && turn == 180, "a PlayStation ride, nothing held: 180");
    for (int32_t rotation = 0; rotation < 360; rotation += 90) {
        Check(Ps1PickRotation(rotation, true, 0, false, turn) == rotation && turn == 0,
              "a PC ride over a held PC ride at " + std::to_string(rotation) + ": kept");
        Check(Ps1PickRotation(rotation, true, 0, true, turn) == (rotation + 180) % 360 && turn == 180,
              "a PlayStation ride over a held PC ride at " + std::to_string(rotation) + ": half a turn more");
        Check(Ps1PickRotation(rotation, true, 180, true, turn) == rotation && turn == 180,
              "a PlayStation ride over a held PlayStation ride at " + std::to_string(rotation) + ": kept");
        Check(Ps1PickRotation(rotation, true, 180, false, turn) == (rotation + 180) % 360 && turn == 0,
              "a PC ride over a held PlayStation ride at " + std::to_string(rotation) + ": the hook's turn taken away");
        Check(Ps1PickRotation(rotation, false, 180, false, turn) == rotation && turn == 0,
              "a PC ride after a PlayStation ride was put down, at " + std::to_string(rotation) + ": kept");
    }

    Game game;
    game.Pick(true);
    Check(game.rotation == 180, "steps: Inca Pot picked: 180");
    game.TurnClockwise();
    Check(game.rotation == 270, "steps: turned once: 270");
    game.Pick(true);
    Check(game.rotation == 270, "steps: Diplo-Dip picked while it is held: 270 (no second half turn)");
    game.Pick(false);
    Check(game.rotation == 90, "steps: Belly Bounce picked while it is held: 90 (the player's turn)");
    game.Pick(false);
    Check(game.rotation == 90, "steps: Belly Bounce picked again: 90");
    game.Pick(true);
    Check(game.rotation == 270, "steps: Aztec Bounce picked while it is held: 270");
    game.Place();
    game.Pick(true);
    Check(game.rotation == 180, "steps: placed, then Inca Pot picked: 180");
    game.Place();
    game.Pick(false);
    Check(game.rotation == 0, "steps: placed, then Belly Bounce picked: 0");

    std::printf("%s\n", gFailures == 0 ? "ps1_pick_rotation: all ok" : "ps1_pick_rotation: FAILED");
    return gFailures == 0 ? 0 : 1;
}
