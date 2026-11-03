#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <filesystem>
#include <format>
#include <fstream>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "app/particle_assets.hpp"
#include "app/sound_banks.hpp"
#include "assets/folder_layer.hpp"
#include "assets/jam_archive.hpp"
#include "assets/loader_pool.hpp"
#include "assets/track_loader.hpp"
#include "assets/vfs.hpp"
#include "game_files.hpp"
#include "sim/race/race_track.hpp"
#include "temp_folder.hpp"

namespace rr::mods {
namespace {

constexpr std::string_view kRetailTrack = "RACEC0R0";
constexpr std::string_view kModTrack = "MODTRACK";
constexpr std::size_t kRetailTrackCount = 13;

struct Rename {
    std::string_view from;
    std::string_view to;
};

constexpr std::array kRenames{Rename{.from = "RACEC0R0", .to = "MODTRACK"},
                              Rename{.from = "LEVC0R0", .to = "MODTRACK"}};

void Touch(const std::filesystem::path& path) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary).put('\0');
}

void Mount(assets::Vfs& vfs, const std::filesystem::path& root) {
    Result<assets::FolderLayer> layer = assets::FolderLayer::Scan(root);
    REQUIRE(layer.has_value());
    vfs.Mount(std::make_unique<assets::FolderLayer>(std::move(*layer)));
}

std::string Renamed(std::string name) {
    for (const Rename& rename : kRenames) {
        if (name.starts_with(rename.from)) {
            return std::string(rename.to) + name.substr(rename.from.size());
        }
    }
    return name;
}

void CopyRetailTrack(const assets::Vfs& vfs, const std::filesystem::path& root) {
    const std::string from = std::format("GAMEDATA/{}", kRetailTrack);
    const std::filesystem::path to = root / "GAMEDATA" / kModTrack;
    std::filesystem::create_directories(to);
    for (const assets::VfsEntry& entry : vfs.List(from).value_or(std::vector<assets::VfsEntry>{})) {
        if (entry.kind != assets::EntryKind::File) {
            continue;
        }
        const Result<assets::FileData> file = vfs.Read(std::format("{}/{}", from, entry.name));
        REQUIRE(file.has_value());
        const std::span<const std::byte> bytes = file->Bytes();
        std::ofstream(to / Renamed(entry.name), std::ios::binary)
            .write(reinterpret_cast<const char*>(bytes.data()), std::ssize(bytes));
    }
}

template <typename T>
std::string Problem(const Result<T>& result) {
    return result ? std::string() : result.error().message;
}

std::optional<assets::Vfs> RetailVfs() {
    const std::optional<std::filesystem::path> gameFolder = test::GameFolder();
    if (!gameFolder) {
        return std::nullopt;
    }
    Result<assets::JamArchive> archive = assets::JamArchive::Open(*gameFolder / "LEGO.JAM");
    if (!archive) {
        return std::nullopt;
    }
    assets::Vfs vfs;
    vfs.Mount(std::make_unique<assets::JamArchive>(std::move(*archive)));
    return vfs;
}

std::string ModTrackProblems(const assets::Vfs& vfs) {
    assets::LoaderPool pool(1);
    const Result<assets::TrackAssets> model =
        assets::TrackLoader(vfs, pool).Load(std::format("GAMEDATA/{}", kModTrack));
    const bool hasTextures =
        model.transform([](const auto& assets) { return assets.missingTextures.empty(); })
            .value_or(true);
    const Result<app::ParticleSources> particles =
        app::TrackParticleFiles(vfs, kModTrack).and_then([&](const app::ParticleFiles& files) {
            return app::ReadParticleSources(vfs, files);
        });
    return Problem(model) + (hasTextures ? "" : "missing textures") +
           Problem(sim::LoadRaceTrack(vfs, kModTrack)) + Problem(particles) +
           Problem(vfs.Read(app::AmbientBankPath(kModTrack)));
}

TEST_CASE("a folder with a start grid and checkpoints is a race track, whatever its name") {
    const test::TempFolder folder;
    Touch(folder.Path() / "GAMEDATA" / "loop" / "STARTPOS.SPB");
    Touch(folder.Path() / "GAMEDATA" / "loop" / "RACE.CPB");
    Touch(folder.Path() / "GAMEDATA" / "ARENA" / "STARTPOS.SPB");
    Touch(folder.Path() / "GAMEDATA" / "ARENA" / "RACE.CPB");
    Touch(folder.Path() / "GAMEDATA" / "TEST" / "STARTPOS.SPB");
    Touch(folder.Path() / "GAMEDATA" / "COMMON" / "RACE.CPB");
    assets::Vfs vfs;
    Mount(vfs, folder.Path());

    CHECK(assets::ListRaceTracks(vfs) == std::vector<std::string>{"ARENA", "LOOP"});
}

TEST_CASE("a race track's files reuse its name, or what follows RACE") {
    CHECK(assets::RaceTrackStem("RACEC2R1") == "C2R1");
    CHECK(assets::RaceTrackStem("TEST") == "TEST");
    CHECK(app::AmbientBankPath("MODTRACK") == "GAMEDATA/MODTRACK/MODTRACK.SBK");
}

TEST_CASE("a retail track copied under a new name loads as a mod track") {
    std::optional<assets::Vfs> vfs = RetailVfs();
    if (!vfs) {
        MESSAGE("RR_GAME_PATH isn't set, skipping");
        return;
    }
    const test::TempFolder mod;
    CopyRetailTrack(*vfs, mod.Path());
    Mount(*vfs, mod.Path());

    const std::vector<std::string> tracks = assets::ListRaceTracks(*vfs);

    CHECK(tracks.size() == kRetailTrackCount + 1);
    CHECK(std::ranges::contains(tracks, std::string(kModTrack)));
    CHECK(ModTrackProblems(*vfs) == std::string());
}

}
}
