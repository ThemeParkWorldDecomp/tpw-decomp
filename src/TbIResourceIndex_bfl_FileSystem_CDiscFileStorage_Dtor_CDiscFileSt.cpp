// ltms 0x100034c0: bfl::FileSystem::CDiscFileStorage::~CDiscFileStorage (out-of-line copy in the ltms library).
#include "bfl_FileSystem.h"

#include <new>

namespace bfl {
namespace FileSystem {

// If this is non-NULL: store the CDiscFileStorage vtable (not observable on the host), run the base destructor,
// and delete the storage when the hidden short flag is positive. Returns this.
CDiscFileStorage* CDiscFileStorage::DestroyLtms(CDiscFileStorage* self, int16_t deleteFlag) {
    if (self) {
        self->CFileStorage::~CFileStorage();
        if (deleteFlag > 0) ::operator delete(static_cast<void*>(self));
    }
    return self;
}

}  // namespace FileSystem
}  // namespace bfl
