// host_no_sound_device.cpp: starting the game on a computer whose sound device does not open. The device's failure reaches
// CAudioComponent::Init, which deletes the sound manager and answers 0 all the same (a quirk of the Mac's library), and
// TbAudioBody::Init went on to call the manager it no longer had: a crash in SOUND_Init, before the first window. The port's
// fix (nosound, src/sound/TbAudioBody.cpp) answers -1, so SOUND_Init leaves the sound system off and the game plays silently.
// Everything is the real code the launcher links: SOUND_Init and what GAMESYSTEM_Init calls after it, the library, CSprocketsModule
// and SoundDevice. Only SDL's driver is chosen, so that no sound hardware and no window are needed. The program is linked as
// tpw_launcher is, with its own stand-ins, so it needs no slicer and is made wherever the launcher is, Windows CI among them.
//   host_no_sound_device driver   SDL's audio does not start (a driver of no such name): SoundDevice::Open's first failure
//   host_no_sound_device open     SDL's audio starts but the device does not open (TPW_SOUND_NO_DEVICE): the second failure,
//                                 and then, the same run with the device opening, that the sound system does come on
// Run by ctest ("no_sound_driver", "no_sound_device") in a build with the port's fixes; a build without them makes the program,
// which crashes. Exits nonzero on a failure.
#include <SDL3/SDL.h>

#include <cstdio>
#include <cstring>

#include "TPMain.h"
#include "TPSound.h"
#include "platform/arena.h"

static int gStandIns = 0;

// What the stand-ins report, launcher/main.cpp's in the game. Any of them reached on this path is a body that is not there.
void tpw_stub_called(const char* name, bool platform, bool first) {
    (void)platform;
    if (first) std::printf("a stand-in was called: %s\n", name);
    gStandIns++;
}

namespace {

int gChecks = 0;
int gFailures = 0;

void Check(bool ok, const char* what) {
    gChecks++;
    if (ok) return;
    gFailures++;
    std::printf("FAILED: %s\n", what);
}

// SDL reads its driver from its own copy of the environment (the C library's is not looked at again once it has one).
void SetEnv(const char* name, const char* value) {
    SDL_SetEnvironmentVariable(SDL_GetEnvironment(), name, value, true);
}

void UnsetEnv(const char* name) {
    SDL_UnsetEnvironmentVariable(SDL_GetEnvironment(), name);
}

constexpr int32_t kQuality = 50;

void Start() {
    SOUND_Init(const_cast<char*>(""), kGameOptionSound400, kQuality);
}

// The sound calls a play starts with and goes on making, with the system off: none may reach the object that is not there.
void PlaySilently() {
    SOUND_SetSoundQuality(kQuality);
    SOUND_SetGlobalVolume(100);
    SOUND_SetMusicVolume(80);
    SOUND_SetSpeechVolume(80);
    SOUND_SetSfxVolume(80);
    Check(SOUND_RegisterCatagory("nowhere", "nothing") == 0, "with no system a sound category answers 0");
    const int32_t handle = SOUND_SendEvent(0, gSoundBanks[3], 0xd2, 0, 0, 0);
    SOUND_SetEarPosition(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f);
    SOUND_Tick();
    SOUND_StopSoundEffect(handle);
    SOUND_Pause();
    SOUND_UnPause();
    SOUND_StopAllSfx();
    SND_StartParkMusic();
    SND_StopParkMusic();
    SOUND_Tick();
}

void TestNoSound(const char* what) {
    std::printf("no_sound_device: %s\n", what);
    Start();
    Check(gSoundSystemOn == 0, "the sound system stays off");
    Check(gAudioSystem == nullptr, "there is no audio object");
    PlaySilently();
    SOUND_Close();
    Check(gSoundSystemOn == 0, "the sound system is still off after SOUND_Close");
}

}  // namespace

int main(int argc, char** argv) {
    const char* which = argc > 1 ? argv[1] : "";
    std::setvbuf(stdout, nullptr, _IONBF, 0);   // what was reached is printed even when the next step crashes
    tpw::ArenaBase();
    if (std::strcmp(which, "driver") == 0) {
        SetEnv("SDL_AUDIO_DRIVER", "no-such-driver");
        TestNoSound("SDL's audio does not start");
    } else if (std::strcmp(which, "open") == 0) {
        SetEnv("SDL_AUDIO_DRIVER", "dummy");
        SetEnv("TPW_SOUND_NO_DEVICE", "1");
        TestNoSound("the device does not open");
        TestNoSound("the device does not open, a second time");
        // The same program with the device opening: the failures above are the device's, and not this test's own doing.
        UnsetEnv("TPW_SOUND_NO_DEVICE");
        std::printf("no_sound_device: the device opens\n");
        Start();
        Check(gSoundSystemOn == 1, "with a device the sound system comes on");
        Check(gAudioSystem != nullptr, "with a device there is an audio object");
    } else {
        std::printf("usage: host_no_sound_device driver|open\n");
        return 2;
    }
    Check(gStandIns == 0, "no stand-in was reached");
    std::printf("no_sound_device: %d checks, %d failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
