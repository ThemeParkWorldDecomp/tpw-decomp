#include "platform.h"
#include <SDL3/SDL.h>

void IntroSequenceConfig(const std::filesystem::path& game, const std::filesystem::path& user, const char* edition) {
    tpw::config().gameDir = game;
    tpw::config().userDir = user;
    tpw::config().edition = edition;
    tpw::config().movies = true;
    tpw::config().mute = true;
}

bool IntroSequenceVideoStarted() { return SDL_WasInit(SDL_INIT_VIDEO) != 0; }
