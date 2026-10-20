// ExitErr.cpp (main app): CExitError, the reason the game gives up and the box that shows it.
// SetError (src/ExitErr.cpp) keeps the reason; ReportAnyError, HandleEngineErrors and
// ConvertEngineErrors (src/ExitErr_report.cpp) show it.
#pragma once
#include <cstdint>

class CExitError {
public:
    // The 43 reasons ReportAnyError knows (0..0x2a). Only two have a name that follows from
    // the code: 12 is the engine-error type (ReportAnyError asks the engine's error stack what
    // went wrong and shows the answer) and 13 is the type with no message of its own (the box
    // shows only the text SetError kept). The others are plain numbers; the string id each one
    // asks the UI string table for is in src/ExitErr_report.cpp. A type above 0x2a also shows
    // no message of its own. Types seen in callers: 2 (a bad command-line argument), 3 (a data
    // file that would not load, with its path as the text), 4, 8, 12.
    enum ErrorType : int32_t {
        kType0 = 0, kType1, kType2, kType3, kType4, kType5, kType6, kType7, kType8, kType9,
        kType10, kType11,
        kEngineErrors = 12,
        kEmpty = 13,
        kType14, kType15, kType16, kType17, kType18, kType19, kType20, kType21, kType22,
        kType23, kType24, kType25, kType26, kType27, kType28, kType29, kType30, kType31,
        kType32, kType33, kType34, kType35, kType36, kType37, kType38, kType39, kType40,
        kType41, kType42,
    };
    // 0x1014b6c8: shows the box for the error SetError kept (nothing when none is set or the
    // text system is not up). Type 12 goes through HandleEngineErrors and shows no box of its own.
    void ReportAnyError();
    // 0x1014bc38: drains the engine's error stack (gei_ErrorHandler), keeps the code of the last
    // level-3 record, turns it into a type with ConvertEngineErrors, clears the value, sets the
    // error and reports it.
    void HandleEngineErrors();
    // 0x1014bd04: the type of an engine error code (HRESULT style, 0xb0754000 + n): n = 0..0x16
    // gives 0xe + n, 0xb0754018 gives 0x26, 0xb0754100 gives 0x27, 0xb0754101 gives 0x28,
    // 0xb0754102 gives 0x29, any other code gives 0x2a. Does not read the object.
    int32_t ConvertEngineErrors(int32_t code) const;
    // 0x1014b62c. ControlInit reports a bad command-line argument as type 2.
    void SetError(ErrorType type, uint32_t value, const char* text);

    int32_t mError;          // 0x00  nonzero once an error is set
    ErrorType mType;         // 0x04
    uint32_t mValue;         // 0x08
    char mText[0x50];        // 0x0c
};
extern CExitError gExitError;   // 0x1037f1c0, original name unknown
