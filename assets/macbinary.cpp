// MacBinary I, II and III: one Macintosh file as a 128-byte header, the data fork padded to a multiple of 128 bytes,
// and the resource fork. The header is accepted only when it is consistent (zero bytes at 0, 74 and 82, a name length of
// 1 to 63, fork sizes that fit in the file, and either the MacBinary II/III CRC of bytes 0 to 123 or, for MacBinary I,
// zeros in bytes 99 to 125), so the sniffer can try it on any file. The entry path is the file's own name (Mac Roman to
// UTF-8, '/' becoming ':'); the fork bytes are streamed from the Blob.
#include <cstring>

#include "macroman.h"
#include "range_stream.h"
#include "source.h"

namespace tpw::assets {

namespace {

uint32_t B32(const uint8_t* p) {
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | uint32_t(p[3]);
}

// CRC-16/XMODEM (polynomial 0x1021, initial value 0), the checksum of MacBinary II and III.
uint16_t Crc16(const uint8_t* p, size_t n) {
    uint16_t crc = 0;
    for (size_t i = 0; i < n; i++) {
        crc = uint16_t(crc ^ (uint16_t(p[i]) << 8));
        for (int b = 0; b < 8; b++) crc = (crc & 0x8000) ? uint16_t((crc << 1) ^ 0x1021) : uint16_t(crc << 1);
    }
    return crc;
}

class MacBinarySource : public Source {
public:
    const char* Kind() const override { return "macbinary"; }
    const std::vector<Entry>& Entries() const override { return entries_; }
    std::unique_ptr<Stream> Open(const Entry& entry, std::string& err) override {
        if (entry.id > 1) {
            err = "not an entry of this MacBinary file";
            return nullptr;
        }
        uint64_t offset = entry.id == 0 ? dataOffset_ : rsrcOffset_;
        uint64_t size = entry.id == 0 ? dataLen_ : rsrcLen_;
        try {
            return std::make_unique<RangeStream>(blob_, std::vector<ByteRange>{{offset, size}}, size);
        } catch (...) {
            err = "out of memory";
            return nullptr;
        }
    }

    bool Load(std::shared_ptr<Blob> blob, std::string& err) {
        blob_ = std::move(blob);
        uint8_t h[128];
        int64_t got = blob_->ReadAt(0, h, sizeof h, err);
        if (got < 0) return false;
        if (got < 128) {
            err = "macbinary: shorter than the 128-byte header";
            return false;
        }
        if (h[0] != 0 || h[74] != 0 || h[82] != 0 || h[1] < 1 || h[1] > 63) {
            err = "macbinary: not a MacBinary header";
            return false;
        }
        for (int i = 0; i < h[1]; i++) {
            if (h[2 + i] < 0x20 || h[2 + i] == 0x7F) {
                err = "macbinary: the file name holds a control character";
                return false;
            }
        }
        bool crcOk = Crc16(h, 124) == uint16_t((h[124] << 8) | h[125]);
        bool v1 = true;  // MacBinary I leaves bytes 99 to 125 zero
        for (int i = 99; i <= 125; i++) v1 = v1 && h[i] == 0;
        if (!crcOk && !v1) {
            err = "macbinary: the header checksum does not match";
            return false;
        }
        dataLen_ = B32(h + 83);
        rsrcLen_ = B32(h + 87);
        if (dataLen_ > 0x7FFFFFFF || rsrcLen_ > 0x7FFFFFFF) {
            err = "macbinary: a fork size is out of range";
            return false;
        }
        uint64_t secondary = (uint64_t(h[120]) << 8) | h[121];  // MacBinary II: a secondary header, skipped
        dataOffset_ = 128 + ((secondary + 127) & ~uint64_t(127));
        uint64_t dataPadded = (dataLen_ + 127) & ~uint64_t(127);
        rsrcOffset_ = dataOffset_ + dataPadded;
        // The last fork may lack its padding.
        uint64_t end = rsrcLen_ > 0 ? rsrcOffset_ + rsrcLen_ : dataOffset_ + dataLen_;
        if (end > blob_->Size()) {
            err = "macbinary: the forks run past the end of the file";
            return false;
        }
        // Without the MacBinary II checksum the header proves little: the file must then be exactly as long as its forks.
        if (!crcOk && blob_->Size() > ((end + 127) & ~uint64_t(127))) {
            err = "macbinary: no checksum and the file is longer than its forks";
            return false;
        }
        std::string name = MacRomanToUtf8(reinterpret_cast<const char*>(h + 2), h[1], true);
        Entry e;
        e.path = name;
        e.size = dataLen_;
        e.kind = EntryKind::Data;
        e.id = 0;
        e.hasFinderInfo = true;
        std::memcpy(e.finderType, h + 65, 4);  // the Finder info's type and creator
        std::memcpy(e.finderCreator, h + 69, 4);
        entries_.push_back(e);
        if (rsrcLen_ > 0) {
            e.size = rsrcLen_;
            e.kind = EntryKind::Rsrc;
            e.hasFinderInfo = false;
            e.id = 1;
            entries_.push_back(e);
        }
        return true;
    }

private:
    std::shared_ptr<Blob> blob_;
    uint64_t dataLen_ = 0, rsrcLen_ = 0, dataOffset_ = 0, rsrcOffset_ = 0;
    std::vector<Entry> entries_;
};

}  // namespace

std::unique_ptr<Source> OpenMacBinary(std::shared_ptr<Blob> blob, std::string& err) {
    if (!blob) {
        err = "no blob";
        return nullptr;
    }
    try {
        auto s = std::make_unique<MacBinarySource>();
        if (!s->Load(std::move(blob), err)) return nullptr;
        return s;
    } catch (...) {
        err = "out of memory";
        return nullptr;
    }
}

}  // namespace tpw::assets
