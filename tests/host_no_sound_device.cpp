// host_no_sound_device.cpp: starting the game on a computer whose sound device does not open. The device's failure reaches
// CAudioComponent::Init, which deletes the sound manager and answers 0 all the same (a quirk of the Mac's library), and
// TbAudioBody::Init went on to call the manager it no longer had: a crash in SOUND_Init, before the first window. The port's
// fix (nosound, src/sound/TbAudioBody.cpp) answers -1, so SOUND_Init leaves the sound system off and the game plays silently.
// Since SoundDevice waits for a device by itself only --mute still reaches that path (the dummy driver failing), so those two modes
// run muted. The other modes are the waiting and the following of the device (platform/sound_device.h).
// Everything is the real code the launcher links: SOUND_Init and what GAMESYSTEM_Init calls after it, the library, CSprocketsModule
// and SoundDevice. Only SDL's driver is chosen, so that no sound hardware and no window are needed. The program is linked as
// tpw_launcher is, with its own stand-ins, so it needs no slicer and is made wherever the launcher is, Windows CI among them.
//   host_no_sound_device driver   --mute, SDL's audio does not start (a driver of no such name): SoundDevice::Open's first failure
//   host_no_sound_device open     --mute, SDL's audio starts but the device does not open (TPW_SOUND_NO_DEVICE): the second failure,
//                                 and then, the same run with the device opening, that the sound system does come on
//   host_no_sound_device silent   no device at launch: the game's whole sound system on the watcher, and a clean quit
//   host_no_sound_device arrive   no device at launch: samples go on in real time, and the device that comes is taken by itself
//   host_no_sound_device late     SDL's audio does not start at launch and starts later, on the main thread
//   host_no_sound_device lost     SDL following the default: moved, the last one gone, and one back (SDL's own hidden calls)
//   host_no_sound_device stale    SDL stays on an old device, as when the new one refused to open: it is opened afresh
//   host_no_sound_device mute     --mute never waits and never tries again
//   host_no_sound_device stress   the game's commands crossing the watcher's moves: nothing hangs, and each stream ends where it was told
// Run by ctest ("no_sound_*"); "no_sound_driver" and "no_sound_device" in a build with the port's fixes only, as a build without
// them crashes there. Exits nonzero on a failure. The line for "stale": SDL's refused open cannot be made on its dummy driver
// (its driver table is private), so TPW_SOUND_DEFAULT_NAME stands in for what the system's default then is.
#include <SDL3/SDL.h>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <functional>
#include <sstream>
#include <string>

#include "TPMain.h"
#include "TPSound.h"
#include "platform/arena.h"
#include "platform.h"
#include "sound_device.h"

// SDL's hidden calls, in the static library this links: what a driver does when the system's devices change.
extern "C" {
struct SDL_AudioDevice;
SDL_AudioDevice* SDL_AddAudioDevice(bool recording, const char* name, const SDL_AudioSpec* spec, void* handle);
void SDL_AudioDeviceDisconnected(SDL_AudioDevice* device);
void SDL_DefaultAudioDeviceChanged(SDL_AudioDevice* device);
}

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
void PlaySilently(bool on = false) {
    SOUND_SetSoundQuality(kQuality);
    SOUND_SetGlobalVolume(100);
    SOUND_SetMusicVolume(80);
    SOUND_SetSpeechVolume(80);
    SOUND_SetSfxVolume(80);
    if (!on) Check(SOUND_RegisterCatagory("nowhere", "nothing") == 0, "with no system a sound category answers 0");
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

constexpr uint32_t kRate = 22050;
constexpr uint32_t kChunk = 4096;
std::string gLogFile;   // one per mode, as ctest can run them side by side

// The log goes to a file while a mode runs: how many times a line comes is part of what is tested.
void CaptureLog() {
    tpw::config().logToStderr = true;
    Check(std::freopen(gLogFile.c_str(), "w", stderr) != nullptr, "the log file opens");
}

int LogLines(const char* text) {
    std::fflush(stderr);
    std::ifstream file(gLogFile);
    std::string line;
    int count = 0;
    while (std::getline(file, line)) {
        if (line.find(text) != std::string::npos) count++;
    }
    return count;
}

bool WaitFor(const std::function<bool()>& what, int ms) {
    const uint64_t end = SDL_GetTicks() + static_cast<uint64_t>(ms);
    while (SDL_GetTicks() < end) {
        if (what()) return true;
        SDL_PumpEvents();
        SDL_Delay(5);
    }
    return what();
}

// A sample that never ends, played on stream 0 of a device.
struct Endless {
    tpw::SoundDevice& device;
    std::atomic<uint32_t> pulls{0};
    explicit Endless(tpw::SoundDevice& d, uint32_t stream = 0) : device(d) {
        const tpw::SoundSource source{kRate, 16, 2};
        Check(device.SetSource(stream, source, kChunk), "the stream takes the format");
        Check(device.Play(stream, [this](void*, uint32_t) { pulls++; return true; }), "the sample starts");
    }
    // Pulled again within the time a device takes for a few chunks, however busy the computer.
    bool Goes(int ms = 5000) {
        const uint32_t from = pulls.load();
        return WaitFor([&] { return pulls.load() >= from + 3; }, ms);
    }
    bool Holds(int ms = 300) {
        SDL_Delay(50);   // the round in progress
        const uint32_t from = pulls.load();
        SDL_Delay(static_cast<uint32_t>(ms));
        return pulls.load() == from;
    }
};

// With no device: a sample goes on at the speed of a device and comes to its end, and Stop waits for a pull in progress.
void CheckNoDeviceProgress(tpw::SoundDevice& device) {
    {
        const tpw::SoundSource source{kRate, 16, 2};
        Check(device.SetSource(2, source, kChunk), "the stream takes the format");
        std::atomic<int> left{6};
        Check(device.Play(2, [&](void*, uint32_t) { return --left > 0; }), "a short sample starts");
        Check(WaitFor([&] { return device.IsEnded(2); }, 5000), "with no device a short sample comes to its end");
        device.Stop(2);
    }
    {
        const tpw::SoundSource source{kRate, 16, 2};
        Check(device.SetSource(3, source, kChunk), "the stream takes the format");
        std::atomic<bool> inPull{false};
        std::atomic<uint32_t> pulls{0};
        Check(device.Play(3, [&](void*, uint32_t) {
            inPull = true;
            pulls++;
            SDL_Delay(30);
            inPull = false;
            return true;
        }), "a slow sample starts");
        const uint32_t first = pulls.load();
        Check(WaitFor([&] { return pulls.load() > first && inPull.load(); }, 5000), "with no device a slow sample is pulled");
        device.Pause(3);
        Check(!inPull.load(), "Pause waits for the pull in progress, as on a device");
        const uint32_t paused = pulls.load();
        SDL_Delay(100);
        Check(pulls.load() == paused, "nothing is pulled while paused");
        device.Restart(3);
        Check(WaitFor([&] { return pulls.load() > paused && inPull.load(); }, 5000), "after Restart the sample is pulled again");
        device.Stop(3);
        Check(!inPull.load(), "Stop waits for the pull in progress, as on a device");
        const uint32_t stopped = pulls.load();
        SDL_Delay(100);
        Check(pulls.load() == stopped, "nothing is pulled after Stop");
    }
    {
        // Pause at every moment of the watcher's round: once it answers, no pull starts.
        Endless sample(device, 4);
        for (uint32_t round = 0; round < 25; round++) {
            SDL_Delay(round);
            device.Pause(4);
            const uint32_t paused = sample.pulls.load();
            SDL_Delay(40);
            Check(sample.pulls.load() == paused, "no pull starts once Pause has answered");
            device.Restart(4);
        }
        device.Stop(4);
    }
    const uint64_t start = SDL_GetTicks();
    Endless timed(device, 1);
    SDL_Delay(1000);
    const uint32_t chunks = timed.pulls.load();
    const uint64_t ms = SDL_GetTicks() - start;
    Check(chunks >= 5, "with no device a sample is pulled in real time, not too slowly");
    Check(chunks <= (ms + 500) * kRate * 4 / 1000 / kChunk + 2, "with no device a sample is pulled in real time, not too fast");
    device.Stop(1);
}

void TestSilent() {
    SetEnv("SDL_AUDIO_DRIVER", "dummy");
    CaptureLog();
    // The whole game's sound with the device coming while it plays.
    SetEnv("TPW_SOUND_NO_DEVICE", "after:2");
    Start();
    Check(gSoundSystemOn == 1, "with no device the sound system is on");
    Check(gAudioSystem != nullptr, "with no device there is an audio object");
    Check(LogLines("sound: no audio device") == 1, "the log says it once");
    PlaySilently(true);
    Check(WaitFor([&] {
        SOUND_Tick();
        return LogLines("sound: a sound device has opened") == 1;
    }, 8000), "the device that comes is taken by the game's sound");
    PlaySilently(true);
    SOUND_Close();
    // No device at all this time: it plays and closes with none.
    SetEnv("TPW_SOUND_NO_DEVICE", "1");
    Start();
    Check(gSoundSystemOn == 1, "with no device the sound system comes on again");
    PlaySilently(true);
    SDL_Delay(300);
    SOUND_Tick();
    SOUND_Close();
    // A game that quits with its sound still up, as a game ended from its menu does: the watcher goes first, as in the launcher.
    Start();
    Check(gSoundSystemOn == 1, "and again");
    SDL_Delay(100);
    tpw::SoundDevice::StopWatchers();   // what launcher/main.cpp does before its SDL_Quit
    SDL_Quit();
    SDL_Delay(100);
}

void TestArrive() {
    SetEnv("SDL_AUDIO_DRIVER", "dummy");
    SetEnv("TPW_SOUND_NO_DEVICE", "1");
    CaptureLog();
    tpw::SoundDevice device;
    Check(device.Open(kRate, 5), "with no device Open answers true");
    Check(!device.IsOpen() && device.DeviceId() == 0, "there is no device");
    Check(device.Watching(), "the watcher runs");
    CheckNoDeviceProgress(device);
    Endless sample(device);
    Check(device.SetGain(0, 0.25f), "the game sets a volume");
    Check(device.SetGain(1, 0.0f), "the game mutes a stream");
    Check(sample.Goes(), "with no device the sample goes on");
    device.SetDevicePaused(true);
    Check(sample.Holds(), "the device paused, nothing is pulled");
    device.SetDevicePaused(false);
    Check(sample.Goes(), "the device resumed, the sample goes on");
    SDL_Delay(4500);   // the watcher tries again more than once
    Check(!device.IsOpen(), "still no device");
    Check(LogLines("sound: no audio device") == 1, "the failure is logged once, however often it is tried");
    Check(LogLines("sound:") == 2, "and nothing else is logged while it waits");

    device.SetDevicePaused(true);
    UnsetEnv("TPW_SOUND_NO_DEVICE");
    Check(WaitFor([&] { return device.IsOpen(); }, 6000), "the device that comes is taken, with no restart");
    Check(sample.Holds(), "a paused game stays paused on the new device");
    device.SetDevicePaused(false);
    Check(device.StreamDevice(0) == device.DeviceId(), "the playing sample is on the device");
    Check(device.StreamDevice(1) == 0, "a stream that does not play is not");
    Check(sample.Goes(), "the sample goes on, now pulled by the device");
    Check(device.Gain(0) == 0.25f, "its volume is as the game left it");
    Check(device.Gain(1) == 0.0f, "a muted stream stays muted");
    Check(LogLines("sound: a sound device has opened") == 1, "the log says so once");
    device.Stop(0);
    Check(device.StreamDevice(0) == 0, "Stop takes it off the device");
    Check(device.Play(0, [](void*, uint32_t) { return true; }), "a new sample starts on the device");
    Check(device.StreamDevice(0) == device.DeviceId(), "and is bound to it at once");
    device.Close();
    Check(!device.Watching(), "Close stops the watcher");
    Check(tpw::SoundDevice::LiveStreams() == 0, "Close leaves no streams");
}

void TestLate() {
    SetEnv("SDL_AUDIO_DRIVER", "no-such-driver");
    Check(SDL_InitSubSystem(SDL_INIT_EVENTS), "SDL's events start");   // the game's main loop pumps them
    CaptureLog();
    tpw::SoundDevice device;
    Check(device.Open(kRate, 5), "with no audio driver Open answers true");
    Check(!device.IsOpen(), "there is no device");
    Endless sample(device);
    Check(sample.Goes(), "with no audio driver the sample goes on");
    Check(!WaitFor([&] { return device.IsOpen(); }, 3000), "a driver that still fails gives no device");
    SetEnv("SDL_AUDIO_DRIVER", "dummy");
    Check(WaitFor([&] { return device.IsOpen(); }, 20000), "the driver that starts later brings the device, with no restart");
    Check(device.StreamDevice(0) == device.DeviceId(), "the playing sample is on the device");
    Check(sample.Goes(), "the sample goes on, now pulled by the device");
    Check(LogLines("sound: SDL audio failed to start") == 1, "the driver's failure is logged once");
    device.Close();
}

std::string NameOf(uint32_t device) {
    const char* name = SDL_GetAudioDeviceName(device);
    return name != nullptr ? name : "";
}

void TestLost() {
    SetEnv("SDL_AUDIO_DRIVER", "dummy");
    CaptureLog();
    Check(SDL_InitSubSystem(SDL_INIT_AUDIO), "SDL audio starts");
    SDL_AudioSpec spec;
    SDL_zero(spec);
    spec.format = SDL_AUDIO_S16;
    spec.channels = 2;
    spec.freq = 44100;
    static int handles[3];
    SDL_AudioDevice* speakers = SDL_AddAudioDevice(false, "speakers", &spec, &handles[0]);
    SDL_AudioDevice* phones = SDL_AddAudioDevice(false, "phones", &spec, &handles[1]);
    Check(speakers != nullptr && phones != nullptr, "two devices come");
    SDL_DefaultAudioDeviceChanged(speakers);

    tpw::SoundDevice device;
    Check(device.Open(kRate, 5), "Open");
    const uint32_t id = device.DeviceId();
    Check(NameOf(id) == "speakers", "the game opens the default");
    Endless sample(device);
    Check(device.SetGain(0, 0.25f), "the game sets a volume");
    Check(sample.Goes(), "the sample plays");

    SDL_DefaultAudioDeviceChanged(phones);
    Check(WaitFor([&] { return NameOf(id) == "phones"; }, 3000), "SDL moves the game to the new default");
    Check(sample.Goes(), "the sample goes on there");
    SDL_AudioDeviceDisconnected(speakers);
    Check(sample.Goes(), "the old one going changes nothing");
    SDL_AudioDeviceDisconnected(phones);   // the last device gone
    Check(sample.Goes(), "the last device gone, the sample goes on and nothing hangs");
    Check(device.DeviceId() == id, "the game keeps its device, SDL's stand-in for the lost one");
    SDL_Delay(4500);   // the watcher looks at this too, twice
    Check(device.DeviceId() == id, "and the watcher does not interfere");
    Check(sample.Goes(), "still going");

    SDL_AudioDevice* back = SDL_AddAudioDevice(false, "phones again", &spec, &handles[2]);
    Check(back != nullptr, "a device comes back");
    SDL_DefaultAudioDeviceChanged(back);
    Check(WaitFor([&] { return NameOf(id) == "phones again"; }, 3000), "SDL moves the game to the one that came back");
    Check(sample.Goes(), "the sample goes on there");
    Check(device.StreamDevice(0) == id, "still bound");
    Check(device.Gain(0) == 0.25f, "its volume is as the game left it");
    Check(LogLines("opened afresh") == 0, "SDL did it all; the watcher did nothing");
    device.Close();
}

void TestStale() {
    SetEnv("SDL_AUDIO_DRIVER", "dummy");
    CaptureLog();
    tpw::SoundDevice device;
    Check(device.Open(kRate, 5), "Open");
    const uint32_t first = device.DeviceId();
    Check(first != 0, "there is a device");
    Endless sample(device);
    Check(device.SetGain(0, 0.25f), "the game sets a volume");
    Check(device.SetGain(1, 0.0f), "the game mutes a stream");
    Check(sample.Goes(), "the sample plays");

    // SDL is left on the old device, and the new one will not open yet either.
    SetEnv("TPW_SOUND_DEFAULT_NAME", "the new default");
    SetEnv("TPW_SOUND_NO_DEVICE", "1");
    Check(WaitFor([&] { return LogLines("will not open yet") == 1; }, 8000), "the watcher notices, and logs that it cannot move");
    Check(device.DeviceId() == first, "the game stays where it is");
    Check(sample.Goes(), "and the sample goes on");
    SDL_Delay(4500);
    Check(LogLines("will not open yet") == 1, "tried again, with no more lines");

    UnsetEnv("TPW_SOUND_NO_DEVICE");
    Check(WaitFor([&] { return device.DeviceId() != first && device.DeviceId() != 0; }, 6000), "when it opens the game moves");
    UnsetEnv("TPW_SOUND_DEFAULT_NAME");
    Check(WaitFor([&] { return device.StreamDevice(0) == device.DeviceId(); }, 3000), "the playing sample is on the new device");
    Check(device.StreamDevice(1) == 0, "a stream that does not play is not");
    Check(sample.Goes(), "the sample goes on, pulled by the new device");
    Check(device.Gain(0) == 0.25f, "its volume is as the game left it");
    Check(device.Gain(1) == 0.0f, "a muted stream stays muted");
    Check(LogLines("opened afresh") == 1, "the log says so once");
    const uint32_t second = device.DeviceId();
    SDL_Delay(4500);
    Check(device.DeviceId() == second, "and the game stays there");
    device.Close();
}

void TestMute() {
    tpw::config().mute = true;
    SetEnv("TPW_SOUND_NO_DEVICE", "1");
    CaptureLog();
    {
        tpw::SoundDevice device;
        Check(!device.Open(kRate, 5), "--mute: the dummy driver's device failing is a failure, as before");
        Check(!device.Watching(), "--mute: nothing waits for a device");
        UnsetEnv("TPW_SOUND_NO_DEVICE");
        SDL_Delay(2500);
        Check(!device.IsOpen(), "--mute: nothing tries again");
        Check(tpw::SoundDevice::LiveStreams() == 0, "--mute: no streams were made");
        Check(LogLines("sound: no audio device") == 1, "--mute: the log says it once");
        Check(device.Open(kRate, 5), "--mute: a later Open works");
        Check(device.IsOpen() && !device.Watching(), "--mute: there is a device and still no watcher");
        Check(std::strcmp(SDL_GetCurrentAudioDriver(), "dummy") == 0, "--mute: on SDL's dummy driver");
        Endless sample(device);
        Check(sample.Goes(), "--mute: the sample runs its course in real time");
        device.Close();
    }
    tpw::config().mute = false;
    tpw::SoundDevice device;
    Check(device.Open(kRate, 5) && device.Watching(), "the same run without --mute does have a watcher");
    device.Close();
    Check(!device.Watching(), "Close stops it");
}

// Commands as fast as they come, from this thread and from inside a pull (a pull may command another stream), while the device comes
// and is then opened afresh again and again. A hang is the failure ctest's time limit catches; the end state is checked here.
void Hammer(tpw::SoundDevice& device, uint64_t ms) {
    const uint64_t end = SDL_GetTicks() + ms;
    uint32_t round = 0;
    while (SDL_GetTicks() < end) {
        const uint32_t stream = round % 5;
        switch ((round / 5) % 4) {
        case 0: device.Play(stream, [&device, stream](void*, uint32_t) {
                    static std::atomic<uint32_t> pulls{0};
                    if (stream == 4 && ++pulls % 5 == 0) device.Pause(3);
                    return true;
                });
                break;
        case 1: device.Pause(stream); break;
        case 2: device.Restart(stream); break;
        case 3: device.Stop(stream); break;
        }
        ++round;
        SDL_Delay(round % 16 == 0 ? 1 : 0);
    }
}

void CheckStreamsAgree(tpw::SoundDevice& device, const char* when) {
    bool agree = true;
    for (uint32_t i = 0; i < 5; ++i) {
        if (device.IsBound(i) && device.StreamDevice(i) != device.DeviceId()) agree = false;
        if (!device.IsBound(i)) device.Stop(i);
        if (device.StreamDevice(i) != 0 && !device.IsBound(i)) agree = false;
    }
    Check(agree, when);
}

void TestStress() {
    SetEnv("SDL_AUDIO_DRIVER", "dummy");
    SetEnv("TPW_SOUND_NO_DEVICE", "after:3");
    CaptureLog();
    tpw::SoundDevice device;
    Check(device.Open(kRate, 5), "with no device Open answers true");
    const tpw::SoundSource source{kRate, 16, 2};
    for (uint32_t i = 0; i < 5; ++i) Check(device.SetSource(i, source, kChunk), "the stream takes the format");
    Hammer(device, 6000);
    Check(device.IsOpen(), "the device came during the commands");
    SDL_Delay(100);
    CheckStreamsAgree(device, "after the device came each stream is where it was told");
    UnsetEnv("TPW_SOUND_NO_DEVICE");
    const uint32_t first = device.DeviceId();
    SetEnv("TPW_SOUND_DEFAULT_NAME", "the new default");
    Hammer(device, 9000);
    UnsetEnv("TPW_SOUND_DEFAULT_NAME");
    Check(WaitFor([&] { return device.IsOpen(); }, 3000), "there is a device");
    Check(device.DeviceId() != first, "the device was opened afresh during the commands");
    SDL_Delay(100);
    CheckStreamsAgree(device, "after the moves each stream is where it was told");
    Check(LogLines("cannot be bound") == 0, "no command failed");
    device.Close();
    Check(tpw::SoundDevice::LiveStreams() == 0, "Close leaves no streams");
}

}  // namespace

int main(int argc, char** argv) {
    const char* which = argc > 1 ? argv[1] : "";
    std::setvbuf(stdout, nullptr, _IONBF, 0);   // what was reached is printed even when the next step crashes
    tpw::ArenaBase();
    gLogFile = std::string("host_no_sound_device_") + which + ".log";
    if (std::strcmp(which, "silent") == 0) {
        TestSilent();
    } else if (std::strcmp(which, "arrive") == 0) {
        TestArrive();
    } else if (std::strcmp(which, "late") == 0) {
        TestLate();
    } else if (std::strcmp(which, "lost") == 0) {
        TestLost();
    } else if (std::strcmp(which, "stale") == 0) {
        TestStale();
    } else if (std::strcmp(which, "mute") == 0) {
        TestMute();
    } else if (std::strcmp(which, "stress") == 0) {
        TestStress();
    } else if (std::strcmp(which, "driver") == 0) {
        tpw::config().mute = true;
        SetEnv("SDL_AUDIO_DRIVER", "no-such-driver");
        TestNoSound("SDL's audio does not start");
    } else if (std::strcmp(which, "open") == 0) {
        tpw::config().mute = true;
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
        std::printf("usage: host_no_sound_device driver|open|silent|arrive|late|lost|stale|mute|stress\n");
        return 2;
    }
    Check(gStandIns == 0, "no stand-in was reached");
    std::printf("no_sound_device: %d checks, %d failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
