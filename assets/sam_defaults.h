// Original-disc settings used only to repair a player's already imported file. The files themselves are the launcher's
// (launcher/sam_originals.cpp, tpw-port only: The maintainer's exception of 2026-11-17); it registers them with SetSamDefaults.
#pragma once

#include "sam_check.h"
#include <cstddef>
#include <string>
#include <vector>

namespace tpw::assets {

struct SamDefault {
    const char* edition;
    const char* dest;
    const char* text;
    size_t size;
};

// Gold uses its US Windows base disc's settings, not TPI. Mac settings are
// supplied only by byte-identical Windows originals or the original Mac CD.
// No default is evidence that the player owns a disc or has imported an edition.
// rows stay owned by the caller and must outlive every lookup; nullptr unregisters.
void SetSamDefaults(const SamDefault* rows, size_t count);
const SamDefault* FindSamDefault(const std::string& edition, const std::string& dest);
const SamDefault* SamDefaults();
size_t SamDefaultCount();

struct SamRepairResult {
    bool repaired = false;
    size_t kept = 0;       // readable player statements, including ones after damage
    size_t discarded = 0;  // unreadable lines or map blocks
};

// A readable file is returned byte for byte. Otherwise start with the original
// defaults, then retain every readable player statement in its original order.
// The game assigns the later value, so readable edits win, including multi-field
// statements and duplicate keys. Missing or unreadable keys keep the defaults.
// Requires the game's registered description table. Failure leaves out untouched.
// Call only for a present imported file, never to supply a missing disc/file.
bool RepairSam(const std::vector<SamRow>& table, const std::string& original,
               const std::string& player, std::string& out, SamRepairResult& result, std::string& error);
bool RepairSam(const std::string& edition, const std::string& dest, const std::string& player,
               std::string& out, SamRepairResult& result, std::string& error);

}  // namespace tpw::assets
