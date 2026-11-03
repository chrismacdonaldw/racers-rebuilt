#include "sim/race/lap_zones.hpp"

namespace rr::sim {

LapZones::LapZones(bool keepsOneZone)
    : keepsOneZone_(keepsOneZone),
      history_{keepsOneZone ? std::array{Kind::Open, Kind::Open, Kind::Open}
                            : std::array{Kind::Open, Kind::PostLine, Kind::Finish}} {}

bool LapZones::Enter(assets::LapZone zone) {
    Kind kind = Kind::Finish;
    if (zone == assets::LapZone::OpenTrack) {
        kind = Kind::Open;
    } else if (zone == assets::LapZone::PostLine) {
        kind = Kind::PostLine;
    }
    return keepsOneZone_ ? EnterState(kind) : EnterHistory(kind);
}

void LapZones::Reverse() {
    if (!keepsOneZone_) {
        history_ = {Kind::Open, Kind::PostLine, Kind::PostLine};
    }
}

bool LapZones::EnterHistory(Kind kind) {
    if (history_[0] == kind) {
        return false;
    }
    const bool completesLap =
        kind == Kind::Finish && history_ == std::array{Kind::Open, Kind::PostLine, Kind::Finish};
    history_ = {kind, history_[0], history_[1]};
    return completesLap;
}

bool LapZones::EnterState(Kind kind) {
    Kind& state = history_[0];
    if (kind == Kind::Open) {
        if (state == Kind::PostLine) {
            state = Kind::Open;
        } else if (state == Kind::Finish) {
            state = Kind::Blocked;
        }
        return false;
    }
    if (kind == Kind::PostLine) {
        if (state == Kind::Finish) {
            state = Kind::PostLine;
        }
        return false;
    }
    if (state == Kind::Finish) {
        return false;
    }
    const bool completesLap = state == Kind::Open;
    state = Kind::Finish;
    return completesLap;
}

}
