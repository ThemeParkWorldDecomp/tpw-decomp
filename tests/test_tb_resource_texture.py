"""Differential tests for engine tb_resource_texture.cpp (see difftest.py, tpwtest.py)."""
from tpwtest import Test, Buf, Glob, I, INT_EDGES, rint, rwords

PROGRAM = "engine symbols shared"

GEI_TEXTURE_SETTINGS = 0x10165fc8      # engine data, export gei_TextureSettings
OBJ = "20I"                           # CTextureSettings, 0x50 bytes, compared as raw words (floats bit-exact)

FILLS = [[0] * 20, [0xFFFFFFFF] * 20, [0x7FC00000] * 20, list(range(20))]


def obj_cases():
    return [dict(obj=f) for f in FILLS]


def obj_random(r):
    return dict(obj=rwords(r, 20))


SIZES = [-0x80000000, -0x100, -1, 0, 1, 2, 3, 4, 5, 7, 8, 9, 15, 16, 17, 31, 32, 33, 63, 64, 65, 127, 128, 129,
         255, 256, 257, 512, 0x10000, 0x7FFFFFFF] + INT_EDGES


def rsize(r):
    k = r.random()
    if k < 0.5:
        return r.choice([1 << b for b in range(32)]) + r.choice([0, 0, -1, 1])
    return rint(r)


TESTS = [
    Test("CTextureSettings::CTextureSettings", "__ct__16CTextureSettingsFv", "tpw_trt_ctor",
         lambda c: [Buf("obj", OBJ, c["obj"])], ret="none", edge=obj_cases, rnd=obj_random),
    Test("CTextureSettings::GetAlphaMask", "GetAlphaMask__16CTextureSettingsFv", "tpw_trt_GetAlphaMask",
         lambda c: [Buf("obj", OBJ, c["obj"])], ret="u32", edge=obj_cases, rnd=obj_random),
    Test("CTextureSettings::CalculateUVFixup", "CalculateUVFixup__16CTextureSettingsFv", "tpw_trt_CalculateUVFixup",
         lambda c: [Buf("obj", OBJ, c["obj"])], ret="none", edge=obj_cases, rnd=obj_random),
    Test("CTextureSettings::OutputDebugInfo", "OutputDebugInfo__16CTextureSettingsFv", "tpw_trt_OutputDebugInfo",
         lambda c: [Buf("obj", OBJ, c["obj"])], ret="none", edge=obj_cases, rnd=obj_random),
    Test("ConvertSurfaceSizeToFlag", "ConvertSurfaceSizeToFlag__Fi", "tpw_ConvertSurfaceSizeToFlag",
         lambda c: [I(c["n"])], ret="i32",
         edge=lambda: [dict(n=n) for n in SIZES] + [dict(n=1 << b) for b in range(32)],
         rnd=lambda r: dict(n=rsize(r))),
    Test("__sinit_tb_resource_texture_cpp", "__sinit_tb_resource_texture_cpp", "tpw_trt_sinit",
         lambda c: [], ret="none",
         globs=lambda c: [Glob("gei_TextureSettings", GEI_TEXTURE_SETTINGS, "tpw_trt_global", OBJ, c["obj"])],
         edge=obj_cases, rnd=obj_random),
]
