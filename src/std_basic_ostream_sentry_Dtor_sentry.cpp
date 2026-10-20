// std::basic_ostream<char>::sentry::~sentry 0x1020758c (unit v5251).
#include "std_basic_istream.h"

MacSentryObject* OstreamSentryDtor(MacSentryObject* self, int16_t deleteFlag) {
    if (self != nullptr) {
        MacSentryStream* stream = self->mStream;
        MacSentryIos* ios = stream->mIos;
        if ((ios->mState & 5) == 0 && (ios->mFlags & 0x2000) != 0 && self->mDone == 0) MslOstreamFlush(stream);
        if (deleteFlag > 0) MslOperatorDelete(self);
    }
    return self;
}
