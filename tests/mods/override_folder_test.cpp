#include "mods/override_folder.hpp"

#include <doctest/doctest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "assets/folder_layer.hpp"
#include "assets/jam_builder.hpp"
#include "assets/vfs.hpp"
#include "settings/registry.hpp"
#include "temp_folder.hpp"

namespace rr::mods {
namespace {

class OverridesFixture {
public:
    OverridesFixture() { registry_.Declare(kOverridesSetting); }

    void WriteText(const std::filesystem::path& relative, std::string_view text) {
        const std::filesystem::path path = folder_.Path() / relative;
        std::filesystem::create_directories(path.parent_path());
        std::ofstream(path, std::ios::binary) << text;
    }

    std::optional<std::uint64_t> MountOverrides() {
        OverrideFolder overrides(folder_.Path() / "overrides");
        overrides.Mount(vfs_, registry_);
        return overrides.Hash();
    }

    std::string Text(std::string_view path) {
        Result<assets::FileData> file = vfs_.Read(path);
        REQUIRE(file.has_value());
        return std::string(test::AsText(file->Bytes()));
    }

protected:
    test::TempFolder folder_;
    settings::Registry registry_;
    assets::Vfs vfs_;
};

TEST_CASE_FIXTURE(OverridesFixture, "overrides replace files from the layers below them") {
    WriteText("base/GAMEDATA/A.TXT", "base");
    WriteText("base/GAMEDATA/B.TXT", "base");
    WriteText("overrides/gamedata/a.txt", "override");
    Result<assets::FolderLayer> base = assets::FolderLayer::Scan(folder_.Path() / "base");
    REQUIRE(base.has_value());
    vfs_.Mount(std::make_unique<assets::FolderLayer>(std::move(*base)));

    CHECK(MountOverrides().has_value());

    CHECK(Text("GAMEDATA/A.TXT") == "override");
    CHECK(Text("GAMEDATA/B.TXT") == "base");
}

TEST_CASE_FIXTURE(OverridesFixture, "turned off or missing, overrides mount nothing") {
    CHECK_FALSE(MountOverrides().has_value());

    WriteText("overrides/GAMEDATA/A.TXT", "override");
    registry_.Set(kOverridesSetting, false);

    CHECK_FALSE(MountOverrides().has_value());
    CHECK(vfs_.Read("GAMEDATA/A.TXT").error().code == ErrorCode::NotFound);
}

TEST_CASE_FIXTURE(OverridesFixture, "the overrides hash follows file names and sizes") {
    WriteText("overrides/GAMEDATA/A.TXT", "one");
    const std::optional<std::uint64_t> first = MountOverrides();
    const std::optional<std::uint64_t> again = MountOverrides();
    WriteText("overrides/GAMEDATA/A.TXT", "three");
    const std::optional<std::uint64_t> resized = MountOverrides();
    WriteText("overrides/GAMEDATA/B.TXT", "three");
    const std::optional<std::uint64_t> added = MountOverrides();

    REQUIRE(first.has_value());
    CHECK(again == first);
    CHECK(resized != first);
    CHECK(added != resized);
}

}
}
