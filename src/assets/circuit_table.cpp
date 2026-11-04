#include "assets/circuit_table.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <format>
#include <functional>
#include <iterator>
#include <map>
#include <set>
#include <utility>

#include "assets/game_path.hpp"
#include "assets/race_catalog.hpp"
#include "assets/racer_catalog.hpp"
#include "assets/track_loader.hpp"

#include <toml++/toml.hpp>

namespace rr::assets {
namespace {

constexpr std::string_view kCircuitsKey = "circuit";
constexpr std::size_t kBossSlot = 1;

using Followers = std::map<std::string, std::string, std::less<>>;

bool IsCircuitName(std::string_view name) {
    return !name.empty() && std::ranges::all_of(name, [](char letter) {
        return (letter >= 'a' && letter <= 'z') || (letter >= '0' && letter <= '9') ||
               letter == '-' || letter == '_';
    });
}

std::optional<CircuitRace> RaceOf(const toml::node& item) {
    if (const std::optional<std::string> track = item.value_exact<std::string>()) {
        return CircuitRace{.track = ToUpperAscii(*track)};
    }
    const toml::table* table = item.as_table();
    if (table == nullptr) {
        return std::nullopt;
    }
    const std::optional<std::string> track = (*table)["track"].value_exact<std::string>();
    const toml::node* mirrored = table->get("mirrored");
    const std::optional<bool> isMirrored =
        mirrored == nullptr ? std::optional(false) : mirrored->value_exact<bool>();
    if (!track || !isMirrored) {
        return std::nullopt;
    }
    return CircuitRace{.track = ToUpperAscii(*track), .isMirrored = *isMirrored};
}

class PatchReader {
public:
    explicit PatchReader(const toml::table& table) : table_(&table) {}

    std::optional<std::string> Text(std::string_view key) {
        const toml::node* node = table_->get(key);
        if (node == nullptr) {
            return std::nullopt;
        }
        std::optional<std::string> value = node->value_exact<std::string>();
        if (!value) {
            Complain(std::format("{} must be text", key));
        }
        return value;
    }

    std::optional<std::vector<std::string>> Grid() {
        const toml::node* node = table_->get("grid");
        if (node == nullptr) {
            return std::nullopt;
        }
        const toml::array* codes = node->as_array();
        if (codes == nullptr || codes->empty() || codes->size() > kGridSlots ||
            !codes->is_homogeneous(toml::node_type::string)) {
            Complain(std::format("grid must list 1 to {} driver codes", kGridSlots));
            return std::nullopt;
        }
        std::vector<std::string> grid;
        codes->for_each([&](const toml::value<std::string>& code) { grid.push_back(code.get()); });
        return grid;
    }

    std::optional<std::vector<CircuitRace>> Races() {
        const toml::node* node = table_->get("races");
        if (node == nullptr) {
            return std::nullopt;
        }
        const toml::array* list = node->as_array();
        if (list == nullptr || list->empty() || list->size() > kCircuitRaces) {
            Complain(std::format("races must list 1 to {} races", kCircuitRaces));
            return std::nullopt;
        }
        std::vector<CircuitRace> races;
        for (const toml::node& item : *list) {
            std::optional<CircuitRace> race = RaceOf(item);
            if (!race) {
                Complain("each race must be a track folder or a table with track and mirrored");
                return std::nullopt;
            }
            races.push_back(std::move(*race));
        }
        return races;
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

Result<CircuitPatch> ParsePatch(const toml::table& table) {
    PatchReader reader(table);
    CircuitPatch patch{.name = reader.Text("name").value_or(std::string()),
                       .title = reader.Text("title"),
                       .boss = reader.Text("boss"),
                       .grid = reader.Grid(),
                       .races = reader.Races(),
                       .after = reader.Text("after"),
                       .partSet = reader.Text("part_set"),
                       .intro = reader.Text("intro")};
    if (reader.Problem()) {
        return Fail(ErrorCode::InvalidData, *reader.Problem());
    }
    if (!IsCircuitName(patch.name)) {
        return Fail(ErrorCode::InvalidData, "name must be lowercase letters, digits, - or _");
    }
    return patch;
}

std::optional<RaceEntry> ModelRace(std::span<const RaceEntry> races, const CircuitRace& race) {
    std::optional<RaceEntry> model;
    for (const RaceEntry& entry : races) {
        if (ToUpperAscii(entry.folder) != race.track) {
            continue;
        }
        if (entry.isMirrored == race.isMirrored) {
            return entry;
        }
        if (!model) {
            model = entry;
        }
    }
    return model;
}

void AddRaces(const std::string& circuit, std::span<const CircuitRace> added,
              std::vector<RaceEntry>& races) {
    for (std::size_t position = 0; position < added.size(); ++position) {
        const CircuitRace& race = added[position];
        const std::optional<RaceEntry> model = ModelRace(races, race);
        races.push_back({.name = std::format("{}:{}", circuit, position),
                         .folder = race.track,
                         .circuit = circuit,
                         .position = static_cast<std::int32_t>(position),
                         .nameLine = model ? model->nameLine : std::nullopt,
                         .isMirrored = race.isMirrored,
                         .previewScene = model ? model->previewScene : std::nullopt,
                         .previewDriver = model ? model->previewDriver : std::nullopt});
    }
}

void Patch(const CircuitPatch& patch, CircuitEntry& circuit) {
    if (patch.title) {
        circuit.title = *patch.title;
    }
    if (patch.grid) {
        circuit.grid = *patch.grid;
        circuit.boss = circuit.grid.size() > kBossSlot ? circuit.grid[kBossSlot] : std::string();
    }
    if (patch.boss) {
        circuit.boss = *patch.boss;
    }
    if (patch.partSet) {
        circuit.namedPartSet = patch.partSet;
    }
    if (patch.intro) {
        circuit.intro = patch.intro;
    }
}

Result<void> Replace(const CircuitPatch& patch, CircuitEntry& circuit,
                     std::vector<RaceEntry>& races) {
    if (patch.after) {
        return Fail(ErrorCode::InvalidData, "after only places a new circuit");
    }
    Patch(patch, circuit);
    if (patch.races) {
        for (RaceEntry& race : races) {
            if (race.circuit == circuit.name) {
                race.circuit.reset();
            }
        }
        AddRaces(circuit.name, *patch.races, races);
    }
    return {};
}

bool Follows(const Followers& followers, std::string_view circuit, std::string_view target) {
    for (auto follower = followers.find(circuit); follower != followers.end();
         follower = followers.find(follower->second)) {
        if (follower->second == target) {
            return true;
        }
    }
    return false;
}

Result<void> Insert(const CircuitPatch& patch, std::vector<CircuitEntry>& circuits,
                    std::vector<RaceEntry>& races, Followers& followers) {
    if (!patch.after || !patch.title || !patch.grid || !patch.races) {
        return Fail(ErrorCode::InvalidData, "a new circuit needs after, title, grid and races");
    }
    auto before = std::ranges::find(circuits, *patch.after, &CircuitEntry::name);
    if (before == circuits.end()) {
        return Fail(ErrorCode::NotFound, std::format("after names no circuit {}", *patch.after));
    }
    while (before->next && Follows(followers, *before->next, *patch.after)) {
        before = std::ranges::find(circuits, *before->next, &CircuitEntry::name);
    }
    CircuitEntry circuit{.name = patch.name,
                         .number = static_cast<std::int32_t>(circuits.size()),
                         .next = before->next,
                         .isFromMod = true};
    Patch(patch, circuit);
    before->next = patch.name;
    followers.emplace(patch.name, *patch.after);
    circuits.push_back(std::move(circuit));
    AddRaces(patch.name, *patch.races, races);
    return {};
}

void AddMissingParts(const RaceCatalog& catalog, std::size_t circuit,
                     const std::vector<std::string>& tracks, const RacerCatalog* racers,
                     std::vector<std::string>& problems) {
    const CircuitEntry& entry = catalog.Circuits()[circuit];
    for (const std::optional<std::size_t>& slot : catalog.RacesOf(circuit)) {
        const std::string folder = slot ? ToUpperAscii(catalog.Races()[*slot].folder) : "";
        if (slot && !std::ranges::contains(tracks, folder)) {
            problems.push_back(std::format("{}: circuit {}: there's no track {}", kCircuitTablePath,
                                           entry.name, folder));
        }
    }
    std::vector<std::string> codes = entry.grid;
    if (!entry.boss.empty()) {
        codes.push_back(entry.boss);
    }
    for (const std::string& code : codes) {
        if (racers != nullptr && !racers->Find(code)) {
            problems.push_back(std::format("{}: circuit {}: there's no driver {}",
                                           kCircuitTablePath, entry.name, code));
        }
    }
}

}

Result<std::vector<CircuitPatch>> ParseCircuitTable(std::string_view text,
                                                    std::string_view sourceName) {
    toml::parse_result parsed = toml::parse(text, sourceName);
    if (!parsed) {
        return Fail(ErrorCode::InvalidData,
                    std::format("{} line {}: {}", sourceName, parsed.error().source().begin.line,
                                parsed.error().description()));
    }
    const toml::node* node = parsed.table().get(kCircuitsKey);
    if (node == nullptr) {
        return std::vector<CircuitPatch>{};
    }
    const toml::array* list = node->as_array();
    if (list == nullptr || (!list->empty() && !list->is_homogeneous(toml::node_type::table))) {
        return Fail(ErrorCode::InvalidData,
                    std::format("{}: circuits must be [[circuit]] tables", sourceName));
    }
    std::vector<CircuitPatch> patches;
    for (std::size_t index = 0; index < list->size(); ++index) {
        Result<CircuitPatch> patch = ParsePatch(*list->get(index)->as_table());
        if (!patch) {
            return Fail(patch.error().code, std::format("{}: circuit {}: {}", sourceName, index + 1,
                                                        patch.error().message));
        }
        patches.push_back(std::move(*patch));
    }
    return patches;
}

Result<CircuitTables> ReadCircuitTables(const Vfs& vfs) {
    Result<std::vector<FileData>> files = vfs.ReadEveryLayer(kCircuitTablePath);
    if (!files) {
        return std::unexpected(std::move(files.error()));
    }
    CircuitTables tables;
    for (const FileData& file : *files) {
        const std::span<const std::byte> bytes = file.Bytes();
        Result<std::vector<CircuitPatch>> patches = ParseCircuitTable(
            std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size()),
            kCircuitTablePath);
        if (patches) {
            std::ranges::move(*patches, std::back_inserter(tables.patches));
        } else {
            tables.problems.push_back(std::move(patches.error().message));
        }
    }
    return tables;
}

std::vector<std::string> ApplyCircuitPatches(std::span<const CircuitPatch> patches,
                                             std::vector<CircuitEntry>& circuits,
                                             std::vector<RaceEntry>& races) {
    std::vector<std::string> problems;
    Followers followers;
    for (const CircuitPatch& patch : patches) {
        const auto circuit = std::ranges::find(circuits, patch.name, &CircuitEntry::name);
        const Result<void> applied = circuit == circuits.end()
                                         ? Insert(patch, circuits, races, followers)
                                         : Replace(patch, *circuit, races);
        if (!applied) {
            problems.push_back(std::format("{}: circuit {}: {}", kCircuitTablePath, patch.name,
                                           applied.error().message));
        }
    }
    return problems;
}

std::vector<std::string> CircuitTableProblems(const Vfs& vfs) {
    std::vector<std::string> problems;
    const Result<RaceCatalog> catalog = RaceCatalog::Load(vfs, problems);
    if (!catalog) {
        problems.push_back(catalog.error().message);
        return problems;
    }
    std::set<std::string, std::less<>> patched;
    if (const Result<CircuitTables> tables = ReadCircuitTables(vfs)) {
        for (const CircuitPatch& patch : tables->patches) {
            patched.insert(patch.name);
        }
    }
    const std::vector<std::string> tracks = ListRaceTracks(vfs);
    const Result<RacerCatalog> racers = RacerCatalog::Load(vfs);
    for (std::size_t circuit = 0; circuit < catalog->Circuits().size(); ++circuit) {
        if (patched.contains(catalog->Circuits()[circuit].name)) {
            AddMissingParts(*catalog, circuit, tracks, racers ? &*racers : nullptr, problems);
        }
    }
    return problems;
}

}
