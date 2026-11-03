#include "app/sound_banks.hpp"

#include <algorithm>
#include <cctype>
#include <format>
#include <iterator>
#include <utility>

#include "assets/game_path.hpp"
#include "assets/sound_list.hpp"
#include "assets/track_loader.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kAmbientPrefix = "LEV";

Result<assets::SoundList> ReadList(const assets::Vfs& vfs, std::string_view path) {
    return vfs.Read(path).and_then(
        [](const assets::FileData& file) { return assets::ParseSoundList(file.Bytes()); });
}

std::string_view FolderOf(std::string_view path) {
    const std::size_t slash = path.rfind('/');
    return slash == std::string_view::npos ? std::string_view{} : path.substr(0, slash + 1);
}

std::string MusicId(std::string_view name) {
    std::string id(name.substr(0, name.rfind('.')));
    std::ranges::transform(id, id.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return id;
}

}

Result<std::span<const audio::SoundId>> SoundBanks::Load(audio::Mixer& mixer,
                                                         const assets::Vfs& vfs,
                                                         std::string_view path) {
    if (const auto bank = std::ranges::find(banks_, path, &Bank::path); bank != banks_.end()) {
        return bank->sounds;
    }
    Result<assets::SoundList> list = ReadList(vfs, path);
    if (!list) {
        return Fail(list.error().code, std::format("{}: {}", path, list.error().message));
    }
    std::vector<audio::SoundId> sounds;
    sounds.reserve(list->names.size());
    for (const std::string& name : list->names) {
        Result<audio::SoundId> sound =
            assets::NormalizeGamePath(std::format("{}{}{}", FolderOf(path), list->folder, name))
                .and_then([&](const std::string& file) { return vfs.Read(file); })
                .and_then([&](assets::FileData file) { return mixer.AddSound(std::move(file)); });
        if (!sound) {
            return Fail(sound.error().code,
                        std::format("{}: {}: {}", path, name, sound.error().message));
        }
        sounds.push_back(*sound);
    }
    return banks_.emplace_back(std::string(path), std::move(sounds)).sounds;
}

std::string AmbientBankPath(std::string_view track) {
    const std::string upper = assets::ToUpperAscii(track);
    const std::string_view stem = assets::RaceTrackStem(upper);
    const std::string bank =
        stem.size() == upper.size() ? upper : std::format("{}{}", kAmbientPrefix, stem);
    return std::format("GAMEDATA/{}/{}.SBK", upper, bank);
}

Result<std::vector<std::string>> LoadMusicList(const assets::Vfs& vfs, std::string_view track) {
    return ReadList(vfs, std::format("GAMEDATA/{}/LEGOMSC", assets::ToUpperAscii(track)))
        .transform([](const assets::SoundList& list) {
            std::vector<std::string> ids;
            ids.reserve(list.names.size());
            std::ranges::transform(list.names, std::back_inserter(ids), MusicId);
            return ids;
        });
}

}
