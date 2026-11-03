#include "app/menu/licence_menu.hpp"

namespace rr::app {
namespace {

constexpr int kRows = static_cast<int>(LicenceRow::Back) + 1;

LicenceRow Stepped(LicenceRow row, int step) {
    return static_cast<LicenceRow>((static_cast<int>(row) + step + kRows) % kRows);
}

}

LicenceRequest LicenceMenu::Tick(const sim::InputFrame& input) {
    if (input.WasPressed(sim::MenuAction::Back)) {
        return LicenceRequest::Leave;
    }
    if (input.WasPressed(sim::MenuAction::Up)) {
        focus_ = Stepped(focus_, -1);
    }
    if (input.WasPressed(sim::MenuAction::Down)) {
        focus_ = Stepped(focus_, 1);
    }
    if (!input.WasPressed(sim::MenuAction::Confirm)) {
        return LicenceRequest::None;
    }
    switch (focus_) {
        case LicenceRow::Expression:
            return LicenceRequest::Expression;
        case LicenceRow::Done:
            return LicenceRequest::Done;
        case LicenceRow::Back:
            return LicenceRequest::Leave;
        case LicenceRow::Name:
            return LicenceRequest::None;
    }
    return LicenceRequest::None;
}

}
