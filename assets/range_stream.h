// A forward stream over byte ranges of a Blob (the extents of an HFS fork, the fork of a MacBinary file). Internal to the
// asset readers.
#pragma once

#include <algorithm>
#include <string>
#include <vector>

#include "source.h"

namespace tpw::assets {

struct ByteRange {
    uint64_t offset;
    uint64_t length;
};

class RangeStream : public Stream {
public:
    // The first `size` bytes of the ranges laid end to end.
    RangeStream(std::shared_ptr<Blob> blob, std::vector<ByteRange> ranges, uint64_t size)
        : blob_(std::move(blob)), ranges_(std::move(ranges)), size_(size) {}
    uint64_t Size() const override { return size_; }
    int64_t Read(void* buf, size_t n, std::string& err) override {
        if (n == 0 || pos_ >= size_) return 0;
        if (n > size_ - pos_) n = static_cast<size_t>(size_ - pos_);
        while (range_ < ranges_.size() && inRange_ >= ranges_[range_].length) {
            range_++;
            inRange_ = 0;
        }
        if (range_ >= ranges_.size()) {
            err = "the entry's data runs out before its size";
            return -1;
        }
        const ByteRange& r = ranges_[range_];
        if (n > r.length - inRange_) n = static_cast<size_t>(r.length - inRange_);
        int64_t got = blob_->ReadAt(r.offset + inRange_, buf, n, err);
        if (got < 0) return -1;
        if (got == 0) {
            err = "the image is cut off";
            return -1;
        }
        inRange_ += got;
        pos_ += got;
        return got;
    }

private:
    std::shared_ptr<Blob> blob_;
    std::vector<ByteRange> ranges_;
    uint64_t size_;
    uint64_t pos_ = 0;
    size_t range_ = 0;
    uint64_t inRange_ = 0;
};

}  // namespace tpw::assets
