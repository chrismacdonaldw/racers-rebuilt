#include "app/menu/best_time_panel.hpp"

#include "core/utf16_text.hpp"

namespace rr::app {

void BestTimePanel::Record::Set(const std::optional<settings::BestTime>& best) {
    isSet = best.has_value();
    holder = best ? Utf16FromUtf8(best->holder) : std::u16string();
    timeLength = best ? WriteRaceTime(Milliseconds{best->ms}, time).size() : 0;
}

void BestTimePanel::Show(const settings::TrackRecords& records, bool hasWon) {
    lap_.Set(records.lap);
    race_.Set(records.race);
    hasWon_ = hasWon;
    shown_ = Milliseconds{0};
}

void BestTimePanel::Tick(Milliseconds elapsed) {
    shown_ = (shown_ + elapsed) % (2 * kTurnTime);
}

}
