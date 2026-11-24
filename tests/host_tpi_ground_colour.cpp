// Exercise the actual procedural tile conversion, including the mod and texture-format gates.
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include "TpiGateFlow.h"
#include "tb_resource_texture.h"

void ConvertSquareFrom32(void*, void*, int32_t);

int TpiGroundColourTest() {
    int checks = 0, failures = 0;
    const auto check = [&](bool ok, const char* why) {
        ++checks;
        if (!ok) { ++failures; std::printf("FAIL: %s\n", why); }
    };
    std::array<uint8_t, 64 * 64 * 4> source{};
    std::array<uint16_t, 64 * 64> out{};
    const auto tile = [&](int size, int r, int g, int b, int a = 0) {
        for (int i = 0; i < size * size; ++i) {
            source[4 * i] = uint8_t(a); source[4 * i + 1] = uint8_t(r);
            source[4 * i + 2] = uint8_t(g); source[4 * i + 3] = uint8_t(b);
        }
        ConvertSquareFrom32(out.data(), source.data(), size);
    };
    const auto mean = [&](int size, int shift) {
        double sum = 0;
        for (int i = 0; i < size * size; ++i) sum += ((out[i] >> shift) & 15) * 17;
        return sum / (size * size);
    };
    gModTpiWorlds = true; tpw::TpiNoteTheme("arabian"); gei_TextureSettings.mFormat = 4;
    tile(16, 253, 222, 145);
    std::array<uint16_t, 16> pattern{};
    for (int y = 0; y < 4; ++y) for (int x = 0; x < 4; ++x) pattern[y * 4 + x] = out[y * 16 + x];
    for (int size : {16, 32, 64}) {
        // The real Arabian LND's sand is near the 224 green and 144 blue truncation thresholds.
        tile(size, 253, 222, 145);
        check(std::fabs(mean(size, 12) - 253) < 0.6, "sand red keeps its source mean");
        check(std::fabs(mean(size, 8) - 222) < 0.6, "sand green keeps its source mean");
        check(std::fabs(mean(size, 4) - 145) < 0.6, "sand blue keeps its source mean");
        bool phase = true;
        for (int y = 0; y < size; ++y) for (int x = 0; x < size; x += 4)
            phase &= std::memcmp(out.data() + y * size + x, pattern.data() + (y & 3) * 4, 4 * sizeof(uint16_t)) == 0;
        check(phase, "all tile sizes keep the same repeating rounding pattern");
        const double before = mean(size, 8);
        tile(size, 253, 224, 145);
        check(mean(size, 8) - before < 3, "two green levels do not become a seventeen-level patch");
    }
    for (const char* theme : {"arabian", "water", "science"}) {
        tpw::TpiNoteTheme(theme);
        for (int value = 0; value < 256; ++value) {
            tile(16, value, value, value, value);
            for (int shift : {4, 8, 12})
                check(std::fabs(mean(16, shift) - value) < 0.6, "every mod-world colour preserves its mean");
            bool alpha = true;
            for (int i = 0; i < 256; ++i) alpha &= (out[i] & 15) == (value >> 4);
            check(alpha, "alpha keeps the original high nibble");
        }
    }
    for (const char* theme : {"jungle", "hallow", "space", "fantasy", "arabian"}) {
        tpw::TpiNoteTheme(theme); gModTpiWorlds = std::strcmp(theme, "arabian") != 0;
        for (int value = 0; value < 256; ++value) {
            tile(16, value, value, value, value);
            const uint16_t legacy = uint16_t((value >> 4) * 0x1111);
            bool same = true;
            for (int i = 0; i < 256; ++i) same &= out[i] == legacy;
            check(same, "original worlds and mods off keep every legacy pixel");
        }
    }
    // 32-bit procedural output already retains all channels and takes the existing path.
    gModTpiWorlds = true; tpw::TpiNoteTheme("arabian"); gei_TextureSettings.mFormat = 5;
    std::array<uint8_t, 16 * 16 * 4> full{};
    for (size_t i = 0; i < full.size(); ++i) source[i] = uint8_t(i);
    ConvertSquareFrom32(full.data(), source.data(), 16);
    bool rgba = true;
    for (int i = 0; i < 256; ++i) {
        rgba &= full[i * 4] == source[i * 4 + 1] && full[i * 4 + 1] == source[i * 4 + 2] &&
                full[i * 4 + 2] == source[i * 4 + 3] && full[i * 4 + 3] == source[i * 4];
    }
    check(rgba, "32-bit output keeps the existing channel conversion");
    std::printf("tpi_ground_colour: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
