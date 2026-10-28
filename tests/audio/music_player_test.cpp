#include "audio/music_player.hpp"

#include <doctest/doctest.h>

#include <format>
#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "audio/ramp_stream.hpp"

namespace rr::audio {
namespace {

constexpr std::uint32_t kRateHz = 22050;
constexpr std::uint64_t kTrackFrames = 64;

class FakeSource final : public MusicSource {
public:
    FakeSource(std::string name, std::optional<ErrorCode> failure, std::vector<std::string>& opened)
        : name_(std::move(name)), failure_(failure), opened_(&opened) {}

    [[nodiscard]] std::string Name() const override { return name_; }

    [[nodiscard]] Result<MusicTrack> Open(std::string_view id) const override {
        opened_->push_back(std::format("{}:{}", name_, id));
        if (failure_.has_value()) {
            return Fail(*failure_, std::format("{} can't open {}", name_, id));
        }
        return MusicTrack{.stream = std::make_unique<test::RampStream>(kTrackFrames), .loop = {}};
    }

private:
    std::string name_;
    std::optional<ErrorCode> failure_;
    std::vector<std::string>* opened_;
};

class PlayerFixture {
public:
    PlayerFixture() : mixer_(CreateMixer()) {}

    MusicPlayer Player(
        std::initializer_list<std::pair<std::string, std::optional<ErrorCode>>> sources) {
        std::vector<std::unique_ptr<MusicSource>> chain;
        for (const auto& [name, failure] : sources) {
            chain.push_back(std::make_unique<FakeSource>(name, failure, opened_));
        }
        return {mixer_, std::move(chain)};
    }

protected:
    static Mixer CreateMixer() {
        Result<Mixer> mixer = Mixer::CreateHeadless(kRateHz);
        REQUIRE(mixer.has_value());
        return std::move(mixer).value();
    }

    Mixer mixer_;
    std::vector<std::string> opened_;
};

TEST_CASE_FIXTURE(PlayerFixture, "the first source with the track plays it") {
    MusicPlayer player = Player({{"mods", std::nullopt}, {"original", std::nullopt}});

    REQUIRE(player.Play("circuit1").has_value());

    CHECK(player.Current() == "circuit1");
    CHECK(opened_ == std::vector<std::string>{"mods:circuit1"});
}

TEST_CASE_FIXTURE(PlayerFixture, "a missing or broken track falls back to the next source") {
    MusicPlayer player = Player({{"missing", ErrorCode::NotFound},
                                 {"broken", ErrorCode::InvalidData},
                                 {"original", std::nullopt}});

    REQUIRE(player.Play("circuit1").has_value());

    CHECK(opened_ ==
          std::vector<std::string>{"missing:circuit1", "broken:circuit1", "original:circuit1"});
}

TEST_CASE_FIXTURE(PlayerFixture, "a track no source has is not found") {
    MusicPlayer player = Player({{"missing", ErrorCode::NotFound}, {"broken", ErrorCode::Io}});

    const Result<void> played = player.Play("circuit1");

    REQUIRE_FALSE(played.has_value());
    CHECK(played.error().code == ErrorCode::NotFound);
    CHECK(player.Current().empty());
}

TEST_CASE_FIXTURE(PlayerFixture, "replaying the current track asks no source") {
    MusicPlayer player = Player({{"original", std::nullopt}});
    REQUIRE(player.Play("circuit1").has_value());

    REQUIRE(player.Play("circuit1").has_value());

    CHECK(opened_.size() == 1);
}

TEST_CASE_FIXTURE(PlayerFixture, "a track id that isn't a file name is rejected") {
    MusicPlayer player = Player({{"original", std::nullopt}});

    CHECK(player.Play("../circuit1").error().code == ErrorCode::InvalidData);
    CHECK(opened_.empty());
}

TEST_CASE_FIXTURE(PlayerFixture, "stopping clears the current track") {
    MusicPlayer player = Player({{"original", std::nullopt}});
    REQUIRE(player.Play("circuit1").has_value());

    player.Stop();

    CHECK(player.Current().empty());
}

}
}
