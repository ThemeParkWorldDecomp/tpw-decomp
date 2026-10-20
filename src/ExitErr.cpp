// ExitErr.cpp (main app of SimTheme Park 2.04b3). PARTIAL: only CExitError::SetError
// (0x1014b62c) so far; ReportAnyError, HandleEngineErrors and ConvertEngineErrors
// (0x1014b6c8..0x1014bd04) are still to do.
#include "ExitErr.h"

#include <cstring>

CExitError gExitError;

// The reason the game is giving up: a type, a value and up to 0x50 characters of text (a
// longer text is cut without a terminator, as strncpy leaves it).
void CExitError::SetError(ErrorType type, uint32_t value, const char* text) {
    mType = type;
    mValue = value;
    if (text != nullptr) std::strncpy(mText, text, 0x50);
    mError = 1;
}
