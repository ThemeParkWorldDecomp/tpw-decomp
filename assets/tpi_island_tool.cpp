// tpi_island_tool: builds the TPI Worlds and Rides mod's lobby islands from a Theme Park Inc folder (the folder that
// holds Data) into an output folder, printing the builder's log. A development tool for looking at the result. With a
// Theme Park World lobby.wad as the third argument the islands get their waves and the larger sea is written too.
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>

#include "tpi_island.h"

int main(int argc, char** argv) {
    if (argc != 3 && argc != 4) {
        std::fprintf(stderr, "usage: tpw_tpi_island_tool <theme park inc folder> <output folder> [<theme park world lobby.wad>]\n");
        return 2;
    }
    std::string err;
    std::vector<tpw::assets::TpiWadMember> lobby;
    if (argc == 4) {
        std::ifstream in(argv[3], std::ios::binary);
        const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        if (!tpw::assets::ReadTpiWad(bytes, lobby, err)) {
            std::fprintf(stderr, "%s: %s\n", argv[3], err.c_str());
            return 1;
        }
    }
    const std::vector<tpw::assets::TpiWadMember>* lobbyMembers = argc == 4 ? &lobby : nullptr;
    std::unique_ptr<tpw::assets::Source> folder = tpw::assets::OpenFolder(argv[1], err);
    if (!folder) {
        std::fprintf(stderr, "%s: %s\n", argv[1], err.c_str());
        return 1;
    }
    tpw::assets::TpiDisc disc;
    disc.edition = "Theme Park Inc";
    disc.path = argv[1];
    disc.source = std::move(folder);
    disc.root = "";
    tpw::assets::TpiIslandFiles built;
    std::string reason;
    const bool ok = tpw::assets::BuildTpiIslandFiles(disc, built, reason, lobbyMembers);
    for (const std::string& line : built.log) std::printf("%s\n", line.c_str());
    if (!ok) {
        std::fprintf(stderr, "failed: %s\n", reason.c_str());
        return 1;
    }
    if (!tpw::assets::BuildTpiIslands(disc, argv[2], reason, lobbyMembers)) {
        std::fprintf(stderr, "failed: %s\n", reason.c_str());
        return 1;
    }
    for (const auto& [path, bytes] : built.files) std::printf("%8zu %s\n", bytes.size(), path.c_str());
    return 0;
}
