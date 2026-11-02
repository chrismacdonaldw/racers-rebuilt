#include "app/menu/best_time_panel.hpp"

#include <doctest/doctest.h>

#include <cstddef>
#include <map>
#include <string>
#include <string_view>

namespace rr::app {
namespace {

constexpr Milliseconds kTurn{2500};

struct Lines {
    std::map<std::size_t, std::u16string> text{
        {0x46, u"46"}, {0x47, u"47"}, {0x49, u"49"}, {0xBE, u"BE"}, {0xBF, u"BF"}};

    [[nodiscard]] std::u16string_view Line(std::size_t line) const {
        const auto found = text.find(line);
        return found == text.end() ? std::u16string_view() : std::u16string_view(found->second);
    }
};

BestTimeRows RowsOf(const BestTimePanel& panel) {
    static const Lines lines;
    return panel.Rows(lines);
}

settings::TrackRecords Records() {
    return {.lap = settings::BestTime{.ms = 31250, .holder = "Basil"},
            .race = settings::BestTime{.ms = 95340,
                                       .holder = "R\xC3\xB8"
                                                 "de"}};
}

TEST_CASE("the panel shows the best lap first and turns to the best total every 2.5 s") {
    BestTimePanel panel;
    panel.Show(Records(), false);

    const BestTimeRows lap = RowsOf(panel);
    panel.Tick(kTurn);
    const BestTimeRows total = RowsOf(panel);
    panel.Tick(kTurn);

    CHECK(lap.type == u"46");
    CHECK(lap.name == u"Basil");
    CHECK(lap.time == u"0:31:25");
    CHECK(total.type == u"47");
    CHECK(total.name == u"Røde");
    CHECK(total.time == u"1:35:34");
    CHECK(RowsOf(panel).type == u"46");
}

TEST_CASE("a new race starts the panel over on its best lap") {
    BestTimePanel panel;
    panel.Show(Records(), false);
    panel.Tick(kTurn);

    panel.Show(Records(), false);

    CHECK(RowsOf(panel).type == u"46");
}

TEST_CASE("without a record the panel names nobody and shows no time") {
    BestTimePanel panel;
    panel.Show({}, false);

    const BestTimeRows lap = RowsOf(panel);
    panel.Tick(kTurn);
    const BestTimeRows total = RowsOf(panel);

    CHECK(lap.name == u"BE");
    CHECK(lap.time.empty());
    CHECK(total.name == u"BF");
    CHECK(lap.status.empty());
}

TEST_CASE("a race whose record has fallen says TIME TRIAL WON") {
    BestTimePanel panel;
    panel.Show(Records(), true);

    CHECK(RowsOf(panel).status == u"49");
}

}
}
