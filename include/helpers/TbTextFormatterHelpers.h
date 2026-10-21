// TbTextFormatterHelpers.h: file-local helpers that src/ltms/TbTextFormatter.cpp shares with tb_depthoffield.cpp.
// Before the regroup, the functions using them shared a source file (1 file), which had one copy of each; that copy now
// lives here, static inline, so each file that includes this header keeps its own internal copy.
#pragma once

#include "TbTextFormatter.h"

// IsParenthesis 0x10006864 (inlined in the original's callers; tested, unit v5L21): 1 for '(', 2 for ')', 0 otherwise.
static inline int IsParenthesis(const CCharacterInfo* character) {
    if (character->mChar == 0x28) return 1;
    if (character->mChar == 0x29) return 2;
    return 0;
}
