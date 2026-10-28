#include "audio/music_catalog.hpp"

#include <algorithm>
#include <cctype>
#include <utility>

namespace rr::audio {
namespace {

constexpr std::string_view kTrackExtension = ".TUN";

std::string Lowercase(std::string_view text) {
    std::string lower(text);
    std::ranges::transform(lower, lower.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return lower;
}

}

Result<MusicCatalog> MusicCatalog::Scan(const assets::Vfs& vfs) {
    Result<std::vector<assets::VfsEntry>> root = vfs.List("");
    if (!root) {
        return std::unexpected(std::move(root.error()));
    }
    std::vector<std::string> ids;
    for (const assets::VfsEntry& entry : *root) {
        if (entry.kind == assets::EntryKind::File && entry.name.ends_with(kTrackExtension)) {
            ids.push_back(Lowercase(std::string_view(entry.name)
                                        .substr(0, entry.name.size() - kTrackExtension.size())));
        }
    }
    return MusicCatalog(std::move(ids));
}

bool MusicCatalog::Contains(std::string_view id) const {
    return std::ranges::find(ids_, Lowercase(id)) != ids_.end();
}

}
