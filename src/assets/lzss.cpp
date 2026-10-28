#include "assets/lzss.hpp"

#include <cstdint>
#include <format>
#include <utility>

#include "assets/byte_reader.hpp"

namespace rr::assets {
namespace {

constexpr std::size_t kFlagBits = 8;
constexpr std::size_t kMatchLengthBase = 18;
constexpr std::uint8_t kLengthMask = 0x0F;
constexpr std::uint8_t kDistanceHighMask = 0xF0;
constexpr int kDistanceHighShift = 4;

class LzssDecoder {
public:
    LzssDecoder(std::span<const std::byte> packed, std::span<std::byte> unpacked)
        : in_(packed, 0), out_(unpacked) {}

    Result<void> Decode() {
        Result<void> first = CopyLiteral();
        while (first) {
            Result<std::uint8_t> flags = in_.ReadU8();
            if (!flags) {
                return std::unexpected(std::move(flags.error()));
            }
            for (std::size_t bit = kFlagBits; bit-- > 0;) {
                const bool isMatch = ((*flags >> bit) & 1U) != 0;
                Result<bool> more =
                    isMatch ? CopyMatch() : CopyLiteral().transform([] { return true; });
                if (!more) {
                    return std::unexpected(std::move(more.error()));
                }
                if (!*more) {
                    return Finish();
                }
            }
        }
        return first;
    }

private:
    Result<void> CopyLiteral() {
        return in_.ReadU8().and_then([this](std::uint8_t value) -> Result<void> {
            if (written_ == out_.size()) {
                return Overrun();
            }
            out_[written_++] = std::byte{value};
            return {};
        });
    }

    Result<bool> CopyMatch() {
        Result<std::uint8_t> head = in_.ReadU8();
        Result<std::uint8_t> low = head.and_then([this](std::uint8_t) { return in_.ReadU8(); });
        if (!low) {
            return std::unexpected(std::move(low.error()));
        }
        const std::size_t distance =
            (static_cast<std::size_t>(*head & kDistanceHighMask) << kDistanceHighShift) | *low;
        if (distance == 0) {
            return false;
        }
        Result<std::size_t> length = MatchLength(*head & kLengthMask);
        if (!length) {
            return std::unexpected(std::move(length.error()));
        }
        if (distance > written_) {
            return Fail(ErrorCode::InvalidData,
                        std::format("LZSS match reaches {} bytes back with only {} written",
                                    distance, written_));
        }
        if (*length > out_.size() - written_) {
            return Overrun();
        }
        for (std::size_t i = 0; i < *length; ++i, ++written_) {
            out_[written_] = out_[written_ - distance];
        }
        return true;
    }

    Result<std::size_t> MatchLength(std::uint8_t code) {
        if (code != 0) {
            return kMatchLengthBase - code;
        }
        return in_.ReadU8().transform([](std::uint8_t extra) { return kMatchLengthBase + extra; });
    }

    [[nodiscard]] Result<void> Finish() const {
        if (written_ != out_.size()) {
            return Fail(
                ErrorCode::InvalidData,
                std::format("LZSS stream ended after {} of {} bytes", written_, out_.size()));
        }
        return {};
    }

    [[nodiscard]] std::unexpected<Error> Overrun() const {
        return Fail(ErrorCode::InvalidData,
                    std::format("LZSS stream unpacks past {} bytes", out_.size()));
    }

    ByteReader in_;
    std::span<std::byte> out_;
    std::size_t written_ = 0;
};

}

Result<void> DecodeLzss(std::span<const std::byte> packed, std::span<std::byte> unpacked) {
    return LzssDecoder(packed, unpacked).Decode();
}

}
