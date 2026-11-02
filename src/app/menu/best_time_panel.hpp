#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

#include "app/hud/race_time_text.hpp"
#include "core/duration.hpp"
#include "settings/progress.hpp"

namespace rr::app {

struct BestTimeRows {
    std::u16string_view type;
    std::u16string_view name;
    std::u16string_view time;
    std::u16string_view status;
};

/// The time trial screen's saved best lap and total, turning between them every 2.5 s.
class BestTimePanel {
public:
    /// Starts on the best lap. `hasWon` is whether the race's record has fallen.
    void Show(const settings::TrackRecords& records, bool hasWon);

    void Tick(Milliseconds elapsed);

    /// The rows view this panel and `text`, so they last until the next Show.
    template <typename Text>
    [[nodiscard]] BestTimeRows Rows(const Text& text) const {
        const Record& record = shown_ < kTurnTime ? lap_ : race_;
        return {
            .type = text.Line(record.typeLine),
            .name = record.isSet ? std::u16string_view(record.holder) : text.Line(record.noneLine),
            .time = std::u16string_view(record.time.data(), record.timeLength),
            .status = hasWon_ ? text.Line(kWonLine) : std::u16string_view()};
    }

private:
    static constexpr Milliseconds kTurnTime{2500};
    static constexpr std::size_t kWonLine = 0x49;

    struct Record {
        std::size_t typeLine = 0;
        std::size_t noneLine = 0;
        bool isSet = false;
        std::u16string holder;
        std::array<char16_t, kRaceTimeLength> time{};
        std::size_t timeLength = 0;

        void Set(const std::optional<settings::BestTime>& best);
    };

    Record lap_{.typeLine = 0x46, .noneLine = 0xBE};
    Record race_{.typeLine = 0x47, .noneLine = 0xBF};
    bool hasWon_ = false;
    Milliseconds shown_{0};
};

}
