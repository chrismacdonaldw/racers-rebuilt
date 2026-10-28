#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <filesystem>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "assets/jam_archive.hpp"
#include "assets/token_reader.hpp"
#include "game_files.hpp"

namespace rr::assets {
namespace {

constexpr std::array<std::string_view, 38> kTokenizedExtensions{
    "ADB", "BDB", "BVB", "CCB", "CDB", "CEB", "CMB", "CPB", "CRB", "DDB", "EMB", "EVB", "FDB",
    "GCB", "GDB", "GHB", "HZB", "IDB", "LEB", "LSB", "MAB", "MDB", "MIB", "MSB", "PCB", "PWB",
    "RAB", "RCB", "RRB", "SDB", "SKB", "SPB", "TDB", "TGB", "TIB", "TMB", "TRB", "WDB",
};
constexpr std::size_t kRetailTokenizedFileCount = 2819;

bool IsTokenized(std::string_view name) {
    const std::size_t dot = name.rfind('.');
    return dot != std::string_view::npos &&
           std::ranges::contains(kTokenizedExtensions, name.substr(dot + 1));
}

Result<void> TokenizeToEnd(std::span<const std::byte> bytes) {
    TokenReader reader(bytes);
    int depth = 0;
    while (true) {
        Result<Token> token = reader.Next();
        if (!token) {
            return std::unexpected(std::move(token.error()));
        }
        switch (token->kind) {
            case TokenKind::OpenBrace:
            case TokenKind::OpenBracket:
                ++depth;
                break;
            case TokenKind::CloseBrace:
            case TokenKind::CloseBracket:
                --depth;
                break;
            case TokenKind::End:
                if (depth != 0) {
                    return Fail(ErrorCode::InvalidData, "unbalanced delimiters");
                }
                return {};
            default:
                break;
        }
        if (depth < 0) {
            return Fail(ErrorCode::InvalidData,
                        std::format("unopened delimiter at offset {}", token->offset));
        }
    }
}

struct Survey {
    std::size_t fileCount = 0;
    std::vector<std::string> failures;
};

Survey TokenizeArchive(const JamArchive& archive) {
    Survey survey;
    for (const IndexEntry& entry : archive.Index().Entries()) {
        if (entry.info.kind != EntryKind::File || !IsTokenized(entry.Name())) {
            continue;
        }
        ++survey.fileCount;
        const Result<void> tokenized = archive.Read(entry.info).and_then([](const FileData& data) {
            return TokenizeToEnd(data.Bytes());
        });
        if (!tokenized) {
            survey.failures.push_back(std::format("{}: {}", entry.path, tokenized.error().message));
        }
    }
    return survey;
}

TEST_CASE("every tokenized file in the retail LEGO.JAM decodes to the end") {
    const std::optional<std::filesystem::path> gameFolder = test::GameFolder();
    if (!gameFolder) {
        MESSAGE("RR_GAME_PATH isn't set, skipping");
        return;
    }
    const Result<JamArchive> archive = JamArchive::Open(*gameFolder / "LEGO.JAM");
    REQUIRE(archive.has_value());

    const Survey survey = TokenizeArchive(*archive);

    CHECK(survey.fileCount == kRetailTokenizedFileCount);
    for (const std::string& failure : survey.failures) {
        FAIL_CHECK(failure);
    }
}

}
}
