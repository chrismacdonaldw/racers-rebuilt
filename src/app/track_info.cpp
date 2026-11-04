#include "app/track_info.hpp"

#include <cstdint>
#include <format>
#include <span>
#include <utility>

#include "app/race_scene_name.hpp"
#include "assets/game_path.hpp"
#include "assets/race_catalog.hpp"
#include "assets/track_loader.hpp"
#include "core/log.hpp"
#include "settings/toml_file.hpp"

namespace rr::app {
namespace {

constexpr std::size_t kRaceMusicSlot = 1;
constexpr std::size_t kFirstExtraMusicSlot = 4;

std::string InfoPath(std::string_view track) {
    return std::format("GAMEDATA/{}/TRACK.TOML", assets::ToUpperAscii(track));
}

class InfoReader {
public:
    explicit InfoReader(const toml::table& table) : table_(&table) {}

    template <typename T>
    std::optional<T> Get(std::string_view key, std::string_view kind) {
        const toml::node* node = table_->get(key);
        if (node == nullptr) {
            return std::nullopt;
        }
        std::optional<T> value = node->value_exact<T>();
        if (!value) {
            Complain(std::format("{} must be {}", key, kind));
        }
        return value;
    }

    std::vector<std::string> Music() {
        std::vector<std::string> music;
        const toml::node* node = table_->get("music");
        if (node == nullptr) {
            return music;
        }
        const toml::array* ids = node->as_array();
        if (ids == nullptr || !ids->is_homogeneous(toml::node_type::string)) {
            Complain("music must be a list of music ids");
            return music;
        }
        ids->for_each([&](const toml::value<std::string>& id) { music.push_back(id.get()); });
        return music;
    }

    void Complain(std::string problem) {
        if (!problem_) {
            problem_ = std::move(problem);
        }
    }

    [[nodiscard]] const std::optional<std::string>& Problem() const { return problem_; }

private:
    const toml::table* table_;
    std::optional<std::string> problem_;
};

Result<TrackInfo> ReadInfo(const toml::table& table, std::string_view track) {
    InfoReader reader(table);
    const std::string folder = std::format("GAMEDATA/{}", assets::ToUpperAscii(track));
    TrackInfo info{
        .name = reader.Get<std::string>("name", "text").value_or(std::string()),
        .preview =
            reader.Get<std::string>("preview", "text").transform([&](const std::string& file) {
                return assets::JoinGamePath(folder, file);
            }),
        .lapCount = std::nullopt,
        .grid = reader.Get<std::string>("grid", "text"),
        .hasTimeTrial = reader.Get<bool>("time_trial", "true or false").value_or(false),
        .hasMirrored = reader.Get<bool>("mirrored", "true or false").value_or(false),
        .music = reader.Music()};
    if (const std::optional<std::int64_t> laps =
            reader.Get<std::int64_t>("laps", "a whole number")) {
        if (*laps < 1 || *laps > kMaxLapCount) {
            reader.Complain(std::format("laps must be 1 to {}", kMaxLapCount));
        }
        info.lapCount = static_cast<int>(*laps);
    }
    if (reader.Problem()) {
        return Fail(ErrorCode::InvalidData, *reader.Problem());
    }
    return info;
}

std::optional<std::string> InfoProblem(const assets::Vfs& vfs, const assets::RaceCatalog* catalog,
                                       std::string_view track) {
    const Result<std::optional<TrackInfo>> info = ReadTrackInfo(vfs, track);
    if (!info) {
        return info.error().message;
    }
    if (!*info) {
        return std::nullopt;
    }
    const TrackInfo& read = **info;
    if (read.grid && catalog != nullptr && !catalog->FindCircuit(*read.grid)) {
        return std::format("{}: grid names no circuit {}", InfoPath(track), *read.grid);
    }
    if (read.preview && !vfs.Contains(*read.preview)) {
        return std::format("{}: preview {} is missing", InfoPath(track), *read.preview);
    }
    return std::nullopt;
}

}

Result<TrackInfo> ParseTrackInfo(std::string_view text, std::string_view track,
                                 std::string_view sourceName) {
    const Result<toml::table> table = settings::ParseToml(text, sourceName);
    if (!table) {
        return std::unexpected(table.error());
    }
    Result<TrackInfo> info = ReadInfo(*table, track);
    if (!info) {
        return Fail(info.error().code, std::format("{}: {}", sourceName, info.error().message));
    }
    return info;
}

Result<std::optional<TrackInfo>> ReadTrackInfo(const assets::Vfs& vfs, std::string_view track) {
    const std::string path = InfoPath(track);
    const Result<assets::FileData> file = vfs.Read(path);
    if (!file) {
        if (file.error().code == ErrorCode::NotFound) {
            return std::optional<TrackInfo>();
        }
        return std::unexpected(file.error());
    }
    const std::span<const std::byte> bytes = file->Bytes();
    return ParseTrackInfo(
               std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size()), track,
               path)
        .transform([](TrackInfo info) { return std::optional(std::move(info)); });
}

std::optional<TrackInfo> FindTrackInfo(const assets::Vfs& vfs, std::string_view track) {
    Result<std::optional<TrackInfo>> info = ReadTrackInfo(vfs, track);
    if (!info) {
        log::Warn("{}, using the track's defaults", info.error().message);
        return std::nullopt;
    }
    return std::move(*info);
}

std::vector<std::string> WithTrackMusic(std::vector<std::string> list,
                                        const std::vector<std::string>& music) {
    if (music.empty()) {
        return list;
    }
    list.resize(kFirstExtraMusicSlot);
    list[kRaceMusicSlot] = music.front();
    list.insert(list.end(), music.begin() + 1, music.end());
    return list;
}

std::vector<std::string> TrackInfoProblems(const assets::Vfs& vfs) {
    const Result<assets::RaceCatalog> catalog = assets::RaceCatalog::Load(vfs);
    std::vector<std::string> problems;
    for (const std::string& track : assets::ListRaceTracks(vfs)) {
        if (std::optional<std::string> problem =
                InfoProblem(vfs, catalog ? &*catalog : nullptr, track)) {
            problems.push_back(std::move(*problem));
        }
    }
    return problems;
}

}
