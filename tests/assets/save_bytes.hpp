#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace rr::test {

inline constexpr std::size_t kSaveHeaderSize = 0x477;
inline constexpr std::size_t kSaveRecordSize = 0x22D;

class SaveBytes {
public:
    SaveBytes() : header_(kSaveHeaderSize) {
        header_[0] = std::byte{'L'};
        header_[1] = std::byte{'R'};
        SetU16(header_, 3, 0x0480);
    }

    SaveBytes& Byte(std::size_t offset, std::uint8_t value) {
        header_[offset] = std::byte{value};
        return *this;
    }

    SaveBytes& U16(std::size_t offset, std::uint16_t value) {
        SetU16(header_, offset, value);
        return *this;
    }

    SaveBytes& Time(std::size_t offset, std::uint32_t ms, std::u16string_view holder) {
        SetU16(header_, offset, static_cast<std::uint16_t>(ms & 0xFFFFU));
        SetU16(header_, offset + 2, static_cast<std::uint16_t>(ms >> 16U));
        for (std::size_t unit = 0; unit < holder.size(); ++unit) {
            SetU16(header_, offset + 4 + (unit * 2), holder[unit]);
        }
        return *this;
    }

    SaveBytes& Driver(std::uint16_t trophies) {
        std::vector<std::byte>& record = records_.emplace_back(kSaveRecordSize);
        SetU16(record, 0x22B, trophies);
        header_[2] = std::byte{static_cast<std::uint8_t>(records_.size())};
        return *this;
    }

    SaveBytes& Racer(std::u16string_view name, std::array<std::uint8_t, 5> look) {
        std::vector<std::byte>& record = records_.back();
        for (std::size_t unit = 0; unit < name.size(); ++unit) {
            SetU16(record, unit * 2, name[unit]);
        }
        for (std::size_t field = 0; field < look.size(); ++field) {
            record[0x1C + field] = std::byte{look.at(field)};
        }
        return *this;
    }

    [[nodiscard]] std::vector<std::byte> File() const {
        std::vector<std::byte> file;
        AppendBlocks(file, header_);
        for (const std::vector<std::byte>& record : records_) {
            AppendBlocks(file, record);
        }
        return file;
    }

private:
    static void SetU16(std::vector<std::byte>& data, std::size_t offset, std::uint16_t value) {
        data[offset] = std::byte{static_cast<std::uint8_t>(value & 0xFFU)};
        data[offset + 1] = std::byte{static_cast<std::uint8_t>(value >> 8U)};
    }

    static void AppendBlocks(std::vector<std::byte>& file, std::span<const std::byte> data) {
        constexpr std::size_t kBlockData = 127;
        for (std::size_t start = 0; start < data.size(); start += kBlockData) {
            unsigned sum = 0;
            for (std::size_t at = start; at < start + kBlockData; ++at) {
                const std::byte value = at < data.size() ? data[at] : std::byte{0};
                sum += std::to_integer<unsigned>(value);
                file.push_back(value);
            }
            file.push_back(std::byte{static_cast<std::uint8_t>(sum & 0xFFU)});
        }
    }

    std::vector<std::byte> header_;
    std::vector<std::vector<std::byte>> records_;
};

}
