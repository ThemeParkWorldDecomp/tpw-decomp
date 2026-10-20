#include "FS_Online.h"
#include "InterfaceWindow.h"
#include "Light.h"
#include "MapWho.h"
#include "TPCamera.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "TPSound.h"
#include "UIInterface.h"
#include "UI_Base.h"
#include "lobby.h"
#include "ppc_math.h"
#include "rsse.h"
#include "ui_main.h"

#include <cstring>

// 0x10032584 (100 bytes): while the first-person ride bit is up, leaves the ride (the remembered instance) and, when that returned
// the player to walking, moves the camera over the ride's exit.
void StopRidingRideIn1stPerson() {
    if ((gFirstPersonFlags & kFirstPersonRideBit) != 0) {
        int32_t out;   // uninitialised in the original; ToggleRideRideIn1stPerson sets it
        ToggleRideRideIn1stPerson(static_cast<int32_t>(gFirstPersonRideInstance), 0, 0, &out, 1, 0);
        if (out != 0) MoveCameraOverRideExit();
    }
}

// 0x1003277c (140 bytes): the entrance object at a place; the place in tenths is clamped into the height field's grid.
CObject* GetThingAtPosnIfEntrance(float x, float z) {
    int32_t gx = ppc_fctiwz(static_cast<double>(x / 10.0f));
    int32_t gz = ppc_fctiwz(static_cast<double>(z / 10.0f));
    if (gx < 0) gx = 0;
    if (gz < 0) gz = 0;
    if (gx >= gHeightfieldWidth) gx = gHeightfieldWidth - 1;
    if (gz >= gHeightfieldHeight) gz = gHeightfieldHeight - 1;
    return GetThingAtPosnIfEntrance(gx, gz);
}

// 0x10032a44 (248 bytes): 1 when the object (words: +0x20 its script, +0x24 its camera mesh, either may be 0) or the ride animSet
// has a first-person camera: a bump camera mesh, a flying tour's camera mesh, a coaster's first-person car, or a data node 0x1000.
int32_t IsRideRideable(void* object, uint32_t animSet) {
    if (object != nullptr) {
        const uint8_t* bytes = static_cast<const uint8_t*>(object);
        int32_t mesh, script;
        std::memcpy(&script, bytes + 0x20, 4);
        std::memcpy(&mesh, bytes + 0x24, 4);
        if (mesh != 0 && BMP_GetCameraMesh(mesh) != 0) return 1;
        int32_t tour = RSSE_GetTourBaseID(RSSE_GetScriptPointer(script));
        if (tour != 0 && FLY_GetCameraMesh(tour) != 0) return 1;
    }
    if (IsCoaster(animSet) != 0 && GetCoaster1stPersonCar(animSet) != 0) return 1;
    return FindDataNodeIndexByID(gRideAnimSets[animSet], 0x1000, 1) != 0xffffffffu ? 1 : 0;
}
