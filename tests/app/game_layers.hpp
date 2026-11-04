#pragma once

#include <doctest/doctest.h>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "assets/folder_layer.hpp"
#include "assets/vfs.hpp"
#include "core/utf8_path.hpp"
#include "image_files.hpp"
#include "temp_folder.hpp"

namespace rr::test {

/// A loose-files layer under a game layer, as the game mounts its folder and its JAM.
class GameLayers {
public:
    static constexpr std::size_t kGameLayer = 1;

    void WriteGame(std::string_view path, const std::vector<std::byte>& bytes) const {
        Write(Game() / PathFromUtf8(path), bytes);
    }
    void WriteLoose(std::string_view path, const std::vector<std::byte>& bytes) const {
        Write(Loose() / PathFromUtf8(path), bytes);
    }
    void WriteGameImage(std::string_view path, const assets::ImageAsset& image) const {
        WriteTga(Game() / PathFromUtf8(path), image);
    }
    void WriteLooseImage(std::string_view path, const assets::ImageAsset& image) const {
        WriteTga(Loose() / PathFromUtf8(path), image);
    }

    [[nodiscard]] std::unique_ptr<assets::Vfs> Mount() const {
        auto vfs = std::make_unique<assets::Vfs>();
        for (const std::filesystem::path& root : {Loose(), Game()}) {
            std::filesystem::create_directories(root);
            Result<assets::FolderLayer> layer = assets::FolderLayer::Scan(root);
            REQUIRE(layer.has_value());
            vfs->Mount(std::make_unique<assets::FolderLayer>(std::move(*layer)));
        }
        return vfs;
    }

    [[nodiscard]] std::filesystem::path Mods() const { return folder_.Path() / "mods"; }

private:
    [[nodiscard]] std::filesystem::path Loose() const { return folder_.Path() / "loose"; }
    [[nodiscard]] std::filesystem::path Game() const { return folder_.Path() / "game"; }

    static void Write(const std::filesystem::path& path, const std::vector<std::byte>& bytes) {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream(path, std::ios::binary)
            .write(reinterpret_cast<const char*>(bytes.data()), std::ssize(bytes));
    }

    TempFolder folder_;
};

}
