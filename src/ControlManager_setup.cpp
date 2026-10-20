// CControlManager::SetupInitialControls (main app, 0x100fa2ac, 68 bytes; unit w129).
#include <cstring>

#include "World.h"

// The 0x12cc bytes of the control table are cleared, then every control is checked to be set up (CheckAllControlsInitialised 0x100fa32c, untranslated).
void CControlManager::SetupInitialControls() {
    std::memset(static_cast<void*>(this), 0, 0x12cc);
    CheckAllControlsInitialised();
}
