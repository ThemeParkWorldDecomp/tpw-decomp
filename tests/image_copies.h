// Unit v5L39: copies of tables of initialised data as a test library was loaded, before a case writes them, for the image tests of
// tests/imagedata.py. Include it in ONE file of a test library (it defines the two C entry points) and name each table once:
//   TPW_IMAGE_COPY(needs, gNeedMatchTable);
// (TPW_IMAGE_COPY_SIZE for a table declared without a bound.)
// tpw_image_copy(label) answers the copy's address (0 for an unknown label), tpw_image_size(label) its size in bytes.
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace tpw_image {

struct Copy {
    const char* label;
    std::vector<uint8_t> bytes;
};

inline std::vector<Copy>& Copies() {
    static std::vector<Copy> copies;
    return copies;
}

struct Take {
    Take(const char* label, const void* object, size_t size) {
        const uint8_t* p = static_cast<const uint8_t*>(object);
        Copies().push_back({label, std::vector<uint8_t>(p, p + size)});
    }
};

inline const Copy* Find(const char* label) {
    for (const Copy& copy : Copies())
        if (std::strcmp(copy.label, label) == 0) return &copy;
    return nullptr;
}

}  // namespace tpw_image

#define TPW_IMAGE_COPY(label, object) static const tpw_image::Take tpw_image_take_##label(#label, &(object), sizeof(object))
// For a table whose declaration has no bound: the size in bytes is given.
#define TPW_IMAGE_COPY_SIZE(label, object, size) static const tpw_image::Take tpw_image_take_##label(#label, &(object), size)

extern "C" uint64_t tpw_image_copy(const char* label) {
    const tpw_image::Copy* copy = tpw_image::Find(label);
    return copy ? static_cast<uint64_t>(reinterpret_cast<uintptr_t>(copy->bytes.data())) : 0;
}

extern "C" uint64_t tpw_image_size(const char* label) {
    const tpw_image::Copy* copy = tpw_image::Find(label);
    return copy ? copy->bytes.size() : 0;
}
