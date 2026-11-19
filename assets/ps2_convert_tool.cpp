// ps2_convert_tool: converts the PS2 Guest Models mod's characters from a Theme Park World PS2 disc (a .cue, an image
// or the disc's files in a folder) into a data folder's assets/ps2, printing the converter's log. The development
// command for the mod's first milestone; the launcher's drop runs the same ConvertPs2.
#include <cstdio>
#include <string>

#include "ps2_convert.h"
#include "ps2_disc.h"

int main(int argc, char** argv) {
    if (argc != 3) {
        std::fprintf(stderr, "usage: tpw_ps2_convert_tool <ps2 disc: .cue, image or folder> <data folder>\n");
        return 2;
    }
    tpw::assets::Ps2Disc disc;
    std::string reason;
    if (tpw::assets::RecognisePs2Path(argv[1], disc, reason) != tpw::assets::Ps2Status::Supported) {
        std::fprintf(stderr, "%s: %s\n", argv[1], reason.c_str());
        return 1;
    }
    std::printf("recognised: boots %s, %zu files in Chars\n", disc.boot.c_str(), disc.chars.size());
    std::vector<std::string> log;
    const bool ok = tpw::assets::ConvertPs2(disc, argv[2], log, reason);
    for (const std::string& line : log) std::printf("%s\n", line.c_str());
    if (!ok) {
        std::fprintf(stderr, "failed: %s\n", reason.c_str());
        return 1;
    }
    std::printf("converter %d written to %s\n", tpw::assets::ReadPs2ConverterStamp(argv[2]),
                tpw::assets::Ps2ContentPath(argv[2]).u8string().c_str());
    return 0;
}
