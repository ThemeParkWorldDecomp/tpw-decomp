// SOUND_SetRect (0x100de828, 148 bytes): hands the audio object a SetBoundingRectangle command for a sound handle.
#include <cstdint>

#include "HLSprite.h"
#include "TPSound.h"
#include "TbAudioSystem.h"

// Does nothing while the wanted word or the system-on word is 0. The command holds the handle, left a, top b, right a + c
// and bottom b + d (the sums wrap like the PowerPC adds). The TbError answer is dropped.
void SOUND_SetRect(int32_t handle, int32_t a, int32_t b, int32_t c, int32_t d) {   // 0x100de828
    if (gSoundWanted != 0 && gSoundSystemOn != 0) {
        TbSysCommand::SetBoundingRectangle command(handle, a, b, a + c, b + d);
        gAudioSystem->SystemCommand(command);
    }
}
